; test_keyboard.asm -- Keyboard input via IRQ1
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; On the IBM PC 5150, the keyboard sends serial scancodes via the
; KBD DATA/CLK lines (J7). U24 (74S322 shift register) clocks them in
; and fires IRQ1 when a full byte arrives. The CPU reads the scancode
; from port 0x60 (8255A PPI Port A) and acknowledges by toggling
; PPI Port B bit 7 (port 0x61).
;
; Without keyboard hardware (U24, U23 not implemented), IRQ1 never
; fires. The test times out and all expectations fail -- equivalent
; to unplugging the keyboard while the system is running.
;
; Expected: someone types "Hello world" (11 characters, 24 scancodes).
; The IRQ1 handler converts scancodes to ASCII using a lookup table
; and stores the result at 0x0600.
;
; Results:
;   [0500] = 0x0018   Scancode count (24 total for "Hello world")
;   [0502] = 0x6548   First word of ASCII buffer ("He" little-endian)
;   [0504] = 0x0001   Full string matches "Hello world"

; @name Keyboard (Hello world)
; @expect 0500 0018 Scancode count (24)
; @expect 0502 6548 First ASCII word ("He")
; @expect 0504 0001 String matches "Hello world"
;
cpu 8086
org 0x0100

PPI_A   equ 0x60               ; 8255A Port A (keyboard scancode)
PPI_B   equ 0x61               ; 8255A Port B (keyboard acknowledge)

; =====================================================================
; Initialize
; =====================================================================
mov ax, 0x0000
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x0800

; Zero results
mov word [0x0500], 0x0000       ; scancode count
mov word [0x0502], 0x0000       ; first ASCII word
mov word [0x0504], 0x0000       ; string match flag

; Zero ASCII output buffer (0x0600-0x060F)
mov di, 0x0600
mov cx, 8
xor ax, ax
rep stosw

; Zero working variables
mov byte [0x05E0], 0            ; shift state (0=off, 1=on)
mov word [0x05E2], 0            ; scancode count
mov word [0x05E4], 0            ; ASCII char count
mov word [0x05E6], 0x0600       ; ASCII write pointer

; =====================================================================
; Initialize PPI (8255A at port 0x60-0x63)
; Control word 0x99: Port A=input, Port B=output, Port C upper=input, lower=output
; This enables Port B output so PB7 toggle actually drives the pin.
; =====================================================================
mov al, 0x99
out 0x63, al

; =====================================================================
; Install IRQ1 handler (INT 9 with PIC vector base 8)
; =====================================================================
mov word [9*4],   irq1_handler
mov word [9*4+2], 0x0100

; Initialize PIC: edge-triggered, single, ICW4
mov al, 0x13
out 0x20, al
mov al, 0x08                    ; vector base 8
out 0x21, al
mov al, 0x01                    ; 8086 mode, normal EOI
out 0x21, al
mov al, 0xFD                    ; unmask IRQ1 only (clear bit 1)
out 0x21, al

; =====================================================================
; Arm keyboard: signal readiness via testcard port 0xFC.
; The TestKeyboard waits for this before delivering scancodes.
; =====================================================================
sti
mov al, 0x01
out 0xFC, al                    ; arm keyboard (TestKeyboard starts delivering)

mov bx, 8                       ; outer loop count
.wait_outer:
    mov cx, 0xFFFF
.wait_inner:
    cmp word [0x05E4], 11       ; got 11 ASCII chars?
    je .input_done
    nop
    loop .wait_inner
    dec bx
    jnz .wait_outer

.input_done:
cli

; =====================================================================
; Check results
; =====================================================================

; Store scancode count
mov ax, [0x05E2]
mov [0x0500], ax

; Store first word of ASCII buffer
mov ax, [0x0600]
mov [0x0502], ax

; Compare buffer against "Hello world"
push ds
mov ax, 0x0100
mov ds, ax
mov si, expected_str
pop ds                          ; DS back to 0 for buffer access
mov di, 0x0600
mov cx, 11
.cmp_loop:
    push ds
    mov ax, 0x0100
    mov ds, ax
    lodsb                       ; AL = expected[SI++] from CS
    pop ds
    cmp al, [di]
    jne .no_match
    inc di
    loop .cmp_loop
    mov word [0x0504], 0x0001   ; match!
.no_match:

hlt

; =====================================================================
; Expected string (in code segment)
; =====================================================================
expected_str:
    db "Hello world"

; =====================================================================
; Scancode-to-ASCII table (128 bytes, 0x00 = no mapping)
; IBM PC Scan Code Set 1, unshifted lowercase
; =====================================================================
scan_table:
    ;      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    db  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00  ; 0x00-0x0F
    db  0x71,0x77,0x65,0x72,0x74,0x79,0x75,0x69,0x6F,0x70,0x00,0x00,0x00,0x00,0x61,0x73  ; 0x10-0x1F: q w e r t y u i o p . . . . a s
    db  0x64,0x66,0x67,0x68,0x6A,0x6B,0x6C,0x00,0x00,0x00,0x00,0x00,0x7A,0x78,0x63,0x76  ; 0x20-0x2F: d f g h j k l . . . . . z x c v
    db  0x62,0x6E,0x6D,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00  ; 0x30-0x3F: b n m . . . . . . sp

; =====================================================================
; IRQ1 handler (INT 9) -- keyboard scancode processing
; =====================================================================
irq1_handler:
    push ax
    push bx
    push di

    ; Read scancode from PPI Port A
    in al, PPI_A
    mov bl, al                  ; save scancode in BL

    ; Acknowledge keyboard: toggle PPI Port B bit 7 (real 5150 protocol)
    in al, PPI_B
    or al, 0x80
    out PPI_B, al
    and al, 0x7F
    out PPI_B, al

    ; Signal testcard that we processed this scancode (port 0xFD).
    ; TestKeyboard uses this as the definitive ACK to send the next key.
    mov al, bl
    out 0xFD, al

    mov al, bl                  ; restore scancode

    ; Count every scancode (make + break)
    inc word [0x05E2]

    ; Handle shift keys
    cmp al, 0x2A                ; Left Shift make
    je .shift_on
    cmp al, 0x36                ; Right Shift make
    je .shift_on
    cmp al, 0xAA                ; Left Shift break
    je .shift_off
    cmp al, 0xB6                ; Right Shift break
    je .shift_off

    ; Ignore break codes (bit 7 set)
    test al, 0x80
    jnz .irq1_done

    ; Convert make code to ASCII via table
    xor ah, ah
    cmp al, 0x3F
    ja .irq1_done               ; out of table range
    push ds
    mov bx, 0x0100
    mov ds, bx
    mov bx, scan_table
    xlat                        ; AL = [DS:BX+AL]
    pop ds
    test al, al
    jz .irq1_done               ; no mapping

    ; Apply shift (uppercase if shift held)
    cmp byte [0x05E0], 0
    je .no_shift
    cmp al, 0x61                ; 'a'
    jb .no_shift
    cmp al, 0x7A                ; 'z'
    ja .no_shift
    sub al, 0x20                ; -> uppercase
.no_shift:

    ; Store ASCII char
    mov di, [0x05E6]
    mov [di], al
    inc word [0x05E6]
    inc word [0x05E4]
    jmp .irq1_done

.shift_on:
    mov byte [0x05E0], 1
    jmp .irq1_done
.shift_off:
    mov byte [0x05E0], 0

.irq1_done:
    ; Send EOI
    mov al, 0x20
    out 0x20, al

    pop di
    pop bx
    pop ax
    iret
