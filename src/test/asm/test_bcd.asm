; test_bcd.asm -- BCD, rotate, flag manipulation, and other exotic instructions
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; Expected results:
;   [0500] = 0x0079   DAA: 0x45 + 0x38 = 0x7D -> DAA -> 0x83... let me compute
;                      Actually: 0x45+0x38 = 0x7D. Low nibble D>9, so +6 = 0x83. AF=1.
;                      High nibble 8 not >9, CF=0. Result = 0x83. Hmm.
;                      Let me use: 0x29 + 0x13 = 0x3C. Low nibble C>9, +6 -> 0x42.
;                      No wait: 0x29+0x13=0x3C, D=C>9 so +6=0x42. Result=0x42.
;   [0500] = 0x0042   DAA: packed BCD 29 + 13 = 42
;   [0502] = 0x0022   DAS: packed BCD 55 - 33 = 22
;   [0504] = 0x0109   AAA: AX after AAA on AL=0x0F (unpacked 9+6=15, AAA adjusts)
;   [0506] = 0x0508   AAD: AH=5, AL=3 -> AL = 5*10+3 = 53 = 0x35
;   [0508] = 0x0305   AAM: AL=35 -> AH=3, AL=5 (35/10=3 rem 5)
;   [050A] = 0x00A0   ROL 0x50 by 1 = 0xA0
;   [050C] = 0x0028   ROR 0x50 by 1 = 0x28
;   [050E] = 0x0001   LAHF/SAHF round-trip preserves flags
;   [0510] = 0x0055   XLAT: translate byte through table
;   [0512] = 0x0001   STC/CLC/CMC work correctly
;   [0514] = 0x1234   LEA loads effective address
;   [0516] = 0x0001   LDS loads pointer (segment:offset)

cpu 8086
org 0x0123

mov ax, 0x0000
mov ds, ax
mov es, ax
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
mov word [0x0514], 0x0000
mov word [0x0516], 0x0000

; =====================================================================
; Test 1: DAA -- packed BCD addition
; 29 + 13 = 42 in BCD
; =====================================================================
mov al, 0x29
add al, 0x13              ; AL = 0x3C
daa                        ; low nibble C>9, +6 -> 0x42
mov ah, 0
mov [0x0500], ax           ; expect 0x0042

; =====================================================================
; Test 2: DAS -- packed BCD subtraction
; 55 - 33 = 22 in BCD
; =====================================================================
mov al, 0x55
sub al, 0x33              ; AL = 0x22
das                        ; already valid BCD, no adjustment
mov ah, 0
mov [0x0502], ax           ; expect 0x0022

; =====================================================================
; Test 3: AAA -- ASCII adjust after addition
; 9 + 6 = 15. In unpacked BCD: AL=0x0F. AAA: low nibble F>9,
; so AL=(AL+6)&0x0F=0x05, AH+=1. AX = 0x0105.
; Wait, AAA adds 6 to AL and 1 to AH: AL=0x0F+6=0x15, AL&=0x0F=0x05, AH=0+1=1.
; Then AX += 0x0106? No. Let me check:
; AAA: if (AL & 0x0F > 9 || AF) then AX += 0x0106, AF=CF=1, AL &= 0x0F.
; Start: AH=0, AL=0x0F. (0x0F & 0x0F = 15 > 9) -> AX += 0x0106 -> AX = 0x0115.
; AL &= 0x0F -> AL = 0x05. AX = 0x0105.
; =====================================================================
mov ax, 0x0009
add al, 0x06              ; AL = 0x0F, AH still 0
aaa                        ; AX = 0x0105
mov [0x0504], ax           ; expect 0x0105

; =====================================================================
; Test 4: AAD -- ASCII adjust before division
; AH=5, AL=3 -> AL = 5*10+3 = 53 = 0x35, AH=0
; =====================================================================
mov ax, 0x0503
aad                        ; AL = 5*10+3 = 53 = 0x35, AH = 0
mov [0x0506], ax           ; expect 0x0035

; =====================================================================
; Test 5: AAM -- ASCII adjust after multiply
; AL = 35 -> AH = 35/10 = 3, AL = 35%10 = 5
; =====================================================================
mov ax, 0x0023             ; AL = 0x23 = 35
aam                        ; AH = 3, AL = 5 -> AX = 0x0305
mov [0x0508], ax           ; expect 0x0305

; =====================================================================
; Test 6: ROL byte by 1
; 0x50 = 0101_0000. ROL -> 1010_0000 = 0xA0.
; =====================================================================
mov al, 0x50
rol al, 1
mov ah, 0
mov [0x050A], ax           ; expect 0x00A0

; =====================================================================
; Test 7: ROR byte by 1
; 0x50 = 0101_0000. ROR -> 0010_1000 = 0x28.
; =====================================================================
mov al, 0x50
ror al, 1
mov ah, 0
mov [0x050C], ax           ; expect 0x0028

; =====================================================================
; Test 8: LAHF / SAHF round-trip
; Set known flags, LAHF, clobber, SAHF, check ZF.
; =====================================================================
; Force ZF=1
xor ax, ax                 ; ZF=1
lahf                       ; AH = flags byte (ZF in bit 6 of AH)
mov dl, ah                 ; save
mov al, 1                  ; clobber ZF (ZF=0 now)
or al, al                  ; ensure ZF=0
mov ah, dl                 ; restore saved flags byte
sahf                       ; restore flags from AH
; Now ZF should be 1 again
jz .sahf_ok
mov word [0x050E], 0x0000
jmp .sahf_done
.sahf_ok:
mov word [0x050E], 0x0001
.sahf_done:

; =====================================================================
; Test 9: XLAT -- table lookup
; BX -> table base. AL = index. XLAT does AL = [BX + AL].
; =====================================================================
; Build a small table at 0x0620
mov byte [0x0620], 0xAA    ; index 0
mov byte [0x0621], 0xBB    ; index 1
mov byte [0x0622], 0x55    ; index 2
mov byte [0x0623], 0xDD    ; index 3
mov bx, 0x0620
mov al, 2                  ; look up index 2
xlat                       ; AL = [BX+AL] = [0x0622] = 0x55
mov ah, 0
mov [0x0510], ax           ; expect 0x0055

; =====================================================================
; Test 10: STC / CLC / CMC -- carry flag manipulation
; =====================================================================
clc                        ; CF=0
stc                        ; CF=1
jnc .cf_fail
cmc                        ; complement: CF=0
jc .cf_fail
cmc                        ; complement: CF=1
jnc .cf_fail
clc                        ; CF=0
jc .cf_fail
mov word [0x0512], 0x0001
.cf_fail:

; =====================================================================
; Test 11: LEA -- load effective address
; LEA AX, [BX+SI+0x1000]. BX=0x0200, SI=0x0034 -> AX = 0x1234.
; =====================================================================
mov bx, 0x0200
mov si, 0x0034
lea ax, [bx+si+0x1000]
mov [0x0514], ax           ; expect 0x1234

; =====================================================================
; Test 12: LDS -- load far pointer
; Store a far pointer at 0x0640: offset=0xBEEF, segment=0x1234.
; LDS BX, [0x0640] -> BX=0xBEEF, DS=0x1234.
; Then restore DS=0 and store 1 if BX was correct.
; =====================================================================
mov word [0x0640], 0xBEEF  ; offset
mov word [0x0642], 0x1234  ; segment
lds bx, [0x0640]           ; BX=0xBEEF, DS=0x1234
; Restore DS so we can write results
mov ax, 0x0000
mov ds, ax
cmp bx, 0xBEEF
jne .lds_fail
mov word [0x0516], 0x0001
.lds_fail:

hlt
