; test_farcall.asm -- FAR CALL, RETF, JMP FAR
; Loaded at F000:0123 (physical 0xF0123). DS=0, SS=0 after reset.
; SP initialized to 0x0800.
;
; Far subroutines are installed at segment 0x2000 (physical 0x20000)
; using ES=0x2000 and byte writes through ES:.
;
; Expected results:
;   [0500] = 0x0001   CALL FAR imm16:imm16 reached target
;   [0502] = 0x0001   RETF returned correctly
;   [0504] = 0x0001   CALL FAR indirect (FF /3) reached target
;   [0506] = 0x0001   RETF imm16 popped extra bytes
;   [0508] = 0x0001   JMP FAR imm16:imm16 reached target
;   [050A] = 0x2000   CS value inside far call

; @name FAR CALL
; @expect 0500 0001 CALL FAR imm
; @expect 0502 0001 RETF
; @expect 0504 0001 CALL FAR indirect
; @expect 0506 0001 RETF imm16
; @expect 0508 0001 JMP FAR imm
; @expect 050A 2000 CS after far call
;
cpu 8086
org 0x0100

; Set up segments and stack
mov ax, 0x0000
mov ds, ax
mov ss, ax
mov sp, 0x0800

; Zero results
mov word [0x0500], 0x0000
mov word [0x0502], 0x0000
mov word [0x0504], 0x0000
mov word [0x0506], 0x0000
mov word [0x0508], 0x0000
mov word [0x050A], 0x0000

; =====================================================================
; Install far subroutines at segment 0x2000 using ES
; =====================================================================
mov ax, 0x2000
mov es, ax

; far_sub1 at 2000:0000 -- mov word [0500], 0x0001; mov [050A], cs; retf
;   C7 06 00 05 01 00  = mov word [0x0500], 0x0001
;   8C 0E 0A 05        = mov [0x050A], cs
;   CB                 = retf
mov byte [es:0x0000], 0xC7
mov byte [es:0x0001], 0x06
mov byte [es:0x0002], 0x00
mov byte [es:0x0003], 0x05
mov byte [es:0x0004], 0x01
mov byte [es:0x0005], 0x00
mov byte [es:0x0006], 0x8C
mov byte [es:0x0007], 0x0E
mov byte [es:0x0008], 0x0A
mov byte [es:0x0009], 0x05
mov byte [es:0x000A], 0xCB

; far_sub2 at 2000:0020 -- mov word [0504], 0x0001; retf
;   C7 06 04 05 01 00  = mov word [0x0504], 0x0001
;   CB                 = retf
mov byte [es:0x0020], 0xC7
mov byte [es:0x0021], 0x06
mov byte [es:0x0022], 0x04
mov byte [es:0x0023], 0x05
mov byte [es:0x0024], 0x01
mov byte [es:0x0025], 0x00
mov byte [es:0x0026], 0xCB

; far_sub3 at 2000:0040 -- mov word [0506], 0x0001; retf 4
;   C7 06 06 05 01 00  = mov word [0x0506], 0x0001
;   CA 04 00           = retf 4
mov byte [es:0x0040], 0xC7
mov byte [es:0x0041], 0x06
mov byte [es:0x0042], 0x06
mov byte [es:0x0043], 0x05
mov byte [es:0x0044], 0x01
mov byte [es:0x0045], 0x00
mov byte [es:0x0046], 0xCA
mov byte [es:0x0047], 0x04
mov byte [es:0x0048], 0x00

; far_jmp_target at 2000:0060 -- mov word [0508], 0x0001; hlt
;   C7 06 08 05 01 00  = mov word [0x0508], 0x0001
;   F4                 = hlt
mov byte [es:0x0060], 0xC7
mov byte [es:0x0061], 0x06
mov byte [es:0x0062], 0x08
mov byte [es:0x0063], 0x05
mov byte [es:0x0064], 0x01
mov byte [es:0x0065], 0x00
mov byte [es:0x0066], 0xCC

; =====================================================================
; Test 1 & 2: CALL FAR imm16:imm16 / RETF
; Also captures CS=0x2000 inside the far subroutine.
; =====================================================================
call 0x2000:0x0000        ; far call to far_sub1
; If we get here, RETF worked
mov word [0x0502], 0x0001

; =====================================================================
; Test 3: CALL FAR indirect (FF /3) via memory pointer
; =====================================================================
mov word [0x0600], 0x0020
mov word [0x0602], 0x2000
call far [0x0600]         ; indirect far call to far_sub2

; =====================================================================
; Test 4: RETF imm16 -- pops extra bytes from stack
; =====================================================================
mov ax, 0xDEAD
push ax
mov ax, 0xBEEF
push ax
call 0x2000:0x0040        ; far_sub3 does RETF 4

; =====================================================================
; Test 5: JMP FAR imm16:imm16
; =====================================================================
jmp 0x2000:0x0060         ; far_jmp_target sets [0508]=1 then HLT

int3
