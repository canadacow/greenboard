; test_post_kbd.asm -- BIOS POST TEST.12: Keyboard reset + stuck key check
; Faithful reproduction of the real IBM PC BIOS TEST.12 from PCBIOS.ASM.
;
; Matches the BIOS state at TEST.12 entry:
;   - PPI initialized (0x99), Port B = 0xFC
;   - PIC initialized at TEST.6 (ICW2=0x08), IMR=0xFF (all masked)
;   - PIT timer 0 running in mode 3, count=0x0000 (max) since TEST.8
;   - D11 ISR installed at INT 8-15
;   - Stack at 0030:0100 (BIOS stack segment)
;
; Sequence:
;   1. Initialize PPI, PIC, PIT to match BIOS state at TEST.12
;   2. Install D11 ISR at INT 8-15 (matching BIOS TEST.6)
;   3. KBD_RESET: toggle Port B, wait IRQ1, read scancode
;   4. Stuck key check
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
%define CMD_PORT 0x63
%define INTA00  0x20
%define INTA01  0x21
%define TIMER0  0x40
%define TIM_CTL 0x43

; =====================================================================
; Initialize -- match BIOS state at TEST.12 entry
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

    ; Initialize PPI: Port A=input, Port B=output (BIOS line 328-331)
    mov al, 0x99
    out CMD_PORT, al
    mov al, 0xFC            ; PB7=1 (SW1 mux), PB6=1 (KBD CLK high)
    out PORT_B, al          ; matches BIOS initial Port B

    ; Initialize PIC (BIOS TEST.6, lines 568-589)
    ; The BIOS does NOT re-init PIC before TEST.12 -- it was init'd at boot.
    ; We must init it here since we start from scratch.
    mov al, 0x13            ; ICW1: edge, single, ICW4
    out INTA00, al
    mov al, 0x08            ; ICW2: vector base 8
    out INTA01, al
    mov al, 0x01            ; ICW4: 8086 mode
    out INTA01, al
    mov al, 0xFF            ; mask all IRQs (BIOS state after TEST.7)
    out INTA01, al

    ; Install D11 ISR at INT 8-15 (BIOS TEST.6, lines 594-602)
    mov cx, 8
    mov di, 8*4             ; INT 8 vector
.install_isr:
    mov word [di], d11_isr
    mov word [di+2], 0x0100
    add di, 4
    loop .install_isr

    ; Initialize PIT timer 0 in mode 3 (BIOS TEST.8, lines 704-712)
    ; Timer is running continuously by TEST.12
    mov al, 0x36            ; SEL TIM 0, LSB/MSB, MODE 3
    out TIM_CTL, al
    mov al, 0x00
    out TIMER0, al          ; LSB = 0
    out TIMER0, al          ; MSB = 0 (count = 65536)

; =====================================================================
; TEST.12 -- KBD_RESET (PCBIOS.ASM lines 998-1021)
; =====================================================================
    call kbd_reset          ; ISSUE SOFTWARE RESET TO KEYBRD
    jcxz .no_irq            ; PRINT ERR MSG IF NO INTERRUPT
    mov al, 0x4D            ; ENABLE KEYBOARD (BIOS line 1005)
    out PORT_B, al
    cmp bl, 0xAA            ; SCAN CODE AS EXPECTED? (BIOS line 1007)
    jne .fail

    ; Store result
    xor bh, bh
    mov [0x0500], bx        ; expect 0x00AA

    ; CHECK FOR STUCK KEYS (BIOS lines 1012-1021)
    mov al, 0xCC            ; CLR KBD, SET CLK LINE HIGH
    out PORT_B, al
    mov al, 0x4C            ; ENABLE KBD, CLK IN NEXT BYTE
    out PORT_B, al
    sub cx, cx
.stuck_wait:
    loop .stuck_wait        ; DELAY FOR A WHILE
    in al, PORT_A           ; CHECK FOR STUCK KEYS
    cmp al, 0               ; SCAN CODE = 0?
    jne .stuck_fail
    mov word [0x0502], 0x0001
    jmp .done

.stuck_fail:
.fail:
.no_irq:
.done:
    cli
    int3

; =====================================================================
; KBD_RESET -- exact copy of PCBIOS.ASM lines 1282-1305
; =====================================================================
kbd_reset:
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
.got_irq:
    in al, PORT_A           ; READ KEYBOARD SCAN CODE
    mov bl, al              ; SAVE SCAN CODE JUST READ
    mov al, 0xCC            ; CLEAR KEYBOARD
    out PORT_B, al
    ret

; =====================================================================
; D11 ISR -- exact copy of PCBIOS.ASM lines 651-660
; Used for ALL IRQ 0-7 during POST. Sets AH=1, masks all, EOI.
; =====================================================================
d11_isr:
    mov ah, 1               ; signal interrupt occurred
    push ax
    mov al, 0xFF            ; MASK ALL INTERRUPTS OFF
    out INTA01, al
    mov al, 0x20            ; EOI
    out INTA00, al
    pop ax
    iret
