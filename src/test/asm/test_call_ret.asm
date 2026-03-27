; test_call_ret.asm -- CALL, RET, PUSH, POP tests
; Loaded at F000:0123 (physical 0xF0123). DS=0, SS=0 after reset.
; SP initialized to 0x0800 (stack at DS:0800 growing down).
;
; Expected results:
;   [0500] = 0x0007   near CALL/RET (3+4 computed by subroutine)
;   [0502] = 0x1234   PUSH/POP round-trip
;   [0504] = 0x000A   nested CALL (factorial-ish: add_n(4) = 4+3+2+1 = 10)
;   [0506] = 0xBEEF   PUSH reg / POP cross-reg

; @name CALL/RET
; @expect 0500 0007 near CALL/RET
; @expect 0502 1234 PUSH/POP
; @expect 0504 000A nested CALL
; @expect 0506 BEEF PUSH/POP cross
;
cpu 8086
org 0x0100

; Set up stack
mov ax, 0x0000
mov ss, ax
mov sp, 0x0800

; --- Test 1: near CALL/RET ---
mov ax, 0x0003
mov bx, 0x0004
call add_ab
mov [0x0500], ax        ; expect 0x0007

; --- Test 2: PUSH/POP ---
mov ax, 0x1234
push ax
mov ax, 0x0000          ; clobber AX
pop ax
mov [0x0502], ax        ; expect 0x1234

; --- Test 3: nested calls (sum 1..N) ---
mov cx, 4
call sum_n
mov [0x0504], ax        ; expect 10

; --- Test 4: PUSH reg / POP to different reg ---
mov bx, 0xBEEF
push bx
pop ax
mov [0x0506], ax        ; expect 0xBEEF

int3

; ---- subroutines ----

; add_ab: returns AX = AX + BX
add_ab:
    add ax, bx
    ret

; sum_n: returns AX = sum(1..CX). Recursive.
; Clobbers CX. Preserves BX.
sum_n:
    cmp cx, 0
    je .sum_zero
    push cx
    dec cx
    call sum_n          ; AX = sum(1..CX-1)
    pop cx
    add ax, cx          ; AX += CX
    ret
.sum_zero:
    xor ax, ax
    ret
