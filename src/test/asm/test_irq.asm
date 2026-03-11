; test_irq.asm -- Advanced hardware interrupt tests
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800.
;
; BusGlue provides:
;   - 8259A PIC at ports 0x20-0x21
;   - Test trigger port 0xF0: writing bit N raises IRQ N (drives High)
;   - Test clear port 0xF1: writing bit N clears IRQ N (drives Low)
;
; Expected results:
;   [0500] = 0x0001   IRQ1 fires with correct vector (INT 9)
;   [0502] = 0x0001   Priority: IRQ0 serviced before IRQ1
;   [0504] = 0x0001   Priority: IRQ1 serviced second
;   [0506] = 0x0001   Masked IRQ does not fire
;   [0508] = 0x0001   Specific EOI clears ISR bit
;   [050A] = 0x0001   Auto-EOI: ISR cleared automatically
;   [050C] = 0x0002   Nested HW interrupts: both handlers ran

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

; Working variables
mov byte [0x05F0], 0       ; test phase
mov word [0x0600], 0       ; order index (for priority test)

; Install IRQ handlers: INT 8 (IRQ0), INT 9 (IRQ1), INT 10 (IRQ2)
mov word [8*4],    irq0_handler
mov word [8*4+2],  0xF000
mov word [9*4],    irq1_handler
mov word [9*4+2],  0xF000
mov word [10*4],   irq2_handler
mov word [10*4+2], 0xF000

; Initialize PIC: edge-triggered, single, ICW4
mov al, 0x13
out 0x20, al
mov al, 0x08              ; vector base = 8
out 0x21, al
mov al, 0x01              ; 8086 mode, normal EOI
out 0x21, al

; =====================================================================
; Test 1: IRQ1 fires with correct vector
; =====================================================================
mov byte [0x05F0], 1
mov al, 0xFC              ; unmask IRQ0 + IRQ1
out 0x21, al
sti
mov al, 0x02              ; trigger IRQ1
out 0xF0, al
nop
nop
nop
nop
cli

; =====================================================================
; Test 2 & 3: Priority -- both IRQ0 and IRQ1 pending, IRQ0 first
; =====================================================================
mov byte [0x05F0], 2
mov word [0x0600], 0      ; order index = 0
mov al, 0x03              ; trigger both IRQ0 and IRQ1
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
mov al, 0x04              ; trigger IRQ2
out 0xF0, al
sti
nop
nop
nop
nop
cli
; If IRQ2 handler ran, [0506] would be 0xDEAD
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
mov al, 0xFE              ; unmask IRQ0
out 0x21, al

mov byte [0x05F0], 5
sti
mov al, 0x01              ; trigger IRQ0
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
mov al, 0xFE              ; unmask IRQ0
out 0x21, al

mov byte [0x05F0], 6
sti
mov al, 0x01              ; trigger IRQ0
out 0xF0, al
nop
nop
nop
nop
cli

; =====================================================================
; Test 7: Nested hardware interrupts
; IRQ0 handler: increment [050C], EOI, STI, trigger IRQ1
; IRQ1 handler: increment [050C], EOI
; Total [050C] = 2
; =====================================================================
; Re-init PIC, normal EOI
mov al, 0x13
out 0x20, al
mov al, 0x08
out 0x21, al
mov al, 0x01
out 0x21, al
mov al, 0xFC              ; unmask IRQ0 + IRQ1
out 0x21, al

mov byte [0x05F0], 7
mov word [0x050C], 0x0000
sti
mov al, 0x01              ; trigger IRQ0
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

hlt

; =====================================================================
; IRQ0 handler (INT 8)
; =====================================================================
irq0_handler:
    push ax
    push bx

    mov al, [0x05F0]

    cmp al, 2
    je .irq0_phase2
    cmp al, 5
    je .irq0_phase5
    cmp al, 6
    je .irq0_phase6
    cmp al, 7
    je .irq0_phase7
    ; Unexpected phase -- just EOI and return
    mov al, 0x20
    out 0x20, al
    jmp .irq0_done

