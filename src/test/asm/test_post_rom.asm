; test_post_rom.asm -- BIOS POST TEST.02: ROS checksum test
; Faithful reproduction of PCBIOS.ASM TEST.02 + ROS_CHECKSUM proc.
;
; Checksums the 8K BIOS ROM at F000:E000 (physical 0xFE000-0xFFFFF).
; The sum of all 8192 bytes should be 0 (mod 256).
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   ROS checksum OK (sum == 0)

; @name POST ROM (TEST.02)
; @expect 0500 0001 ROS checksum
;
cpu 8086
org 0x0100

; =====================================================================
; Initialize
; =====================================================================
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x0800
    mov word [0x0500], 0x0000

; =====================================================================
; TEST.02: ROS Checksum (PCBIOS.ASM lines 319-343, 541-550)
;
; Sum all 8192 bytes at F000:E000. Result in AL should be 0.
; =====================================================================
    mov ax, 0xF000
    mov es, ax                  ; ES = F000 (ROM segment)
    mov bx, 0xE000              ; starting offset
    mov cx, 8192                ; byte count
    xor al, al                  ; accumulator
.checksum:
    add al, [es:bx]
    inc bx
    loop .checksum

    or al, al                   ; sum == 0?
    jnz .done                   ; no -- fail
    mov word [0x0500], 0x0001   ; yes -- pass

.done:
    hlt
