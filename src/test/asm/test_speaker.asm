; test_speaker.asm -- PC Speaker via PIT Channel 2 + PPI Port B
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; The IBM PC 5150 speaker path:
;   PPI PB0 (port 0x61 bit 0) = GATE for PIT Channel 2
;   PPI PB1 (port 0x61 bit 1) = SPKR_DATA (direct speaker drive)
;   PIT CH2 (port 0x42) = tone frequency generator
;   PIT CH2 OUT -> PPI PC5 (port 0x62 bit 5, readable)
;   U63 Gate 4: NAND(SPKR_DATA, T/C_2_OUT) -> speaker output
;
; Test plan:
;   1. Init PPI (0x99): Port A=in, Port B=out, Port C upper=in
;   2. Verify PIT CH2 is gated off (PB0=0): OUT2 should stay Low
;   3. Program PIT CH2 mode 3 (square wave), count=100
;   4. Enable gate (PB0=1) and speaker data (PB1=1)
;   5. Count transitions of PC5 (port 0x62 bit 5) over N iterations
;   6. Disable speaker (PB0=0, PB1=0)
;   7. Verify transition count is reasonable
;
; At 1.193 MHz CLK with count=100, the square wave frequency is ~11.93 kHz.
; Each half-period = 50 CLK ticks. Over 2000 reads (~10000 CLK ticks),
; we expect ~100 transitions.
;
; Results:
;   [0500] = 0x0001   PIT CH2 gated off: PC5 stays Low before enable
;   [0502] = transition count (expect > 20, verifies square wave)
;   [0504] = 0x0001   Transitions stopped after gate disabled

; @name Speaker (PIT CH2 + PPI)
; @expect 0500 0001 PIT CH2 gated off (PC5 Low)
; @expect 0504 0001 No transitions after disable
; @dump 0502 Transition count while enabled
;
cpu 8086
org 0x0100

PPI_A   equ 0x60
PPI_B   equ 0x61
PPI_C   equ 0x62
PPI_CMD equ 0x63
PIT_CH2 equ 0x42
PIT_CMD equ 0x43

mov ax, 0x0000
mov ds, ax
mov ss, ax
mov sp, 0x0800

; Zero results
mov word [0x0500], 0x0000
mov word [0x0502], 0x0000
mov word [0x0504], 0x0000

; =====================================================================
; Init PPI: Port A=input, Port B=output, Port C upper=input, lower=output
; =====================================================================
mov al, 0x99
out PPI_CMD, al

; Speaker off: PB0=0 (gate off), PB1=0 (SPKR_DATA off)
mov al, 0x00
out PPI_B, al

; =====================================================================
; Test 1: PIT CH2 gated off -- PC5 should be Low
; =====================================================================
; Read port C, check bit 5
in al, PPI_C
test al, 0x20               ; bit 5 = T/C_2_OUT
jnz .gate_test_fail
mov word [0x0500], 0x0001   ; pass: PC5 is Low when gate off
.gate_test_fail:

; =====================================================================
; Program PIT Channel 2: mode 3 (square wave), count=100
; =====================================================================
mov al, 0xB6               ; CH2, lobyte/hibyte, mode 3, binary
out PIT_CMD, al
mov al, 100                 ; count low byte
out PIT_CH2, al
mov al, 0                   ; count high byte
out PIT_CH2, al

; =====================================================================
; Test 2: Enable speaker gate and data, count PC5 transitions
; =====================================================================
; PB0=1 (gate on), PB1=1 (SPKR_DATA on)
mov al, 0x03
out PPI_B, al

; Count transitions of PC5 over 2000 reads
xor dx, dx                  ; transition count
in al, PPI_C
and al, 0x20
mov bl, al                  ; BL = previous PC5 state
mov cx, 2000

.count_loop:
    in al, PPI_C
    and al, 0x20
    cmp al, bl
    je .no_transition
    mov bl, al              ; update previous state
    inc dx                  ; count transition
.no_transition:
    loop .count_loop

mov [0x0502], dx            ; store transition count

; =====================================================================
; Test 3: Disable speaker, verify PC5 stops toggling
; =====================================================================
; PB0=0 (gate off), PB1=0 (SPKR_DATA off)
mov al, 0x00
out PPI_B, al

; Count transitions over 500 reads -- should be zero (output frozen)
xor dx, dx
in al, PPI_C
and al, 0x20
mov bl, al
mov cx, 500
.disabled_loop:
    in al, PPI_C
    and al, 0x20
    cmp al, bl
    je .no_trans2
    mov bl, al
    inc dx
.no_trans2:
    loop .disabled_loop

; Pass if zero transitions after disable
test dx, dx
jnz .disable_fail
mov word [0x0504], 0x0001
.disable_fail:

hlt
