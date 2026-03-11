; test_dos.asm -- Mock DOS INT 21h services
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; INT 21h handler (in this file) implements:
;   AH=02h: Write character (DL) to output buffer
;   AH=09h: Write '$'-terminated string at DS:DX to buffer
;   AH=4Ch: Store exit code in AL and HLT
;
; Output buffer at [0x0600+]. Pointer at [0x05F0], count at [0x05F2].
;
; Expected results:
;   [0500] = 0x0005   AH=02 char count (5 chars: "Hello")
;   [0502] = 0x0048   AH=02 first char = 'H' (0x48)
;   [0504] = 0x006F   AH=02 last char = 'o' (0x6F)
;   [0506] = 0x000D   AH=09 string length ("Hello, World!" = 13)
;   [0508] = 0x0048   AH=09 first char = 'H'
;   [050A] = 0x0021   AH=09 last char = '!'
;   [050C] = 0x002A   AH=4C exit code = 42 (0x2A)

cpu 8086
org 0x0123

mov ax, 0x0000
mov ss, ax
mov sp, 0x0800
mov ds, ax

; Zero result area
mov word [0x0500], 0x0000
mov word [0x0502], 0x0000
mov word [0x0504], 0x0000
mov word [0x0506], 0x0000
mov word [0x0508], 0x0000
mov word [0x050A], 0x0000
mov word [0x050C], 0x0000

; Install INT 21h handler
mov word [0x21*4], int21_handler
mov word [0x21*4+2], 0xF000

; =====================================================================
; Test 1: AH=02h single character output -- print "Hello"
; =====================================================================
; Reset output buffer
mov word [0x05F0], 0x0600
mov word [0x05F2], 0x0000

mov ah, 0x02
mov dl, 'H'
int 0x21
mov dl, 'e'
int 0x21
mov dl, 'l'
int 0x21
mov dl, 'l'
int 0x21
mov dl, 'o'
int 0x21

; Record results
mov ax, [0x05F2]
mov [0x0500], ax               ; char count = 5
mov al, [0x0600]
mov ah, 0
mov [0x0502], ax               ; first char = 'H' (0x48)
mov al, [0x0604]
mov ah, 0
mov [0x0504], ax               ; last char = 'o' (0x6F)

; =====================================================================
; Test 2: AH=09h string output -- print "Hello, World!"
; =====================================================================
; Reset output buffer to separate area
mov word [0x05F0], 0x0700
mov word [0x05F2], 0x0000

; Copy string to low memory at 0x0400 (DS=0 can't reach F000 segment)
cld
push ds
push es
mov ax, 0xF000
mov ds, ax
mov si, hello_str              ; source: F000:hello_str
mov ax, 0
mov es, ax
mov di, 0x0400                 ; dest: 0000:0400
mov cx, 14                     ; "Hello, World!$" = 14 bytes
rep movsb
pop es
pop ds

; Now print string from DS:DX = 0000:0400
mov ah, 0x09
mov dx, 0x0400
int 0x21

; Record results
mov ax, [0x05F2]
mov [0x0506], ax               ; char count = 13
mov al, [0x0700]
mov ah, 0
mov [0x0508], ax               ; first char = 'H' (0x48)
mov al, [0x070C]               ; 0x0700 + 12 = last char
mov ah, 0
mov [0x050A], ax               ; last char = '!' (0x21)

; =====================================================================
; Test 3: AH=4Ch -- exit with code 42
; =====================================================================
mov ah, 0x4C
mov al, 42                     ; exit code
int 0x21

; Should not reach here (handler does HLT)
hlt

; =====================================================================
; Data
; =====================================================================
hello_str: db "Hello, World!$"

; =====================================================================
; INT 21h handler
; =====================================================================
int21_handler:
    cmp ah, 0x02
    je .fn02
    cmp ah, 0x09
    je .fn09
    cmp ah, 0x4C
    je .fn4c
    iret

.fn02:
    ; Write character DL to output buffer
    push bx
    mov bx, [0x05F0]
    mov [bx], dl
    inc bx
    mov [0x05F0], bx
    add word [0x05F2], 1
    pop bx
    iret

.fn09:
    ; Write '$'-terminated string at DS:DX to output buffer
    push bx
    push si
    push cx
    mov si, dx
    mov bx, [0x05F0]
.fn09_loop:
    mov cl, [si]
    cmp cl, '$'
    je .fn09_done
    mov [bx], cl
    inc si
    inc bx
    add word [0x05F2], 1
    jmp .fn09_loop
.fn09_done:
    mov [0x05F0], bx
    pop cx
    pop si
    pop bx
    iret

.fn4c:
    ; Exit: store exit code (AL) to [050C] and halt
    mov ah, 0
    mov [0x050C], ax
    hlt
    iret                       ; never reached
