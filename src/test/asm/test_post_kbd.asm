; test_post_kbd.asm -- BIOS POST TEST.12: Keyboard reset + stuck key check
; Exact reproduction of PCBIOS.ASM TEST.12 / KBD_RESET (lines 1282-1304)
; and stuck key check (lines 1012-1021).
;
; The KBD_RESET procedure:
;   1. Pull KBD CLK low via PPI PB6 (port 0x61 = 0x0C) for ~20ms
;   2. Release CLK high (0xCC), then enable keyboard (0x4C)
;   3. Unmask IRQ1, STI, wait for interrupt (AH flag)
;   4. Read scancode from port 0x60 -- expect 0xAA (self-test passed)
;
; The ISR (D11 in BIOS) just sets AH=1, masks all IRQs, sends EOI.
; It does NOT read Port A or toggle PB7.
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

    ; Initialize PPI: Port A=input, Port B=output
    mov al, 0x99
    out 0x63, al

    ; Install temporary ISR at INT 9 (matches BIOS D11 proc)
    mov word [9*4],   temp_isr
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
; KBD_RESET -- exact copy of PCBIOS.ASM lines 1282-1304
; =====================================================================

    ; Pull KBD CLK low via PB6
    mov al, 0x0C            ; SET KBD CLK LINE LOW
    out PORT_B, al          ; WRITE 8255 PORT B
    mov cx, 10582           ; HOLD KBD CLK LOW FOR 20 MS
.hold_clk:
    loop .hold_clk          ; LOOP FOR 20 MS

    mov al, 0xCC            ; SET CLK, ENABLE LINES HIGH
    out PORT_B, al

    mov al, 0x4C            ; SET KBD CLK HIGH, ENABLE LOW
    out PORT_B, al

    mov al, 0xFD            ; ENABLE KEYBOARD INTERRUPTS
    out INTA01, al          ; WRITE 8259 IMR

    sti                     ; ENABLE SYSTEM INTERRUPTS
    mov ah, 0               ; RESET INTERRUPT INDICATOR
    sub cx, cx              ; SETUP INTERRUPT TIMEOUT CNT
.wait_irq:
    test ah, 0xFF           ; DID A KEYBOARD INTR OCCUR?
    jnz .got_irq            ; YES - READ SCAN CODE RETURNED
    loop .wait_irq          ; NO - LOOP TILL TIMEOUT
    jmp .no_irq             ; timeout

.got_irq:
    in al, PORT_A           ; READ KEYBOARD SCAN CODE
    mov bl, al              ; SAVE SCAN CODE JUST READ
    mov al, 0xCC            ; CLEAR KEYBOARD
    out PORT_B, al

    ; Store result (BIOS compares BL to 0xAA at line 1007)
    xor bh, bh
    mov [0x0500], bx        ; expect 0x00AA

    ; Enable keyboard (BIOS line 1005-1006)
    mov al, 0x4D            ; ENABLE KEYBOARD
    out PORT_B, al

; =====================================================================
; Stuck key check -- exact copy of PCBIOS.ASM lines 1012-1021
; =====================================================================
    mov al, 0xCC            ; CLR KBD, SET CLK LINE HIGH
    out PORT_B, al
    mov al, 0x4C            ; ENABLE KBD, CLK IN NEXT BYTE
    out PORT_B, al
    sub cx, cx              ; DELAY FOR A WHILE
.stuck_delay:
    loop .stuck_delay
    in al, PORT_A           ; CHECK FOR STUCK KEYS
    cmp al, 0               ; SCAN CODE = 0?
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
; Temporary ISR -- exact copy of BIOS D11 proc (lines 651-660)
; Sets AH=1, masks all IRQs, sends EOI. Does NOT read PA or toggle PB7.
; =====================================================================
temp_isr:
    mov ah, 1               ; signal interrupt occurred
    push ax
    mov al, 0xFF            ; MASK ALL INTERRUPTS OFF
    out INTA01, al
    mov al, 0x20            ; EOI
    out INTA00, al
    pop ax
    iret
