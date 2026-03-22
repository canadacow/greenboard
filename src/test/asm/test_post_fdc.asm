; test_post_fdc.asm -- BIOS POST TEST.14: FDC recalibrate + seek
; Mirrors the real IBM PC BIOS TEST.14 / SEEK from PCBIOS.ASM.
; Requires: ISA FDC (NEC uPD765), 8259A PIC, 8253 PIT
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Sequence (matching BIOS):
;   1. Motor on (DOR = 0x1C: motor A on, FDC enable, DMA/IRQ enable)
;   2. Recalibrate drive 0 (NEC cmd 0x07)
;   3. Wait for IRQ 6
;   4. Sense interrupt status (NEC cmd 0x08) -- verify ST0 OK, PCN=0
;   5. Seek to track 34 (NEC cmd 0x0F, drive 0, cylinder 34)
;   6. Wait for IRQ 6
;   7. Sense interrupt status -- verify PCN=0x22 (34)
;
; Expected results:
;   [0500] = 0x0001   Recalibrate OK (ST0 seek end, PCN=0)
;   [0502] = 0x0022   Seek PCN (34 decimal = 0x22)

; @name POST FDC (TEST.14)
; @expect 0500 0001 Recalibrate OK
; @expect 0502 0022 Seek PCN=34
;
cpu 8086
org 0x0100

%define FDC_DOR   0x3F2
%define FDC_MSR   0x3F4
%define FDC_FIFO  0x3F5
%define PIC_CMD   0x20
%define PIC_DATA  0x21

; =====================================================================
; Initialize
; =====================================================================
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results
    mov word [0x0500], 0x0000
    mov word [0x0502], 0x0000

    ; IRQ 6 indicator
    mov byte [0x05E0], 0

    ; Install IRQ 6 handler (INT 14 with PIC base 8)
    mov word [14*4],   irq6_handler
    mov word [14*4+2], 0x0100

    ; Initialize PIC
    mov al, 0x13            ; ICW1: edge, single, ICW4
    out PIC_CMD, al
    mov al, 0x08            ; ICW2: vector base 8
    out PIC_DATA, al
    mov al, 0x01            ; ICW4: 8086 mode
    out PIC_DATA, al
    mov al, 0xBF            ; unmask IRQ6 only (clear bit 6)
    out PIC_DATA, al

; =====================================================================
; Motor on + FDC enable
; =====================================================================
    mov al, 0x1C            ; motor A on, FDC enable, DMA/IRQ enable
    mov dx, FDC_DOR
    out dx, al

    ; Motor spin-up delay (BIOS does two 65536-iteration loops)
    xor cx, cx
.motor1:
    loop .motor1
.motor2:
    loop .motor2

; =====================================================================
; Test 1: Recalibrate drive 0
; =====================================================================
    ; Send RECALIBRATE command (0x07, drive 0)
    mov ah, 0x07
    call nec_out
    jc .done                ; MSR timeout
    mov ah, 0x00            ; drive 0
    call nec_out
    jc .done

    ; Wait for IRQ 6
    call wait_irq6
    jc .done                ; timeout

    ; Sense interrupt status
    mov ah, 0x08
    call nec_out
    jc .done
    ; Read ST0
    call nec_in
    jc .done
    mov bl, al              ; save ST0
    ; Read PCN
    call nec_in
    jc .done
    mov bh, al              ; save PCN

    ; Verify: ST0 bits 7-6 = 00 (normal), bit 5 = 1 (seek end). PCN = 0.
    and bl, 0xE0            ; isolate IC + SE bits
    cmp bl, 0x20            ; seek end, normal termination
    jne .test2
    cmp bh, 0x00            ; PCN = 0
    jne .test2
    mov word [0x0500], 0x0001

; =====================================================================
; Test 2: Seek to track 34
; =====================================================================
.test2:
    mov byte [0x05E0], 0    ; clear IRQ indicator

    ; Send SEEK command (0x0F, drive 0, cylinder 34)
    mov ah, 0x0F
    call nec_out
    jc .done
    mov ah, 0x00            ; drive 0
    call nec_out
    jc .done
    mov ah, 34              ; cylinder 34
    call nec_out
    jc .done

    ; Wait for IRQ 6
    call wait_irq6
    jc .done

    ; Sense interrupt status
    mov ah, 0x08
    call nec_out
    jc .done
    ; Read ST0
    call nec_in
    jc .done
    ; Read PCN
    call nec_in
    jc .done
    ; AL = PCN, should be 34 (0x22)
    xor ah, ah
    mov [0x0502], ax

.done:
    ; Motor off
    mov al, 0x0C            ; FDC enable, DMA/IRQ enable, motor off
    mov dx, FDC_DOR
    out dx, al
    cli
    hlt

; =====================================================================
; nec_out: send AH to FDC data register
; Polls MSR until RQM=1 and DIO=0 (ready for CPU->FDC).
; Returns: CF=0 success, CF=1 timeout
; =====================================================================
nec_out:
    push cx
    xor cx, cx              ; 65536 attempts
.nec_out_poll:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80           ; RQM?
    jz .nec_out_next
    test al, 0x40           ; DIO must be 0 (CPU->FDC)
    jnz .nec_out_next
    ; Ready -- write command byte
    mov dx, FDC_FIFO
    mov al, ah
    out dx, al
    pop cx
    clc
    ret
.nec_out_next:
    loop .nec_out_poll
    pop cx
    stc
    ret

; =====================================================================
; nec_in: read one byte from FDC data register
; Polls MSR until RQM=1 and DIO=1 (FDC->CPU).
; Returns: AL = data byte, CF=0 success, CF=1 timeout
; =====================================================================
nec_in:
    push cx
    xor cx, cx
.nec_in_poll:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80           ; RQM?
    jz .nec_in_next
    test al, 0x40           ; DIO must be 1 (FDC->CPU)
    jz .nec_in_next
    mov dx, FDC_FIFO
    in al, dx
    pop cx
    clc
    ret
.nec_in_next:
    loop .nec_in_poll
    pop cx
    stc
    ret

; =====================================================================
; wait_irq6: wait for IRQ 6 with timeout
; Returns: CF=0 success, CF=1 timeout
; =====================================================================
wait_irq6:
    sti
    push cx
    mov cx, 8               ; outer loop
.wait_outer:
    push cx
    xor cx, cx              ; 65536 inner iterations
.wait_inner:
    cmp byte [0x05E0], 0
    jne .wait_got
    loop .wait_inner
    pop cx
    loop .wait_outer
    pop cx
    stc                     ; timeout
    ret
.wait_got:
    pop cx                  ; discard inner
    pop cx                  ; discard outer
    mov byte [0x05E0], 0    ; reset indicator
    clc
    ret

; =====================================================================
; IRQ 6 handler (INT 14)
; =====================================================================
irq6_handler:
    push ax
    mov byte [0x05E0], 0xFF
    mov al, 0x20
    out PIC_CMD, al         ; EOI
    pop ax
    iret
