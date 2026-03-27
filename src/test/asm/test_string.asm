; test_string.asm -- string operations (MOVS, STOS, LODS, CMPS, SCAS, REP)
; Loaded at F000:0123 (physical 0xF0123). DS=0, ES=0 after reset.
; SP initialized to 0x0800.
;
; Expected results:
;   [0500] = 0x0001   REP MOVSB copied 4 bytes correctly
;   [0502] = 0x0001   REP STOSB filled 4 bytes correctly
;   [0504] = 0x0004   LODSB loaded last byte (4th element)
;   [0506] = 0x0001   REPNE SCASB found match (AL=0x33 in string)
;   [0508] = 0x0002   SCASB found at correct position (CX remaining)
;   [050A] = 0x0001   REPE CMPSB matched 4-byte string
;   [050C] = 0x0001   MOVSW (word move) works
;   [050E] = 0x0001   STD reverses direction

; @name Strings
; @expect 0500 0001 REP MOVSB
; @expect 0502 0001 REP STOSB
; @expect 0504 0044 LODSB
; @expect 0506 0001 REPNE SCASB found
; @expect 0508 0001 SCASB position
; @expect 050A 0001 REPE CMPSB
; @expect 050C 0001 MOVSW
; @expect 050E 0001 STD reverse
;
cpu 8086
org 0x0100

; Set up segments and stack
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
mov word [0x050A], 0x0000
mov word [0x050C], 0x0000
mov word [0x050E], 0x0000

; Set up source data at 0x0600
mov byte [0x0600], 0x11
mov byte [0x0601], 0x22
mov byte [0x0602], 0x33
mov byte [0x0603], 0x44

; =====================================================================
; Test 1: REP MOVSB -- copy 4 bytes from 0x0600 to 0x0700
; =====================================================================
cld
mov si, 0x0600
mov di, 0x0700
mov cx, 4
rep movsb
; Verify: compare [0700..0703] with source
cmp byte [0x0700], 0x11
jne .movs_fail
cmp byte [0x0701], 0x22
jne .movs_fail
cmp byte [0x0702], 0x33
jne .movs_fail
cmp byte [0x0703], 0x44
jne .movs_fail
mov word [0x0500], 0x0001
.movs_fail:

; =====================================================================
; Test 2: REP STOSB -- fill 4 bytes at 0x0710 with 0xAA
; =====================================================================
mov al, 0xAA
mov di, 0x0710
mov cx, 4
rep stosb
cmp byte [0x0710], 0xAA
jne .stos_fail
cmp byte [0x0711], 0xAA
jne .stos_fail
cmp byte [0x0712], 0xAA
jne .stos_fail
cmp byte [0x0713], 0xAA
jne .stos_fail
mov word [0x0502], 0x0001
.stos_fail:

; =====================================================================
; Test 3: LODSB x4 -- load successive bytes, AL gets the last one
; =====================================================================
mov si, 0x0600
cld
lodsb                     ; AL = [0600] = 0x11
lodsb                     ; AL = [0601] = 0x22
lodsb                     ; AL = [0602] = 0x33
lodsb                     ; AL = [0603] = 0x44
mov ah, 0
mov [0x0504], ax          ; expect 0x0044... wait, 0x44 = 68, but we want 4
; Actually we want AL as loaded = 0x44
; Change expectation to 0x0044

; =====================================================================
; Test 4 & 5: REPNE SCASB -- find 0x33 in the string at 0x0600
; =====================================================================
mov di, 0x0600
mov cx, 4
mov al, 0x33
cld
repne scasb
je .scas_found
jmp .scas_fail
.scas_found:
mov word [0x0506], 0x0001  ; found it
mov [0x0508], cx           ; CX = remaining count (should be 1: scanned 3, stopped at 4th)
.scas_fail:

; =====================================================================
; Test 6: REPE CMPSB -- compare two identical 4-byte strings
; =====================================================================
; Copy source to 0x0720 for comparison target
mov si, 0x0600
mov di, 0x0720
mov cx, 4
rep movsb
; Now compare
mov si, 0x0600
mov di, 0x0720
mov cx, 4
cld
repe cmpsb
cmp cx, 0                  ; if all matched, CX=0
jne .cmps_fail
mov word [0x050A], 0x0001
.cmps_fail:

; =====================================================================
; Test 7: MOVSW -- word-size move
; =====================================================================
mov word [0x0730], 0xBEEF
mov si, 0x0730
mov di, 0x0740
cld
movsw
cmp word [0x0740], 0xBEEF
jne .movsw_fail
mov word [0x050C], 0x0001
.movsw_fail:

; =====================================================================
; Test 8: STD -- reverse direction (DI decrements)
; =====================================================================
; STOSB backwards: fill 0x0753..0x0750 with 0x55
mov al, 0x55
mov di, 0x0753             ; start at high end
mov cx, 4
std
rep stosb
cld                        ; restore direction
cmp byte [0x0750], 0x55
jne .std_fail
cmp byte [0x0751], 0x55
jne .std_fail
cmp byte [0x0752], 0x55
jne .std_fail
cmp byte [0x0753], 0x55
jne .std_fail
mov word [0x050E], 0x0001
.std_fail:

int3
