; test_alias.asm -- 8088 undocumented opcode aliases.
; A real 8088 has no 186 encodings; the decode PLA ignores the
; distinguishing bit and executes documented twins:
;   C0,C1 -> C2,C3 (RET imm16 / RET)     -- NOT 186 shift-imm
;   C8,C9 -> CA,CB (RETF imm16 / RETF)   -- NOT ENTER/LEAVE
;   60-6F -> 70-7F (conditional jumps)   -- NOT PUSHA/POPA/...
; CTMOUSE 2.1 (built for 186) executes `ror bx,2` = C1 CB 02 in its
; IRQ4 handler; on a real 8088 that must behave as RET or the ISR
; falls out of instruction alignment.

; @name 8088 opcode aliases
; @expect 0500 0001 C1 executes as RET
; @expect 0502 0800 C1-RET leaves SP balanced
; @expect 0504 0002 C0 imm16 executes as RET imm16
; @expect 0506 0800 C0-RET4 discards operands
; @expect 0508 0003 60 executes as JO (not taken, no PUSHA)
; @expect 050A 0800 60 does not touch SP
; @expect 050C 0004 61 executes as JNO (taken)
; @expect 050E 0005 C9 executes as RETF
; @expect 0510 0006 C8 imm16 executes as RETF imm16
; @expect 0512 0800 C8-RETF4 discards operands
;
cpu 8086
org 0x0100

xor ax, ax
mov ds, ax
mov ss, ax
mov sp, 0x0800

; --- T1: C1 -> RET (CTMOUSE's `ror bx,2` byte sequence) ---
mov ax, t1_target
push ax
db 0xC1, 0xCB, 0x02        ; 186 `ror bx,2`; 8088 executes C3 = RET
mov word [0x0500], 0xBAD1  ; only reached if C1 was not RET
jmp t2
t1_target:
mov word [0x0500], 0x0001
mov [0x0502], sp           ; expect 0x0800

; --- T2: C0 imm16 -> RET imm16 ---
t2:
mov ax, 0x1111
push ax                    ; junk 1
push ax                    ; junk 2
mov ax, t2_target
push ax
db 0xC0, 0x04, 0x00        ; 186 shift-imm; 8088 executes C2 = RET 4
mov word [0x0504], 0xBAD2
jmp t3
t2_target:
mov word [0x0504], 0x0002
mov [0x0506], sp           ; expect 0x0800 (junk discarded by imm16)

; --- T3: 60 -> JO (OF clear: must fall through, must not push) ---
t3:
xor ax, ax                 ; clears OF
db 0x60, 0x03              ; 8088: JO +3 (not taken). PUSHA would SP-=16.
mov word [0x0508], 0x0003  ; 6 bytes -- reached only on fall-through...
jmp t3b
t3b:
mov [0x050A], sp           ; expect 0x0800

; --- T3c: 61 -> JNO (OF clear: taken, skips the fail jump) ---
db 0x61, 0x03              ; 8088: JNO +3 (taken)
jmp t3_fail                ; 3 bytes, skipped when alias works
mov word [0x050C], 0x0004
jmp t4
t3_fail:
mov word [0x050C], 0xBAD4
jmp t4

; --- T4: C9 -> RETF ---
t4:
push cs
mov ax, t4_target
push ax
db 0xC9                    ; 186 LEAVE; 8088 executes CB = RETF
mov word [0x050E], 0xBAD5
jmp t5
t4_target:
mov word [0x050E], 0x0005

; --- T5: C8 imm16 -> RETF imm16 ---
t5:
mov ax, 0x2222
push ax                    ; junk 1
push ax                    ; junk 2
push cs
mov ax, t5_target
push ax
db 0xC8, 0x04, 0x00        ; 186 ENTER; 8088 executes CA = RETF 4
mov word [0x0510], 0xBAD6
jmp done
t5_target:
mov word [0x0510], 0x0006
mov [0x0512], sp           ; expect 0x0800

done:
int3
