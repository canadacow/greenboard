; test_post_sw1_readback.asm -- Can we read DIP switches?
;
; Test 1: Init PPI, read Port A. No keyboard, no interrupts, nothing.
; Test 2: After full KBD_RESET sequence.

; @name SW1 readback
; @expect 0500 003D Simple read
; @expect 0502 003D After KBD_RESET
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
    mov word [0x0502], 0x0000

    ; Init PPI
    mov al, 0x99
    out 0x63, al

    ; Port B = 0xFC: PB7=1 enables U23 (switch mux), same as BIOS TEST.02
    mov al, 0xFC
    out PORT_B, al

    ; Test 1: read Port A
    in al, PORT_A
    xor ah, ah
    mov [0x0500], ax

    ; Test 2: KBD_RESET then read Port A
    mov al, 0xFC
    out PORT_B, al

    mov al, 0x13
    out INTA00, al
    mov al, 0x08
    out INTA01, al
    mov al, 0x01
    out INTA01, al
    mov al, 0xFF
    out INTA01, al

    mov word [9*4], temp_isr
    mov word [9*4+2], 0x0100

    mov al, 0x0C
    out PORT_B, al
    mov cx, 10582
.hold:
    loop .hold
    mov al, 0xCC
    out PORT_B, al
    mov al, 0x4C
    out PORT_B, al
    mov al, 0xFD
    out INTA01, al
    sti
    mov ah, 0
    xor cx, cx
.wait:
    test ah, 0xFF
    jnz .got_it
    loop .wait
.got_it:
    mov al, 0xCC
    out PORT_B, al

    in al, PORT_A
    xor ah, ah
    mov [0x0502], ax

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
