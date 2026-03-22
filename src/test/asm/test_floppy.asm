; test_floppy.asm -- Floppy Disk Controller (NEC uPD765 / Intel 8272A)
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Tests low-level FDC access via DMA channel 2 and IRQ 6.
; The IBM PC 5150 FDC uses:
;   - I/O ports 0x3F0-0x3F7 (primary controller)
;   - DMA channel 2 for data transfer
;   - IRQ 6 (INT 14 with vector base 8) for completion
;
; FDC registers:
;   0x3F2  DOR   Digital Output Register (write)
;                   Bits 0-1: drive select (0=A)
;                   Bit 2:    FDC enable (1=active, 0=reset)
;                   Bit 3:    DMA/IRQ enable
;                   Bits 4-7: motor on for drives A-D
;   0x3F4  MSR   Main Status Register (read)
;                   Bit 7: RQM  (1=data register ready)
;                   Bit 6: DIO  (1=FDC->CPU, 0=CPU->FDC)
;                   Bit 5: NDMA (non-DMA execution mode)
;                   Bit 4: BUSY (command in progress)
;                   Bits 0-3: drive seeking flags
;   0x3F5  FIFO  Data Register (read/write)
;                   Command bytes, parameters, result bytes
;
; Read Data command (9 bytes):
;   Byte 0: 0x46  (MFM=1, skip-deleted=1, READ DATA=0x06)
;   Byte 1: (head << 2) | drive   (head 0, drive 0 = 0x00)
;   Byte 2: cylinder               (0)
;   Byte 3: head                    (0)
;   Byte 4: sector                  (1, 1-based)
;   Byte 5: sector size code        (2 = 512 bytes)
;   Byte 6: end of track            (18 for 1.44M, 9 for 360K)
;   Byte 7: gap length              (0x1B for 3.5", 0x2A for 5.25")
;   Byte 8: data length             (0xFF when sector size != 0)
;
; Test plan:
;   1. Initialize PIC for IRQ 6 (INT 14)
;   2. Set up DMA channel 2 for 512-byte write-to-memory
;   3. Reset FDC via DOR (toggle bit 2)
;   4. Read MSR -- check if FDC responds (RQM bit)
;   5. Enable motor + FDC + DMA via DOR
;   6. Poll MSR, attempt to send READ DATA command
;   7. Wait for IRQ 6 or timeout
;   8. Check DMA buffer for sector data
;
; Expected results (require working FDC + drive + disk):
;   [0500] = 0x0080   MSR after reset (RQM=1, ready for commands)
;   [0502] = 0x0001   Command phase completed (all 9 bytes accepted)
;   [0504] = 0x0001   IRQ 6 fired (execution phase complete)
;   [0506] = 0x0001   DMA buffer has non-zero data (sector read)

; @name Floppy (FDC read sector)
; @expect 0500 0080 MSR shows RQM after reset
; @expect 0502 0001 Command phase complete
; @expect 0504 0001 IRQ 6 fired
; @expect 0506 0001 DMA buffer non-zero
;
cpu 8086
org 0x0100

; DMA channel 2 ports
FDC_DMA_ADDR equ 0x04      ; channel 2 base address (byte-flipped)
FDC_DMA_CNT  equ 0x05      ; channel 2 word count (byte-flipped)
FDC_DMA_PAGE equ 0x81      ; channel 2 page register
DMA_MASK     equ 0x0A      ; single mask register
DMA_MODE     equ 0x0B      ; mode register
DMA_FLIPFLOP equ 0x0C      ; clear byte pointer flip-flop
DMA_MASTER   equ 0x0D      ; master clear

; FDC I/O ports
FDC_DOR  equ 0x3F2         ; Digital Output Register
FDC_MSR  equ 0x3F4         ; Main Status Register
FDC_FIFO equ 0x3F5         ; Data / FIFO Register

; Buffer address for DMA transfer (physical 0x03000, page 0, offset 0x3000)
DMA_BUF_PAGE   equ 0x00
DMA_BUF_OFFSET equ 0x3000
DMA_BUF_LEN    equ 512     ; one sector

; =====================================================================
; Initialize
; =====================================================================
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

; Zero DMA target buffer
mov di, DMA_BUF_OFFSET
mov cx, DMA_BUF_LEN / 2
xor ax, ax
rep stosw

; =====================================================================
; Install IRQ 6 handler (INT 14 with PIC vector base 8)
; =====================================================================
mov word [14*4],   irq6_handler
mov word [14*4+2], 0x0100      ; CS = 0x0100

; Initialize PIC: edge-triggered, single, ICW4 needed
mov al, 0x13                    ; ICW1: edge, single, ICW4
out 0x20, al
mov al, 0x08                    ; ICW2: vector base = 8
out 0x21, al
mov al, 0x01                    ; ICW4: 8086 mode, normal EOI
out 0x21, al
mov al, 0xBF                    ; OCW1: unmask IRQ6 (clear bit 6)
out 0x21, al

; =====================================================================
; Set up DMA channel 2 for floppy read (device -> memory = write mode)
; =====================================================================
; Master clear
mov al, 0x00
out DMA_MASTER, al

; Mask channel 2 while programming
mov al, 0x06                    ; set mask bit for channel 2
out DMA_MASK, al

; Clear flip-flop
out DMA_FLIPFLOP, al

