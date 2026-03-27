; test_post_fdc.asm -- BIOS POST TEST.14: FDC reset, recalibrate, seek
; Faithful reproduction of the real IBM PC BIOS TEST.14 from PCBIOS.ASM.
; Requires: ISA FDC (NEC uPD765), 8259A PIC, 8253 PIT
; Loaded at 0100:0100 (physical 0x01100). DS=0 after setup.
;
; Sequence (matching BIOS exactly):
;   1. DISK_RESET: DOR reset toggle, wait IRQ6, SENSE INT (expect 0xC0),
;      SPECIFY (03h, 0xCF, 0x02)
;   2. Motor on (DOR = 0x1C), spin-up delay
;   3. SEEK track 1: since SEEK_STATUS=0, RECALIBRATE first (07h),
;      wait IRQ6, SENSE INT (expect 0x20), then SEEK (0Fh, cyl 1),
;      wait IRQ6, SENSE INT (expect PCN=1)
;   4. SEEK track 34: SEEK (0Fh, cyl 34), wait IRQ6, SENSE INT
;      (expect PCN=0x22)
;   5. Motor off
;
; Expected results:
;   [0500] = 0x0001   DISK_RESET OK (SENSE INT returned 0xC0)
;   [0502] = 0x0001   Recalibrate OK (ST0=0x20, PCN=0)
;   [0504] = 0x0001   Seek to track 1 OK (ST0=0x20, PCN=1)
;   [0506] = 0x0022   Seek to track 34 PCN (34 = 0x22)

; @name POST FDC (TEST.14)
; @expect 0500 0001 DISK_RESET OK
; @expect 0502 0001 Recalibrate OK
; @expect 0504 0001 Seek track 1 OK
; @expect 0506 0022 Seek PCN=34

cpu 8086
org 0x0100

%define FDC_DOR   0x3F2
%define FDC_MSR   0x3F4
%define FDC_FIFO  0x3F5
%define PIC_CMD   0x20
%define PIC_DATA  0x21

; BIOS data area locations (in segment 0)
%define SEEK_STATUS  0x05E0     ; bit 0 = drive 0 recal done, bit 7 = IRQ flag
%define RESULT_RESET 0x0500
%define RESULT_RECAL 0x0502
%define RESULT_SEEK1 0x0504
%define RESULT_SEEK2 0x0506

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
    mov word [RESULT_RESET], 0x0000
    mov word [RESULT_RECAL], 0x0000
    mov word [RESULT_SEEK1], 0x0000
    mov word [RESULT_SEEK2], 0x0000

    ; Clear SEEK_STATUS (all drives need recal, no IRQ pending)
    mov byte [SEEK_STATUS], 0

    ; Install IRQ 6 handler (PIC base 8, so IRQ6 = INT 0x0E)
    mov word [0x0E*4],     irq6_handler
    mov word [0x0E*4+2],   0x0100

    ; Initialize PIC: edge-triggered, single, ICW4, vector base 8
    mov al, 0x13            ; ICW1: edge, single, ICW4
    out PIC_CMD, al
    mov al, 0x08            ; ICW2: vector base 8
    out PIC_DATA, al
    mov al, 0x01            ; ICW4: 8086 mode
    out PIC_DATA, al
    mov al, 0xBC            ; unmask IRQ6, IRQ1 (keyboard), IRQ0 (timer)
    out PIC_DATA, al

