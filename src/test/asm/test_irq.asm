; test_irq.asm -- Advanced hardware interrupt tests
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; ISA Test Card provides:
;   - 8259A PIC at ports 0x20-0x21
;   - Test trigger port 0xF0: writing bit N raises IRQ N (drives High)
;   - Test clear port 0xF1: writing bit N clears IRQ N (drives Low)
;   - Only ISA-bus IRQs available: IRQ2-IRQ7 (bits 2-7)
;
; Tests use IRQ3 (INT 11), IRQ4 (INT 12), IRQ5 (INT 13).
; Priority: IRQ3 > IRQ4 > IRQ5.

; @name IRQ (advanced)
; @expect 0500 0001 IRQ4 fires (INT 12)
; @expect 0502 0001 Priority: IRQ3 first
; @expect 0504 0001 Priority: IRQ4 second
; @expect 0506 0001 Masked IRQ blocked
; @expect 0508 0001 Specific EOI
; @expect 050A 0001 Auto-EOI
; @expect 050C 0002 Nested HW interrupts
;
cpu 8086
org 0x0100

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

; Working variables
mov byte [0x05F0], 0       ; test phase
mov word [0x0600], 0       ; order index (for priority test)

; Install IRQ handlers: INT 11 (IRQ3), INT 12 (IRQ4), INT 13 (IRQ5)
mov word [11*4],   irq3_handler
mov word [11*4+2], 0x0100
mov word [12*4],   irq4_handler
mov word [12*4+2], 0x0100
mov word [13*4],   irq5_handler
mov word [13*4+2], 0x0100

; Initialize PIC: edge-triggered, single, ICW4
mov al, 0x13
out 0x20, al
mov al, 0x08              ; vector base = 8
out 0x21, al
mov al, 0x01              ; 8086 mode, normal EOI
out 0x21, al

; =====================================================================
; Test 1: IRQ4 fires with correct vector
; =====================================================================
mov byte [0x05F0], 1
mov al, 0xE7              ; unmask IRQ3 + IRQ4 (clear bits 3,4)
out 0x21, al
sti
mov al, 0x10              ; trigger IRQ4 (bit 4)
out 0xF0, al
nop
nop
nop
nop
cli

; =====================================================================
; Test 2 & 3: Priority -- both IRQ3 and IRQ4 pending, IRQ3 first
; =====================================================================
mov byte [0x05F0], 2
mov word [0x0600], 0      ; order index = 0
mov al, 0x18              ; trigger both IRQ3 and IRQ4 (bits 3,4)
out 0xF0, al
sti
nop
nop
nop
nop
nop
nop
nop
nop
cli

; =====================================================================
; Test 4: Masked IRQ does not fire
; =====================================================================
mov byte [0x05F0], 4
mov al, 0xFF              ; mask all IRQs
out 0x21, al
mov al, 0x20              ; trigger IRQ5 (bit 5)
out 0xF0, al
sti
nop
nop
nop
nop
cli
; If IRQ5 handler ran, [0506] would be 0xDEAD
cmp word [0x0506], 0x0000
jne .mask_fail
mov word [0x0506], 0x0001
.mask_fail:

; =====================================================================
; Test 5: Specific EOI clears correct ISR bit
; =====================================================================
; Re-init PIC (clears any stale state)
mov al, 0x13
out 0x20, al
mov al, 0x08
out 0x21, al
mov al, 0x01
out 0x21, al
mov al, 0xF7              ; unmask IRQ3 (clear bit 3)
out 0x21, al

mov byte [0x05F0], 5
sti
mov al, 0x08              ; trigger IRQ3 (bit 3)
out 0xF0, al
nop
nop
nop
nop
cli

; =====================================================================
; Test 6: Auto-EOI mode -- ISR cleared automatically
; =====================================================================
; Re-init PIC with auto-EOI
mov al, 0x13
out 0x20, al
mov al, 0x08
out 0x21, al
mov al, 0x03              ; 8086 mode + auto-EOI (bit 1)
out 0x21, al
mov al, 0xF7              ; unmask IRQ3 (clear bit 3)
out 0x21, al

mov byte [0x05F0], 6
sti
mov al, 0x08              ; trigger IRQ3 (bit 3)
out 0xF0, al
nop
nop
nop
nop
cli

; =====================================================================
; Test 7: Nested hardware interrupts
; IRQ3 handler: increment [050C], EOI, STI, trigger IRQ4
; IRQ4 handler: increment [050C], EOI
; Total [050C] = 2
; =====================================================================
; Re-init PIC, normal EOI
mov al, 0x13
out 0x20, al
mov al, 0x08
out 0x21, al
mov al, 0x01
out 0x21, al
mov al, 0xE7              ; unmask IRQ3 + IRQ4
out 0x21, al

