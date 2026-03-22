; test_post_kbd.asm -- BIOS POST TEST.12: Keyboard reset + stuck key check
; Mirrors the real IBM PC BIOS TEST.12 / KBD_RESET from PCBIOS.ASM.
; Requires: 8255A PPI, 8259A PIC, TestKeyboard harness
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; The KBD_RESET procedure:
;   1. Pull KBD CLK low via PPI PB6 (port 0x61 = 0x0C) for ~20ms
;   2. Release CLK high (0xCC), then enable keyboard (0x4C)
;   3. Unmask IRQ1, STI, wait for interrupt
;   4. Read scancode from port 0x60 -- expect 0xAA (self-test passed)
;
; Stuck key check:
;   After 0xAA, clear keyboard (0xCC), enable (0x4C), delay, read port 0x60.
;   Should be 0x00 (no stuck scancode).
;
; Expected results:
;   [0500] = 0x00AA   Keyboard reset scancode (0xAA = self-test OK)
;   [0502] = 0x0001   Stuck key check passed (port 0x60 == 0x00)

; @name POST Keyboard (TEST.12)
; @expect 0500 00AA KBD reset scancode
; @expect 0502 0001 Stuck key check
;
cpu 8086
org 0x0100

%define PORT_A  0x60
%define PORT_B  0x61
%define INTA00  0x20
%define INTA01  0x21

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

    ; Working variable: interrupt indicator
    mov byte [0x05E0], 0

    ; Initialize PPI: Port A=input, Port B=output
    mov al, 0x99
    out 0x63, al

    ; Install IRQ1 handler (INT 9, PIC base 8)
    mov word [9*4],   irq1_handler
    mov word [9*4+2], 0x0100

    ; Initialize PIC
    mov al, 0x13            ; ICW1: edge, single, ICW4
    out INTA00, al
    mov al, 0x08            ; ICW2: vector base 8
    out INTA01, al
    mov al, 0x01            ; ICW4: 8086 mode
    out INTA01, al
    mov al, 0xFF            ; mask all IRQs for now
    out INTA01, al

; =====================================================================
; KBD_RESET -- mirrors PCBIOS.ASM KBD_RESET proc (lines 1282-1305)
; =====================================================================

    ; Pull KBD CLK low via PB6
    mov al, 0x0C            ; PB3=1 (enable), PB2=1 (clr kbd), PB6 stays 0
    out PORT_B, al

    ; Hold CLK low for ~20ms (BIOS uses CX=10582 loop)
    mov cx, 10582
.hold_clk:
    loop .hold_clk

    ; Release: set CLK and ENABLE lines high
    mov al, 0xCC            ; PB7=1, PB6=1, PB3=1, PB2=1
    out PORT_B, al

    ; Enable keyboard: CLK high, enable low
    mov al, 0x4C            ; PB6=1, PB3=1, PB2=1
    out PORT_B, al

    ; Unmask IRQ1
    mov al, 0xFD            ; mask all except IRQ1
    out INTA01, al

    ; Enable interrupts and wait for keyboard interrupt
    sti
    mov byte [0x05E0], 0    ; clear interrupt indicator
    xor cx, cx              ; timeout loop (65536 iterations)
.wait_irq:
    cmp byte [0x05E0], 0
    jne .got_irq
    loop .wait_irq
    jmp .no_irq             ; timeout -- no interrupt received

.got_irq:
    ; Read scancode from PPI Port A
    in al, PORT_A
    xor ah, ah
    mov [0x0500], ax        ; expect 0x00AA

    ; Clear keyboard
    mov al, 0xCC
    out PORT_B, al

; =====================================================================
; Stuck key check -- mirrors PCBIOS.ASM TEST.12 (lines 1012-1021)
; =====================================================================
    ; Enable keyboard for next byte
    mov al, 0x4C
    out PORT_B, al

    ; Delay (BIOS uses SUB CX,CX / LOOP)
    xor cx, cx
.stuck_delay:
    loop .stuck_delay

    ; Read port 0x60 -- should be 0x00 (no stuck key)
    in al, PORT_A
    cmp al, 0x00
    jne .stuck_fail
    mov word [0x0502], 0x0001
    jmp .done
.stuck_fail:
    mov word [0x0502], 0x0000

.no_irq:
.done:
    cli
    hlt

; =====================================================================
; IRQ1 handler -- just sets the interrupt indicator flag
; Mirrors how BIOS KBD_RESET uses AH as interrupt indicator.
; =====================================================================
irq1_handler:
    push ax

    ; Signal interrupt occurred
    mov byte [0x05E0], 0xFF

    ; ACK to testcard (port 0xFD) so TestKeyboard advances queue
    in al, PORT_A
    out 0xFD, al

    ; EOI
    mov al, 0x20
    out INTA00, al

    pop ax
    iret
