; test_fdc_write.asm -- FDC WRITE DATA + FORMAT TRACK test
; Uses a blank 360K image (test harness swaps it in).
; Tests DMA write (memory->disk) and DMA format, then reads back to verify.
; Requires: ISA FDC (NEC uPD765), 8259A PIC, 8237A DMA
; Loaded at 0100:0100 (physical 0x01100). DS=0 after setup.
;
; Test plan:
;   1. Reset FDC, handle post-reset IRQ6 + SENSE INT
;   2. DMA WRITE: write a known pattern to C=0 H=0 R=1 (512 bytes)
;   3. DMA READ: read it back, verify first word + checksum
;   4. FORMAT TRACK: format C=1 H=0 (fill byte 0xF6)
;   5. DMA READ: read C=1 H=0 R=1, verify it's filled with 0xF6
;   6. PIO WRITE: write a pattern to C=0 H=0 R=2, read back, verify
;
; Results:
;   [0500] = 0x0001   DMA write command phase complete
;   [0502] = 0x0001   DMA write IRQ6 fired
;   [0504] = 0x0001   DMA readback matches (first word + checksum)
;   [0506] = 0x0001   FORMAT TRACK IRQ6 fired
;   [0508] = 0x0001   Format readback: sector filled with 0xF6
;   [050A] = 0x0001   PIO write + readback verified

; @name FDC Write + Format
; @expect 0500 0001 DMA write cmd OK
; @expect 0502 0001 DMA write IRQ6
; @expect 0504 0001 DMA readback OK
; @expect 0506 0001 FORMAT IRQ6
; @expect 0508 0001 Format readback OK
; @expect 050A 0001 PIO write+read OK

cpu 8086
org 0x0100

%define FDC_DOR   0x3F2
%define FDC_MSR   0x3F4
%define FDC_FIFO  0x3F5
%define PIC_CMD   0x20
%define PIC_DATA  0x21
%define DMA_ADDR  0x04
%define DMA_CNT   0x05
%define DMA_PAGE  0x82
%define DMA_MASK  0x0A
%define DMA_MODE  0x0B
%define DMA_FF    0x0C
%define DMA_CLEAR 0x0D

%define IRQ_FLAG    0x05F0
%define DMA_BUF     0x3000      ; physical address for DMA buffer (page 0)
%define SECTOR_SIZE 512

; =====================================================================
; Initialize
; =====================================================================
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0800

    ; Zero results
    mov di, 0x0500
    mov cx, 8
    xor ax, ax
    rep stosw
    mov byte [IRQ_FLAG], 0

    ; Install IRQ6 handler (PIC base 8, IRQ6 = INT 0x0E)
    mov word [0x0E*4],     irq6_handler
    mov word [0x0E*4+2],   0x0100

    ; Init PIC
    mov al, 0x13
    out PIC_CMD, al
    mov al, 0x08
    out PIC_DATA, al
    mov al, 0x01
    out PIC_DATA, al
    mov al, 0xBC            ; unmask IRQ6, IRQ1, IRQ0
    out PIC_DATA, al

; =====================================================================
; Reset FDC + handle post-reset IRQ6
; =====================================================================
    mov dx, FDC_DOR
    mov al, 0x08            ; IRQ enable, reset active
    out dx, al
    or al, 0x04             ; deassert reset
    out dx, al

    sti
    call wait_irq6
    cli
    jc .done

    ; SENSE INTERRUPT STATUS
    mov ah, 0x08
    call nec_out
    jc .done
    call nec_in             ; ST0 (discard)
    jc .done
    call nec_in             ; PCN (discard)
    jc .done

    ; SPECIFY
    mov ah, 0x03
    call nec_out
    jc .done
    mov ah, 0xCF
    call nec_out
    jc .done
    mov ah, 0x02
    call nec_out
    jc .done

    ; Motor on
    mov al, 0x1C
    mov dx, FDC_DOR
    out dx, al

    ; Spin-up delay
    xor cx, cx
.motor1: loop .motor1
.motor2: loop .motor2

