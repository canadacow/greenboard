; test_alu.asm -- ALU and logic instruction tests
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; Results written to DS:0200+. Each word = one test result.
;
; Expected results (little-endian at 0x0200):
;   [0200] = 0x0042   ADD
;   [0202] = 0x0010   SUB
;   [0204] = 0xFFBE   NEG
;   [0206] = 0x1234   AND
;   [0208] = 0xFFFF   OR
;   [020A] = 0xEDCB   XOR
;   [020C] = 0xEDCA   NOT
;   [020E] = 0x2468   SHL
;   [0210] = 0x048D   SHR
;   [0212] = 0x0001   CMP (1 if equal)
;   [0214] = 0x008A   ADC chain
;   [0216] = 0x00FE   SBB

cpu 8086
org 0x0123

; ADD
mov ax, 0x0030
add ax, 0x0012
mov [0x0200], ax        ; expect 0x0042

; SUB
mov ax, 0x0042
sub ax, 0x0032
mov [0x0202], ax        ; expect 0x0010

; NEG
mov ax, 0x0042
neg ax
mov [0x0204], ax        ; expect 0xFFBE

; AND
mov ax, 0x12FF
and ax, 0xFF34
mov [0x0206], ax        ; expect 0x1234

; OR
mov ax, 0xFF00
or ax, 0x00FF
mov [0x0208], ax        ; expect 0xFFFF

; XOR
mov ax, 0xFFFF
xor ax, 0x1234
mov [0x020A], ax        ; expect 0xEDCB

; NOT
mov ax, 0x1235
not ax
mov [0x020C], ax        ; expect 0xEDCA

; SHL by 1
mov ax, 0x1234
shl ax, 1
mov [0x020E], ax        ; expect 0x2468

; SHR by 1
mov ax, 0x091A
shr ax, 1
mov [0x0210], ax        ; expect 0x048D

; CMP + conditional store
mov ax, 0x0055
cmp ax, 0x0055
je .cmp_eq
mov word [0x0212], 0x0000
jmp .cmp_done
.cmp_eq:
mov word [0x0212], 0x0001   ; expect 0x0001
.cmp_done:

; ADC: add with carry from previous add
mov ax, 0xFFFF
add ax, 0x0001          ; CF=1, AX=0
mov ax, 0x0089
adc ax, 0x0000          ; AX = 0x0089 + 0 + CF(1) = 0x008A
mov [0x0214], ax        ; expect 0x008A

; SBB: subtract with borrow
mov ax, 0x0000
sub ax, 0x0001          ; CF=1 (borrow), AX=0xFFFF
mov ax, 0x00FF
sbb ax, 0x0000          ; AX = 0x00FF - 0 - CF(1) = 0x00FE
mov [0x0216], ax        ; expect 0x00FE

hlt
