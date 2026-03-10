; test_mov.asm -- MOV and data movement tests (original test, plus extras)
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
;
; Expected results:
;   [0200] = 0x1234   MOV imm -> reg -> mem
;   [0202] = 0x5678   MOV reg -> reg -> mem
;   [0204] = 0x00AB   MOV imm8 -> reg8 -> mem8 (byte)
;   [0206] = 0x9ABC   XCHG
;   [0208] = 0xDEF0   XCHG (other half)

cpu 8086
org 0x0123

; MOV imm16 to AX, store to memory
mov ax, 0x1234
mov [0x0200], ax        ; expect 0x1234

; MOV reg to reg
mov bx, 0x5678
mov ax, bx
mov [0x0202], ax        ; expect 0x5678

; MOV byte
mov al, 0xAB
mov [0x0204], al        ; expect 0xAB at byte, 0x00AB as word if high byte is 0
mov byte [0x0205], 0x00

; XCHG
mov ax, 0x9ABC
mov bx, 0xDEF0
xchg ax, bx
mov [0x0206], ax        ; expect 0xDEF0 (was in BX)
mov [0x0208], bx        ; expect 0x9ABC (was in AX)

hlt
