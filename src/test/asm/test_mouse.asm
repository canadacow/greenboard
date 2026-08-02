; test_mouse.asm -- Microsoft serial mouse on COM1 (AST SixPakPlus, J4)
;
; Exercises the full real path an app/driver sees: program the 8250
; UART at 3F8h for 1200 baud 7N1, unmask IRQ4 on the PIC, enable the
; UART's "data available" interrupt, then toggle RTS to power/reset
; the mouse. The mouse identifies itself with 'M' (0x4D), interrupt-
; driven through INT 0Ch, exactly like MOUSE.COM's detection sequence.
;
; After identification, the test harness injects synthetic host motion
; via SerialMouse::host_update() (C++ side, between polling windows).
; The mouse should emit a standard 3-byte Microsoft packet:
;   byte1 = 01LRYYXX (bit6 sync, L/R buttons, Y7-6, X7-6)
;   byte2 = 00XXXXXX (X5-0)
;   byte3 = 00YYYYYY (Y5-0)
; caught byte-by-byte via the same IRQ4 handler, then decoded and
; checked against the known injected (dx, dy, buttons).
;
; 8250 register file at 3F8-3FF (LCR bit7 selects divisor latch):
;   3F8 RBR/THR (DLAB=0) / DLL (DLAB=1)
;   3F9 IER (DLAB=0) / DLM (DLAB=1)
;   3FA IIR (read)
;   3FB LCR
;   3FC MCR
;   3FD LSR
;   3FE MSR
;
; Expected results:
;   [0500] = 0x0001   Mouse identified ('M' received via IRQ4)
;   [0502] = 0x004D   Ident byte value
;   [0504] = 0x0001   Movement packet received (3 bytes via IRQ4)
;   [0506] = dx (sign-extended word, expect +0x0014 = 20)
;   [0508] = dy (sign-extended word, expect -0x000A = -10, i.e. 0xFFF6)
;   [050A] = 0x0001   Left button reported down
;   [050C] = 0x0001   Right button reported up (not pressed)
;   [050E] = 0x0001   Sync bit (bit6) was set on packet byte 1

; @name Serial mouse (COM1, IRQ4)
; @expect 0500 0001 Mouse identified via IRQ4
; @expect 0502 004D Ident byte value ('M')
; @expect 0504 0001 Movement packet received
; @expect 0506 0014 dx = +20
; @expect 0508 FFF6 dy = -10
; @expect 050A 0001 Left button down
; @expect 050C 0001 Right button up
; @expect 050E 0001 Packet sync bit set

cpu 8086
org 0x0100

