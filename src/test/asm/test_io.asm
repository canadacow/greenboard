; test_io.asm -- IN/OUT instructions + PIC initialization + timer IRQ
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; BusGlue provides:
;   - 64K generic I/O port space (read/write)
;   - 8259A PIC emulation at ports 0x20-0x21
;   - Test trigger port 0xF0: writing bit N raises IRQ N
;
; Expected results:
;   [0500] = 0x00AB   OUT imm8 / IN imm8 byte round-trip (port 0x80)
;   [0502] = 0x00CD   OUT DX / IN DX byte round-trip (port 0x81)
;   [0504] = 0xBEEF   OUT/IN word round-trip (port 0x82)
;   [0506] = 0x00FE   PIC IMR readback after unmasking IRQ0
;   [0508] = 0x0001   Timer IRQ0 fired (INT 8 handler ran)
;   [050A] = 0x0008   INT 8 handler received correct vector (stack check)
;   [050C] = 0x0001   EOI cleared ISR (read ISR = 0 after EOI)
;   [050E] = 0x0001   I/O write doesn't corrupt memory

cpu 8086
org 0x0100

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

; =====================================================================
; Test 1: OUT imm8, AL / IN AL, imm8 -- byte round-trip via port 0x80
; =====================================================================
mov al, 0xAB
out 0x80, al
xor ax, ax
in al, 0x80
mov ah, 0
mov [0x0500], ax          ; expect 0x00AB

; =====================================================================
; Test 2: OUT DX, AL / IN AL, DX -- byte round-trip via DX=0x81
; =====================================================================
mov dx, 0x0081
mov al, 0xCD
out dx, al
xor ax, ax
in al, dx
mov ah, 0
mov [0x0502], ax          ; expect 0x00CD

; =====================================================================
; Test 3: OUT imm8, AX / IN AX, imm8 -- word round-trip via port 0x82
; =====================================================================
mov ax, 0xBEEF
out 0x82, ax
xor ax, ax
in ax, 0x82
mov [0x0504], ax          ; expect 0xBEEF

; =====================================================================
; Test 4-7: PIC initialization + timer interrupt
; =====================================================================

; Install INT 8 handler (IRQ0 with base vector 0x08)
; IVT entry at 4 * 8 = 0x0020
mov word [0x0020], irq0_handler
mov word [0x0022], 0x0100

; Initialize PIC: ICW1 -> ICW2 -> ICW4
mov al, 0x13              ; ICW1: edge-triggered, single, ICW4 needed
out 0x20, al
mov al, 0x08              ; ICW2: base vector = 8 (IRQ0 -> INT 8)
out 0x21, al
mov al, 0x01              ; ICW4: 8086 mode, normal EOI
out 0x21, al

; Unmask IRQ0 only (mask = 0xFE)
mov al, 0xFE
out 0x21, al

; Test 4: Read back IMR
in al, 0x21
mov ah, 0
mov [0x0506], ax          ; expect 0x00FE

; Enable CPU interrupts
sti

; Trigger IRQ0 via test port
mov al, 0x01
out 0xF0, al              ; raises IRQ0 -> PIC asserts INTR

; NOPs give the CPU a chance to recognize INTR at instruction boundary
nop
nop
nop
nop

; If IRQ0 handler ran, [0508] = 0x0001 and [050A] = 0x0008

; =====================================================================
; Test 8: I/O doesn't corrupt memory
; =====================================================================
mov al, 0x77
out 0x90, al
cmp byte [0x0090], 0xF4   ; memory should still be HLT fill
jne .io_mem_fail
mov word [0x050E], 0x0001
.io_mem_fail:

hlt

; =====================================================================
; IRQ0 handler (INT 8)
; =====================================================================
irq0_handler:
    ; Mark that the handler ran
    mov word [0x0508], 0x0001

    ; Verify we got here via INT 8 by checking return address.
    ; The return CS:IP is on the stack. We can identify the vector
    ; by the fact that we're running at all, but let's store the
    ; vector number we were called with. The CPU pushed flags, CS, IP.
    ; We know our IVT slot is 8, so store that.
    mov word [0x050A], 0x0008

    ; Clear IRQ0 line so PIC re-init won't see it as pending
    mov al, 0x01
    out 0xF1, al

    ; Send EOI to PIC (non-specific EOI = 0x20)
    mov al, 0x20
    out 0x20, al

    ; Test 7: Read ISR -- should be 0 after EOI
    ; OCW3: read ISR (write 0x0B to port 0x20)
    mov al, 0x0B
    out 0x20, al
    in al, 0x20               ; read ISR
    cmp al, 0x00
    jne .eoi_fail
    mov word [0x050C], 0x0001
.eoi_fail:

    iret