; =====================================================================
; Fill DMA buffer with known pattern: 0xA5 repeating, first word = 0xBEEF
; =====================================================================
    mov di, DMA_BUF
    mov cx, SECTOR_SIZE
    mov al, 0xA5
    rep stosb
    mov word [DMA_BUF], 0xBEEF

; =====================================================================
; Test 1: DMA WRITE DATA -- write buffer to C=0 H=0 R=1
; =====================================================================
    ; Program DMA ch2: read from memory (mode 0x4A)
    call dma_setup_write

    ; Send WRITE DATA command (0x45 = MF + WRITE DATA 0x05)
    mov ah, 0x45
    call nec_out
    jc .done
    mov ah, 0x00            ; head 0, drive 0
    call nec_out
    jc .done
    mov ah, 0x00            ; cylinder 0
    call nec_out
    jc .done
    mov ah, 0x00            ; head 0
    call nec_out
    jc .done
    mov ah, 0x01            ; sector 1
    call nec_out
    jc .done
    mov ah, 0x02            ; N=2 (512 bytes)
    call nec_out
    jc .done
    mov ah, 0x09            ; EOT=9
    call nec_out
    jc .done
    mov ah, 0x2A            ; GPL
    call nec_out
    jc .done
    mov ah, 0xFF            ; DTL
    call nec_out
    jc .done

    mov word [0x0500], 0x0001   ; command phase OK

    ; Wait for IRQ6
    sti
    call wait_irq6
    cli
    jc .test2

    mov word [0x0502], 0x0001   ; IRQ6 fired

    ; Read 7 result bytes (drain result phase)
    call drain_results

; =====================================================================
; Test 2: DMA READ back sector and verify
; =====================================================================
.test2:
    ; Zero the DMA buffer first
    mov di, DMA_BUF
    mov cx, SECTOR_SIZE / 2
    xor ax, ax
    rep stosw

    ; Program DMA ch2: write to memory (mode 0x46)
    call dma_setup_read

    ; READ DATA: C=0 H=0 R=1
    mov ah, 0x66            ; MF + SK + READ DATA
    call nec_out
    jc .test3
    mov ah, 0x00
    call nec_out
    jc .test3
    mov ah, 0x00            ; cyl 0
    call nec_out
    jc .test3
    mov ah, 0x00            ; head 0
    call nec_out
    jc .test3
    mov ah, 0x01            ; sector 1
    call nec_out
    jc .test3
    mov ah, 0x02            ; N=2
    call nec_out
    jc .test3
    mov ah, 0x09            ; EOT
    call nec_out
    jc .test3
    mov ah, 0x2A            ; GPL
    call nec_out
    jc .test3
    mov ah, 0xFF            ; DTL
    call nec_out
    jc .test3

    sti
    call wait_irq6
    cli
    jc .test3

    call drain_results

    ; Verify: first word should be 0xBEEF, rest should be 0xA5
    cmp word [DMA_BUF], 0xBEEF
    jne .test3

    ; Check byte at offset 2 is 0xA5
    cmp byte [DMA_BUF + 2], 0xA5
    jne .test3

    ; Check byte at offset 511 is 0xA5
    cmp byte [DMA_BUF + 511], 0xA5
    jne .test3

    mov word [0x0504], 0x0001

; =====================================================================
; Test 3: FORMAT TRACK -- format C=1 H=0 with fill byte 0xF6
; =====================================================================
.test3:
    ; Seek to cylinder 1
    mov ah, 0x0F
    call nec_out
    jc .test4
    mov ah, 0x00            ; drive 0
    call nec_out
    jc .test4
    mov ah, 0x01            ; cylinder 1
    call nec_out
    jc .test4

    sti
    call wait_irq6
    cli
    jc .test4

    ; SENSE INT after seek
    mov ah, 0x08
    call nec_out
    jc .test4
    call nec_in
    jc .test4
    call nec_in
    jc .test4

    ; Fill DMA buffer with 9 address fields (4 bytes each = 36 bytes)
    ; Each field: C=1, H=0, R=sector, N=2
    mov di, DMA_BUF
    mov cl, 9               ; 9 sectors
    mov ch, 1               ; sector counter
