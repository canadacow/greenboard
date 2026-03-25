; test_io_floppy.asm -- Combined I/O port test + FDC boot sector read
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
; SP initialized to 0x0800.
;
; Runs the I/O port round-trip and PIC tests first, then without rebooting,
; programs DMA channel 2 and reads boot sector 1 from the floppy controller.
; This exercises the real interaction: the I/O test writes to port 0x81
; (DMA ch2 page register), and the floppy setup must reprogram it.
;
; Part 1 results (I/O):
;   [0500] = 0x00AB   OUT imm8 / IN imm8 byte round-trip (port 0x80)
;   [0502] = 0x00CD   OUT DX / IN DX byte round-trip (port 0x81)
;   [0504] = 0xBEEF   OUT/IN word round-trip (port 0x82)
;   [0506] = 0x00F7   PIC IMR readback after unmasking IRQ3
;   [0508] = 0x0001   IRQ3 fired (INT 11 handler ran)
;   [050A] = 0x000B   INT 11 handler received correct vector
;   [050C] = 0x0001   EOI cleared ISR
;   [050E] = 0x0001   I/O write doesn't corrupt memory
;
; Part 2 results (Floppy):
;   [0510] = 0x0080   MSR shows RQM after reset
;   [0512] = 0x0001   Command phase complete
;   [0514] = 0x0001   IRQ 6 fired
;   [0516] = 0x0001   DMA buffer non-zero
;   [0518] = 0x34EB   First word of boot sector
;   [051A] = 0xAA55   Boot signature (0x55AA little-endian)

; @name I/O + Floppy (combined)
; @expect 0500 00AB OUT imm8 / IN imm8 byte
; @expect 0502 00CD OUT DX / IN DX byte
; @expect 0504 BEEF OUT/IN word
; @expect 0506 00F7 PIC IMR readback
; @expect 0508 0001 Timer IRQ3 -> INT 11
; @expect 050A 000B INT 11 vector correct
; @expect 050C 0001 EOI clears ISR
; @expect 050E 0001 I/O doesn't touch memory
; @expect 0510 0080 MSR shows RQM after reset
; @expect 0512 0001 Command phase complete
; @expect 0514 0001 IRQ 6 fired
; @expect 0516 0001 DMA buffer non-zero
; @expect 0518 34EB First word of boot sector
; @expect 051A AA55 Boot signature
;
cpu 8086
org 0x0100

; DMA channel 2 ports
FDC_DMA_ADDR equ 0x04
FDC_DMA_CNT  equ 0x05
FDC_DMA_PAGE equ 0x81      ; channel 2 page register (port 0x81 per BIOS)
DMA_MASK     equ 0x0A
DMA_MODE     equ 0x0B
DMA_FLIPFLOP equ 0x0C
DMA_MASTER   equ 0x0D

; FDC I/O ports
FDC_DOR  equ 0x3F2
FDC_MSR  equ 0x3F4
FDC_FIFO equ 0x3F5

; DMA buffer
DMA_BUF_PAGE   equ 0x00
DMA_BUF_OFFSET equ 0x3000
DMA_BUF_LEN    equ 512

; =====================================================================
; Initialize
; =====================================================================
mov ax, 0x0000
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x0800

; Zero all results
mov di, 0x0500
mov cx, 16              ; 32 bytes = 16 words
xor ax, ax
rep stosw
mov byte [0x05F0], 0x00         ; generic IRQ6 flag

; #####################################################################
; PART 1: I/O PORT TESTS
; #####################################################################

; =====================================================================
; Test 1: OUT imm8, AL / IN AL, imm8 -- byte round-trip via port 0x80
; =====================================================================
mov al, 0xAB
out 0x80, al
xor ax, ax
in al, 0x80
mov ah, 0
mov [0x0500], ax

; =====================================================================
; Test 2: OUT DX, AL / IN AL, DX -- byte round-trip via DX=0x81
; This writes 0xCD to port 0x81 which is also the DMA ch2 page register.
; =====================================================================
mov dx, 0x0081
mov al, 0xCD
out dx, al
xor ax, ax
in al, dx
mov ah, 0
mov [0x0502], ax

; =====================================================================
; Test 3: OUT imm8, AX / IN AX, imm8 -- word round-trip via port 0x82
; =====================================================================
mov ax, 0xBEEF
out 0x82, ax
xor ax, ax
in ax, 0x82
mov [0x0504], ax

; =====================================================================
; Test 4-7: PIC initialization + IRQ3 interrupt
; =====================================================================
mov word [0x002C], irq3_handler
mov word [0x002E], 0x0100

mov al, 0x13
out 0x20, al
mov al, 0x08
out 0x21, al
mov al, 0x01
out 0x21, al

mov al, 0xF7
out 0x21, al

in al, 0x21
mov ah, 0
mov [0x0506], ax

sti
mov al, 0x08
out 0xF0, al
nop
nop
nop
nop
cli

; =====================================================================
; Test 8: I/O doesn't corrupt memory
; =====================================================================
mov al, 0x77
out 0x90, al
cmp byte [0x0090], 0xF4
jne .io_mem_fail
mov word [0x050E], 0x0001
.io_mem_fail:

; #####################################################################
; PART 2: FLOPPY DISK CONTROLLER
; The DMA ch2 page register (port 0x81) currently holds 0xCD from
; test 2 above. The floppy setup must reprogram it to 0x00.
; #####################################################################

; Zero DMA target buffer
mov di, DMA_BUF_OFFSET
mov cx, DMA_BUF_LEN / 2
xor ax, ax
rep stosw

; Install IRQ 6 handler (INT 14 with PIC vector base 8)
mov word [14*4],   irq6_handler
mov word [14*4+2], 0x0100

