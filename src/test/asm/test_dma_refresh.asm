; test_dma_refresh.asm -- DRAM refresh via DMA channel 0
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Tests that DMA channel 0 DRAM refresh cycles don't corrupt memory.
; On the IBM PC 5150, PIT channel 1 fires DRQ0 every ~15us to trigger
; a dummy read cycle that refreshes one DRAM row. The 8237A auto-inits
; ch0 to cycle through all 256 row addresses.
;
; Test plan:
;   1. Write a known pattern across multiple DRAM pages
;   2. Enable DMA ch0 refresh (auto-init, single, read, ch0)
;   3. Start PIT ch1 generating DRQ0 pulses
;   4. Spin for many cycles (let refresh run)
;   5. Stop refresh
;   6. Verify pattern is intact (not corrupted by refresh reads)
;
; @name DMA (DRAM refresh)
; @expect 0500 0001 Pattern at 0x2000 intact
; @expect 0502 0001 Pattern at 0x4000 intact
; @expect 0504 0001 Pattern at 0x6000 intact
; @expect 0506 0001 Pattern at 0x8000 intact
; @expect 0508 0001 Refresh count > 0
;
cpu 8086
org 0x0100

mov ax, 0x0000
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x0800

; Zero results
mov word [0x0500], 0x0000
mov word [0x0502], 0x0000
mov word [0x0504], 0x0000
mov word [0x0506], 0x0000
mov word [0x0508], 0x0000

; =====================================================================
; Write known pattern to DRAM at several locations
; Pattern: addr low byte XOR 0xA5
; =====================================================================
mov di, 0x2000
mov cx, 16
.fill0:
    mov al, cl
    xor al, 0xA5
    mov [di], al
    inc di
    loop .fill0

mov di, 0x4000
mov cx, 16
.fill1:
    mov al, cl
    xor al, 0xA5
    mov [di], al
    inc di
    loop .fill1

mov di, 0x6000
mov cx, 16
.fill2:
    mov al, cl
    xor al, 0xA5
    mov [di], al
    inc di
    loop .fill2

mov di, 0x8000
mov cx, 16
.fill3:
    mov al, cl
    xor al, 0xA5
    mov [di], al
    inc di
    loop .fill3

; =====================================================================
; Program DMA ch0 for DRAM refresh
; =====================================================================

; Master clear
mov al, 0x00
out 0x0D, al

; Clear flip-flop, set ch0 address = 0x0000
out 0x0C, al
mov al, 0x00
out 0x00, al              ; ch0 addr low
out 0x00, al              ; ch0 addr high

; Clear flip-flop, set ch0 count = 0xFFFF (256 rows, wraps with auto-init)
out 0x0C, al
mov al, 0xFF
out 0x01, al              ; count low
out 0x01, al              ; count high

; Mode: single transfer, auto-init, read (mem->pseudo-IO), ch0
; [7:6]=01 single, [5]=0 inc, [4]=1 auto-init, [3:2]=10 read, [1:0]=00 ch0
; = 0101_1000 = 0x58
mov al, 0x58
out 0x0B, al

; Page register ch0 = 0 (port 0x87)
mov al, 0x00
out 0x87, al

; Unmask ch0
mov al, 0x00
out 0x0A, al

; =====================================================================
; Program PIT channel 1 to generate DRQ0 pulses (mode 2, rate generator)
; Real BIOS programs it with count=18 for ~15us refresh interval.
; =====================================================================
mov al, 0x54              ; ch1, lobyte only, mode 2, binary
out 0x43, al
mov al, 18                ; count = 200 (slower refresh to give CPU bus time)
out 0x41, al

; =====================================================================
; Spin for a while to let refresh cycles run
; =====================================================================
mov cx, 0x0010
.spin:
    nop
    nop
    loop .spin

; =====================================================================
; Stop refresh: mask ch0
; =====================================================================
mov al, 0x04              ; mask ch0: bit 2 = set mask, bits 1:0 = ch0
out 0x0A, al

; Read DMA ch0 current address to see how many refreshes occurred
out 0x0C, al              ; clear flip-flop
in al, 0x00               ; ch0 addr low
mov ah, 0x00
mov [0x050A], ax           ; store for debugging

; =====================================================================
; Verify patterns are intact
; =====================================================================

; Check 0x2000 region
mov di, 0x2000
mov cx, 16
mov bl, 16
.chk0:
    mov al, bl
    xor al, 0xA5
    cmp [di], al
    jne .fail0
    inc di
    dec bl
    loop .chk0
mov word [0x0500], 0x0001
.fail0:

; Check 0x4000 region
mov di, 0x4000
mov cx, 16
mov bl, 16
.chk1:
    mov al, bl
    xor al, 0xA5
    cmp [di], al
    jne .fail1
    inc di
    dec bl
    loop .chk1
mov word [0x0502], 0x0001
.fail1:

; Check 0x6000 region
mov di, 0x6000
mov cx, 16
mov bl, 16
.chk2:
    mov al, bl
    xor al, 0xA5
    cmp [di], al
    jne .fail2
    inc di
    dec bl
    loop .chk2
mov word [0x0504], 0x0001
.fail2:

; Check 0x8000 region
mov di, 0x8000
mov cx, 16
mov bl, 16
.chk3:
    mov al, bl
    xor al, 0xA5
    cmp [di], al
    jne .fail3
    inc di
    dec bl
    loop .chk3
mov word [0x0506], 0x0001
.fail3:

; Check that at least some refreshes happened (ch0 addr moved from 0)
cmp word [0x050A], 0x0000
je .fail4
mov word [0x0508], 0x0001
.fail4:

hlt