; =====================================================================
; Step 1: DISK_RESET (matches BIOS DISK_RESET proc)
; Toggle DOR reset, wait for IRQ6, SENSE INT, expect ST0=0xC0, SPECIFY
; =====================================================================

    ; DOR: motor off, IRQ enable, reset ACTIVE (bit 2 = 0)
    ; BIOS: AL = motor_status << 4 | drive_select | 0x08
    ; For drive 0, no motors: AL = 0x08
    mov dx, FDC_DOR
    mov al, 0x08            ; IRQ enable, reset active
    cli
    out dx, al

    ; DOR: turn off reset (bit 2 = 1)
    or al, 0x04             ; AL = 0x0C
    out dx, al
    sti

    ; Wait for IRQ 6 (CHK_STAT_2 calls WAIT_INT)
    call wait_irq6
    jc .reset_fail

    ; SENSE INTERRUPT STATUS (0x08)
    mov ah, 0x08
    call nec_out
    jc .reset_fail

    ; Read results (BIOS RESULTS proc reads until NEC not busy)
    ; SENSE INT returns 2 bytes: ST0, PCN
    call nec_in             ; ST0
    jc .reset_fail
    mov bl, al              ; save ST0

    call nec_in             ; PCN (ignored)
    jc .reset_fail

    ; BIOS checks: CMP AL,0C0H (ST0 = drive ready transition)
    cmp bl, 0xC0
    jne .reset_fail

    ; SPECIFY command (03h, 0xCF, 0x02) -- BIOS DISK_BASE bytes 0-1
    mov ah, 0x03
    call nec_out
    jc .reset_fail
    mov ah, 0xCF            ; SRT=12ms, HUT=240ms
    call nec_out
    jc .reset_fail
    mov ah, 0x02            ; HLD=16ms, DMA mode
    call nec_out
    jc .reset_fail

    mov word [RESULT_RESET], 0x0001

.reset_fail:

; =====================================================================
; Step 2: Motor on + spin-up delay
; BIOS: DOR = 0x1C (motor A on, FDC enable, DMA/IRQ enable, drive 0)
; =====================================================================
    mov al, 0x1C
    mov dx, FDC_DOR
    out dx, al

    ; Motor spin-up delay (BIOS does two 65536-iteration loops)
    xor cx, cx
.motor1:
    loop .motor1
.motor2:
    loop .motor2

; =====================================================================
; Step 3: SEEK to track 1 (BIOS: MOV CH,1 / CALL SEEK)
; SEEK proc checks SEEK_STATUS -- bit 0 clear means recal needed first
; =====================================================================

    ; --- BIOS SEEK proc: recalibrate (SEEK_STATUS bit 0 = 0) ---

    ; Mark recal done: OR SEEK_STATUS, 01h
    or byte [SEEK_STATUS], 0x01

    ; RECALIBRATE command (07h, drive 0)
    mov ah, 0x07
    call nec_out
    jc .done
    mov ah, 0x00            ; drive 0
    call nec_out
    jc .done

    ; CHK_STAT_2: wait IRQ, SENSE INT, check ST0
    call wait_irq6
    jc .done

    mov ah, 0x08
    call nec_out
    jc .done
    call nec_in             ; ST0
    jc .done
    mov bl, al
    call nec_in             ; PCN
    jc .done
    mov bh, al

    ; BIOS CHK_STAT_2: AND AL,060H / CMP AL,060H / JZ error
    ; ST0 bits 6:5 must NOT be 11 (0x60 = abnormal + seek end = error)
    mov al, bl
    and al, 0x60
    cmp al, 0x60
    je .done                ; error

    ; Verify recal: ST0 bit 5 = seek end, PCN = 0
    test bl, 0x20
    jz .done
    cmp bh, 0x00
    jne .done
    mov word [RESULT_RECAL], 0x0001

    ; --- BIOS SEEK proc: seek to track 1 ---
    mov ah, 0x0F            ; SEEK command
    call nec_out
    jc .done
    mov ah, 0x00            ; drive 0
    call nec_out
    jc .done
    mov ah, 1               ; cylinder 1
    call nec_out
    jc .done

    ; CHK_STAT_2: wait IRQ, SENSE INT, check ST0
    call wait_irq6
    jc .done

    mov ah, 0x08
    call nec_out
    jc .done
    call nec_in             ; ST0
    jc .done
    mov bl, al
    call nec_in             ; PCN
    jc .done
    mov bh, al

    ; Check: ST0 bits 6:5 != 11, PCN = 1
    mov al, bl
    and al, 0x60
    cmp al, 0x60
    je .done
    cmp bh, 0x01
    jne .done
    mov word [RESULT_SEEK1], 0x0001

    ; Head settle delay (BIOS: 25ms via nested loop)
    mov cx, 0x3800
.settle1:
    loop .settle1

