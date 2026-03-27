; test_pit.asm -- 8253 PIT counter modes via I/O ports 0x40-0x43
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
; SP initialized to 0x0800.
;
; PIT channels on the 5150:
;   Ch0 (port 0x40): OUT0 -> IRQ0 (timer tick)
;   Ch1 (port 0x41): OUT1 -> DRQ0 (DRAM refresh)
;   Ch2 (port 0x42): OUT2 -> speaker
;   Control (port 0x43): control word register
;
; PIT_CLK = PCLK/2 = 1.193 MHz. Each CLK falling edge decrements
; the active counter. The test uses short counts and busy-waits to
; verify counter behavior.

; @name PIT (8253 counter modes)
; @expect 0500 0001 Mode 0: OUT0 goes low after control word
; @expect 0502 0001 Mode 0: OUT0 goes high at terminal count
; @expect 0504 0001 Mode 0: counter decremented after wait
; @dump  0510 PIT counter before wait
; @dump  0512 PIT counter after wait
; @dump  0514 PIT read byte 1 (should be LSB)
; @dump  0516 PIT read byte 2 (should be MSB)
; @expect 0506 0001 Mode 3: square wave toggles IRQ0
; @expect 0508 0001 Counter latch reads stable value
; @expect 050A 0001 Mode 2: rate generator OUT0 pulses
; @expect 050C 0001 LSB-then-MSB write/read round-trip
; @expect 050E 0001 PIT Ch0 fires IRQ0 interrupt
; @expect 0520 0001 Mode 2 periodic: multiple IRQ0 interrupts fired
; @expect 0522 0001 GATE2 via PPI: gate controls Ch2 counting
; @expect 0524 0001 BCD counting: valid BCD decrement
; @expect 0526 0001 Two channels: Ch0 and Ch2 count independently
; @expect 0528 0001 Reload mid-count: new count takes effect
; @dump  052A BCD latched count (expect valid BCD < 0x9000)

cpu 8086
org 0x0100

    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results
    mov di, 0x0500
    mov cx, 8
    xor ax, ax
.zero:
    mov [di], ax
    add di, 2
    loop .zero

    ; Initialize PIC: ICW1 -> ICW2 -> ICW4, mask all IRQs.
    mov al, 0x13
    out 0x20, al
    mov al, 0x08
    out 0x21, al
    mov al, 0x01
    out 0x21, al
    mov al, 0xFF              ; mask all
    out 0x21, al

    ; =====================================================================
    ; Test 1: Mode 0 -- OUT goes low immediately after control word
    ; =====================================================================
    ; Program Ch0 mode 0, LSB only
    mov al, 0x10              ; Ch0, LSB only, mode 0, binary
    out 0x43, al
    ; OUT0 should now be low. Read IRQ0 line via PIC IRR.
    ; OCW3: read IRR
    mov al, 0x0A
    out 0x20, al
    in al, 0x20
    ; IRQ0 is bit 0. OUT0 low = IRQ0 low = bit 0 clear.
    test al, 0x01
    jnz .test1_fail
    mov word [0x0500], 0x0001
.test1_fail:

    ; =====================================================================
    ; Test 2: Mode 0 -- OUT goes high at terminal count
    ; =====================================================================
    ; Write count = 3 (short count so it expires quickly)
    mov al, 3
    out 0x40, al
    ; Busy-wait: PIT_CLK is ~1.19 MHz, each CPU CLK = ~4.77 MHz,
    ; so ~4 CPU clocks per PIT tick. Count=3 needs ~12 CPU clocks.
    ; 50 iterations of a simple loop is plenty.
    mov cx, 50
.wait2:
    loop .wait2
    ; OUT0 should now be high -> IRQ0 pending in IRR
    mov al, 0x0A
    out 0x20, al
    in al, 0x20
    test al, 0x01
    jz .test2_fail
    mov word [0x0502], 0x0001