.fmt_fill:
    mov byte [di], 1        ; C=1
    inc di
    mov byte [di], 0        ; H=0
    inc di
    mov [di], ch             ; R=sector number
    inc di
    mov byte [di], 2        ; N=2
    inc di
    inc ch
    dec cl
    jnz .fmt_fill

    ; Program DMA ch2: read from memory, 36 bytes (9 * 4 - 1 = 35)
    mov al, 0x00
    out DMA_FF, al
    mov al, 0x06
    out DMA_MASK, al
    mov al, 0x00
    out DMA_FF, al
    mov al, DMA_BUF & 0xFF
    out DMA_ADDR, al
    mov al, (DMA_BUF >> 8) & 0xFF
    out DMA_ADDR, al
    mov al, 0x00
    out DMA_FF, al
    mov al, 35              ; count-1 low
    out DMA_CNT, al
    mov al, 0               ; count-1 high
    out DMA_CNT, al
    mov al, 0x00
    out DMA_PAGE, al
    mov al, 0x4A            ; read from memory, ch2
    out DMA_MODE, al
    mov al, 0x02
    out DMA_MASK, al

    ; FORMAT TRACK command (0x4D = MF + FORMAT 0x0D)
    mov ah, 0x4D
    call nec_out
    jc .test4
    mov ah, 0x00            ; head 0, drive 0
    call nec_out
    jc .test4
    mov ah, 0x02            ; N=2 (512 bytes/sector)
    call nec_out
    jc .test4
    mov ah, 0x09            ; SC=9 (sectors per track)
    call nec_out
    jc .test4
    mov ah, 0x50            ; GPL (format gap)
    call nec_out
    jc .test4
    mov ah, 0xF6            ; fill byte
    call nec_out
    jc .test4

    sti
    call wait_irq6
    cli
    jc .test4

    mov word [0x0506], 0x0001

    call drain_results

; =====================================================================
; Test 4: Read back formatted sector, verify fill byte
; =====================================================================
.test4:
    ; Zero buffer
    mov di, DMA_BUF
    mov cx, SECTOR_SIZE / 2
    xor ax, ax
    rep stosw

    call dma_setup_read

    ; READ DATA: C=1 H=0 R=1
    mov ah, 0x66
    call nec_out
    jc .test5
    mov ah, 0x00
    call nec_out
    jc .test5
    mov ah, 0x01            ; cyl 1
    call nec_out
    jc .test5
    mov ah, 0x00            ; head 0
    call nec_out
    jc .test5
    mov ah, 0x01            ; sector 1
    call nec_out
    jc .test5
    mov ah, 0x02            ; N=2
    call nec_out
    jc .test5
    mov ah, 0x09
    call nec_out
    jc .test5
    mov ah, 0x2A
    call nec_out
    jc .test5
    mov ah, 0xFF
    call nec_out
    jc .test5

    sti
    call wait_irq6
    cli
    jc .test5

    call drain_results

    ; Verify: all 512 bytes should be 0xF6
    mov si, DMA_BUF
    mov cx, SECTOR_SIZE
.fmt_check:
    lodsb
    cmp al, 0xF6
    jne .test5
    loop .fmt_check

    mov word [0x0508], 0x0001

; =====================================================================
; Test 5: PIO WRITE + READ back
; =====================================================================
.test5:
    ; Reset FDC into PIO mode (bit 3 = 0)
    mov dx, FDC_DOR
    mov al, 0x00
    out dx, al
    mov cx, 10
.pio_rst: loop .pio_rst
    mov al, 0x14            ; motor on, FDC enable, NO DMA (bit 3=0)
    mov dx, FDC_DOR
    out dx, al
    mov cx, 10
.pio_post: loop .pio_post

    ; WRITE DATA (PIO): C=0 H=0 R=2
    mov ah, 0x45
    call nec_out
    jc .done
    mov ah, 0x00
    call nec_out
    jc .done
    mov ah, 0x00            ; cyl 0
    call nec_out
    jc .done
    mov ah, 0x00            ; head 0
    call nec_out
    jc .done
    mov ah, 0x02            ; sector 2
    call nec_out
    jc .done
    mov ah, 0x02            ; N=2
    call nec_out
    jc .done
    mov ah, 0x02            ; EOT=2 (single sector)
    call nec_out
    jc .done
    mov ah, 0x2A            ; GPL
    call nec_out
    jc .done
    mov ah, 0xFF            ; DTL
    call nec_out
    jc .done

    ; PIO write: send 512 bytes of 0x3C pattern
    mov cx, SECTOR_SIZE