; Set DMA address (low byte first, then high byte)
mov al, DMA_BUF_OFFSET & 0xFF
out FDC_DMA_ADDR, al
mov al, (DMA_BUF_OFFSET >> 8) & 0xFF
out FDC_DMA_ADDR, al

; Clear flip-flop for count
mov al, 0x00
out DMA_FLIPFLOP, al

; Set DMA count (length - 1, low byte first)
mov al, (DMA_BUF_LEN - 1) & 0xFF
out FDC_DMA_CNT, al
mov al, ((DMA_BUF_LEN - 1) >> 8) & 0xFF
out FDC_DMA_CNT, al

; Set page register
mov al, DMA_BUF_PAGE
out FDC_DMA_PAGE, al

; Set mode: single transfer, address increment, write (to memory), ch 2
; Bits: 01 (single) 0 (inc) 0 (no auto) 01 (write) 10 (ch2) = 0x46
mov al, 0x46
out DMA_MODE, al

; Unmask channel 2
mov al, 0x02                    ; clear mask bit for channel 2
out DMA_MASK, al

; =====================================================================
; Reset FDC: toggle bit 2 of DOR (0=reset, 1=enable)
; =====================================================================
mov dx, FDC_DOR
mov al, 0x00                    ; FDC reset: all bits 0
out dx, al

; Brief delay for reset to take effect
mov cx, 10
.reset_delay:
    nop
    loop .reset_delay

mov dx, FDC_DOR
mov al, 0x0C                    ; FDC enable + DMA/IRQ enable (no motor yet)
out dx, al

; Brief delay after coming out of reset
mov cx, 10
.post_reset_delay:
    nop
    loop .post_reset_delay

; =====================================================================
; Test 1: Read MSR -- check RQM bit
; =====================================================================
mov dx, FDC_MSR
in al, dx
xor ah, ah
mov [0x0500], ax                ; store raw MSR value

; Check RQM (bit 7) -- if not set, FDC not present, skip command phase
test al, 0x80
jz .no_fdc

; =====================================================================
; Enable motor for drive A
; =====================================================================
mov dx, FDC_DOR
mov al, 0x1C                    ; motor A on + FDC enable + DMA enable + drive A
out dx, al

; Motor spin-up delay (abbreviated for simulation)
mov cx, 100
.motor_delay:
    nop
    loop .motor_delay

; =====================================================================
; Test 2: Send READ DATA command (9 bytes) to FIFO
; =====================================================================
; read_cmd is at org-relative offset within CS=0x0100.  Set DS=CS so
; LODSB can reach the table directly.
push ds
mov ax, 0x0100
mov ds, ax
mov si, read_cmd                ; DS:SI -> command table
mov cx, 9

.send_cmd:
    ; Poll MSR: wait for RQM=1, DIO=0 (ready to accept data)
    push cx
    mov cx, 50                  ; timeout counter
.poll_msr:
    mov dx, FDC_MSR
    in al, dx
    test al, 0x80               ; RQM set?
    jnz .rqm_ready
    loop .poll_msr
    pop cx
    jmp .cmd_timeout            ; MSR never became ready
.rqm_ready:
    pop cx                      ; restore command byte counter
    test al, 0x40               ; DIO=0 means CPU->FDC direction
    jnz .cmd_timeout_ds         ; wrong direction = error

    ; Send next command byte
    lodsb                       ; AL = [DS:SI++]
    mov dx, FDC_FIFO
    out dx, al
    loop .send_cmd

    ; All 9 bytes sent
    pop ds                      ; restore DS=0
    mov word [0x0502], 0x0001
    jmp .wait_irq

.cmd_timeout_ds:
.cmd_timeout:
    pop ds                      ; restore DS=0
    jmp .wait_irq

; =====================================================================
; Test 3: Wait for IRQ 6 (execution phase completion)
; =====================================================================
.wait_irq:
    sti
    mov cx, 0xFFFF              ; timeout
.irq_wait_loop:
    cmp word [0x0504], 0x0001   ; IRQ handler sets this
    je .irq_done
    nop
    loop .irq_wait_loop
.irq_done:
    cli

; =====================================================================
; Test 4: Check DMA buffer for non-zero data
; =====================================================================
.no_fdc:
    mov si, DMA_BUF_OFFSET
    mov cx, DMA_BUF_LEN
    xor dx, dx                  ; accumulator
.check_buf:
    lodsb
    or dl, al
    loop .check_buf

    test dl, dl
    jz .buf_empty
    mov word [0x0506], 0x0001   ; buffer has data
.buf_empty:

    hlt

; =====================================================================
; READ DATA command bytes (loaded via CS-relative LODSB)
; =====================================================================
read_cmd:
    db 0x46                     ; MFM + skip deleted + READ DATA
    db 0x00                     ; head 0, drive 0
    db 0x00                     ; cylinder 0
    db 0x00                     ; head 0
    db 0x01                     ; sector 1 (1-based)
    db 0x02                     ; 512 bytes/sector
    db 0x09                     ; end of track (9 sectors for 360K)
    db 0x2A                     ; gap length (5.25" standard)
    db 0xFF                     ; data length (unused when size=2)

; =====================================================================
; IRQ 6 handler (INT 14)
; =====================================================================
irq6_handler:
    push ax
    mov word [0x0504], 0x0001   ; flag: IRQ 6 fired
    ; Clear IRQ6 line on ISA test card
    mov al, 0x40                ; bit 6
    out 0xF1, al
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    pop ax
    iret
