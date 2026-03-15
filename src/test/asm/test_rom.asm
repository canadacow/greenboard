; test_rom.asm -- ROM chip access through real decode chain.
;
; Loaded at F000:0123 (physical 0xF0123). DS=SS=0 after reset.
; The BIOS ROM (U33) is wired at FE000-FFFFF via 74S20 + 74S138 + IC_ROM_8K.
;
; Test 1: Verify identity string at ROM offset 0 (physical FE000).
;   The first 22 bytes of the BIOS ROM are "1501476 COPR. IBM 1982".
;   We read them via the 8088 and compare byte-by-byte.
;
; Test 2: Compute a 16-bit byte sum of the entire 8K ROM.
;   Sum all 8192 bytes into a 16-bit accumulator.
;   Expected: 0xB000 (verified offline).
;
; Expected results:
;   [0500] = 0x0001   string match (1=pass, 0=fail)
;   [0502] = 0x0031   first ROM byte ('1' = 0x31)
;   [0504] = 0x0032   last string byte ('2' = 0x32)
;   [0506] = 0xB000   16-bit byte sum of 8K ROM

; @name ROM (BIOS U33)
; @expect 0500 0001 ID string match
; @expect 0502 0031 first ROM byte ('1')
; @expect 0504 0032 last string byte ('2')
; @expect 0506 B000 8K byte sum
;
cpu 8086
org 0x0100

; ---- Test 1: Verify identity string ----

    ; Read first byte from ROM before any segment changes (sanity).
    ; ES defaults to 0, set it to FE00 for ROM access.
    mov ax, 0xFE00
    mov es, ax
    mov al, [es:0]          ; first ROM byte
    xor ah, ah
    push ax                 ; save for later

    ; Read byte at offset 21 (last char of "1501476 COPR. IBM 1982").
    mov al, [es:21]
    xor ah, ah
    push ax                 ; save for later

    ; Set DS=0100 so we can address the expected string embedded in our code.
    mov ax, 0x0100
    mov ds, ax

    ; Compare 22 bytes: DS:SI (expected) vs ES:DI (ROM at FE00:0000).
    mov si, expected_string ; offset within 0100 segment (NASM org 0x0100)
    xor di, di              ; ES:0000 = physical FE000
    mov cx, 22
    cld
    repe cmpsb

    ; Store result to DS=0 address space.
    xor ax, ax
    mov ds, ax              ; DS=0 for result writes

    ; ZF=1 means all 22 bytes matched.
    jne .no_match
    mov word [0x0500], 1    ; pass
    jmp .match_done
.no_match:
    mov word [0x0500], 0    ; fail
.match_done:

    ; Store first ROM byte.
    pop ax                  ; last string byte (pushed second)
    mov [0x0504], ax
    pop ax                  ; first ROM byte (pushed first)
    mov [0x0502], ax

; ---- Test 2: 16-bit byte sum of entire 8K ROM ----

    mov ax, 0xFE00
    mov ds, ax              ; DS=FE00, ROM segment
    xor si, si              ; start at offset 0
    xor dx, dx              ; 16-bit accumulator
    mov cx, 8192
.sum_loop:
    lodsb                   ; AL = [DS:SI], SI++
    xor ah, ah
    add dx, ax
    loop .sum_loop

    ; Store checksum at DS=0.
    xor ax, ax
    mov ds, ax
    mov [0x0506], dx        ; 16-bit byte sum

    hlt

; ---- Data ----
expected_string:
    db "1501476 COPR. IBM 1982"