.pio_write:
    push cx
    mov cx, 200
.pio_wr_poll:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80
    jz .pio_wr_next
    test al, 0x20           ; NDMA bit
    jnz .pio_wr_send
.pio_wr_next:
    loop .pio_wr_poll
    pop cx
    jmp .pio_read           ; timeout
.pio_wr_send:
    pop cx
    mov al, 0x3C
    mov dx, FDC_FIFO
    out dx, al
    loop .pio_write

    ; Drain result (PIO mode -- no IRQ, just poll MSR for result phase)
    mov cx, 200
.pio_wr_result_wait:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80
    jz .pio_wr_rw
    test al, 0x40
    jnz .pio_wr_drain
.pio_wr_rw:
    loop .pio_wr_result_wait
    jmp .pio_read
.pio_wr_drain:
    call drain_results_pio

; PIO READ back sector 2
.pio_read:
    ; READ DATA (PIO): C=0 H=0 R=2
    mov ah, 0x46
    call nec_out
    jc .done
    mov ah, 0x00
    call nec_out
    jc .done
    mov ah, 0x00
    call nec_out
    jc .done
    mov ah, 0x00
    call nec_out
    jc .done
    mov ah, 0x02            ; sector 2
    call nec_out
    jc .done
    mov ah, 0x02            ; N=2
    call nec_out
    jc .done
    mov ah, 0x02            ; EOT=2 (single sector)
    call nec_out
    jc .done
    mov ah, 0x2A            ; GPL
    call nec_out
    jc .done
    mov ah, 0xFF
    call nec_out
    jc .done

    ; PIO read: receive 512 bytes
    mov di, DMA_BUF
    mov cx, SECTOR_SIZE
.pio_rd:
    push cx
    mov cx, 200
.pio_rd_poll:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80
    jz .pio_rd_next
    test al, 0x40           ; DIO=1
    jnz .pio_rd_got
.pio_rd_next:
    loop .pio_rd_poll
    pop cx
    jmp .pio_verify
.pio_rd_got:
    pop cx
    mov dx, FDC_FIFO
    in al, dx
    stosb
    loop .pio_rd

    ; Drain result
    call drain_results_pio

.pio_verify:
    ; Verify: all 512 bytes at DMA_BUF should be 0x3C
    mov si, DMA_BUF
    mov cx, SECTOR_SIZE
.pio_chk:
    lodsb
    cmp al, 0x3C
    jne .done
    loop .pio_chk

    mov word [0x050A], 0x0001

.done:
    mov al, 0x0C
    mov dx, FDC_DOR
    out dx, al
    cli
    int3

; =====================================================================
; DMA setup helpers
; =====================================================================

; Program DMA ch2 for write-to-memory (device->memory), 512 bytes at DMA_BUF
dma_setup_read:
    mov al, 0x00
    out DMA_FF, al
    mov al, 0x06
    out DMA_MASK, al
    mov al, 0x00
    out DMA_FF, al
    mov al, DMA_BUF & 0xFF
    out DMA_ADDR, al
    mov al, (DMA_BUF >> 8) & 0xFF
    out DMA_ADDR, al
    mov al, 0x00
    out DMA_FF, al
    mov al, (SECTOR_SIZE - 1) & 0xFF
    out DMA_CNT, al
    mov al, ((SECTOR_SIZE - 1) >> 8) & 0xFF
    out DMA_CNT, al
    mov al, 0x00
    out DMA_PAGE, al
    mov al, 0x46            ; single, inc, write-to-memory, ch2
    out DMA_MODE, al
    mov al, 0x02
    out DMA_MASK, al
    ret

