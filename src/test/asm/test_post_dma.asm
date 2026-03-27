; test_post_dma.asm -- BIOS POST TEST.03: DMA init + Timer 1 bit walk
; Mirrors the real IBM PC BIOS TEST.03 sequence from PCBIOS.ASM.
; Requires: 8237A DMA, 8253 PIT (timer 1)
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   Timer 1 bits-on (all bits seen High)
;   [0502] = 0x0001   Timer 1 bits-off (all bits seen Low)
;   [0504] = 0x0001   DMA register wrap (0xFF pattern)
;   [0506] = 0x0001   DMA register wrap (0x00 pattern)

; @name POST DMA (TEST.03)
; @expect 0500 0001 Timer1 bits-on
; @expect 0502 0001 Timer1 bits-off
; @expect 0504 0001 DMA wrap 0xFF
; @expect 0506 0001 DMA wrap 0x00
;
cpu 8086
org 0x0100

; Port constants (matching PCBIOS.ASM)
%define TIMER   0x40
%define DMA     0x00
%define DMA08   0x08

; ---------------------------------------------------------------------------
; Disable DMA controller
; ---------------------------------------------------------------------------
    mov al, 0x04
    out DMA08, al

; ---------------------------------------------------------------------------
; Test 1: Timer 1 bits-on
; Program timer 1: LSB only, mode 2, count=0 (wraps to 65536 in mode 2).
; Read repeatedly until OR of all reads = 0xFF (every bit seen High).
; ---------------------------------------------------------------------------
    mov al, 0x54            ; sel timer 1, LSB only, mode 2
    out TIMER+3, al
    xor cx, cx              ; CX = 0 (loop 65536 times max)
    mov bl, cl              ; BL = 0 (accumulator)
    mov al, cl              ; count = 0
    out TIMER+1, al         ; load timer 1

.bits_on:
    mov al, 0x40            ; latch timer 1 (counter 1, latch cmd)
    out TIMER+3, al
    in  al, TIMER+1         ; read latched count
    or  bl, al              ; accumulate bits
    cmp bl, 0xFF
    je  .bits_on_pass
    loop .bits_on
    ; timeout -- not all bits seen
    mov word [0x0500], 0x0000
    jmp .test2
.bits_on_pass:
    mov word [0x0500], 0x0001

; ---------------------------------------------------------------------------
; Test 2: Timer 1 bits-off
; Reload timer 1 with count=0xFF. Read repeatedly until AND of all reads = 0.
; (Every bit seen Low at least once.)
; ---------------------------------------------------------------------------
.test2:
    mov al, bl              ; BL = 0xFF from bits-on
    xor cx, cx              ; loop 65536 times max
    out TIMER+1, al         ; load timer 1 with 0xFF

.bits_off:
    mov al, 0x40            ; latch timer 1
    out TIMER+3, al
    in  al, TIMER+1         ; read latched count
    and bl, al
    jz  .bits_off_pass
    loop .bits_off
    ; timeout -- not all bits cleared
    mov word [0x0502], 0x0000
    jmp .test3
.bits_off_pass:
    mov word [0x0502], 0x0001

; ---------------------------------------------------------------------------
; Re-initialize timer 1 for refresh (as BIOS does before DMA test)
; ---------------------------------------------------------------------------
.test3:
    mov al, 0x54            ; sel timer 1, LSB, mode 2
    out TIMER+3, al
    mov al, 18              ; divisor for refresh
    out TIMER+1, al
    out DMA+0x0D, al        ; master clear DMA

; ---------------------------------------------------------------------------
; Test 3 & 4: DMA channel register wrap (0xFF then 0x00)
; Write pattern to all 8 channel registers (ports 0x00-0x07), each is 16-bit
; (LSB then MSB). Read back and verify.
; ---------------------------------------------------------------------------
    ; -- 0xFF pattern --
    mov al, 0xFF
    call .wrap_dma_regs
    jnz .wrap_ff_fail
    mov word [0x0504], 0x0001
    jmp .wrap_00
.wrap_ff_fail:
    mov word [0x0504], 0x0000

.wrap_00:
    ; -- 0x00 pattern --
    mov al, 0x00
    call .wrap_dma_regs
    jnz .wrap_00_fail
    mov word [0x0506], 0x0001
    jmp .done
.wrap_00_fail:
    mov word [0x0506], 0x0000

.done:
    int3

; ---------------------------------------------------------------------------
; Subroutine: write AL pattern to all 8 DMA channel regs, verify readback.
; Returns ZF=1 on success, ZF=0 on failure.
; Mirrors BIOS C16/C17/C18 loop exactly.
; ---------------------------------------------------------------------------
.wrap_dma_regs:
    mov bl, al              ; save pattern
    mov bh, al              ; BX = expected 16-bit value
    mov cx, 8               ; 8 registers (ch0 addr, ch0 cnt, ch1 addr, ...)
    mov dx, DMA             ; port 0x00
.wrap_loop:
    mov al, bl
    out dx, al              ; write LSB
    out dx, al              ; write MSB
    mov ax, 0x0101          ; trash AX before read
    in  al, dx              ; read LSB
    mov ah, al              ; save LSB in AH
    in  al, dx              ; read MSB
    ; Now AX = {LSB, MSB} -- but note: AH=LSB read, AL=MSB read
    ; BIOS compares BX to AX where BH=BL=pattern, AH=LSB, AL=MSB
    cmp bx, ax
    jne .wrap_fail
    inc dx
    loop .wrap_loop
    xor al, al              ; set ZF=1 (success)
    ret
.wrap_fail:
    or al, 1                ; clear ZF (failure)
    ret
