; test_io.asm -- IN/OUT instructions, targeting PIC ports (0x20, 0x21)
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; The test harness provides a 64K I/O port space (initialized to 0xFF).
; We write values to ports and read them back.
;
; Expected results:
;   [0500] = 0x00AB   OUT imm8 / IN imm8 byte round-trip (port 0x80)
;   [0502] = 0x00CD   OUT DX / IN DX byte round-trip (port 0x81)
;   [0504] = 0xBEEF   OUT imm8 / IN imm8 word round-trip (port 0x82)
;   [0506] = 0x0001   PIC ICW1: write 0x13 to port 0x20, read back matches
;   [0508] = 0x0001   PIC OCW1: write 0x08 to port 0x21, read back matches
;   [050A] = 0x0001   I/O write doesn't corrupt memory at same address

cpu 8086
org 0x0123

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
; Test 1: OUT imm8, AL / IN AL, imm8 -- byte round-trip via port 0x80
; Port 0x80 is the POST diagnostic port on real hardware.
; =====================================================================
mov al, 0xAB
out 0x80, al              ; write 0xAB to port 0x80
xor ax, ax                ; clobber AX
in al, 0x80               ; read port 0x80 back
mov ah, 0
mov [0x0500], ax          ; expect 0x00AB

; =====================================================================
; Test 2: OUT DX, AL / IN AL, DX -- byte round-trip via DX=0x81
; =====================================================================
mov dx, 0x0081
mov al, 0xCD
out dx, al                ; write 0xCD to port 0x81
xor ax, ax
in al, dx                 ; read port 0x81 back
mov ah, 0
mov [0x0502], ax          ; expect 0x00CD

; =====================================================================
; Test 3: OUT imm8, AX / IN AX, imm8 -- word round-trip via port 0x82
; Word I/O: writes AL to port, AH to port+1.
; =====================================================================
mov ax, 0xBEEF
out 0x82, ax              ; write 0xEF to port 0x82, 0xBE to port 0x83
xor ax, ax
in ax, 0x82               ; read back word
mov [0x0504], ax          ; expect 0xBEEF

; =====================================================================
; Test 4: PIC ICW1 -- write to port 0x20 (PIC command register)
; On a real 5150, writing 0x13 to port 0x20 starts PIC initialization:
;   ICW1: edge-triggered, single PIC, ICW4 needed.
; We just verify the I/O round-trip works at the PIC's address.
; =====================================================================
mov al, 0x13
out 0x20, al              ; ICW1 to PIC
xor ax, ax
in al, 0x20               ; read back
cmp al, 0x13
jne .pic_icw1_fail
mov word [0x0506], 0x0001
.pic_icw1_fail:

; =====================================================================
; Test 5: PIC OCW1 -- write to port 0x21 (PIC data/mask register)
; On a real 5150, writing 0x08 to port 0x21 sets IRQ mask:
;   Only IRQ3 masked. We just verify the round-trip.
; =====================================================================
mov al, 0x08
out 0x21, al              ; OCW1 mask
xor ax, ax
in al, 0x21               ; read back
cmp al, 0x08
jne .pic_ocw1_fail
mov word [0x0508], 0x0001
.pic_ocw1_fail:

; =====================================================================
; Test 6: Verify I/O doesn't corrupt memory
; Write to I/O port 0x0090, then check that memory address 0x0090
; still has its original value (0xF4 -- the HLT fill pattern).
; =====================================================================
mov al, 0x77
out 0x90, al              ; write to I/O port 0x90
cmp byte [0x0090], 0xF4   ; memory at address 0x90 should still be 0xF4
jne .io_mem_fail
mov word [0x050A], 0x0001
.io_mem_fail:

hlt
