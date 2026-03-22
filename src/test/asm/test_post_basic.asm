; test_post_basic.asm -- BIOS POST TEST.05: ROS Checksum II (BASIC ROMs)
; Faithful reproduction of PCBIOS.ASM TEST.05 (lines 738-754).
;
; Checksums the 4 Cassette BASIC ROM modules (U29-U32), each 8K:
;   F000:6000 (U32), F000:8000 (U31), F000:A000 (U30), F000:C000 (U29)
; Each module's 8192 bytes must sum to 0 (mod 256).
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   All 4 BASIC ROM checksums OK

; @name POST BASIC ROMs (TEST.05)
; @expect 0500 0001 BASIC ROM checksums
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
; TEST.05: Checksum 4 BASIC ROM modules (PCBIOS.ASM lines 743-751)
; ROS_CHECKSUM: sum 8192 bytes at CS:BX, result in AL, ZF set if OK.
; =====================================================================
    mov ax, 0xF000
    mov es, ax              ; ES = ROM segment
    mov dl, 4               ; 4 modules
    mov bx, 0x6000          ; starting offset

.check_ros:
    mov cx, 8192
    xor al, al
.sum_loop:
    add al, [es:bx]
    inc bx
    loop .sum_loop

    or al, al               ; sum == 0?
    jnz .done               ; fail -- bad checksum
    dec dl
    jnz .check_ros          ; next module

    mov word [0x0500], 0x0001

.done:
    hlt