; =====================================================================
; Step 4: SEEK to track 34 (BIOS: MOV CH,34 / CALL SEEK)
; SEEK_STATUS bit 0 is now set, so no recalibrate -- straight to seek
; =====================================================================

    mov ah, 0x0F            ; SEEK command
    call nec_out
    jc .done
    mov ah, 0x00            ; drive 0
    call nec_out
    jc .done
    mov ah, 34              ; cylinder 34
    call nec_out
    jc .done

    ; CHK_STAT_2
    call wait_irq6
    jc .done

    mov ah, 0x08
    call nec_out
    jc .done
    call nec_in             ; ST0
    jc .done
    mov bl, al
    call nec_in             ; PCN
    jc .done
    mov bh, al              ; save PCN

    ; Check: ST0 bits 6:5 != 11
    mov al, bl
    and al, 0x60
    cmp al, 0x60
    je .done

    ; Store PCN (should be 0x22 = 34 decimal)
    xor ah, ah
    mov al, bh
    mov [RESULT_SEEK2], ax

.done:
    ; Motor off
    mov al, 0x0C
    mov dx, FDC_DOR
    out dx, al
    cli
    int3

; =====================================================================
; nec_out: send AH to FDC data register
; Polls MSR until RQM=1 and DIO=0 (CPU->FDC direction).
; BIOS NEC_OUTPUT: checks DIO first (must be 0), then RQM (must be 1).
; Returns: CF=0 success, CF=1 timeout
; =====================================================================
nec_out:
    push cx
    push dx
    mov dx, FDC_MSR
    ; Phase 1: wait for DIO=0 (BIOS checks direction first)
    xor cx, cx
.nec_out_dir:
    in al, dx
    test al, 0x40           ; DIO bit -- must be 0
    jz .nec_out_rqm
    loop .nec_out_dir
    jmp .nec_out_timeout

    ; Phase 2: wait for RQM=1
.nec_out_rqm:
    xor cx, cx
.nec_out_rqm_poll:
    in al, dx
    test al, 0x80           ; RQM?
    jnz .nec_out_send
    loop .nec_out_rqm_poll

.nec_out_timeout:
    pop dx
    pop cx
    stc
    ret

.nec_out_send:
    mov dx, FDC_FIFO
    mov al, ah
    out dx, al
    pop dx
    pop cx
    clc
    ret

; =====================================================================
; nec_in: read one byte from FDC data register
; Polls MSR until RQM=1 and DIO=1 (FDC->CPU direction).
; Returns: AL = data byte, CF=0 success, CF=1 timeout
; =====================================================================
nec_in:
    push cx
    push dx
    mov dx, FDC_MSR
    xor cx, cx
.nec_in_poll:
    in al, dx
    test al, 0x80           ; RQM?
    jz .nec_in_next
    test al, 0x40           ; DIO must be 1 (FDC->CPU)
    jnz .nec_in_read
.nec_in_next:
    loop .nec_in_poll
    pop dx
    pop cx
    stc
    ret
.nec_in_read:
    mov dx, FDC_FIFO
    in al, dx
    pop dx
    pop cx
    clc
    ret

; =====================================================================
; wait_irq6: wait for IRQ 6 via SEEK_STATUS bit 7 (INT_FLAG)
; Matches BIOS WAIT_INT: BL=2, CX=0 (two 65536 loops = ~2 sec timeout)
; Returns: CF=0 success, CF=1 timeout
; =====================================================================
wait_irq6:
    sti
    push bx
    push cx
    mov bl, 2
    xor cx, cx
.wait_loop:
    test byte [SEEK_STATUS], 0x80  ; INT_FLAG
    jnz .wait_got
    loop .wait_loop
    dec bl
    jnz .wait_loop
    ; timeout
    pop cx
    pop bx
    stc
    ret
.wait_got:
    pushf
    and byte [SEEK_STATUS], 0x7F   ; clear INT_FLAG
    popf
    pop cx
    pop bx
    clc
    ret

; =====================================================================
; IRQ 6 handler (INT 0x0E) -- matches BIOS DISK_INT
; Sets SEEK_STATUS bit 7 (INT_FLAG), sends EOI
; =====================================================================
irq6_handler:
    sti
    push ds
    push ax
    xor ax, ax
    mov ds, ax
    or byte [SEEK_STATUS], 0x80    ; INT_FLAG
    mov al, 0x20
    out PIC_CMD, al                ; EOI
    pop ax
    pop ds
    iret
