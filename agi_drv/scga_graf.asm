; scga_graf.asm -- Sierra AGI v2 CGA driver targeting the SCGA TSR.
;
; Requires SCGA.COM loaded first: it provides INT 10h video mode 8
; (160x200x16 on CGA via ANSI-from-Hell text mode) plus the private
; services this driver uses:
;   AH=F8h fill rect      (DrawWndw)
;   AH=FAh AGI blit       (blk2Scrn: ES=src seg, DL=x, DH=picReloc,
;                          AL=bottom y, BL=w, BH=h)
; Text (status line, input, menus, dialogs) is handled entirely by the
; TSR through the interpreter's normal INT 10h calls, including XOR
; (INVERSE 8Fh) text.  Mode changes deactivate/reactivate the TSR
; automatically, so text screens and quit are lifecycle-clean.
;
; Same overlay ABI as the stock driver (see cga_graf.asm disasm):
;   +00 initgrfx  +03 textscrn  +06 grafscrn  +09 sethorz
;   +0C blk2Scrn  +0F initrows  +12 DrawWndw
;   +15 pcxc (identity)  +37 pcxo (identity)
; Interpreter (SQ2 build): BackToScrn=0xBD46; DS vars: s_ram=0x136F,
; picReloc=0x1379, row_table=0x137B, hsync 0x1365/66/67.
;
; Assemble:  nasm -f bin -o cga_graf.ovl scga_graf.asm

BITS 16
CPU 8086
ORG 0

BACKTOSCRN   equ 0xBD46
HSYNC_GFX    equ 0x1365
HSYNC_TXT    equ 0x1366
HSYNC_DELTA  equ 0x1367
BUF_SEG      equ 0x136F      ; s_ram
PIC_RELOC    equ 0x1379
ROW_TABLE    equ 0x137B

; ---------------- jump table (fixed ABI) ----------------
    jmp near initgrfx        ; +0x00
    jmp near textscrn        ; +0x03
    jmp near grafscrn        ; +0x06
    jmp near sethorz         ; +0x09
    jmp near blk2scrn        ; +0x0C
    jmp near initrows        ; +0x0F
    jmp near drawwndw        ; +0x12

; +0x15 pcxc: color translate -- identity (16-color indices stay)
pcxc:
    mov  ah,al
    ret

    times 0x37-($-$$) db 0x90

; +0x37 pcxo: view-cel remap -- identity
pcxo:
    ret

; ---------------- entry 0: enter graphics mode ----------------
initgrfx:
    push si
    push di
    push bp
    mov  ax,0x0008           ; SCGA mode 8
    int  0x10
    mov  word [HSYNC_GFX],0x5A5A   ; CRTC R2 base for the 80-col mode
    call sethorz
    pop  bp
    pop  di
    pop  si
    ret

; ---------------- entry 2: graphics + full redraw ----------------
grafscrn:
    call initgrfx            ; preserves SI/DI/BP
    call BACKTOSCRN          ; interpreter re-blits via blk2Scrn
    ret

; ---------------- entry 1: text mode (40x25) ----------------
textscrn:
    push si
    push di
    push bp
    mov  ax,0x0001           ; mode 1: TSR deactivates, BIOS text
    int  0x10
    push ds
    mov  ax,0x40
    mov  ds,ax
    mov  al,0x08
    mov  [0x65],al
    mov  dx,0x3D8
    out  dx,al
    pop  ds
    mov  byte [HSYNC_TXT],0x2D
    call sethorz
    mov  ah,1                ; hide cursor
    mov  cx,0x1000
    int  0x10
    pop  bp
    pop  di
    pop  si
    ret

; ---------------- entry 3: CRTC hsync position (shake) ----------
sethorz:
    mov  ah,0x0F
    int  0x10
    xor  bx,bx
    cmp  al,4                ; mode 8 -> graphics var
    jc   .txt
    mov  bl,[HSYNC_GFX]
    add  bx,[HSYNC_DELTA]
    mov  [HSYNC_GFX],bl
    jmp  .prog
.txt:
    mov  bl,[HSYNC_TXT]
    add  bx,[HSYNC_DELTA]
    mov  [HSYNC_TXT],bl
.prog:
    mov  word [HSYNC_DELTA],0
    mov  dx,0x3D4
    mov  al,2
    out  dx,al
    inc  dx
    mov  al,bl
    out  dx,al
    ret

; ---------------- entry 5: one-time table init ----------------
initrows:
    push di
    mov  di,ROW_TABLE
    xor  ax,ax
    cmp  ax,[es:di]          ; one-shot guard (as original)
    je   .done
    mov  word [HSYNC_GFX],0x5A5A
    mov  cx,100
.t: stosw                    ; row_table[y] = (y>>1)*160
    stosw
    add  ax,160
    loop .t
.done:
    pop  di
    ret

; ---------------- entry 4: blit rect shadow -> screen ----------------
; AGI: AH=x, AL=bottom y, BL=w, BH=h  ->  TSR AH=FAh
blk2scrn:
    push ax
    push bx
    push cx
    push dx
    push es
    mov  dl,ah               ; x
    mov  dh,[PIC_RELOC]      ; vertical placement
    mov  es,[BUF_SEG]        ; source = interpreter shadow ram
    mov  ah,0xFA
    int  0x10
    pop  es
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; ---------------- entry 6: draw window fill ----------------
; AGI: AH=x, AL=bottom y, BL=w, BH=h, DL=color  ->  TSR AH=F8h
drawwndw:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov  cl,ah
    xor  ch,ch               ; CX = x
    push dx                  ; save color
    mov  dl,bl
    xor  dh,dh
    mov  si,dx               ; SI = w
    mov  dl,bh
    mov  di,dx               ; DI = h
    xor  ah,ah               ; AX = bottom y
    sub  ax,di
    inc  ax
    add  ax,[PIC_RELOC]
    mov  dx,ax               ; DX = top screen row
    pop  ax                  ; AL = color (was DL)
    and  al,0x0F
    mov  ah,0xF8
    int  0x10
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

    times 1456-($-$$) db 0   ; arena guard: graf region is 0x5B0 bytes