.test2_fail:

    ; =====================================================================
    ; Test 3: Mode 0 -- counter read-back via latch (LSB+MSB)
    ; =====================================================================
    ; Reprogram Ch0 mode 0, LSB+MSB, count=60000 (0xEA60)
    mov al, 0x30              ; Ch0, LSB+MSB, mode 0, binary
    out 0x43, al
    mov al, 0x60
    out 0x40, al              ; LSB
    mov al, 0xEA
    out 0x40, al              ; MSB
    ; Wait for null_count transfer (needs at least 1 PIT CLK falling edge)
    mov cx, 20
.wait3_init:
    loop .wait3_init
    ; Latch (before wait)
    mov al, 0x00              ; Counter latch Ch0
    out 0x43, al
    in al, 0x40               ; Read 1 (should be LSB)
    mov ah, 0
    mov [0x0514], ax          ; dump byte 1
    mov bl, al
    in al, 0x40               ; Read 2 (should be MSB)
    mov ah, 0
    mov [0x0516], ax          ; dump byte 2
    mov bh, al
    mov [0x0510], bx          ; counter before wait
    ; Wait ~800 PIT ticks
    mov cx, 3200
.wait3:
    loop .wait3
    ; Latch again (after wait)
    mov al, 0x00
    out 0x43, al
    in al, 0x40
    mov bl, al
    in al, 0x40
    mov bh, al
    mov [0x0512], bx          ; counter after wait
    ; After should be < before
    cmp bx, word [0x0510]
    jae .test3_fail
    mov word [0x0504], 0x0001
.test3_fail:

    ; =====================================================================
    ; Test 4: Mode 3 -- square wave
    ; =====================================================================
    ; Program Ch0 mode 3, LSB only, count=6
    ; Square wave: OUT toggles every count/2 = 3 PIT ticks.
    mov al, 0x16              ; Ch0, LSB only, mode 3, binary
    out 0x43, al
    mov al, 6
    out 0x40, al
    ; OUT0 starts high. Wait for it to go low (3 PIT ticks).
    mov cx, 30
.wait4a:
    loop .wait4a
    ; Read IRR to see if OUT0 toggled. It should have gone low then back
    ; to high in 6 PIT ticks. With our wait, we should see at least one
    ; toggle. Check that OUT0 transitioned by reading IRR at two points.
    ; After 30 loop iterations (~120 CPU clocks, ~30 PIT ticks = 5 full
    ; periods of count=6), OUT0 could be high or low. Instead, just verify
    ; the PIC saw an edge (IRR bit set) from the toggling.
    mov al, 0x0A
    out 0x20, al
    in al, 0x20
    test al, 0x01
    jz .test4_fail
    mov word [0x0506], 0x0001
.test4_fail:

    ; =====================================================================
    ; Test 5: Counter latch command
    ; =====================================================================
    ; Program Ch0 mode 0, LSB+MSB, count=1000 (0x03E8)
    mov al, 0x30              ; Ch0, LSB+MSB, mode 0, binary
    out 0x43, al
    mov al, 0xE8
    out 0x40, al              ; LSB = 0xE8
    mov al, 0x03
    out 0x40, al              ; MSB = 0x03
    ; Wait a bit
    mov cx, 10
.wait5:
    loop .wait5
    ; Latch counter
    mov al, 0x00              ; Counter latch command for Ch0
    out 0x43, al
    ; Read latched value (LSB then MSB)
    in al, 0x40               ; LSB
    mov bl, al
    in al, 0x40               ; MSB
    mov bh, al
    ; BX now has the latched count. It should be < 1000 and > 0.
    cmp bx, 1000
    jae .test5_fail
    cmp bx, 0
    je .test5_fail
    mov word [0x0508], 0x0001
.test5_fail:

    ; =====================================================================
    ; Test 6: Mode 2 -- rate generator
    ; =====================================================================
    ; Program Ch0 mode 2, LSB only, count=4
    ; OUT is high for N-1 ticks, low for 1 tick, auto-reload.
    mov al, 0x14              ; Ch0, LSB only, mode 2, binary
    out 0x43, al
    mov al, 4
    out 0x40, al
    ; Wait enough for several periods (~40 PIT ticks)
    mov cx, 200
.wait6:
    loop .wait6
    ; IRQ0 should have been pulsed many times. Check IRR.
    mov al, 0x0A
    out 0x20, al
    in al, 0x20
    test al, 0x01
    jz .test6_fail
    mov word [0x050A], 0x0001
