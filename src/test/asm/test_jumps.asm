; test_jumps.asm -- conditional jumps and loops
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   JE taken
;   [0502] = 0x0001   JNE taken
;   [0504] = 0x0001   JL taken (signed less)
;   [0506] = 0x0001   JG taken (signed greater)
;   [0508] = 0x0001   JB taken (unsigned below)
;   [050A] = 0x0001   JA taken (unsigned above)
;   [050C] = 0x0005   LOOP counter (5 iterations)
;   [050E] = 0x0037   LOOP sum (1+2+...+10 = 55 = 0x37)

; @name Jumps/Loops
; @expect 0500 0001 JE
; @expect 0502 0001 JNE
; @expect 0504 0001 JL
; @expect 0506 0001 JG
; @expect 0508 0001 JB
; @expect 050A 0001 JA
; @expect 050C 0005 LOOP count
; @expect 050E 0037 LOOP sum
;
cpu 8086
org 0x0100

; JE
mov ax, 42
cmp ax, 42
je .je_yes
mov word [0x0500], 0x0000
jmp .je_done
.je_yes:
mov word [0x0500], 0x0001
.je_done:

; JNE
mov ax, 42
cmp ax, 99
jne .jne_yes
mov word [0x0502], 0x0000
jmp .jne_done
.jne_yes:
mov word [0x0502], 0x0001
.jne_done:

; JL (signed: -1 < 1)
mov ax, 0xFFFF          ; -1
cmp ax, 0x0001
jl .jl_yes
mov word [0x0504], 0x0000
jmp .jl_done
.jl_yes:
mov word [0x0504], 0x0001
.jl_done:

; JG (signed: 1 > -1)
mov ax, 0x0001
cmp ax, 0xFFFF          ; compare 1 vs -1
jg .jg_yes
mov word [0x0506], 0x0000
jmp .jg_done
.jg_yes:
mov word [0x0506], 0x0001
.jg_done:

; JB (unsigned: 1 < 0xFFFF)
mov ax, 0x0001
cmp ax, 0xFFFF
jb .jb_yes
mov word [0x0508], 0x0000
jmp .jb_done
.jb_yes:
mov word [0x0508], 0x0001
.jb_done:

; JA (unsigned: 0xFFFF > 1)
mov ax, 0xFFFF
cmp ax, 0x0001
ja .ja_yes
mov word [0x050A], 0x0000
jmp .ja_done
.ja_yes:
mov word [0x050A], 0x0001
.ja_done:

; LOOP: count 5 iterations
mov cx, 5
xor bx, bx
.loop1:
inc bx
loop .loop1
mov [0x050C], bx        ; expect 5

; LOOP: sum 1..10
mov cx, 10
xor ax, ax
.loop2:
add ax, cx
loop .loop2
mov [0x050E], ax        ; expect 55 = 0x37

hlt
