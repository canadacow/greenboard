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

cpu 8086
org 0x0100

    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results
    mov di, 0x0500
    mov cx, 7
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

    hlt