.test6_fail:

    ; =====================================================================
    ; Test 7: LSB-then-MSB write/read round-trip
    ; =====================================================================
    ; Program Ch0 mode 0, LSB+MSB, count=0x1234
    mov al, 0x30              ; Ch0, LSB+MSB, mode 0, binary
    out 0x43, al
    mov al, 0x34
    out 0x40, al              ; LSB = 0x34
    mov al, 0x12
    out 0x40, al              ; MSB = 0x12
    ; Immediately latch and read back
    mov al, 0x00              ; Counter latch Ch0
    out 0x43, al
    in al, 0x40               ; LSB
    mov bl, al
    in al, 0x40               ; MSB
    mov bh, al
    ; BX should be <= 0x1234 (count may have decremented slightly)
    cmp bx, 0x1234
    ja .test7_fail
    cmp bx, 0x1200            ; shouldn't have decremented by more than 0x34
    jb .test7_fail
    mov word [0x050C], 0x0001
.test7_fail:

    ; =====================================================================
    ; Test 8: PIT Ch0 fires IRQ0 interrupt (real hardware path)
    ; =====================================================================
    ; PIT OUT0 -> PIC IR0 -> INT 8. No BusGlue trigger port -- the PIT
    ; output pin drives the PIC input pin directly through the board wiring.

    ; Install INT 8 handler (IRQ0 vector, since ICW2=0x08)
    mov word [8*4],     pit_irq0_handler
    mov word [8*4+2],   0x0100         ; CS of loaded test binary

    ; Clear the result flag and ISR-ran marker
    mov word [0x050E], 0x0000
    mov byte [0x05F0], 0

    ; Re-init PIC: edge-triggered, single, ICW4, normal EOI
    mov al, 0x13
    out 0x20, al
    mov al, 0x08              ; vector base = 8
    out 0x21, al
    mov al, 0x01              ; 8086 mode, normal EOI
    out 0x21, al
    mov al, 0xFE              ; unmask IRQ0 only
    out 0x21, al

    ; Program PIT Ch0: mode 0, LSB only, count=5
    ; Mode 0: OUT starts low on control write, goes high at terminal count.
    ; With count=5, OUT0 rises after 5 PIT CLK falling edges (~20 CPU clocks).
    ; The rising edge on IR0 will latch IRQ0 in the PIC.
    mov al, 0x10              ; Ch0, LSB only, mode 0, binary
    out 0x43, al
    mov al, 5
    out 0x40, al

    ; Enable interrupts and busy-wait for the ISR to set the flag
    sti
    mov cx, 500
.wait8:
    cmp byte [0x05F0], 1
    je .test8_done
    loop .wait8
.test8_done:
    cli

    ; Check if handler ran
    cmp byte [0x05F0], 1
    jne .test8_fail
    mov word [0x050E], 0x0001
.test8_fail:

    ; Zero new result area (0x0520-0x0528)
    mov word [0x0520], 0x0000
    mov word [0x0522], 0x0000
    mov word [0x0524], 0x0000
    mov word [0x0526], 0x0000
    mov word [0x0528], 0x0000

    ; =====================================================================
    ; Test 9: Mode 2 periodic interrupts -- multiple IRQ0 fires
    ; =====================================================================
    ; Program Ch0 as rate generator with short period. Count how many
    ; times the ISR fires. Mode 2 auto-reloads, so OUT0 pulses repeatedly.

    ; Install counting ISR
    mov word [8*4],     pit_count_handler
    mov word [8*4+2],   0x0100
    mov word [0x05F2], 0       ; IRQ fire counter

    ; Re-init PIC
    mov al, 0x13
    out 0x20, al
    mov al, 0x08
    out 0x21, al
    mov al, 0x01
    out 0x21, al
    mov al, 0xFE              ; unmask IRQ0
    out 0x21, al

    ; Program Ch0: mode 2, LSB+MSB, count=200
    ; Period = 200 PIT ticks (~800 CPU clocks). ISR takes ~30 CPU clocks,
    ; so the CPU has plenty of time between interrupts.
    mov al, 0x34              ; Ch0, LSB+MSB, mode 2, binary
    out 0x43, al
    mov al, 200
    out 0x40, al              ; LSB
    mov al, 0
    out 0x40, al              ; MSB

    sti
    mov cx, 5000              ; ~5000 loop iters = ~20000 CPU clks = ~5000 PIT ticks
