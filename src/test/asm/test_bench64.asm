; test_bench64.asm -- 64-bit increment benchmark.
;
; Loaded at F000:0123 (physical 0xF0123). DS=SS=0 after reset.
;
; Increments a 64-bit counter at [0x0500]-[0x0507] in a tight loop.
; The test harness fires NMI after a wall-time delay to halt execution.
;
; NMI handler (INT 2) simply halts. The counter is already in memory.
;
; Results:
;   [0500] = low  word of 64-bit count
;   [0502] = mid-low word
;   [0504] = mid-high word
;   [0506] = high word

cpu 8086
org 0x0123

; Install NMI handler (INT 2) at vector 0x0008.
    xor ax, ax
    mov ds, ax
    mov word [0x0008], nmi_handler
    mov word [0x000A], 0xF000

; Zero the 64-bit counter.
    mov word [0x0500], 0
    mov word [0x0502], 0
    mov word [0x0504], 0
    mov word [0x0506], 0

; Enable interrupts (this 8088 impl requires IF=1 for NMI).
    sti

; Tight increment loop -- writes to memory every iteration.
.loop:
    add word [0x0500], 1
    adc word [0x0502], 0
    adc word [0x0504], 0
    adc word [0x0506], 0
    jmp .loop

nmi_handler:
    hlt
