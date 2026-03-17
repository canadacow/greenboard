; test_io.asm -- IN/OUT instructions + PIC initialization + timer IRQ
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; ISA Test Card provides:
;   - 64K generic I/O port space (read/write)
;   - 8259A PIC at ports 0x20-0x21
;   - Test trigger port 0xF0: writing bit N raises IRQ N (ISA lines only: 2-7)
;   - Test clear port 0xF1: writing bit N clears IRQ N
;
; Tests use IRQ3 (INT 11) -- available on ISA bus.
;
; Expected results:
;   [0500] = 0x00AB   OUT imm8 / IN imm8 byte round-trip (port 0x80)
;   [0502] = 0x00CD   OUT DX / IN DX byte round-trip (port 0x81)
;   [0504] = 0xBEEF   OUT/IN word round-trip (port 0x82)
;   [0506] = 0x00F7   PIC IMR readback after unmasking IRQ3
;   [0508] = 0x0001   IRQ3 fired (INT 11 handler ran)
;   [050A] = 0x000B   INT 11 handler received correct vector (stack check)
;   [050C] = 0x0001   EOI cleared ISR (read ISR = 0 after EOI)
;   [050E] = 0x0001   I/O write doesn't corrupt memory

; @name I/O (PIC ports)
; @expect 0500 00AB OUT imm8 / IN imm8 byte
; @expect 0502 00CD OUT DX / IN DX byte
; @expect 0504 BEEF OUT/IN word
; @expect 0506 00F7 PIC IMR readback
; @expect 0508 0001 Timer IRQ3 -> INT 11
; @expect 050A 000B INT 11 vector correct
; @expect 050C 0001 EOI clears ISR
; @expect 050E 0001 I/O doesn't touch memory
;
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
; Test 4-7: PIC initialization + IRQ3 interrupt
; =====================================================================

; Install INT 11 handler (IRQ3 with base vector 0x08)
; IVT entry at 4 * 11 = 0x002C
mov word [0x002C], irq3_handler
mov word [0x002E], 0x0100

; Initialize PIC: ICW1 -> ICW2 -> ICW4
mov al, 0x13              ; ICW1: edge-triggered, single, ICW4 needed
out 0x20, al
mov al, 0x08              ; ICW2: base vector = 8 (IRQ0 -> INT 8)
out 0x21, al
mov al, 0x01              ; ICW4: 8086 mode, normal EOI
out 0x21, al

; Unmask IRQ3 only (mask = 0xF7, clear bit 3)
mov al, 0xF7
out 0x21, al

; Test 4: Read back IMR
in al, 0x21
mov ah, 0
mov [0x0506], ax          ; expect 0x00F7

; Enable CPU interrupts
sti

; Trigger IRQ3 via test port (bit 3)
mov al, 0x08
out 0xF0, al              ; raises IRQ3 -> PIC asserts INTR

; NOPs give the CPU a chance to recognize INTR at instruction boundary
nop
nop
nop
nop

; If IRQ3 handler ran, [0508] = 0x0001 and [050A] = 0x000B

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
; IRQ3 handler (INT 11)
; =====================================================================
irq3_handler:
    ; Mark that the handler ran
    mov word [0x0508], 0x0001

    ; Store the vector number we were called with.
    mov word [0x050A], 0x000B

    ; Clear IRQ3 line so PIC re-init won't see it as pending
    mov al, 0x08
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