.wait9:
    loop .wait9
    cli

    ; Mask IRQs to stop further interrupts
    mov al, 0xFF
    out 0x21, al

    ; Should have fired more than 1 time (expect ~25 fires in 5000 PIT ticks / 200)
    cmp word [0x05F2], 2
    jb .test9_fail
    mov word [0x0520], 0x0001
.test9_fail:

    ; =====================================================================
    ; Test 10: GATE2 via PPI -- gate controls Ch2 counting
    ; =====================================================================
    ; Ch2 GATE is wired to PPI port B bit 0. Disable gate, program Ch2,
    ; verify count stays frozen. Enable gate, verify count decrements.

    ; Program PPI: control word 0x99 (port A=in, port B=out, port C=in)
    mov al, 0x99
    out 0x63, al

    ; Disable GATE2: clear PB0
    mov al, 0x00
    out 0x61, al

    ; Program Ch2: mode 0, LSB+MSB, count=0x7F00
    mov al, 0xB0              ; Ch2, LSB+MSB, mode 0, binary
    out 0x43, al
    mov al, 0x00
    out 0x42, al              ; LSB
    mov al, 0x7F
    out 0x42, al              ; MSB

    ; Wait plenty -- but GATE is low, so count should NOT decrement
    mov cx, 200
.wait10a:
    loop .wait10a

    ; Latch Ch2 and read (should still be ~0x7F00)
    mov al, 0x80              ; Counter latch Ch2
    out 0x43, al
    in al, 0x42               ; LSB
    mov bl, al
    in al, 0x42               ; MSB
    mov bh, al
    mov [0x05F4], bx          ; save frozen count

    ; Enable GATE2: set PB0
    mov al, 0x01
    out 0x61, al

    ; Wait for counting to happen
    mov cx, 400
.wait10b:
    loop .wait10b

    ; Latch Ch2 and read (should be less than frozen count)
    mov al, 0x80              ; Counter latch Ch2
    out 0x43, al
    in al, 0x42               ; LSB
    mov bl, al
    in al, 0x42               ; MSB
    mov bh, al

    ; Active count should be less than frozen count
    cmp bx, word [0x05F4]
    jae .test10_fail
    mov word [0x0522], 0x0001
.test10_fail:

    ; =====================================================================
    ; Test 11: BCD counting
    ; =====================================================================
    ; Program Ch0 mode 0, LSB+MSB, BCD mode, count=0x0500 (500 BCD).
    ; Wait, latch, read. Value should be < 0x0500 and valid BCD
    ; (all nibbles 0-9).

    mov al, 0x31              ; Ch0, LSB+MSB, mode 0, BCD (bit 0 = 1)
    out 0x43, al
    mov al, 0x00
    out 0x40, al              ; LSB = 0x00
    mov al, 0x90
    out 0x40, al              ; MSB = 0x90 -> count = 0x9000 BCD (9000 decimal)

    ; Wait for some decrements (~100 PIT ticks)
    mov cx, 100
.wait11:
    loop .wait11

    ; Latch and read
    mov al, 0x00              ; Counter latch Ch0
    out 0x43, al
    in al, 0x40               ; LSB
    mov bl, al
    in al, 0x40               ; MSB
    mov bh, al

    mov [0x052A], bx          ; dump BCD count
    ; Must be < 0x9000
    cmp bx, 0x9000
    jae .test11_fail
    ; Must be > 0
    cmp bx, 0
    je .test11_fail
    ; Validate BCD: each nibble must be <= 9
    mov dx, bx
    ; Check nibble 0
    mov al, dl
    and al, 0x0F
    cmp al, 0x0A
    jae .test11_fail
    ; Check nibble 1
    mov al, dl
    mov cl, 4
    shr al, cl
    cmp al, 0x0A
    jae .test11_fail
    ; Check nibble 2
    mov al, dh
    and al, 0x0F
    cmp al, 0x0A
    jae .test11_fail
    ; Check nibble 3
    mov al, dh
    shr al, cl
    cmp al, 0x0A
    jae .test11_fail
    mov word [0x0524], 0x0001
