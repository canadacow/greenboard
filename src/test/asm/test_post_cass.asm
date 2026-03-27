; test_post_cass.asm -- BIOS POST TEST.13: Cassette data wrap test
; Faithful reproduction of the real IBM PC BIOS TEST.13 from PCBIOS.ASM.
;
; On the real 5150, timer 2 output goes through U63 (NAND gate), R8,
; the cassette relay/connector path, and the LM339 comparator (U1) back
; to PC4 (CASS_DATA_IN). The net effect is PC4 follows T/C_2_OUT.
;
; The test:
;   1. Turn cassette motor off (PB = 0x4D)
;   2. Mask all interrupts
;   3. Program timer 2: mode 3, LSB+MSB, count=1235 (1000us square wave)
;   4. Read PC4 initial value, save as LAST_VAL
;   5. Call READ_HALF_BIT twice (wait for two transitions, measure period)
;   6. Verify CX != 0 (no timeout) and BX in [MIN_PERIOD, MAX_PERIOD)
;
; Loaded at 0100:0100 (physical 0x01100). DS=0 after reset.
;
; Expected results:
;   [0500] = 0x0001   Cassette wrap test passed

; @name POST Cassette (TEST.13)
; @expect 0500 0001 Cassette wrap
;
cpu 8086
org 0x0100

%define PORT_B     0x61
%define PORT_C     0x62
%define TIMER0     0x40
%define TIMER2     0x42
%define TIM_CTL    0x43
%define INTA01     0x21
%define MAX_PERIOD 0x0540
%define MIN_PERIOD 0x0410

; =====================================================================
; Initialize
; =====================================================================
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x0800
    mov word [0x0500], 0x0000

    ; Initialize PPI
    mov al, 0x99
    out 0x63, al

    ; Zero working variables
    mov word [0x05E0], 0        ; EDGE_CNT
    mov byte [0x05E2], 0        ; LAST_VAL

; =====================================================================
; BIOS TEST.13: Cassette data wrap (lines 1053-1082)
; =====================================================================

    ; Turn cassette motor off, gate timer 2 to speaker
    mov al, 0x4D            ; PB0=1, PB2=1, PB3=1, PB6=1
    out PORT_B, al

    ; Mask all device interrupts
    mov al, 0xFF
    out INTA01, al

    ; Program timer 0 as free-running reference counter (mode 3, count=0).
    ; On real POST, this is done between TEST.08 and TEST.12 (line 708).
    ; READ_HALF_BIT latches timer 0 to measure the cassette signal period.
    mov al, 0x36            ; sel timer 0, LSB+MSB, mode 3
    out TIM_CTL, al
    xor al, al
    out TIMER0, al          ; LSB = 0
    out TIMER0, al          ; MSB = 0 (count = 65536)

    ; Program timer 2: mode 3 (square wave), LSB+MSB, count=1235
    mov al, 0xB6            ; sel timer 2, LSB+MSB, mode 3
    out TIM_CTL, al
    mov ax, 1235
    out TIMER2, al          ; LSB
    mov al, ah
    out TIMER2, al          ; MSB

    ; Read initial cassette data in (PC4, bit 4 of Port C)
    in al, PORT_C
    and al, 0x10            ; isolate bit 4
    mov [0x05E2], al        ; LAST_VAL

    ; Read two half-bits (two transitions)
    call read_half_bit
    call read_half_bit

    ; Check results: CX must be non-zero (no timeout)
    jcxz .fail
    ; BX must be < MAX_PERIOD
    cmp bx, MAX_PERIOD
    jnc .fail
    ; BX must be >= MIN_PERIOD
    cmp bx, MIN_PERIOD
    jnc .pass

.fail:
    jmp .done

.pass:
    mov word [0x0500], 0x0001

.done:
    int3

; =====================================================================
; READ_HALF_BIT -- PCBIOS.ASM lines 5271-5300
;
; Waits for PC4 to toggle, then latches timer 0 to measure the period.
;
; On entry: EDGE_CNT [0x05E0] = last edge timer value
; On exit:  BX = pulse width (half-bit period)
;           AX = old edge count
;           CX = 0 if timeout (no transition detected)
;           LAST_VAL [0x05E2] updated
; =====================================================================
read_half_bit:
    mov cx, 100             ; timeout counter
    mov ah, [0x05E2]        ; get present input value (LAST_VAL)
.rd_h_bit:
    in al, PORT_C           ; read data bit
    and al, 0x10            ; isolate bit 4
    cmp al, ah              ; same as before?
    loope .rd_h_bit         ; loop till it changes or timeout
    mov [0x05E2], al        ; update LAST_VAL

    ; Latch and read timer 0 counter (free-running reference)
    mov al, 0x00            ; latch timer 0 count
    out TIM_CTL, al
    in al, TIMER0           ; get LSB
    mov ah, al
    in al, TIMER0           ; get MSB
    xchg al, ah             ; AX = timer count (MSB:LSB)

    mov bx, [0x05E0]        ; BX = last edge count (EDGE_CNT)
    sub bx, ax              ; BX = half-bit period
    mov [0x05E0], ax        ; update EDGE_CNT
    ret