.irq0_phase2:
    ; Priority test: record that IRQ0 fired at current order index
    mov bx, [0x0600]
    cmp bx, 0
    jne .irq0_p2_not_first
    mov word [0x0502], 0x0001  ; IRQ0 came first
.irq0_p2_not_first:
    inc bx
    mov [0x0600], bx
    mov al, 0x01
    out 0xF1, al               ; clear IRQ0 line
    mov al, 0x20
    out 0x20, al               ; non-specific EOI
    jmp .irq0_done

.irq0_phase5:
    ; Specific EOI for IRQ0: OCW2 = 0x60 (cmd=3, L=0)
    mov al, 0x01
    out 0xF1, al               ; clear IRQ0 line
    mov al, 0x60
    out 0x20, al
    ; Read ISR
    mov al, 0x0B
    out 0x20, al
    in al, 0x20
    cmp al, 0x00
    jne .irq0_p5_fail
    mov word [0x0508], 0x0001
.irq0_p5_fail:
    jmp .irq0_done

.irq0_phase6:
    ; Auto-EOI: ISR should already be 0
    mov al, 0x01
    out 0xF1, al               ; clear IRQ0 line
    mov al, 0x0B
    out 0x20, al
    in al, 0x20
    cmp al, 0x00
    jne .irq0_p6_fail
    mov word [0x050A], 0x0001
.irq0_p6_fail:
    ; No EOI needed (auto-EOI mode)
    jmp .irq0_done

.irq0_phase7:
    ; Nested: increment counter, EOI, STI, trigger IRQ1
    add word [0x050C], 1
    mov al, 0x01
    out 0xF1, al               ; clear IRQ0 line
    mov al, 0x20
    out 0x20, al               ; EOI for IRQ0 first (so IRQ1 can nest)
    sti                        ; re-enable interrupts
    mov al, 0x02               ; trigger IRQ1
    out 0xF0, al
    nop
    nop
    nop
    nop
    cli
    jmp .irq0_done

.irq0_done:
    pop bx
    pop ax
    iret

; =====================================================================
; IRQ1 handler (INT 9)
; =====================================================================
irq1_handler:
    push ax
    push bx

    mov al, [0x05F0]

    cmp al, 1
    je .irq1_phase1
    cmp al, 2
    je .irq1_phase2
    cmp al, 7
    je .irq1_phase7
    mov al, 0x20
    out 0x20, al
    jmp .irq1_done

.irq1_phase1:
    ; IRQ1 fires test: mark success
    mov word [0x0500], 0x0001
    mov al, 0x02
    out 0xF1, al               ; clear IRQ1 line
    mov al, 0x20
    out 0x20, al
    jmp .irq1_done

.irq1_phase2:
    ; Priority test: record that IRQ1 fired at current order index
    mov bx, [0x0600]
    cmp bx, 1
    jne .irq1_p2_not_second
    mov word [0x0504], 0x0001  ; IRQ1 came second
.irq1_p2_not_second:
    inc bx
    mov [0x0600], bx
    mov al, 0x02
    out 0xF1, al               ; clear IRQ1 line
    mov al, 0x20
    out 0x20, al
    jmp .irq1_done

.irq1_phase7:
    ; Nested: increment counter, EOI
    add word [0x050C], 1
    mov al, 0x02
    out 0xF1, al               ; clear IRQ1 line
    mov al, 0x20
    out 0x20, al
    jmp .irq1_done

.irq1_done:
    pop bx
    pop ax
    iret

; =====================================================================
; IRQ2 handler (INT 10) -- should never fire when masked
; =====================================================================
irq2_handler:
    mov word [0x0506], 0xDEAD
    mov al, 0x04
    out 0xF1, al               ; clear IRQ2 line
    mov al, 0x20
    out 0x20, al
    iret