.test11_fail:

    ; =====================================================================
    ; Test 12: Two channels count independently
    ; =====================================================================
    ; Program Ch0 mode 0, LSB+MSB, count=0x2000
    ; Program Ch2 mode 0, LSB+MSB, count=0x4000 (GATE2 still enabled from test 10)
    ; Wait, latch both, verify both decremented and Ch2 > Ch0 (started higher).

    ; Ch0: mode 0, LSB+MSB, count=0x2000
    mov al, 0x30
    out 0x43, al
    mov al, 0x00
    out 0x40, al
    mov al, 0x20
    out 0x40, al

    ; Ch2: mode 0, LSB+MSB, count=0x4000
    mov al, 0xB0
    out 0x43, al
    mov al, 0x00
    out 0x42, al
    mov al, 0x40
    out 0x42, al

    ; Wait for both to decrement
    mov cx, 400
.wait12:
    loop .wait12

    ; Latch and read Ch0
    mov al, 0x00              ; Counter latch Ch0
    out 0x43, al
    in al, 0x40
    mov bl, al
    in al, 0x40
    mov bh, al
    mov si, bx                ; SI = Ch0 count

    ; Latch and read Ch2
    mov al, 0x80              ; Counter latch Ch2
    out 0x43, al
    in al, 0x42
    mov bl, al
    in al, 0x42
    mov bh, al                ; BX = Ch2 count

    ; Ch0 should be < 0x2000 (decremented)
    cmp si, 0x2000
    jae .test12_fail
    ; Ch2 should be < 0x4000 (decremented)
    cmp bx, 0x4000
    jae .test12_fail
    ; Ch2 should be > Ch0 (started with higher count, same rate)
    cmp bx, si
    jbe .test12_fail
    mov word [0x0526], 0x0001
.test12_fail:

    ; =====================================================================
    ; Test 13: Reload mid-count -- new count takes effect
    ; =====================================================================
    ; Program Ch0 mode 0, LSB only, count=200.
    ; Wait a bit (count decrements to ~150).
    ; Write new count=5 (without new control word).
    ; Wait. OUT0 should go high (terminal count on new short count).

    mov al, 0x10              ; Ch0, LSB only, mode 0, binary
    out 0x43, al
    mov al, 200
    out 0x40, al

    ; Wait ~50 PIT ticks so count is around 150
    mov cx, 200
.wait13a:
    loop .wait13a

    ; Verify OUT0 is still low (count > 0, hasn't reached TC yet)
    ; Re-init PIC to clear any stale IRR from previous tests
    mov al, 0x13
    out 0x20, al
    mov al, 0x08
    out 0x21, al
    mov al, 0x01
    out 0x21, al
    mov al, 0xFF
    out 0x21, al

    ; Now write new count=5 (same LSB-only rw_mode, no new control word)
    mov al, 5
    out 0x40, al

    ; Wait for the new short count to expire
    mov cx, 100
.wait13b:
    loop .wait13b

    ; OUT0 should now be high -> IRR bit 0 set
    mov al, 0x0A
    out 0x20, al
    in al, 0x20
    test al, 0x01
    jz .test13_fail
    mov word [0x0528], 0x0001
.test13_fail:

    int3

; =====================================================================
; PIT IRQ0 handler (INT 8) -- for Test 8 (single-shot)
; =====================================================================
pit_irq0_handler:
    push ax
    mov byte [0x05F0], 1      ; signal that ISR ran
    mov al, 0x20
    out 0x20, al              ; non-specific EOI
    pop ax
    iret

; =====================================================================
; PIT counting handler (INT 8) -- for Test 9 (periodic)
; =====================================================================
pit_count_handler:
    push ax
    add word [0x05F2], 1      ; increment fire counter
    mov al, 0x20
    out 0x20, al              ; non-specific EOI
    pop ax
    iret