mov byte [0x05F0], 7
mov word [0x050C], 0x0000
sti
mov al, 0x08              ; trigger IRQ3 (bit 3)
out 0xF0, al
nop
nop
nop
nop
nop
nop
nop
nop
cli

int3

; =====================================================================
; IRQ3 handler (INT 11)
; =====================================================================
irq3_handler:
    push ax
    push bx

    mov al, [0x05F0]

    cmp al, 2
    je .irq3_phase2
    cmp al, 5
    je .irq3_phase5
    cmp al, 6
    je .irq3_phase6
    cmp al, 7
    je .irq3_phase7
    ; Unexpected phase -- just EOI and return
    mov al, 0x20
    out 0x20, al
    jmp .irq3_done

.irq3_phase2:
    ; Priority test: record that IRQ3 fired at current order index
    mov bx, [0x0600]
    cmp bx, 0
    jne .irq3_p2_not_first
    mov word [0x0502], 0x0001  ; IRQ3 came first
.irq3_p2_not_first:
    inc bx
    mov [0x0600], bx
    mov al, 0x08
    out 0xF1, al               ; clear IRQ3 line (bit 3)
    mov al, 0x20
    out 0x20, al               ; non-specific EOI
    jmp .irq3_done

.irq3_phase5:
    ; Specific EOI for IRQ3: OCW2 = 0x63 (cmd=3, L=3)
    mov al, 0x08
    out 0xF1, al               ; clear IRQ3 line
    mov al, 0x63
    out 0x20, al
    ; Read ISR
    mov al, 0x0B
    out 0x20, al
    in al, 0x20
    cmp al, 0x00
    jne .irq3_p5_fail
    mov word [0x0508], 0x0001
.irq3_p5_fail:
    jmp .irq3_done

.irq3_phase6:
    ; Auto-EOI: ISR should already be 0
    mov al, 0x08
    out 0xF1, al               ; clear IRQ3 line
    mov al, 0x0B
    out 0x20, al
    in al, 0x20
    cmp al, 0x00
    jne .irq3_p6_fail
    mov word [0x050A], 0x0001
.irq3_p6_fail:
    ; No EOI needed (auto-EOI mode)
    jmp .irq3_done

.irq3_phase7:
    ; Nested: increment counter, EOI, STI, trigger IRQ4
    add word [0x050C], 1
    mov al, 0x08
    out 0xF1, al               ; clear IRQ3 line
    mov al, 0x20
    out 0x20, al               ; EOI for IRQ3 first (so IRQ4 can nest)
    sti                        ; re-enable interrupts
    mov al, 0x10               ; trigger IRQ4 (bit 4)
    out 0xF0, al
    nop
    nop
    nop
    nop
    cli
    jmp .irq3_done

.irq3_done:
    pop bx
    pop ax
    iret

; =====================================================================
; IRQ4 handler (INT 12)
; =====================================================================
irq4_handler:
    push ax
    push bx

    mov al, [0x05F0]

    cmp al, 1
    je .irq4_phase1
    cmp al, 2
    je .irq4_phase2
    cmp al, 7
    je .irq4_phase7
    mov al, 0x20
    out 0x20, al
    jmp .irq4_done

.irq4_phase1:
    ; IRQ4 fires test: mark success
    mov word [0x0500], 0x0001
    mov al, 0x10
    out 0xF1, al               ; clear IRQ4 line (bit 4)
    mov al, 0x20
    out 0x20, al
    jmp .irq4_done

.irq4_phase2:
    ; Priority test: record that IRQ4 fired at current order index
    mov bx, [0x0600]
    cmp bx, 1
    jne .irq4_p2_not_second
    mov word [0x0504], 0x0001  ; IRQ4 came second
.irq4_p2_not_second:
    inc bx
    mov [0x0600], bx
    mov al, 0x10
    out 0xF1, al               ; clear IRQ4 line
    mov al, 0x20
    out 0x20, al
    jmp .irq4_done

.irq4_phase7:
    ; Nested: increment counter, EOI
    add word [0x050C], 1
    mov al, 0x10
    out 0xF1, al               ; clear IRQ4 line
    mov al, 0x20
    out 0x20, al
    jmp .irq4_done

.irq4_done:
    pop bx
    pop ax
    iret

; =====================================================================
; IRQ5 handler (INT 13) -- should never fire when masked
; =====================================================================
irq5_handler:
    mov word [0x0506], 0xDEAD
    mov al, 0x20
    out 0xF1, al               ; clear IRQ5 line (bit 5)
    mov al, 0x20
    out 0x20, al
    iret
