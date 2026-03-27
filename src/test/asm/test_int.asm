; test_int.asm -- software interrupts, INT 3, INTO, IRET
; Loaded at F000:0123 (physical 0xF0123). DS=0 after reset.
; SP initialized to 0x0800 (stack at DS:0800 growing down).
;
; The IVT is at 0x0000-0x03FF. Each vector is 4 bytes: offset, segment.
; We install our own handlers into the IVT, invoke them, and check results.
;
; Expected results:
;   [0500] = 0xAA55   INT 0x40 -- handler ran and wrote marker
;   [0502] = 0x0001   INT 3 (breakpoint) -- handler ran
;   [0504] = 0x0001   INTO with OF=1 -- handler ran
;   [0506] = 0x0000   INTO with OF=0 -- handler did NOT run (stays 0)
;   [0508] = 0x0001   IRET restores flags (IF was 1 before INT, 0 during handler)
;   [050A] = 0x0003   Nested INT: handler calls INT again

; @name Interrupts
; @expect 0500 AA55 INT 0x40
; @expect 0502 0001 INT 3 (breakpoint)
; @expect 0504 0001 INTO (OF=1)
; @expect 0506 0000 INTO (OF=0, skip)
; @expect 0508 0001 IRET restores IF
; @expect 050A 0003 nested INT
;
cpu 8086
org 0x0100

; Set up stack
mov ax, 0x0000
mov ss, ax
mov sp, 0x0800

; Zero result area
mov word [0x0500], 0x0000
mov word [0x0502], 0x0000
mov word [0x0504], 0x0000
mov word [0x0506], 0x0000
mov word [0x0508], 0x0000
mov word [0x050A], 0x0000

; =====================================================================
; Install IVT entries.
; All handlers live in segment 0100. Offsets are relative to org 0x0100
; so the handler address = org_offset.
; =====================================================================

; INT 0x40 -> handler_40
mov word [0x40*4], handler_40
mov word [0x40*4+2], 0x0100

; INT 3 -> handler_bp
mov word [0x03*4], handler_bp
mov word [0x03*4+2], 0x0100

; INT 4 (overflow) -> handler_ovf
mov word [0x04*4], handler_ovf
mov word [0x04*4+2], 0x0100

; INT 0x41 -> handler_41 (for nested test)
mov word [0x41*4], handler_41
mov word [0x41*4+2], 0x0100

; INT 0x42 -> handler_42 (inner handler for nested test)
mov word [0x42*4], handler_42
mov word [0x42*4+2], 0x0100

; =====================================================================
; Test 1: INT 0x40 -- basic software interrupt
; =====================================================================
int 0x40
; handler_40 writes 0xAA55 to [0x0500]

; =====================================================================
; Test 2: INT 3 -- breakpoint (single-byte 0xCC opcode)
; =====================================================================
int 3
; handler_bp writes 0x0001 to [0x0502]

; =====================================================================
; Test 3: INTO with OF=1 -- should fire INT 4
; =====================================================================
; Force overflow: 0x7FFF + 1 = 0x8000 (signed overflow)
mov ax, 0x7FFF
add ax, 1               ; OF=1
into                     ; should fire INT 4
; handler_ovf writes 0x0001 to [0x0504]

; =====================================================================
; Test 4: INTO with OF=0 -- should NOT fire
; =====================================================================
; Clear overflow with a harmless operation
mov ax, 0x0001
add ax, 0x0001           ; 1+1=2, OF=0
into                     ; should NOT fire
; [0x0506] stays 0x0000

; =====================================================================
; Test 5: IRET restores flags
; INT clears IF. After IRET, IF should be restored.
; =====================================================================
sti                      ; IF=1
int 0x40                 ; INT clears IF; handler_40 already ran, fine
; After IRET, IF should be 1 again.
pushf
pop ax
and ax, 0x0200           ; isolate IF (bit 9)
mov cl, 9
shr ax, cl               ; AX = 0 or 1
mov [0x0508], ax         ; expect 0x0001

; =====================================================================
; Test 6: Nested interrupts
; handler_41 increments [0x050A] then calls INT 0x42
; handler_42 increments [0x050A] then calls INT 0x40 (which adds 0 this time)
; We pre-patch handler_40 result to just increment [0x050A]
; Actually simpler: handler_41 adds 1, calls INT 0x42. handler_42 adds 2.
; =====================================================================
; Re-point INT 0x40 to a "add 1" handler for nesting
mov word [0x40*4], handler_inc
mov word [0x40*4+2], 0x0100
int 0x41
; handler_41: [050A]+=1, then INT 0x42
; handler_42: [050A]+=1, then INT 0x40
; handler_inc: [050A]+=1
; total = 3

int3

; =====================================================================
; Interrupt handlers
; =====================================================================

handler_40:
    mov word [0x0500], 0xAA55
    iret

handler_bp:
    mov word [0x0502], 0x0001
    iret

handler_ovf:
    mov word [0x0504], 0x0001
    iret

handler_41:
    add word [0x050A], 1
    int 0x42
    iret

handler_42:
    add word [0x050A], 1
    int 0x40
    iret

handler_inc:
    add word [0x050A], 1
    iret
