; test_dma_m2m.asm -- Memory-to-memory DMA transfer
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Tests the 8237A memory-to-memory transfer mode:
;   - Command register bit 0 enables mem-to-mem
;   - Channel 0 = source (read), Channel 1 = destination (write)
;   - Software DREQ on ch0 triggers the transfer
;   - TC is based on ch1's word count
;
; Setup:
;   1. CPU writes "Lorem ipsum dolor sit amet, " to 0x2000 (source)
;   2. Program DMA: ch0 addr=0x2000 (src), ch1 addr=0x7000 (dst)
;   3. Set command register bit 0 (mem-to-mem enable)
;   4. Software request on ch0 triggers block transfer
;   5. Verify destination at 0x7000 matches source
;
; @name DMA (mem-to-mem)
; @expect 0500 0001 First 4 bytes "Lore"
; @expect 0502 0001 Bytes 4-7 "m ip"
; @expect 0504 0001 Last 4 bytes "et, "
; @expect 0506 001C Byte count (28)
;
cpu 8086
org 0x0100

COPY_COUNT  equ 28

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

; =====================================================================
; Write source data to DRAM at 0x2000 (CPU writes, not DMA)
; =====================================================================
mov si, lorem_data
mov di, 0x2000
mov cx, COPY_COUNT
.copy_src:
    mov al, [cs:si]
    mov [di], al
    inc si
    inc di
    loop .copy_src

; =====================================================================
; Program DMA for memory-to-memory transfer
; =====================================================================

; Master clear
mov al, 0x00
out 0x0D, al

; Set command register: bit 0 = mem-to-mem enable
mov al, 0x01
out 0x08, al

; Clear flip-flop, set ch0 address = 0x2000 (source)
mov al, 0x00
out 0x0C, al
mov al, 0x00
out 0x00, al              ; ch0 addr low
mov al, 0x20
out 0x00, al              ; ch0 addr high

; Clear flip-flop, set ch0 count
mov al, 0x00
out 0x0C, al
mov al, ((COPY_COUNT - 1) & 0xFF)
out 0x01, al
mov al, ((COPY_COUNT - 1) >> 8)
out 0x01, al

; Clear flip-flop, set ch1 address = 0x7000 (destination)
mov al, 0x00
out 0x0C, al
mov al, 0x00
out 0x02, al              ; ch1 addr low
mov al, 0x70
out 0x02, al              ; ch1 addr high

; Clear flip-flop, set ch1 count = COPY_COUNT - 1 (TC based on ch1)
mov al, 0x00
out 0x0C, al
mov al, ((COPY_COUNT - 1) & 0xFF)
out 0x03, al
mov al, ((COPY_COUNT - 1) >> 8)
out 0x03, al

; Mode registers (transfer type ignored for m2m, but set increment)
mov al, 0x88              ; ch0: block, increment, read, ch0
out 0x0B, al
mov al, 0x85              ; ch1: block, increment, write, ch1
out 0x0B, al

; Page registers = 0
mov al, 0x00
out 0x87, al              ; ch0 page
out 0x83, al              ; ch1 page

; Unmask ch0 and ch1
mov al, 0x00
out 0x0A, al              ; unmask ch0
mov al, 0x01
out 0x0A, al              ; unmask ch1

; =====================================================================
; Trigger transfer via software request on ch0
; =====================================================================
mov al, 0x04              ; bit 2 = set request, bits 1:0 = ch0
out 0x09, al

; Mem-to-mem runs as block transfer to completion.
; Wait for it to finish.
mov cx, 0x2000
.wait_m2m:
    nop
    loop .wait_m2m

; =====================================================================
; Verify destination at 0x7000
; =====================================================================

; Check first 4 bytes: "Lore" = 0x4C 0x6F 0x72 0x65
cmp byte [0x7000], 0x4C
jne .fail_first4
cmp byte [0x7001], 0x6F
jne .fail_first4
cmp byte [0x7002], 0x72
jne .fail_first4
cmp byte [0x7003], 0x65
jne .fail_first4
mov word [0x0500], 0x0001
.fail_first4:

; Check bytes 4-7: "m ip" = 0x6D 0x20 0x69 0x70
cmp byte [0x7004], 0x6D
jne .fail_mid4
cmp byte [0x7005], 0x20
jne .fail_mid4
cmp byte [0x7006], 0x69
jne .fail_mid4
cmp byte [0x7007], 0x70
jne .fail_mid4
mov word [0x0502], 0x0001
.fail_mid4:

; Check last 4 bytes (positions 24-27): "et, " = 0x65 0x74 0x2C 0x20
cmp byte [0x7018], 0x65
jne .fail_last4
cmp byte [0x7019], 0x74
jne .fail_last4
cmp byte [0x701A], 0x2C
jne .fail_last4
cmp byte [0x701B], 0x20
jne .fail_last4
mov word [0x0504], 0x0001
.fail_last4:

; Store byte count
mov word [0x0506], COPY_COUNT

int3

; =====================================================================
; Source data (in code segment, copied to 0x2000 by CPU at startup)
; =====================================================================
lorem_data:
    db "Lorem ipsum dolor sit amet, "
