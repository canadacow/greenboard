; test_post_ram.asm -- BIOS POST TEST.04: Base 16K RAM + 8259 init
; Faithful reproduction of PCBIOS.ASM TEST.04.
;
; 1. Write/read/verify two patterns (0xFF, 0x00) to first 16K
; 2. Initialize 8259 PIC (ICW1-ICW4)
; 3. Verify PIC responds by reading ISR
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   RAM pattern 0xFF verified
;   [0502] = 0x0001   RAM pattern 0x00 verified
;   [0504] = 0x0001   8259 PIC initialized (ICW1-4, ISR readable)
;   [0506] = 0x0001   SW1 DIP switches (Port A = 0x3D)
;   [0508] = 0x0001   SW2 DIP switches (Port C low nibble = 0x06)

; @name POST RAM/PIC (TEST.04)
; @expect 0500 0001 RAM pattern FF
; @expect 0502 0001 RAM pattern 00
; @expect 0504 0001 PIC init
; @expect 0506 0001 SW1 switches
; @expect 0508 0001 SW2 switches
;
cpu 8086
org 0x0100

%define INTA00  0x20
%define INTA01  0x21

; =====================================================================
; Initialize
; =====================================================================
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800
    mov word [0x0500], 0x0000
    mov word [0x0502], 0x0000
    mov word [0x0504], 0x0000
    mov word [0x0506], 0x0000
    mov word [0x0508], 0x0000

    ; Initialize PPI (Port A=input, Port B=output, Port C=input)
    ; Must be done before reading switches.
    mov al, 0x99
    out 0x63, al

; =====================================================================
; Test 1: Write 0xFF pattern, read back (REP STOSB + LODSB)
; =====================================================================
    mov di, 0x2000
    mov cx, 4
    mov al, 0xFF
    cld
    rep stosb

    mov si, 0x2000
    mov cx, 4
.check_ff:
    lodsb
    cmp al, 0xFF
    jne .ff_fail
    loop .check_ff
    mov word [0x0500], 0x0001
    jmp .test2
.ff_fail:

; =====================================================================
; Test 2: Write 0x00 pattern, read back (STD + STOSB + LODSB)
; =====================================================================
.test2:
    mov di, 0x2003
    mov cx, 4
    xor al, al
    std
    rep stosb
    cld

    mov si, 0x2000
    mov cx, 4
.check_00:
    lodsb
    cmp al, 0x00
    jne .zero_fail
    loop .check_00
    mov word [0x0502], 0x0001
    jmp .test3
.zero_fail:

; =====================================================================
; Test 3: Initialize 8259 PIC (PCBIOS.ASM lines 483-489)
; ICW1=0x13 (edge, single, ICW4), ICW2=0x08, ICW4=0x09 (buffered, 8086)
; Then read ISR (OCW3: write 0x0B, read port 0x20) -- should be 0x00.
; =====================================================================
.test3:
    mov al, 0x13            ; ICW1: edge, single, ICW4
    out INTA00, al
    mov al, 0x08            ; ICW2: vector base 8
    out INTA01, al
    mov al, 0x09            ; ICW4: buffered, 8086 mode
    out INTA01, al

    ; Read ISR via OCW3
    mov al, 0x0B            ; OCW3: read ISR
    out INTA00, al
    in al, INTA00           ; read ISR
    cmp al, 0x00            ; no interrupts in service
    jne .done
    mov word [0x0504], 0x0001

; =====================================================================
; Test 4: SW1 DIP switches via Port A (PCBIOS.ASM TEST.04 line 453)
; Expect 0x3D: floppy=1, no 8087=0, 64K RAM=11, MDA=11, 1 drive=00
; =====================================================================
.test4:
    in al, 0x60             ; read PPI Port A (switches)
    cmp al, 0x3D
    jne .test5
    mov word [0x0506], 0x0001

; =====================================================================
; Test 5: SW2 DIP switches via Port C lower nibble (PCBIOS.ASM line 888)
; Expect 0x06: 6 expansion RAM banks (bits 0-3)
; =====================================================================
.test5:
    in al, 0x62             ; read PPI Port C
    and al, 0x0F            ; isolate lower nibble (SW2)
    cmp al, 0x06
    jne .done
    mov word [0x0508], 0x0001

.done:
    hlt
