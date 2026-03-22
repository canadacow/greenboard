; test_mmio.asm -- Memory-mapped I/O test via ISA Test Card
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Tests read/write to 4KB MMIO region at 0xB0000-0xB0FFF provided
; by the ISA Test Card (MDA framebuffer region).
;
; Tests:
;   1. Write byte, read it back
;   2. Write word, read it back
;   3. Write pattern to multiple locations, verify all
;   4. Read-modify-write (OR a bit in)
;   5. Verify non-MMIO RAM is unaffected
;
; @name MMIO (MDA region)
; @expect 0500 0001 Byte write/read
; @expect 0502 0001 Word write/read
; @expect 0504 0001 Pattern fill (8 bytes)
; @expect 0506 0001 Read-modify-write (OR)
; @expect 0508 0001 DRAM at 0x2000 unaffected
;
cpu 8086
org 0x0100

MMIO_SEG    equ 0xB000      ; segment for 0xB0000
RESULT_BASE equ 0x0500

mov ax, 0x0000
mov ds, ax
mov ss, ax
mov sp, 0x0800

; Zero results
mov word [RESULT_BASE + 0], 0x0000
mov word [RESULT_BASE + 2], 0x0000
mov word [RESULT_BASE + 4], 0x0000
mov word [RESULT_BASE + 6], 0x0000
mov word [RESULT_BASE + 8], 0x0000

; Point ES at MMIO segment
mov ax, MMIO_SEG
mov es, ax

; =====================================================================
; Test 1: Byte write/read
; =====================================================================
mov byte [es:0x0000], 0xA5      ; write 0xA5 to 0xB0000
mov al, [es:0x0000]             ; read it back
cmp al, 0xA5
jne .skip1
mov word [RESULT_BASE + 0], 0x0001
.skip1:

; =====================================================================
; Test 2: Word write/read
; =====================================================================
mov word [es:0x0010], 0xBEEF    ; write 0xBEEF to 0xB0010
mov ax, [es:0x0010]             ; read it back
cmp ax, 0xBEEF
jne .skip2
mov word [RESULT_BASE + 2], 0x0001
.skip2:

; =====================================================================
; Test 3: Pattern fill -- write 8 bytes, verify all
; =====================================================================
mov di, 0x0100                  ; start at 0xB0100
mov cx, 8
mov al, 0x41                   ; 'A'
.fill:
    mov [es:di], al
    inc di
    inc al
    loop .fill

; Verify: should be 'A','B','C','D','E','F','G','H'
mov si, 0x0100
mov bl, 0x41
mov cx, 8
.check:
    mov al, [es:si]
    cmp al, bl
    jne .skip3
    inc si
    inc bl
    loop .check
mov word [RESULT_BASE + 4], 0x0001
.skip3:

; =====================================================================
; Test 4: Read-modify-write (OR a bit)
; =====================================================================
mov byte [es:0x0200], 0x0F      ; write 0x0F
mov al, [es:0x0200]             ; read
or al, 0xF0                     ; modify
mov [es:0x0200], al             ; write back
mov al, [es:0x0200]             ; read final
cmp al, 0xFF
jne .skip4
mov word [RESULT_BASE + 6], 0x0001
.skip4:

; =====================================================================
; Test 5: Verify DRAM at 0x2000 is not corrupted by MMIO writes
; Write a known value to DRAM, do MMIO ops, check DRAM unchanged.
; =====================================================================
mov byte [0x2000], 0x42          ; write 'B' to DRAM
mov byte [es:0x0000], 0xFF      ; MMIO write (different value)
mov al, [0x2000]                ; read DRAM back
cmp al, 0x42
jne .skip5
mov word [RESULT_BASE + 8], 0x0001
.skip5:

hlt
