; test_dma.asm -- DMA channel 1 IO->memory transfer via ISA Test Card
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
; SP initialized to 0x0800.
;
; Tests DMA channel 1 transferring lorem ipsum text from the ISA Test Card
; into DRAM, with IRQ5 fired on completion via T/C.
;
; Setup:
;   1. Initialize PIC (ICW1/2/4), unmask IRQ5
;   2. Install IRQ5 handler (INT 0x0D)
;   3. Program DMA ch1: base address, word count, mode (single, write, ch1)
;   4. Write DMA page register for ch1 (port 0x83) with page 0
;   5. Unmask DMA ch1
;   6. Trigger ISA Test Card DMA (port 0xF4) -- card asserts DRQ1
;   7. DMA controller transfers bytes from card to DRAM
;   8. Card detects T/C, fires IRQ5
;   9. IRQ5 handler sets completion flag
;  10. CPU verifies transferred data in DRAM matches expected lorem ipsum
;
; ISA Test Card DMA buffer is preloaded by the test harness with:
;   "Lorem ipsum dolor sit amet, " (29 bytes)
;
; DMA target address: 0x02000 (physical), page 0, offset 0x2000
;
; Expected results:
;   [0500] = 0x0001   DMA ch1 status shows TC reached (bit 1)
;   [0502] = 0x0001   IRQ5 completion handler ran
;   [0504] = 0x0001   First 4 bytes match "Lore" (0x4C 0x6F 0x72 0x65)
;   [0506] = 0x0001   Bytes 4-7 match "m ip" (0x6D 0x20 0x69 0x70)
;   [0508] = 0x0001   Last 4 bytes match "et, " (0x65 0x74 0x2C 0x20)
;   [050A] = 0x001D   Total bytes transferred (29)

; @name DMA (channel 1)
; @expect 0500 0001 DMA ch1 TC reached
; @expect 0502 0001 IRQ5 completion fired
; @expect 0504 0001 First 4 bytes "Lore"
; @expect 0506 0001 Bytes 4-7 "m ip"
; @expect 0508 0001 Last 4 bytes "et, "
; @expect 050A 001C Byte count (28)
; @expect 0510 0001 Run2: DMA ch1 TC reached
; @expect 0512 0001 Run2: IRQ5 completion fired
; @expect 0514 0001 Run2: First 4 bytes "Lore"
; @expect 0516 0001 Run2: Bytes 4-7 "m ip"
; @expect 0518 0001 Run2: Last 4 bytes "et, "
; @expect 051A 001C Run2: Byte count (28)
; @expect 0520 0001 Ch3 single: DMA TC reached
; @expect 0522 0001 Ch3 single: IRQ5 completion fired
; @expect 0524 0001 Ch3 single: First 4 bytes "Lore"
; @expect 052A 001C Ch3 single: Byte count (28)
; @expect 0530 0001 Ch3 block: DMA TC reached
; @expect 0532 0001 Ch3 block: IRQ5 completion fired
; @expect 0534 0001 Ch3 block: First 4 bytes "Lore"
; @expect 053A 001C Ch3 block: Byte count (28)
;
cpu 8086
org 0x0100

DMA_DEST    equ 0x2000      ; physical address for DMA target (page 0)
DMA_COUNT   equ 28          ; number of bytes to transfer

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

; =====================================================================
; Initialize PIC: ICW1 -> ICW2 -> ICW4
; =====================================================================
mov al, 0x13              ; ICW1: edge-triggered, single, ICW4 needed
out 0x20, al
mov al, 0x08              ; ICW2: base vector = 8 (IRQ0 -> INT 8)
out 0x21, al
mov al, 0x01              ; ICW4: 8086 mode, normal EOI
out 0x21, al

; Unmask IRQ5 (bit 5 = 0 in mask)
mov al, 0xDF              ; mask = 1101_1111 = all masked except IRQ5
out 0x21, al

; Install IRQ5 handler at INT 0x0D (vector 8 + 5 = 13 = 0x0D)
; IVT entry at 4 * 13 = 0x0034
mov word [0x0034], irq5_handler
mov word [0x0036], 0x0100   ; segment (matches CS from reset vector)

