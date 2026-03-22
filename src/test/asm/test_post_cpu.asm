; test_post_cpu.asm -- BIOS POST TEST.01: CPU flags and register verification
; Mirrors the real IBM PC BIOS TEST.01 sequence from PCBIOS.ASM (lines 261-317).
; Pure CPU -- no I/O, no peripherals.
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   Flags set (SF, CF, ZF, PF via SAHF 0xD5)
;   [0502] = 0x0001   AF flag verified
;   [0504] = 0x0001   OF flag verified
;   [0506] = 0x0001   Flags clear (SAHF 0x00)
;   [0508] = 0x0001   Register walk (all 1s)
;   [050A] = 0x0001   Register walk (all 0s)
;   [050C] = 0x0001   Segment register round-trip

; @name POST CPU (TEST.01)
; @expect 0500 0001 Flags set
; @expect 0502 0001 AF flag
; @expect 0504 0001 OF flag
; @expect 0506 0001 Flags clear
; @expect 0508 0001 Reg walk 1s
; @expect 050A 0001 Reg walk 0s
; @expect 050C 0001 Seg reg round-trip
;
cpu 8086
org 0x0100

; ---------------------------------------------------------------------------
; Test 1: Flags set -- SAHF with AH=0xD5 sets SF, CF, ZF, AF, PF
; ---------------------------------------------------------------------------
    cli
    mov ah, 0xD5
    sahf
    jnc .fail1              ; CF must be set
    jnz .fail1              ; ZF must be set
    jnp .fail1              ; PF must be set
    jns .fail1              ; SF must be set
    mov word [0x0500], 0x0001
    jmp .test2
.fail1:
    mov word [0x0500], 0x0000

; ---------------------------------------------------------------------------
; Test 2: AF flag -- LAHF, shift right 5 to move AF into CF position
; ---------------------------------------------------------------------------
.test2:
    mov ah, 0xD5
    sahf                    ; set flags including AF
    lahf                    ; load flags into AH
    mov cl, 5
    shr ah, cl              ; shift AF (bit 4) into CF (bit 0)
    jnc .fail2              ; AF must have been set
    mov word [0x0502], 0x0001
    jmp .test3
.fail2:
    mov word [0x0502], 0x0000

; ---------------------------------------------------------------------------
; Test 3: OF flag -- shift 0x40 left by 1 to trigger overflow
; ---------------------------------------------------------------------------
.test3:
    mov al, 0x40
    shl al, 1               ; 0x40 << 1 = 0x80, sets OF (sign change)
    jno .fail3              ; OF must be set
    mov word [0x0504], 0x0001
    jmp .test4
.fail3:
    mov word [0x0504], 0x0000

; ---------------------------------------------------------------------------
; Test 4: Flags clear -- SAHF with AH=0x00 clears SF, CF, ZF, PF
; ---------------------------------------------------------------------------
.test4:
    xor ah, ah
    sahf                    ; clear all SAHF-accessible flags
    jc  .fail4              ; CF must be clear
    jz  .fail4              ; ZF must be clear
    js  .fail4              ; SF must be clear
    jp  .fail4              ; PF must be clear
    ; Also verify AF is clear
    lahf
    mov cl, 5
    shr ah, cl
    jc  .fail4              ; AF must be clear
    ; Also verify OF is clear (SHL AH,1 with AH=0 should not set OF)
    xor ah, ah
    shl ah, 1
    jo  .fail4              ; OF must be clear
    mov word [0x0506], 0x0001
    jmp .test5
.fail4:
    mov word [0x0506], 0x0000

; ---------------------------------------------------------------------------
; Test 5: Register walk (all 1s) -- chain 0xFFFF through all regs
; Mirrors BIOS: AX -> DS -> BX -> ES -> CX -> SS -> DX -> SP -> BP -> SI -> DI
; ---------------------------------------------------------------------------
.test5:
    ; Save a known stack pointer first (we'll trash SP during the test)
    mov ax, 0xFFFF
    mov ds, ax
    mov bx, ds
    mov es, bx
    mov cx, es
    mov ss, cx
    mov dx, ss
    mov sp, dx
    mov bp, sp
    mov si, bp
    mov di, si
    ; Verify: DI should be 0xFFFF. XOR AX,DI should be 0.
    xor ax, di
    ; Restore DS=0 so we can write results
    xor bx, bx
    mov ds, bx
    mov ss, bx
    mov sp, 0xFFFE          ; restore a sane stack
    test ax, ax
    jnz .fail5
    mov word [0x0508], 0x0001
    jmp .test6
.fail5:
    xor bx, bx
    mov ds, bx
    mov ss, bx
    mov sp, 0xFFFE
    mov word [0x0508], 0x0000

; ---------------------------------------------------------------------------
; Test 6: Register walk (all 0s) -- chain 0x0000 through all regs
; ---------------------------------------------------------------------------
.test6:
    xor ax, ax
    mov ds, ax
    mov bx, ds
    mov es, bx
    mov cx, es
    mov ss, cx
    mov dx, ss
    mov sp, dx
    mov bp, sp
    mov si, bp
    mov di, si
    ; Verify: DI should be 0x0000. OR AX,DI should be 0.
    or ax, di
    ; Restore DS=0 and sane SP
    xor bx, bx
    mov ds, bx
    mov ss, bx
    mov sp, 0xFFFE
    test ax, ax
    jnz .fail6
    mov word [0x050A], 0x0001
    jmp .test7
.fail6:
    xor bx, bx
    mov ds, bx
    mov ss, bx
    mov sp, 0xFFFE
    mov word [0x050A], 0x0000

; ---------------------------------------------------------------------------
; Test 7: Segment register round-trip -- write distinct values, read back
; ---------------------------------------------------------------------------
.test7:
    mov ax, 0x1111
    mov ds, ax
    mov ax, 0x2222
    mov es, ax
    mov ax, 0x3333
    mov ss, ax
    ; Read them back
    mov bx, ds              ; should be 0x1111
    mov cx, es              ; should be 0x2222
    mov dx, ss              ; should be 0x3333
    ; Restore DS=0 so we can write results
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0xFFFE
    cmp bx, 0x1111
    jne .fail7
    cmp cx, 0x2222
    jne .fail7
    cmp dx, 0x3333
    jne .fail7
    mov word [0x050C], 0x0001
    jmp .done
.fail7:
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0xFFFE
    mov word [0x050C], 0x0000

.done:
    hlt
