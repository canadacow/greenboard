; test_mul.asm -- MUL, IMUL, shifts by CL, SAR, RCL/RCR, multi-shift
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; Expected results:
;   [0500] = 0x0048   MUL byte: 8 * 9 = 72 = 0x48 (AL)
;   [0502] = 0x0000   MUL byte: AH = 0 (no overflow)
;   [0504] = 0x4000   MUL word: 0x0200 * 0x0020 = 0x4000 (AX low)
;   [0506] = 0x0000   MUL word: DX = 0 (no overflow)
;   [0508] = 0xFFC8   IMUL byte: (-7) * 8 = -56 = 0xFFC8 (AX, sign-extended)
;   [050A] = 0x0100   MUL byte overflow: 16 * 16 = 256, AH=1, AL=0
;   [050C] = 0x00A0   SHL AL, CL (CL=3): 0x14 << 3 = 0xA0
;   [050E] = 0x0003   SHR AX, CL (CL=4): 0x0030 >> 4 = 0x0003
;   [0510] = 0xFFFE   SAR AX, 1: 0xFFFC >> 1 = 0xFFFE (sign preserved)
;   [0512] = 0x0030   SHL AX, CL (CL=4): 0x0003 << 4 = 0x0030

; @name MUL/IMUL/Shifts
; @expect 0500 0048 MUL byte
; @expect 0502 0000 MUL byte hi
; @expect 0504 4000 MUL word lo
; @expect 0506 0000 MUL word hi
; @expect 0508 FFC8 IMUL byte
; @expect 050A 0100 MUL overflow
; @expect 050C 00A0 SHL AL,CL
; @expect 050E 0003 SHR AX,CL
; @expect 0510 FFFE SAR AX,1
; @expect 0512 0030 SHL AX,CL
;
cpu 8086
org 0x0100

mov ax, 0x0000
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
mov word [0x0510], 0x0000
mov word [0x0512], 0x0000

; =====================================================================
; Test 1 & 2: MUL byte -- 8 * 9 = 72
; AX = AL * r/m8. Result in AX.
; =====================================================================
mov al, 8
mov bl, 9
mul bl                     ; AX = 72 = 0x0048
mov [0x0500], al           ; quotient low byte
mov byte [0x0501], 0x00
mov [0x0502], ah           ; high byte (should be 0)
mov byte [0x0503], 0x00

; =====================================================================
; Test 3 & 4: MUL word -- 0x0200 * 0x0020 = 0x4000
; DX:AX = AX * r/m16.
; =====================================================================
mov ax, 0x0200
mov bx, 0x0020
mul bx                     ; DX:AX = 0x00004000
mov [0x0504], ax           ; expect 0x4000
mov [0x0506], dx           ; expect 0x0000

; =====================================================================
; Test 5: IMUL byte -- (-7) * 8 = -56
; AX = AL * r/m8 (signed). -56 = 0xFFC8.
; =====================================================================
mov al, -7                 ; 0xF9
mov bl, 8
imul bl                    ; AX = -56 = 0xFFC8
mov [0x0508], ax           ; expect 0xFFC8

; =====================================================================
; Test 6: MUL byte overflow -- 16 * 16 = 256
; AX = 0x0100. AH=1, AL=0.
; =====================================================================
mov al, 16
mov bl, 16
mul bl                     ; AX = 256 = 0x0100
mov [0x050A], ax           ; expect 0x0100

; =====================================================================
; Test 7: SHL AL, CL -- 0x14 << 3 = 0xA0
; =====================================================================
mov al, 0x14
mov cl, 3
shl al, cl
mov ah, 0
mov [0x050C], ax           ; expect 0x00A0

; =====================================================================
; Test 8: SHR AX, CL -- 0x0030 >> 4 = 0x0003
; =====================================================================
mov ax, 0x0030
mov cl, 4
shr ax, cl
mov [0x050E], ax           ; expect 0x0003

; =====================================================================
; Test 9: SAR AX, 1 -- arithmetic right shift preserves sign
; 0xFFFC = -4. SAR by 1 -> 0xFFFE = -2.
; =====================================================================
mov ax, 0xFFFC
sar ax, 1
mov [0x0510], ax           ; expect 0xFFFE

; =====================================================================
; Test 10: SHL AX, CL -- 0x0003 << 4 = 0x0030
; =====================================================================
mov ax, 0x0003
mov cl, 4
shl ax, cl
mov [0x0512], ax           ; expect 0x0030

hlt
