; test_mouse.asm -- Microsoft serial mouse: driver + polling application
;
; Structured the way real software is layered, in isolation:
;
;   DRIVER (interrupt side): a nominal mouse driver whose ISR does only
;   driver work -- read UART bytes on IRQ4, sync on bit 6, assemble
;   3-byte Microsoft packets, sign-extend and ACCUMULATE dx/dy into a
;   driver state block (X, Y, buttons, packet count), exactly the job
;   MOUSE.COM's INT 0Ch handler does behind INT 33h.
;
;   APPLICATION (main line): installs the driver (vector + PIC),
;   starts it (UART 1200 7N1, IER, MCR OUT2, RTS power-on), then runs
;   a regular non-interrupt polling loop over the driver's state --
;   never touching the UART itself -- verifying each observed movement
;   against the known injected motion.
;
; The harness injects motion via SerialMouse::host_update() when the
; app writes a stage marker to [0x05F0]:
;   marker 1 -> inject dx=+20, dy=-10, left down
;   marker 2 -> inject dx=+5,  dy=+15, left up, right down
; Final accumulated state: X=+25, Y=+5, buttons=right only.
;
; Microsoft packet (1200 baud 7N1):
;   byte1 = 01LRYYXX (bit6 sync, L/R buttons, Y7-6, X7-6)
;   byte2 = 00XXXXXX (X5-0)
;   byte3 = 00YYYYYY (Y5-0)      Y positive = down
;
; Results:
;   [0500] = 0x0001   Driver online (mouse ident 'M' received via IRQ4)
;   [0502] = 0x004D   Ident byte value
;   [0504] = 0x0001   Move 1 observed correctly by polling (X=20 Y=-10 L)
;   [0506] = 0x0019   Final accumulated X (+20 +5)
;   [0508] = 0x0005   Final accumulated Y (-10 +15)
;   [050A] = 0x0002   Final buttons (right down, left up)
;   [050C] = 0x0001   At least two packets serviced by the driver

; @name Serial mouse (driver + polling app)
; @expect 0500 0001 Driver online (ident 'M' via IRQ4)
; @expect 0502 004D Ident byte value ('M')
; @expect 0504 0001 Move 1 observed via polling
; @expect 0506 0019 Final X = +25
; @expect 0508 0005 Final Y = +5
; @expect 050A 0002 Final buttons = right only
; @expect 050C 0001 Two+ packets serviced

cpu 8086
org 0x0100

PIC_CMD   equ 0x20
PIC_DATA  equ 0x21
COM1_RBR  equ 0x3F8
COM1_IER  equ 0x3F9
COM1_LCR  equ 0x3FB
COM1_MCR  equ 0x3FC
COM1_LSR  equ 0x3FD

; ---- Driver state block (the "device driver's" data segment) ----
DRV_ONLINE  equ 0x0600      ; byte: 1 once ident received
DRV_IDENT   equ 0x0601      ; byte: the ident byte itself
DRV_X       equ 0x0602      ; word: accumulated X (signed)
DRV_Y       equ 0x0604      ; word: accumulated Y (signed)
DRV_BTN     equ 0x0606      ; byte: current buttons (bit0=L, bit1=R)
DRV_COUNT   equ 0x0607      ; byte: packets serviced
DRV_PHASE   equ 0x0608      ; byte: bytes collected of current packet
DRV_B1      equ 0x0609      ; byte: packet byte 1
DRV_B2      equ 0x060A      ; byte: packet byte 2

MARKER      equ 0x05F0      ; harness injection stage marker

; =====================================================================
; APPLICATION
; =====================================================================

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results + driver state
    mov word [0x0500], 0
    mov word [0x0502], 0
    mov word [0x0504], 0
    mov word [0x0506], 0
    mov word [0x0508], 0
    mov word [0x050A], 0
    mov word [0x050C], 0
    mov word [DRV_ONLINE], 0    ; online + ident
    mov word [DRV_X], 0
    mov word [DRV_Y], 0
    mov word [DRV_BTN], 0       ; buttons + count
    mov word [DRV_PHASE], 0
    mov byte [MARKER], 0

    ; ---- 1. Install the driver ----
    mov word [0x0C*4],   mouse_driver_isr
    mov word [0x0C*4+2], 0x0100

    mov al, 0x13                ; PIC: edge, single, ICW4, base 8
    out PIC_CMD, al
    mov al, 0x08
    out PIC_DATA, al
    mov al, 0x01
    out PIC_DATA, al
    mov al, 0xEF                ; unmask IRQ4 only
    out PIC_DATA, al

    ; ---- 2. Start the driver: program the UART, power the mouse ----
    ; (Ports above 0xFF need the DX-indirect OUT/IN form.)
    mov dx, COM1_LCR
    mov al, 0x80                ; DLAB=1
    out dx, al
    mov dx, COM1_RBR
    mov al, 0x60                ; divisor 96 -> 1200 baud
    out dx, al
    mov dx, COM1_IER
    mov al, 0x00
    out dx, al
    mov dx, COM1_LCR
    mov al, 0x02                ; DLAB=0, 7N1
    out dx, al
    mov dx, COM1_IER
    mov al, 0x01                ; RX data available interrupt
    out dx, al
    mov dx, COM1_MCR
    mov al, 0x09                ; DTR=1, RTS=0, OUT2=1 (mouse unpowered)
    out dx, al
    sti

    mov dx, COM1_MCR
    mov al, 0x0B                ; RTS=1: power/reset the mouse
    out dx, al

    ; ---- 3. Poll (regular loop) until the driver reports online ----
    mov cx, 0xFFFF
