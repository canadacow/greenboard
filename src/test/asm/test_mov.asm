; test_mov.asm -- MOV and data movement tests (original test, plus extras)
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x1234   MOV imm -> reg -> mem
;   [0502] = 0x5678   MOV reg -> reg -> mem
;   [0504] = 0x00AB   MOV imm8 -> reg8 -> mem8 (byte)
;   [0506] = 0xDEF0   XCHG
;   [0508] = 0x9ABC   XCHG (other half)

cpu 8086
org 0x0100

; MOV imm16 to AX, store to memory
mov ax, 0x1234
mov [0x0500], ax        ; expect 0x1234

; MOV reg to reg
mov bx, 0x5678
mov ax, bx
mov [0x0502], ax        ; expect 0x5678

; MOV byte
mov al, 0xAB
mov [0x0504], al        ; expect 0xAB at byte, 0x00AB as word if high byte is 0
mov byte [0x0505], 0x00

; XCHG
mov ax, 0x9ABC
mov bx, 0xDEF0
xchg ax, bx
mov [0x0506], ax        ; expect 0xDEF0 (was in BX)
mov [0x0508], bx        ; expect 0x9ABC (was in AX)

hlt