; =====================================================================
; Program DMA channel 1
; =====================================================================

; Master clear (reset DMA controller)
mov al, 0x00
out 0x0D, al

; Clear byte flip-flop
out 0x0C, al

; Set DMA ch1 base address = DMA_DEST (0x2000)
mov al, (DMA_DEST & 0xFF)       ; low byte of address
out 0x02, al
mov al, (DMA_DEST >> 8)         ; high byte of address
out 0x02, al

; Clear byte flip-flop again for count
mov al, 0x00
out 0x0C, al

; Set DMA ch1 word count = DMA_COUNT - 1 (transfers N+1 bytes)
mov al, ((DMA_COUNT - 1) & 0xFF)
out 0x03, al
mov al, ((DMA_COUNT - 1) >> 8)
out 0x03, al

; Set DMA ch1 mode: single transfer, address increment, write (IO->mem), ch1
; Mode byte: bits [7:6]=00 (demand), [5]=0 (increment), [4]=0 (no auto-init),
;            [3:2]=01 (write = IO->memory), [1:0]=01 (channel 1)
; = 0000_0101 = 0x05
; Actually: [7:6]=01 (single transfer) for proper handshake
; = 0100_0101 = 0x45
mov al, 0x45
out 0x0B, al

; Write DMA page register for ch1: port 0x83, page = 0
mov al, 0x00
out 0x83, al

; Unmask DMA ch1: single mask register, channel 1, clear mask bit
; Bits [1:0]=01 (ch1), bit 2=0 (unmask)
mov al, 0x01
out 0x0A, al

; =====================================================================
; Trigger DMA transfer via ISA Test Card
; =====================================================================

; Enable CPU interrupts so we can receive IRQ5
sti

; Tell test card to assert DRQ1 (port 0xF4)
mov al, 0x01
out 0xF4, al

; Wait for IRQ5 handler to set completion flag.
; The DMA controller will handle the transfer autonomously.
; We poll [0x0502] for the completion flag set by the IRQ5 handler.
mov cx, 0xFFFF
.wait_loop:
    cmp word [0x0502], 0x0001
    je .dma_done
    nop
    loop .wait_loop
.dma_done:

; =====================================================================
; Read DMA status register (port 0x08) to check TC flag
; =====================================================================
in al, 0x08
test al, 0x02              ; bit 1 = ch1 TC reached
jz .no_tc
mov word [0x0500], 0x0001   ; TC reached
.no_tc:

; =====================================================================
; Verify transferred data in DRAM at DMA_DEST
; =====================================================================

; Check first 4 bytes: "Lore" = 0x4C 0x6F 0x72 0x65
cmp byte [DMA_DEST + 0], 0x4C
jne .fail_first4
cmp byte [DMA_DEST + 1], 0x6F
jne .fail_first4
cmp byte [DMA_DEST + 2], 0x72
jne .fail_first4
cmp byte [DMA_DEST + 3], 0x65
jne .fail_first4
mov word [0x0504], 0x0001
.fail_first4:

; Check bytes 4-7: "m ip" = 0x6D 0x20 0x69 0x70
cmp byte [DMA_DEST + 4], 0x6D
jne .fail_mid4
cmp byte [DMA_DEST + 5], 0x20
jne .fail_mid4
cmp byte [DMA_DEST + 6], 0x69
jne .fail_mid4
cmp byte [DMA_DEST + 7], 0x70
jne .fail_mid4
mov word [0x0506], 0x0001
.fail_mid4:

; Check last 4 bytes (positions 24-27): "et, " = 0x65 0x74 0x2C 0x20
cmp byte [DMA_DEST + 24], 0x65
jne .fail_last4
cmp byte [DMA_DEST + 25], 0x74
jne .fail_last4
cmp byte [DMA_DEST + 26], 0x2C
jne .fail_last4
cmp byte [DMA_DEST + 27], 0x20
jne .fail_last4
mov word [0x0508], 0x0001
.fail_last4:

