; test_post_sw1_readback.asm -- Read SW1 DIP switches after KBD_RESET
;
; Reproduces the exact BIOS POST sequence that fails:
; 1. Init PPI (0x99)
; 2. Write 0xFC to Port B (same as BIOS TEST.02)
; 3. KBD_RESET: pull PB6 low, delay, release, unmask IRQ1, wait timeout
; 4. Read Port A -- must return DIP switch values, not keyboard zeros
;
; This is the path from TEST.04 KBD_RESET (line 509) to TEST.08 E6
; where the BIOS reads Port A for equipment flags.
;
; Expected: Port A = 0x3D (floppy, 64K, MDA, 1 drive)

; @name SW1 readback after KBD_RESET
; @expect 0500 003D Port A after KBD_RESET
;
cpu 8086
org 0x0100

%define PORT_A  0x60
%define PORT_B  0x61
%define INTA00  0x20
%define INTA01  0x21

    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x0800
    mov word [0x0500], 0x0000

    ; Init PPI
    mov al, 0x99
    out 0x63, al

    ; Port B = 0xFC (same as BIOS TEST.02 line 330)
    mov al, 0xFC
    out PORT_B, al

    ; Init PIC
    mov al, 0x13
    out INTA00, al
    mov al, 0x08
    out INTA01, al
    mov al, 0x01
    out INTA01, al
    mov al, 0xFF
    out INTA01, al

    ; Install INT 9 handler (temp ISR that sets AH=1)
    mov word [9*4], temp_isr
    mov word [9*4+2], 0x0100

    ; KBD_RESET: pull CLK low via PB6
    mov al, 0x0C
    out PORT_B, al
    mov cx, 10582
.hold:
    loop .hold

    ; Release CLK
    mov al, 0xCC
    out PORT_B, al

    ; Enable keyboard
    mov al, 0x4C
    out PORT_B, al

    ; Unmask IRQ1
    mov al, 0xFD
    out INTA01, al
    sti

    ; Wait for keyboard interrupt (timeout)
    mov ah, 0
    xor cx, cx
.wait:
    test ah, 0xFF
    jnz .got_it
    loop .wait
.got_it:

    ; Clear keyboard
    mov al, 0xCC
    out PORT_B, al

    ; NOW read Port A -- this is what BIOS E6 does
    in al, PORT_A
    xor ah, ah
    mov [0x0500], ax

    hlt

temp_isr:
    mov ah, 1
    push ax
    mov al, 0xFF
    out INTA01, al
    mov al, 0x20
    out INTA00, al
    pop ax
    iret