.wait_online:
    cmp byte [DRV_ONLINE], 1
    je .online
    loop .wait_online
    jmp .done                   ; timeout: results show what happened
.online:
    mov word [0x0500], 1
    mov al, [DRV_IDENT]
    xor ah, ah
    mov [0x0502], ax

    ; ---- 4. Move 1: inject, poll driver state, verify ----
    mov byte [MARKER], 1        ; harness: inject dx=+20 dy=-10 L-down

    mov cx, 0xFFFF
.wait_move1:
    cmp byte [DRV_COUNT], 1
    jae .got_move1
    loop .wait_move1
    jmp .done
.got_move1:
    ; The application confirms the movement it observed is the movement
    ; that happened: X=+20, Y=-10, left button down.
    cmp word [DRV_X], 20
    jne .move1_bad
    cmp word [DRV_Y], 0xFFF6    ; -10
    jne .move1_bad
    cmp byte [DRV_BTN], 0x01
    jne .move1_bad
    mov word [0x0504], 1
.move1_bad:

    ; ---- 5. Move 2: inject, poll for it, record final state ----
    mov byte [MARKER], 2        ; harness: inject dx=+5 dy=+15 L-up R-down

    mov cx, 0xFFFF
.wait_move2:
    cmp byte [DRV_COUNT], 2
    jae .got_move2
    loop .wait_move2
    jmp .done
.got_move2:

    ; ---- 6. Record what the application observed ----
    mov ax, [DRV_X]
    mov [0x0506], ax
    mov ax, [DRV_Y]
    mov [0x0508], ax
    mov al, [DRV_BTN]
    xor ah, ah
    mov [0x050A], ax
    cmp byte [DRV_COUNT], 2
    jb .done
    mov word [0x050C], 1

.done:
    cli
    int3

; =====================================================================
; DRIVER -- IRQ4 ISR. Driver work only: no test logic, no result
; writes. Reads UART bytes, tracks ident, syncs and assembles packets,
; accumulates movement into the driver state block.
; =====================================================================
mouse_driver_isr:
    push ax
    push bx
    push cx
    push dx
    push ds
    xor ax, ax
    mov ds, ax

.drain:
    mov dx, COM1_LSR
    in al, dx
    test al, 1                  ; data ready?
    jz .out

    mov dx, COM1_RBR
    in al, dx

    cmp byte [DRV_ONLINE], 1
    je .packet_engine

    ; Detection phase: first byte after power-on is the ident.
    mov [DRV_IDENT], al
    cmp al, 0x4D                ; 'M' = Microsoft mouse present
    jne .drain
    mov byte [DRV_ONLINE], 1
    mov byte [DRV_PHASE], 0
    jmp .drain

.packet_engine:
    ; Sync: bit 6 set = first byte of a packet, always resync on it.
    test al, 0x40
    jz .data_byte
    mov [DRV_B1], al
    mov byte [DRV_PHASE], 1
    jmp .drain

.data_byte:
    cmp byte [DRV_PHASE], 1
    je .second
    cmp byte [DRV_PHASE], 2
    je .third
    jmp .drain                  ; stray byte with no sync: drop

.second:
    mov [DRV_B2], al
    mov byte [DRV_PHASE], 2
    jmp .drain

.third:
    ; Full packet: byte1=[DRV_B1] byte2=[DRV_B2] byte3=AL. Decode.
    mov bl, al                  ; BL = byte3 (Y5-0)

    ; dx = (byte1<1:0> << 6) | byte2<5:0>, sign-extended, added to X
    mov al, [DRV_B1]
    and al, 0x03
    mov cl, 6
    shl al, cl
    mov ah, [DRV_B2]
    and ah, 0x3F
    or  al, ah
    cbw
    add [DRV_X], ax

    ; dy = (byte1<3:2> << 4) | byte3<5:0>, sign-extended, added to Y
    mov al, [DRV_B1]
    and al, 0x0C
    mov cl, 4
    shl al, cl
    and bl, 0x3F
    or  al, bl
    cbw
    add [DRV_Y], ax

    ; buttons from byte1: bit5=L, bit4=R -> bit0=L, bit1=R
    mov al, [DRV_B1]
    mov ah, 0
    test al, 0x20
    jz .no_l
    or ah, 0x01
.no_l:
    test al, 0x10
    jz .no_r
    or ah, 0x02
.no_r:
    mov [DRV_BTN], ah

    inc byte [DRV_COUNT]
    mov byte [DRV_PHASE], 0
    jmp .drain

.out:
    mov al, 0x20                ; EOI
    out PIC_CMD, al
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    iret