; Re-init PIC: unmask IRQ6
mov al, 0x13
out 0x20, al
mov al, 0x08
out 0x21, al
mov al, 0x01
out 0x21, al
mov al, 0xBF                    ; unmask IRQ6
out 0x21, al

; DMA channel 2 setup
mov al, 0x00
out DMA_MASTER, al

mov al, 0x06
out DMA_MASK, al

mov al, 0x00
out DMA_FLIPFLOP, al

mov al, DMA_BUF_OFFSET & 0xFF
out FDC_DMA_ADDR, al
mov al, (DMA_BUF_OFFSET >> 8) & 0xFF
out FDC_DMA_ADDR, al

mov al, 0x00
out DMA_FLIPFLOP, al

mov al, (DMA_BUF_LEN - 1) & 0xFF
out FDC_DMA_CNT, al
mov al, ((DMA_BUF_LEN - 1) >> 8) & 0xFF
out FDC_DMA_CNT, al

; Page register: 0x00 (overwrites the 0xCD from test 2)
mov al, DMA_BUF_PAGE
out FDC_DMA_PAGE, al

mov al, 0x46
out DMA_MODE, al

mov al, 0x02
out DMA_MASK, al

; Reset FDC
mov dx, FDC_DOR
mov al, 0x00
out dx, al

mov cx, 10
.reset_delay:
    nop
    loop .reset_delay

mov dx, FDC_DOR
mov al, 0x0C
out dx, al

; Handle post-reset IRQ6
sti
mov cx, 0xFFFF
.reset_irq_wait:
    cmp byte [0x05F0], 0
    jne .reset_irq_got
    loop .reset_irq_wait
.reset_irq_got:
    cli
    mov byte [0x05F0], 0
    mov word [0x0514], 0x0000   ; reset data-transfer IRQ flag

; SENSE INTERRUPT STATUS after reset
mov dx, FDC_MSR
mov cx, 50
.sense_wait1:
    in al, dx
    test al, 0x80
    jnz .sense_rdy1
    loop .sense_wait1
    jmp .sense_done
.sense_rdy1:
    test al, 0x40
    jnz .sense_done
    mov al, 0x08
    mov dx, FDC_FIFO
    out dx, al
    ; Read ST0
    mov dx, FDC_MSR
    mov cx, 50
.sense_wait2:
    in al, dx
    test al, 0x80
    jz .sense_wait2l
    test al, 0x40
    jnz .sense_read_st0
.sense_wait2l:
    loop .sense_wait2
    jmp .sense_done
.sense_read_st0:
    mov dx, FDC_FIFO
    in al, dx
    ; Read PCN
    mov dx, FDC_MSR
    mov cx, 50
.sense_wait3:
    in al, dx
    test al, 0x80
    jz .sense_wait3l
    test al, 0x40
    jnz .sense_read_pcn
.sense_wait3l:
    loop .sense_wait3
    jmp .sense_done
.sense_read_pcn:
    mov dx, FDC_FIFO
    in al, dx
.sense_done:

; Read MSR
mov dx, FDC_MSR
in al, dx
xor ah, ah
mov [0x0510], ax

test al, 0x80
jz .no_fdc

; Enable motor
mov dx, FDC_DOR
mov al, 0x1C
out dx, al

mov cx, 100
.motor_delay:
    nop
    loop .motor_delay

; Send READ DATA command
push ds
mov ax, 0x0100
mov ds, ax
mov si, read_cmd
mov cx, 9

.send_cmd:
    push cx
    mov cx, 50
.poll_msr:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80
    jnz .rqm_ready
    loop .poll_msr
    pop cx
    jmp .cmd_timeout
.rqm_ready:
    pop cx
    test al, 0x40
    jnz .cmd_timeout_ds

    lodsb
    mov dx, FDC_FIFO
    out dx, al
    loop .send_cmd

    pop ds
    mov word [0x0512], 0x0001
    jmp .wait_irq

.cmd_timeout_ds:
.cmd_timeout:
    pop ds
    jmp .wait_irq

; Wait for IRQ 6
.wait_irq:
    sti
    mov cx, 0xFFFF
.irq_wait_loop:
    cmp word [0x0514], 0x0001
    je .irq_done
    nop
    loop .irq_wait_loop
.irq_done:
    cli

; Check DMA buffer
.no_fdc:
    mov si, DMA_BUF_OFFSET
    mov cx, DMA_BUF_LEN
    xor dx, dx
.check_buf:
    lodsb
    or dl, al
    loop .check_buf

    test dl, dl
    jz .buf_empty
    mov word [0x0516], 0x0001
.buf_empty:

    mov ax, [DMA_BUF_OFFSET]
    mov [0x0518], ax
    mov ax, [DMA_BUF_OFFSET + 510]
    mov [0x051A], ax

    hlt

; =====================================================================
; Data
; =====================================================================
read_cmd:
    db 0x46, 0x00, 0x00, 0x00, 0x01, 0x02, 0x09, 0x2A, 0xFF

; =====================================================================
; IRQ3 handler (INT 11) -- I/O test
; =====================================================================
irq3_handler:
    mov word [0x0508], 0x0001
    mov word [0x050A], 0x000B
    mov al, 0x08
    out 0xF1, al
    mov al, 0x20
    out 0x20, al
    mov al, 0x0B
    out 0x20, al
    in al, 0x20
    cmp al, 0x00
    jne .eoi_fail
    mov word [0x050C], 0x0001
.eoi_fail:
    iret

; =====================================================================
; IRQ6 handler (INT 14) -- Floppy test
; =====================================================================
irq6_handler:
    push ax
    mov word [0x0514], 0x0001   ; data-transfer IRQ flag
    mov byte [0x05F0], 0xFF     ; generic IRQ6 flag
    mov al, 0x20
    out 0x20, al
    pop ax
    iret
