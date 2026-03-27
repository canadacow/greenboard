; test_alu.asm -- ALU and logic instruction tests
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; Results written to DS:0500+. Each word = one test result.
;
; Expected results (little-endian at 0x0500):
;   [0500] = 0x0042   ADD
;   [0502] = 0x0010   SUB
;   [0504] = 0xFFBE   NEG
;   [0506] = 0x1234   AND
;   [0508] = 0xFFFF   OR
;   [050A] = 0xEDCB   XOR
;   [050C] = 0xEDCA   NOT
;   [050E] = 0x2468   SHL
;   [0510] = 0x048D   SHR
;   [0512] = 0x0001   CMP (1 if equal)
;   [0514] = 0x008A   ADC chain
;   [0516] = 0x00FE   SBB

; @name ALU
; @expect 0500 0042 ADD
; @expect 0502 0010 SUB
; @expect 0504 FFBE NEG
; @expect 0506 1234 AND
; @expect 0508 FFFF OR
; @expect 050A EDCB XOR
; @expect 050C EDCA NOT
; @expect 050E 2468 SHL
; @expect 0510 048D SHR
; @expect 0512 0001 CMP/JE
; @expect 0514 008A ADC
; @expect 0516 00FE SBB
;
cpu 8086
org 0x0100

; ADD
mov ax, 0x0030
add ax, 0x0012
mov [0x0500], ax        ; expect 0x0042

; SUB
mov ax, 0x0042
sub ax, 0x0032
mov [0x0502], ax        ; expect 0x0010

; NEG
mov ax, 0x0042
neg ax
mov [0x0504], ax        ; expect 0xFFBE

; AND
mov ax, 0x12FF
and ax, 0xFF34
mov [0x0506], ax        ; expect 0x1234

; OR
mov ax, 0xFF00
or ax, 0x00FF
mov [0x0508], ax        ; expect 0xFFFF

; XOR
mov ax, 0xFFFF
xor ax, 0x1234
mov [0x050A], ax        ; expect 0xEDCB

; NOT
mov ax, 0x1235
not ax
mov [0x050C], ax        ; expect 0xEDCA

; SHL by 1
mov ax, 0x1234
shl ax, 1
mov [0x050E], ax        ; expect 0x2468

; SHR by 1
mov ax, 0x091A
shr ax, 1
mov [0x0510], ax        ; expect 0x048D

; CMP + conditional store
mov ax, 0x0055
cmp ax, 0x0055
je .cmp_eq
mov word [0x0512], 0x0000
jmp .cmp_done
.cmp_eq:
mov word [0x0512], 0x0001   ; expect 0x0001
.cmp_done:

; ADC: add with carry from previous add
mov ax, 0xFFFF
add ax, 0x0001          ; CF=1, AX=0
mov ax, 0x0089
adc ax, 0x0000          ; AX = 0x0089 + 0 + CF(1) = 0x008A
mov [0x0514], ax        ; expect 0x008A

; SBB: subtract with borrow
mov ax, 0x0000
sub ax, 0x0001          ; CF=1 (borrow), AX=0xFFFF
mov ax, 0x00FF
sbb ax, 0x0000          ; AX = 0x00FF - 0 - CF(1) = 0x00FE
mov [0x0516], ax        ; expect 0x00FE

int3