PIC_CMD   equ 0x20
PIC_DATA  equ 0x21
COM1_RBR  equ 0x3F8
COM1_IER  equ 0x3F9
COM1_LCR  equ 0x3FB
COM1_MCR  equ 0x3FC

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results
    mov word [0x0500], 0
    mov word [0x0502], 0
    mov word [0x0504], 0
    mov word [0x0506], 0
    mov word [0x0508], 0
    mov word [0x050A], 0
    mov word [0x050C], 0
    mov word [0x050E], 0

    ; Working state
    mov byte [0x05E0], 0    ; bytes received so far (this phase)
    mov byte [0x05E1], 0    ; phase: 0=waiting ident, 1=collecting packet
    mov byte [0x05E2], 0    ; packet byte 0
    mov byte [0x05E3], 0    ; packet byte 1
    mov byte [0x05E4], 0    ; packet byte 2

    ; Install INT 0Ch handler (IRQ4, vector base 8 -> INT 0Ch)
    mov word [0x0C*4],   irq4_handler
    mov word [0x0C*4+2], 0x0100

    ; Initialize PIC: edge-triggered, single, ICW4, vector base 8
    mov al, 0x13
    out PIC_CMD, al
    mov al, 0x08
    out PIC_DATA, al
    mov al, 0x01
    out PIC_DATA, al
    mov al, 0xFF             ; mask all initially
    out PIC_DATA, al

    ; ---- Program the 8250: 1200 baud, 7N1 ----
    ; Ports above 0xFF need the DX-indirect OUT/IN form -- the 8-bit
    ; imm8 encoding can only address ports 0-255.
    ; Divisor = 115200 / 1200 = 96 = 0x0060
    mov dx, COM1_LCR
    mov al, 0x80              ; LCR: DLAB=1
    out dx, al
    mov dx, COM1_RBR
    mov al, 0x60              ; DLL = 0x60
    out dx, al
    mov dx, COM1_IER
    mov al, 0x00              ; DLM = 0x00
    out dx, al
    mov dx, COM1_LCR
    mov al, 0x02              ; LCR: DLAB=0, 7 data bits, no parity, 1 stop
    out dx, al

    ; Enable "data available" interrupt
    mov dx, COM1_IER
    mov al, 0x01
    out dx, al

    ; MCR: OUT2=1 (gate IRQ to bus), DTR=1, RTS=0 initially (mouse unpowered)
    mov dx, COM1_MCR
    mov al, 0x09              ; DTR=1, RTS=0, OUT2=1
    out dx, al

    ; Unmask IRQ4 only
    mov al, 0xEF
    out PIC_DATA, al
    sti

    ; Small delay so DTR settles before RTS toggles (not required by
    ; the model, but mirrors a real driver's init sequence).
    mov cx, 0x0100
.predelay:
    nop
    loop .predelay

    ; Raise RTS: power/reset the mouse -- it should identify with 'M'.
    mov dx, COM1_MCR
    mov al, 0x0B               ; DTR=1, RTS=1, OUT2=1
    out dx, al

    ; ---- Wait for identification (IRQ4-driven) ----
    mov cx, 0xFFFF
.wait_ident:
    cmp word [0x0500], 1
    je .ident_done
    loop .wait_ident
.ident_done:

    ; ---- Inject synthetic motion, then wait for a movement packet ----
    ; The harness watches for [0x05F0] going non-zero and calls
    ; mouse.host_update(dx=20, dy=-10, left=true, right=false) exactly
    ; once, then the mouse device produces a real 3-byte packet through
    ; the same poll_rx() path the UART already exercises for 'M'.
    mov byte [0x05F0], 1        ; signal harness: inject motion now

    mov cx, 0xFFFF
.wait_packet:
    cmp word [0x0504], 1
    je .packet_done
    loop .wait_packet
.packet_done:

    cli
    int3

; =====================================================================
; INT 0Ch handler -- IRQ4. Reads LSR/RBR from the UART, tracks
; ident-vs-packet phase, decodes the 3-byte packet on completion.
; =====================================================================
irq4_handler:
    push ax
    push ds
    xor ax, ax
    mov ds, ax

    mov dx, COM1_RBR + 5        ; LSR (0x3FD)
    in al, dx
    test al, 1                 ; data ready?
    jz .no_data

    mov dx, COM1_RBR
    in al, dx                   ; read the byte, clears DR

    cmp byte [0x05E1], 0
    jne .in_packet

    ; ---- Ident phase ----
    xor ah, ah
    mov [0x0502], ax             ; store ident byte
    cmp al, 0x4D                 ; 'M'?
    jne .not_ident
    mov word [0x0500], 1         ; mouse identified
    mov byte [0x05E1], 1         ; switch to packet-collection phase
    mov byte [0x05E0], 0
.not_ident:
    jmp .eoi

.in_packet:
    mov bl, [0x05E0]
    xor bh, bh
    mov [0x05E2 + bx], al
    inc byte [0x05E0]
    cmp byte [0x05E0], 3
    jne .eoi

    ; Full packet collected -- decode it.
    mov al, [0x05E2]             ; byte1: 01LRYYXX
    test al, 0x40
    jz .no_sync
    mov word [0x050E], 1
.no_sync:
    test al, 0x20
    jz .no_left
    mov word [0x050A], 1
.no_left:
    test al, 0x10
    jz .right_up
    jmp .right_checked
.right_up:
    mov word [0x050C], 1
.right_checked:

    ; Reassemble dx: bits X7-6 from byte1<1:0>, X5-0 from byte2<5:0>
    mov bl, al
    and bl, 0x03
    mov cl, 6
    shl bl, cl
    mov ah, [0x05E3]
    and ah, 0x3F
    or ah, bl
    ; sign-extend the 8-bit dx into AX
    mov al, ah
    cbw
    mov [0x0506], ax

    ; Reassemble dy: bits Y7-6 from byte1<3:2>, Y5-0 from byte3<5:0>
    mov al, [0x05E2]
    and al, 0x0C
    mov cl, 4
    shl al, cl
    mov ah, [0x05E4]
    and ah, 0x3F
    or ah, al
    mov al, ah
    cbw
    mov [0x0508], ax

    mov word [0x0504], 1         ; packet received
    mov byte [0x05E1], 0
    mov byte [0x05E0], 0

.eoi:
.no_data:
    mov al, 0x20
    out PIC_CMD, al
    pop ds
    pop ax
    iret