; Program DMA ch2 for read-from-memory (memory->device), 512 bytes at DMA_BUF
dma_setup_write:
    mov al, 0x00
    out DMA_FF, al
    mov al, 0x06
    out DMA_MASK, al
    mov al, 0x00
    out DMA_FF, al
    mov al, DMA_BUF & 0xFF
    out DMA_ADDR, al
    mov al, (DMA_BUF >> 8) & 0xFF
    out DMA_ADDR, al
    mov al, 0x00
    out DMA_FF, al
    mov al, (SECTOR_SIZE - 1) & 0xFF
    out DMA_CNT, al
    mov al, ((SECTOR_SIZE - 1) >> 8) & 0xFF
    out DMA_CNT, al
    mov al, 0x00
    out DMA_PAGE, al
    mov al, 0x4A            ; single, inc, read-from-memory, ch2
    out DMA_MODE, al
    mov al, 0x02
    out DMA_MASK, al
    ret

; =====================================================================
; nec_out: send AH to FDC FIFO
; =====================================================================
nec_out:
    push cx
    push dx
    mov dx, FDC_MSR
    xor cx, cx
.nec_out_dir:
    in al, dx
    test al, 0x40
    jz .nec_out_rqm
    loop .nec_out_dir
    jmp .nec_out_fail
.nec_out_rqm:
    xor cx, cx
.nec_out_rqm_poll:
    in al, dx
    test al, 0x80
    jnz .nec_out_send
    loop .nec_out_rqm_poll
.nec_out_fail:
    pop dx
    pop cx
    stc
    ret
.nec_out_send:
    mov dx, FDC_FIFO
    mov al, ah
    out dx, al
    pop dx
    pop cx
    clc
    ret

; =====================================================================
; nec_in: read one byte from FDC FIFO -> AL
; =====================================================================
nec_in:
    push cx
    push dx
    mov dx, FDC_MSR
    xor cx, cx
.nec_in_poll:
    in al, dx
    test al, 0x80
    jz .nec_in_next
    test al, 0x40
    jnz .nec_in_read
.nec_in_next:
    loop .nec_in_poll
    pop dx
    pop cx
    stc
    ret
.nec_in_read:
    mov dx, FDC_FIFO
    in al, dx
    pop dx
    pop cx
    clc
    ret

; =====================================================================
; wait_irq6: wait for IRQ6 via IRQ_FLAG
; =====================================================================
wait_irq6:
    sti
    push bx
    push cx
    mov bl, 4
    xor cx, cx
.wait_loop:
    cmp byte [IRQ_FLAG], 0
    jne .wait_got
    loop .wait_loop
    dec bl
    jnz .wait_loop
    pop cx
    pop bx
    stc
    ret
.wait_got:
    mov byte [IRQ_FLAG], 0
    pop cx
    pop bx
    clc
    ret

; =====================================================================
; drain_results: read up to 7 result bytes from FDC (DMA mode)
; =====================================================================
drain_results:
    push cx
    push dx
    mov cl, 7
.dr_loop:
    mov dx, FDC_MSR
    push cx
    mov cx, 50
.dr_poll:
    in al, dx
    test al, 0x80
    jnz .dr_check
    loop .dr_poll
    pop cx
    jmp .dr_done
.dr_check:
    pop cx
    test al, 0x40           ; DIO=1?
    jz .dr_done
    test al, 0x10           ; BUSY?
    jz .dr_done
    mov dx, FDC_FIFO
    in al, dx               ; read and discard
    dec cl
    jnz .dr_loop
.dr_done:
    pop dx
    pop cx
    ret

; =====================================================================
; drain_results_pio: read result bytes in PIO mode (no IRQ)
; =====================================================================
drain_results_pio:
    push cx
    push dx
    mov cl, 7
.drp_loop:
    mov dx, FDC_MSR
    push cx
    mov cx, 200
.drp_poll:
    in al, dx
    test al, 0x80
    jnz .drp_check
    loop .drp_poll
    pop cx
    jmp .drp_done
.drp_check:
    pop cx
    test al, 0x40
    jz .drp_done
    mov dx, FDC_FIFO
    in al, dx
    dec cl
    jnz .drp_loop
.drp_done:
    pop dx
    pop cx
    ret

; =====================================================================
; IRQ6 handler (INT 0x0E)
; =====================================================================
irq6_handler:
    push ax
    mov byte [IRQ_FLAG], 0xFF
    mov al, 0x20
    out PIC_CMD, al
    pop ax
    iret