; Store byte count
mov word [0x050A], DMA_COUNT

; =====================================================================
; REPEAT: Second DMA transfer to 0x3000 (same data, fresh setup)
; Tests whether first-byte-lost is a one-time init bug or persistent.
; =====================================================================

; Reset completion flag
mov word [0x0512], 0x0000

; Reinstall IRQ5 handler (same handler, sets [0x0512] this time)
mov word [0x0034], irq5_handler2
mov word [0x0036], 0x0100

; Clear byte flip-flop
mov al, 0x00
out 0x0C, al

; Set DMA ch1 base address = 0x3000
mov al, 0x00
out 0x02, al
mov al, 0x30
out 0x02, al

; Clear byte flip-flop for count
mov al, 0x00
out 0x0C, al

; Set DMA ch1 word count = DMA_COUNT - 1
mov al, ((DMA_COUNT - 1) & 0xFF)
out 0x03, al
mov al, ((DMA_COUNT - 1) >> 8)
out 0x03, al

; Set DMA ch1 mode: single transfer, write, ch1
mov al, 0x45
out 0x0B, al

; Page register = 0
mov al, 0x00
out 0x83, al

; Unmask DMA ch1
mov al, 0x01
out 0x0A, al

; Tell test card to reset DMA pointer and re-trigger
mov al, 0x01
out 0xF4, al

; Wait for IRQ5 completion
mov cx, 0xFFFF
.wait_loop2:
    cmp word [0x0512], 0x0001
    je .dma2_done
    nop
    loop .wait_loop2
.dma2_done:

; Read DMA status for TC
in al, 0x08
test al, 0x02
jz .no_tc2
mov word [0x0510], 0x0001
.no_tc2:

; Check first 4 bytes at 0x3000: "Lore"
cmp byte [0x3000], 0x4C
jne .fail2_first4
cmp byte [0x3001], 0x6F
jne .fail2_first4
cmp byte [0x3002], 0x72
jne .fail2_first4
cmp byte [0x3003], 0x65
jne .fail2_first4
mov word [0x0514], 0x0001
.fail2_first4:

; Check bytes 4-7 at 0x3000: "m ip"
cmp byte [0x3004], 0x6D
jne .fail2_mid4
cmp byte [0x3005], 0x20
jne .fail2_mid4
cmp byte [0x3006], 0x69
jne .fail2_mid4
cmp byte [0x3007], 0x70
jne .fail2_mid4
mov word [0x0516], 0x0001
.fail2_mid4:

; Check last 4 bytes: "et, "
cmp byte [0x3018], 0x65
jne .fail2_last4
cmp byte [0x3019], 0x74
jne .fail2_last4
cmp byte [0x301A], 0x2C
jne .fail2_last4
cmp byte [0x301B], 0x20
jne .fail2_last4
mov word [0x0518], 0x0001
.fail2_last4:

; Store byte count for run 2
mov word [0x051A], DMA_COUNT

; =====================================================================
; RUN 3: DMA channel 3, single transfer mode to 0x4000
; (Channel 2 is reserved for the floppy disk controller)
; =====================================================================
mov word [0x0522], 0x0000
mov word [0x0034], irq5_handler3
mov word [0x0036], 0x0100

; Master clear
mov al, 0x00
out 0x0D, al

; Clear flip-flop, set ch3 address = 0x4000
out 0x0C, al
mov al, 0x00
out 0x06, al              ; ch3 addr low
mov al, 0x40
out 0x06, al              ; ch3 addr high

; Clear flip-flop, set ch3 count = DMA_COUNT - 1
mov al, 0x00
out 0x0C, al
mov al, ((DMA_COUNT - 1) & 0xFF)
out 0x07, al              ; ch3 count low
mov al, ((DMA_COUNT - 1) >> 8)
out 0x07, al              ; ch3 count high

; Mode: single transfer, write (IO->mem), ch3 = 0100_0111 = 0x47
mov al, 0x47
out 0x0B, al

