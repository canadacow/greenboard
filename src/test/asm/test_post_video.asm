; test_post_video.asm -- BIOS POST TEST.08-11: Video RAM + extended RAM
; Adapted from PCBIOS.ASM TEST.08 (video RAM test), TEST.09 (display bar),
; TEST.10 (CRT line test -- skipped, no 6845), TEST.11 (extended RAM).
;
; Uses MDA framebuffer at B000:0000 (physical 0xB0000), 4KB.
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   Video RAM write/read (0xFF pattern)
;   [0502] = 0x0001   Video RAM write/read (0x00 pattern)
;   [0504] = 0x0001   Video RAM addressability (walking byte)
;   [0506] = 0x0001   CRT hsync line transitions (TEST.10)
;   [0508] = 0x0001   CRT video line transitions (TEST.10)
;   [050A] = 0x0001   Extended RAM write/read (TEST.11)

; @name POST Video/RAM (TEST.08-11)
; @expect 0500 0001 VRAM pattern FF
; @expect 0502 0001 VRAM pattern 00
; @expect 0504 0001 VRAM addressing
; @expect 0506 0001 CRT hsync
; @expect 0508 0001 CRT video
; @expect 050A 0001 Extended RAM
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
    mov word [0x0502], 0x0000
    mov word [0x0504], 0x0000
    mov word [0x0506], 0x0000
    mov word [0x0508], 0x0000
    mov word [0x050A], 0x0000

    mov ax, 0xB000
    mov es, ax              ; ES = MDA framebuffer segment

; =====================================================================
; Test 1: Write 0xFF to video RAM, read back (TEST.08 pattern test)
; =====================================================================
    xor di, di
    mov cx, 8
    mov al, 0xFF
    cld
    rep stosb

    xor si, si
    mov cx, 8
.check_vff:
    mov al, [es:si]
    cmp al, 0xFF
    jne .test2
    inc si
    loop .check_vff
    mov word [0x0500], 0x0001

; =====================================================================
; Test 2: Write 0x00 to video RAM, read back
; =====================================================================
.test2:
    xor di, di
    mov cx, 8
    xor al, al
    rep stosb

    xor si, si
    mov cx, 8
.check_v00:
    mov al, [es:si]
    cmp al, 0x00
    jne .test3
    inc si
    loop .check_v00
    mov word [0x0502], 0x0001

; =====================================================================
; Test 3: Video RAM addressability -- write sequential values, verify
; Mirrors STGTST address verification logic.
; =====================================================================
.test3:
    xor di, di
    mov cx, 8
    xor al, al
.fill_seq:
    stosb
    inc al
    loop .fill_seq

    xor si, si
    mov cx, 8
    xor ah, ah
.check_seq:
    mov al, [es:si]
    cmp al, ah
    jne .test4
    inc si
    inc ah
    loop .check_seq
    mov word [0x0504], 0x0001

; =====================================================================
; Test 4: CRT hsync line test (TEST.10, PCBIOS.ASM lines 837-855)
; Read MDA status port 0x3BA, check bit 0 (hsync) transitions.
; Wait for it to be on, then off.
; =====================================================================
.test4:
    mov dx, 0x3BA           ; MDA status port
    mov ah, 0x01            ; bit 0 = hsync

    ; Wait for hsync on
    xor cx, cx
.hsync_on:
    in al, dx
    and al, ah
    jnz .hsync_off_wait
    loop .hsync_on
    jmp .test5              ; timeout

    ; Wait for hsync off
.hsync_off_wait:
    xor cx, cx
.hsync_off:
    in al, dx
    and al, ah
    jz .hsync_pass
    loop .hsync_off
    jmp .test5              ; timeout

.hsync_pass:
    mov word [0x0506], 0x0001

; =====================================================================
; Test 5: CRT video line test (TEST.10 continued)
; Check bit 3 (video) transitions.
; =====================================================================
.test5:
    mov ah, 0x08            ; bit 3 = video

    xor cx, cx
.video_on:
    in al, dx
    and al, ah
    jnz .video_off_wait
    loop .video_on
    jmp .test6

.video_off_wait:
    xor cx, cx
.video_off:
    in al, dx
    and al, ah
    jz .video_pass
    loop .video_off
    jmp .test6

.video_pass:
    mov word [0x0508], 0x0001

; =====================================================================
; Test 6: Extended RAM test (TEST.11)
; Write/read a few bytes at segment 0x0400 (physical 0x04000).
; =====================================================================
.test6:
    mov ax, 0x0400
    mov es, ax
    xor di, di
    mov cx, 4
    mov al, 0xA5
    cld
    rep stosb

    xor si, si
    mov cx, 4
.check_ext:
    mov al, [es:si]
    cmp al, 0xA5
    jne .done
    inc si
    loop .check_ext
    mov word [0x050A], 0x0001

.done:
    hlt
