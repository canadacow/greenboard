; test_div.asm -- DIV, IDIV, and divide-by-zero exception
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800 (stack at DS:0800 growing down).
;
; Expected results:
;   [0500] = 0x0003   DIV byte: 10 / 3 = 3 (quotient in AL)
;   [0502] = 0x0001   DIV byte: 10 / 3 = 1 (remainder from AH via CL)
;   [0504] = 0x000A   DIV word: 100 / 10 = 10 (quotient)
;   [0506] = 0x0000   DIV word: 100 / 10 = 0 (remainder)
;   [0508] = 0xFFFD   IDIV byte: -10 / 3 = -3 (sign-extended quotient)
;   [050A] = 0xFFFF   IDIV byte: -10 / 3 = -1 (sign-extended remainder)
;   [050C] = 0x0001   DIV by zero: INT 0 handler ran
;   [050E] = 0x0001   DIV overflow: INT 0 handler ran

; @name DIV/IDIV
; @expect 0500 0003 DIV byte quot
; @expect 0502 0001 DIV byte rem
; @expect 0504 000A DIV word quot
; @expect 0506 0000 DIV word rem
; @expect 0508 FFFD IDIV byte quot
; @expect 050A FFFF IDIV byte rem
; @expect 050C 0001 DIV by zero
; @expect 050E 0001 DIV overflow
;
cpu 8086
org 0x0100

; Set up stack
mov ax, 0x0000
mov ss, ax
mov sp, 0x0800

; Zero result area
mov word [0x0500], 0x0000
mov word [0x0502], 0x0000
mov word [0x0504], 0x0000
mov word [0x0506], 0x0000
mov word [0x0508], 0x0000
mov word [0x050A], 0x0000
mov word [0x050C], 0x0000
mov word [0x050E], 0x0000

; =====================================================================
; Install INT 0 handler (divide error)
; =====================================================================
mov word [0x00], handler_div0
mov word [0x02], 0x0100

; =====================================================================
; Test 1 & 2: DIV byte -- 10 / 3
; AX = dividend, r/m8 = divisor. Result: AL = quotient, AH = remainder.
; Exercise byte reg moves: save AH into CL, then store separately.
; =====================================================================
mov ax, 10
mov bl, 3
div bl
mov cl, ah               ; CL = remainder (exercises MOV r8, r8 with AH)
mov byte [0x0500], al    ; quotient = 3
mov byte [0x0501], 0x00
mov byte [0x0502], cl    ; remainder = 1
mov byte [0x0503], 0x00

; =====================================================================
; Test 3 & 4: DIV word -- 100 / 10
; DX:AX = dividend, r/m16 = divisor. Result: AX = quotient, DX = remainder.
; =====================================================================
mov dx, 0
mov ax, 100
mov bx, 10
div bx
mov [0x0504], ax         ; expect 10 (quotient)
mov [0x0506], dx         ; expect 0 (remainder)

; =====================================================================
; Test 5 & 6: IDIV byte -- -10 / 3
; AX = -10 (0xFFF6), divisor = 3.
; AL = quotient = -3 (0xFD), AH = remainder = -1 (0xFF).
; =====================================================================
mov ax, -10              ; 0xFFF6
mov bl, 3
idiv bl
mov cl, ah               ; save remainder before CBW clobbers AH
cbw                      ; sign-extend AL -> AX = 0xFFFD
mov [0x0508], ax         ; expect 0xFFFD (-3)
mov al, cl               ; restore remainder
cbw                      ; sign-extend remainder -> AX = 0xFFFF
mov [0x050A], ax         ; expect 0xFFFF (-1)

; =====================================================================
; Test 7: DIV by zero -- should trigger INT 0
; =====================================================================
mov ax, 42
mov bl, 0
div bl                   ; divide by zero -> INT 0
; handler_div0 writes 1 to [0x050C]

; =====================================================================
; Test 8: DIV overflow -- quotient doesn't fit in AL
; AX = 0x0400 (1024), divisor = 1. Quotient = 1024, won't fit in AL.
; =====================================================================
mov ax, 0x0400
mov bl, 1
div bl                   ; overflow -> INT 0
; handler_div0 writes 1 to [0x050E]

int3

; =====================================================================
; INT 0 handler: divide error
; On 8088, INT 0 pushes the address of the NEXT instruction (not the
; faulting one), so plain IRET resumes correctly.
; =====================================================================
handler_div0:
    cmp word [0x050C], 0x0000
    jne .second_div0
    mov word [0x050C], 0x0001
    iret
.second_div0:
    mov word [0x050E], 0x0001
    iret
