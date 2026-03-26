; test_dma_isa_ram.asm -- DMA transfer to ISA expansion RAM (0x40000+)
;
; Exercises the DMA-to-ISA-MMIO path: the DMA controller writes data from
; the ISA Test Card into ISA expansion RAM at physical address 0x40000.
; The CPU then reads it back (via normal MMIO) to verify.
;
; This tests that ~MEMW during DMA correctly routes through the ISA bus
; to expansion cards, even though AEN is High (disabling CPU I/O decode).
;
; Setup:
;   1. Init PIC, unmask IRQ5
;   2. Program DMA ch1: page=4 (A16-A19), offset=0x0000, count=28
;      -> physical address = page(4) << 16 | offset(0) = 0x40000
;   3. Trigger TestCard DMA on ch1
;   4. Wait for T/C -> IRQ5
;   5. CPU reads 0x40000+ via segment 4000:0000 and verifies "Lorem ipsum..."
;
; @name DMA to ISA expansion RAM
; @expect 0500 0001 DMA ch1 TC reached
; @expect 0502 0001 IRQ5 completion fired
; @expect 0504 0001 First 4 bytes "Lore"
; @expect 0506 0001 Bytes 4-7 "m ip"
; @expect 0508 0001 Last 4 bytes "et, "
; @expect 050A 001C Byte count (28)

cpu 8086
org 0x0100

DMA_OFFSET  equ 0x0000     ; offset within page
DMA_PAGE    equ 0x04        ; page 4 -> A16-A19 = 0100 -> physical 0x40000
DMA_COUNT   equ 28
DMA_SEG     equ 0x4000      ; segment to read back (4000:0000 = 0x40000)

    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results
    mov word [0x0500], 0
    mov word [0x0502], 0
    mov word [0x0504], 0
    mov word [0x0506], 0
    mov word [0x0508], 0
    mov word [0x050A], 0

; =====================================================================
; Init PIC: ICW1/2/4, unmask IRQ5
; =====================================================================
    mov al, 0x13
    out 0x20, al
    mov al, 0x08
    out 0x21, al
    mov al, 0x01
    out 0x21, al
    mov al, 0xDF            ; unmask IRQ5 only
    out 0x21, al

    ; Install IRQ5 handler at INT 0x0D
    mov word [0x0034], irq5_handler
    mov word [0x0036], 0x0100

; =====================================================================
; Program DMA ch1: page 4, offset 0, count 28
; =====================================================================
    ; Master clear
    mov al, 0x00
    out 0x0D, al

    ; Clear flip-flop
    out 0x0C, al

    ; Ch1 base address = DMA_OFFSET
    mov al, (DMA_OFFSET & 0xFF)
    out 0x02, al
    mov al, (DMA_OFFSET >> 8)
    out 0x02, al

    ; Clear flip-flop for count
    mov al, 0x00
    out 0x0C, al

    ; Ch1 word count = DMA_COUNT - 1
    mov al, ((DMA_COUNT - 1) & 0xFF)
    out 0x03, al
    mov al, ((DMA_COUNT - 1) >> 8)
    out 0x03, al

    ; Ch1 mode: single transfer, increment, write (IO->mem), ch1
    mov al, 0x45
    out 0x0B, al

    ; Page register for ch1: port 0x83 = page 4
    mov al, DMA_PAGE
    out 0x83, al

    ; Unmask ch1
    mov al, 0x01
    out 0x0A, al

; =====================================================================
; Trigger DMA via TestCard
; =====================================================================
    sti

    ; Tell test card to assert DRQ1
    mov al, 0x01
    out 0xF4, al

    ; Wait for IRQ5 completion flag
    mov cx, 0xFFFF
.wait:
    cmp word [0x0502], 1
    je .done_wait
    nop
    loop .wait
.done_wait:

; =====================================================================
; Check DMA status
; =====================================================================
    in al, 0x08
    test al, 0x02           ; bit 1 = ch1 TC
    jz .no_tc
    mov word [0x0500], 1
.no_tc:

; =====================================================================
; Verify data at 0x40000 (segment 4000:0000)
; =====================================================================
    mov ax, DMA_SEG
    mov es, ax

    ; First 4 bytes: "Lore" = 4C 6F 72 65
    cmp byte [es:0x0000], 0x4C
    jne .fail1
    cmp byte [es:0x0001], 0x6F
    jne .fail1
    cmp byte [es:0x0002], 0x72
    jne .fail1
    cmp byte [es:0x0003], 0x65
    jne .fail1
    mov word [0x0504], 1
.fail1:

    ; Bytes 4-7: "m ip" = 6D 20 69 70
    cmp byte [es:0x0004], 0x6D
    jne .fail2
    cmp byte [es:0x0005], 0x20
    jne .fail2
    cmp byte [es:0x0006], 0x69
    jne .fail2
    cmp byte [es:0x0007], 0x70
    jne .fail2
    mov word [0x0506], 1
.fail2:

    ; Last 4 bytes (24-27): "et, " = 65 74 2C 20
    cmp byte [es:0x0018], 0x65
    jne .fail3
    cmp byte [es:0x0019], 0x74
    jne .fail3
    cmp byte [es:0x001A], 0x2C
    jne .fail3
    cmp byte [es:0x001B], 0x20
    jne .fail3
    mov word [0x0508], 1
.fail3:

    ; Count transferred bytes (scan for non-zero from start)
    xor bx, bx
    xor cx, cx
.count:
    cmp byte [es:bx], 0
    je .count_done
    inc bx
    inc cx
    cmp bx, 64
    jb .count
.count_done:
    mov [0x050A], cx

    hlt

; =====================================================================
; IRQ5 handler
; =====================================================================
irq5_handler:
    push ax
    mov word [0x0502], 1
    mov al, 0x20
    out 0x20, al            ; EOI
    pop ax
    iret
