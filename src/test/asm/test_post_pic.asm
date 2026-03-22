; test_post_pic.asm -- BIOS POST TEST.06 + TEST.07: PIC + Timer 0 speed
; Mirrors the real IBM PC BIOS TEST.06 (8259 test) and TEST.07 (8253 timer 0).
; Requires: 8259A PIC, 8253 PIT
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; TEST.06:
;   1. Write 0x00 to IMR (port 0x21), read back, verify 0x00
;   2. Write 0xFF to IMR, read back, verify 0xFF
;   3. Install temp ISR for IRQs 0-7, mask all, STI, spin -- no interrupt
;
; TEST.07:
;   4. Program timer 0: count=22 (0x16), mode 0, LSB only
;      Unmask IRQ0, wait for interrupt within loop -- must fire
;   5. Program timer 0: count=255, mode 0, reset flag, unmask IRQ0
;      Spin for short loop -- must NOT fire (too fast = error)
;
; Expected results:
;   [0500] = 0x0001   IMR read/write passed
;   [0502] = 0x0001   Hot interrupt check passed (no spurious IRQ)
;   [0504] = 0x0001   Timer 0 speed OK (IRQ0 fired with count=22)
;   [0506] = 0x0001   Timer 0 not too fast (no IRQ0 with count=255 in short loop)

; @name POST PIC/Timer (TEST.06+07)
; @expect 0500 0001 IMR read/write
; @expect 0502 0001 Hot IRQ check
; @expect 0504 0001 Timer0 speed OK
; @expect 0506 0001 Timer0 not fast
;
cpu 8086
org 0x0100

%define INTA00   0x20
%define INTA01   0x21
%define TIMER0   0x40
%define TIM_CTL  0x43

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
    mov word [0x0504], 0x0000
    mov word [0x0506], 0x0000

    ; Initialize PIC
    mov al, 0x13            ; ICW1: edge, single, ICW4
    out INTA00, al
    mov al, 0x08            ; ICW2: vector base 8
    out INTA01, al
    mov al, 0x01            ; ICW4: 8086 mode
    out INTA01, al
    mov al, 0xFF            ; mask all
    out INTA01, al

; =====================================================================
; Test 1: IMR read/write (BIOS TEST.06 lines 579-589)
; =====================================================================
    cli
    mov al, 0x00
    out INTA01, al          ; write 0x00 to IMR
    in  al, INTA01          ; read back
    or  al, al
    jnz .test2              ; fail -- IMR != 0x00

    mov al, 0xFF
    out INTA01, al          ; write 0xFF to IMR
    in  al, INTA01          ; read back
    add al, 1               ; if 0xFF, this overflows to 0x00
    jnz .test2              ; fail -- IMR != 0xFF

    mov word [0x0500], 0x0001

; =====================================================================
; Test 2: Hot interrupt check (BIOS TEST.06 lines 593-612)
; Install temp ISR for IRQs 0-7, mask all, STI, spin.
; If AH becomes non-zero, a spurious interrupt occurred.
; =====================================================================
.test2:
    ; Install temp ISR at INT 8-15 (IRQ 0-7)
    cld
    mov cx, 8
    mov di, 8*4             ; INT 8 vector
    mov ax, 0x0100
    mov es, ax
.vec_loop:
    mov ax, temp_isr
    stosw                   ; offset
    mov ax, 0x0100
    stosw                   ; segment
    loop .vec_loop

    xor ax, ax
    mov es, ax              ; restore ES=0

    ; All interrupts masked (already 0xFF from test 1)
    mov al, 0xFF
    out INTA01, al

    xor ah, ah              ; clear interrupt indicator
    sti
    xor cx, cx              ; 65536 iterations
.hot1:
    loop .hot1
.hot2:
    loop .hot2              ; second 65536 loop (matches BIOS)

    or ah, ah
    jnz .test3              ; fail -- spurious interrupt
    mov word [0x0502], 0x0001

; =====================================================================
; Test 3: Timer 0 speed check (BIOS TEST.07 lines 624-637)
; Program timer 0: count=22 (0x16), mode 0, LSB only.
; Unmask IRQ0, wait for interrupt. Must fire within CL=22 loop iterations.
; =====================================================================
.test3:
    mov ah, 0               ; reset interrupt flag
    xor ch, ch
    mov al, 0xFE            ; unmask IRQ0 only
    out INTA01, al
    mov al, 0x10            ; sel timer 0, LSB, mode 0, binary
    out TIM_CTL, al
    mov cl, 0x16            ; count = 22
    mov al, cl
    out TIMER0, al          ; load timer 0

.wait_t0:
    test ah, 0xFF           ; did timer 0 interrupt occur?
    jnz .t0_fired
    loop .wait_t0
    jmp .test4              ; fail -- timer 0 didn't fire

.t0_fired:
    mov word [0x0504], 0x0001

; =====================================================================
; Test 4: Timer 0 not too fast (BIOS TEST.07 lines 638-647)
; Program timer 0: count=255 (0xFF), mode 0.
; Spin for CL=18 iterations. Timer must NOT fire (too fast = error).
; =====================================================================
.test4:
    mov cl, 18              ; short loop count
    mov al, 0xFF            ; timer 0 count = 255
    out TIMER0, al
    mov ah, 0               ; reset interrupt flag
    mov al, 0xFE            ; unmask IRQ0
    out INTA01, al

.wait_fast:
    test ah, 0xFF           ; did timer 0 interrupt?
    jnz .done               ; fail -- timer fired too fast
    loop .wait_fast

    ; Good -- no interrupt in short window
    mov word [0x0506], 0x0001

.done:
    cli
    hlt

; =====================================================================
; Temporary ISR for hot interrupt check + timer 0 tests
; Mirrors BIOS D11 proc (lines 651-660)
; =====================================================================
temp_isr:
    mov ah, 1               ; signal that an interrupt occurred
    push ax
    mov al, 0xFF            ; mask all interrupts
    out INTA01, al
    mov al, 0x20            ; EOI
    out INTA00, al
    pop ax
    iret