; Page register ch3 = 0 (port 0x82)
mov al, 0x00
out 0x82, al

; Unmask ch3: bits [1:0]=11 (ch3), bit 2=0 (unmask) = 0x03
mov al, 0x03
out 0x0A, al

; Start DMA on ch3 via test card
mov al, 0x03
out 0xF4, al

; Wait for completion
mov cx, 0xFFFF
.wait_loop3:
    cmp word [0x0522], 0x0001
    je .dma3_done
    nop
    loop .wait_loop3
.dma3_done:

; Check TC
in al, 0x08
test al, 0x08             ; bit 3 = ch3 TC
jz .no_tc3
mov word [0x0520], 0x0001
.no_tc3:

; Check first 4 bytes at 0x4000: "Lore"
cmp byte [0x4000], 0x4C
jne .fail3_first4
cmp byte [0x4001], 0x6F
jne .fail3_first4
cmp byte [0x4002], 0x72
jne .fail3_first4
cmp byte [0x4003], 0x65
jne .fail3_first4
mov word [0x0524], 0x0001
.fail3_first4:

mov word [0x052A], DMA_COUNT

; =====================================================================
; RUN 4: DMA channel 3, block transfer mode to 0x5000
; =====================================================================
mov word [0x0532], 0x0000
mov word [0x0034], irq5_handler4
mov word [0x0036], 0x0100

; Master clear
mov al, 0x00
out 0x0D, al

; Clear flip-flop, set ch3 address = 0x5000
out 0x0C, al
mov al, 0x00
out 0x06, al              ; ch3 addr low
mov al, 0x50
out 0x06, al              ; ch3 addr high

; Clear flip-flop, set ch3 count = DMA_COUNT - 1
mov al, 0x00
out 0x0C, al
mov al, ((DMA_COUNT - 1) & 0xFF)
out 0x07, al              ; ch3 count low
mov al, ((DMA_COUNT - 1) >> 8)
out 0x07, al              ; ch3 count high

; Mode: block transfer, write (IO->mem), ch3 = 1000_0111 = 0x87
mov al, 0x87
out 0x0B, al

; Page register ch3 = 0 (port 0x82)
mov al, 0x00
out 0x82, al

; Unmask ch3: bits [1:0]=11 (ch3), bit 2=0 (unmask) = 0x03
mov al, 0x03
out 0x0A, al

; Start DMA on ch3 via test card
mov al, 0x03
out 0xF4, al

; Wait for completion
mov cx, 0xFFFF
.wait_loop4:
    cmp word [0x0532], 0x0001
    je .dma4_done
    nop
    loop .wait_loop4
.dma4_done:

; Check TC
in al, 0x08
test al, 0x08             ; bit 3 = ch3 TC
jz .no_tc4
mov word [0x0530], 0x0001
.no_tc4:

; Check first 4 bytes at 0x5000: "Lore"
cmp byte [0x5000], 0x4C
jne .fail4_first4
cmp byte [0x5001], 0x6F
jne .fail4_first4
cmp byte [0x5002], 0x72
jne .fail4_first4
cmp byte [0x5003], 0x65
jne .fail4_first4
mov word [0x0534], 0x0001
.fail4_first4:

mov word [0x053A], DMA_COUNT

hlt

; =====================================================================
; IRQ5 handler (INT 0x0D)
; =====================================================================
irq5_handler:
    ; Mark completion
    mov word [0x0502], 0x0001

    ; Clear IRQ5 line on test card (port 0xF1, bit 5)
    mov al, 0x20
    out 0xF1, al

    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al

    iret

irq5_handler2:
    mov word [0x0512], 0x0001
    mov al, 0x20
    out 0xF1, al
    mov al, 0x20
    out 0x20, al
    iret

irq5_handler3:
    mov word [0x0522], 0x0001
    mov al, 0x20
    out 0xF1, al
    mov al, 0x20
    out 0x20, al
    iret

irq5_handler4:
    mov word [0x0532], 0x0001
    mov al, 0x20
    out 0xF1, al
    mov al, 0x20
    out 0x20, al
    iret
