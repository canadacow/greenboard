; test_jumps.asm -- conditional jumps and loops
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
;
; Expected results:
;   [0200] = 0x0001   JE taken
;   [0202] = 0x0001   JNE taken
;   [0204] = 0x0001   JL taken (signed less)
;   [0206] = 0x0001   JG taken (signed greater)
;   [0208] = 0x0001   JB taken (unsigned below)
;   [020A] = 0x0001   JA taken (unsigned above)
;   [020C] = 0x0005   LOOP counter (5 iterations)
;   [020E] = 0x0037   LOOP sum (1+2+...+10 = 55 = 0x37)

cpu 8086
org 0x0123

; JE
mov ax, 42
cmp ax, 42
je .je_yes
mov word [0x0200], 0x0000
jmp .je_done
.je_yes:
mov word [0x0200], 0x0001
.je_done:

; JNE
mov ax, 42
cmp ax, 99
jne .jne_yes
mov word [0x0202], 0x0000
jmp .jne_done
.jne_yes:
mov word [0x0202], 0x0001
.jne_done:

; JL (signed: -1 < 1)
mov ax, 0xFFFF          ; -1
cmp ax, 0x0001
jl .jl_yes
mov word [0x0204], 0x0000
jmp .jl_done
.jl_yes:
mov word [0x0204], 0x0001
.jl_done:

; JG (signed: 1 > -1)
mov ax, 0x0001
cmp ax, 0xFFFF          ; compare 1 vs -1
jg .jg_yes
mov word [0x0206], 0x0000
jmp .jg_done
.jg_yes:
mov word [0x0206], 0x0001
.jg_done:

; JB (unsigned: 1 < 0xFFFF)
mov ax, 0x0001
cmp ax, 0xFFFF
jb .jb_yes
mov word [0x0208], 0x0000
jmp .jb_done
.jb_yes:
mov word [0x0208], 0x0001
.jb_done:

; JA (unsigned: 0xFFFF > 1)
mov ax, 0xFFFF
cmp ax, 0x0001
ja .ja_yes
mov word [0x020A], 0x0000
jmp .ja_done
.ja_yes:
mov word [0x020A], 0x0001
.ja_done:

; LOOP: count 5 iterations
mov cx, 5
xor bx, bx
.loop1:
inc bx
loop .loop1
mov [0x020C], bx        ; expect 5

; LOOP: sum 1..10
mov cx, 10
xor ax, ax
.loop2:
add ax, cx
loop .loop2
mov [0x020E], ax        ; expect 55 = 0x37

hlt
