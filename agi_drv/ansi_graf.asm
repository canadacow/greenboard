; ansi_graf.asm -- replacement Sierra AGI v2 CGA driver, v2
;
; Renders the AGI 160x200 16-color shadow buffer in "ANSI from Hell"
; tweaked text mode: BIOS mode 3 with the 6845 set to 2-scanline character
; rows -> 80x100 cells covering 200 scanlines.  Each cell = 2x2 AGI pixels,
; 2 colors (attr fg/bg) + a pattern glyph from the IBM char ROM:
;
;   1111 solid=0xDB  1010 L/R=0xDD  1100 T/B=0x1F(S)  1001 checker=0xB1
;   1000 TL=0x85  0100 TR: 1011=0xFB(S)  0010 BL: 1101=0x95(S)
;   0001 BR: 1110=0x11(S)          (S) = attr fg/bg swapped
;
; ABI (from Sierra AGI source, lanceewing/agi; verified against SQ2 2.0D
; interpreter binary):
;   +0x00 initgrfx  +0x03 textscrn  +0x06 grafscrn  +0x09 sethorz
;   +0x0C blk2Scrn  +0x0F initrows  +0x12 DrawWndw
;   +0x15 pcxc (xlatColor tail-jumps here for CGA)  +0x37 pcxo (cel remap)
;   blk2Scrn/DrawWndw: AH=x AL=base(bottom) y BL=width BH=height, DL=color
;   Interpreter helpers (near, same-segment): BackToScrn=0xBD46,
;   getVidOfst=0xBE24, getShdwOfst=0xBE66 (inlined here: y*160+x)
;   DS vars: s_ram=0x136F v_ram=0x1371 picReloc=0x1379 row_table=0x137B
;   hsync: gfx=0x1365 txt=0x1366 delta=0x1367
;
; Text: interpreter prints via INT 10h (WrtChar=AH 9, ScrollUp=AH 6, set
; cursor).  Mode 4 got BIOS graphics text for free; we hook INT 10h like the
; Hercules driver (HVideoInt) and render chars through a 256-byte LUT that
; maps glyph half-row nibble pairs to best-matching ROM chars.
;
; Assemble:  nasm -f bin -o cga_graf.ovl ansi_graf.asm

BITS 16
CPU 8086
ORG 0

BACKTOSCRN   equ 0xBD46
DISP_TYPE    equ 0x1130
HSYNC_GFX    equ 0x1365
HSYNC_TXT    equ 0x1366
HSYNC_DELTA  equ 0x1367
BUF_SEG      equ 0x136F      ; s_ram
VRAM_SEG     equ 0x1371      ; v_ram
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

; +0x15 pcxc: color translate -- identity (keep 16-color indices)
pcxc:
    mov  ah,al
    ret

; gap to +0x37 reused for helpers and driver state
get_base:                    ; returns BX = overlay load offset
    call .here
.here:
    pop  bx
    sub  bx,.here
    ret

hooked  db 0
attrtmp db 0
oldvec  dd 0

pattbl  db 0x85,0xB1,0xDD,0xFB,0x1F,0x95,0x11,0xDB  ; pattern 8-15 glyphs

    times 0x37-($-$$) db 0x90

; +0x37 pcxo: view-cel remap -- identity
pcxo:
    ret

; ---------------- entry 0: enter graphics mode ----------------
initgrfx:
    push si
    push di
    push bp
    mov  ax,0x0003
    int  0x10
    mov  ah,1                ; hide the hardware cursor
    mov  cx,0x2000
    int  0x10
    mov  dx,0x3D8
    mov  al,0x09             ; 80-col, video on, blink OFF
    out  dx,al
    push ds
    mov  ax,0x40
    mov  ds,ax
    mov  byte [0x65],0x09
    pop  ds
    mov  si,crtc_tbl
    call get_base
    add  si,bx               ; SI = &crtc_tbl (position independent)
    lea  ax,[bx+pattbl]      ; patch blit's pattern-table immediate
    mov  [cs:bx+blk2scrn.tblimm+1],ax
    mov  dx,0x3D4
    mov  cx,5
.crtc:
    mov  ax,[cs:si]          ; AL=reg AH=val
    inc  si
    inc  si
    out  dx,al
    inc  dx
    mov  al,ah
    out  dx,al
    dec  dx
    loop .crtc
    mov  word [HSYNC_GFX],0x5A5A   ; R2 base for 80-col (gfx+txt bytes)
    ; call hook_int10        ; text hook disabled (known-good hookless build)
    ; identity-fill CGA dither tables (interpreter may read them directly)
    xor  bx,bx
.tbl:
    mov  al,bl
    mov  ah,al
    mov  di,bx
    shl  di,1
    add  di,bx
    mov  [di+0x1D36],al      ; 3-byte entry: composite byte + RGB word
    mov  [di+0x1D37],ax
    mov  [bx+0x1D66],al
    mov  [bx+0x1D76],al
    mov  [bx+0x1D86],al
    inc  bx
    cmp  bx,16
    jb   .tbl
    call sethorz
    pop  bp
    pop  di
    pop  si
    ret


crtc_tbl:
    db 4,0x7F, 5,0x06, 6,0x64, 7,0x70, 9,0x01  ; vtot 127, adj 6, disp 100, VSYNC 112, 2-line rows

; ---------------- entry 2: graphics + full redraw ----------------
grafscrn:
    push si
    push di
    push bp
    call initgrfx
    call BACKTOSCRN
    pop  bp
    pop  di
    pop  si
    ret

; ---------------- entry 1: text mode (40x25) ----------------
textscrn:
    push si
    push di
    push bp
    ; call unhook_int10      ; text hook disabled (known-good hookless build)
    mov  ax,0x0001
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
    mov  ah,1
    mov  cx,0x1000
    int  0x10
    pop  bp
    pop  di
    pop  si
    ret

; ---------------- entry 3: CRTC hsync position ----------------
sethorz:
    mov  ah,0x0F
    int  0x10
    xor  bx,bx
    cmp  al,4
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
.t: stosw                    ; row_table[y] = (y>>1)*160, y = scanline
    stosw
    add  ax,160
    loop .t
.done:
    pop  di
    ret

; ---------------- entry 4: blit rect shadow -> screen ----------------
; AH=x (0-159), AL=bottom y, BL=w, BH=h
blk2scrn:
    push si
    push di
    push bp
    test ah,1
    jz   .xok
    dec  ah
    inc  bl
.xok:
    inc  bl
    and  bl,0xFE
    mov  cx,bx               ; CL=w CH=h
    ; source offset of (x, y): DI = y*160 + x  (getShdwOfst inlined)
    push ax
    push cx
    mov  dl,al
    xor  dh,dh               ; DX = y
    mov  al,ah
    xor  ah,ah
    mov  si,ax               ; SI = x
    mov  ax,160
    mul  dx
    add  si,ax               ; SI = y*160 + x
    pop  cx
    pop  ax
    ; screen row = y + picReloc; parity decides cell alignment
    mov  dx,[PIC_RELOC]
    add  dl,al
    adc  dh,0                ; DX = screen row of bottom line
    test dl,1
    jz   .aligned
    sub  si,160              ; source starts one line up (cell top)
.aligned:
    ; rows = (srow>>1) - ((srow-h+1)>>1) + 1
    push ax
    mov  ax,dx
    shr  ax,1                ; AX = bottom cell row
    mov  bp,ax               ; BP = bottom cell row (temp)
    sub  dl,ch
    sbb  dh,0
    inc  dx                  ; DX = top screen row
    shr  dx,1                ; DX = top cell row
    sub  bp,dx
    inc  bp                  ; BP = cell row count
    ; VRAM DI = bottomcellrow*160 + x
    mov  dx,160
    mul  dx                  ; AX = bottom cell row * 160
    pop  dx                  ; DX = entry AX (AH=x AL=y)
    mov  di,ax
    mov  al,dh
    xor  ah,ah
    add  di,ax               ; DI = cell row * 160 + x
    ; pack BP = w<<8 | rows  (rows < 105, w < 161)
    mov  ax,bp
    mov  ah,cl               ; AH=w AL=rows
    mov  bp,ax
    push ds
    push es
    mov  es,[VRAM_SEG]
    mov  ds,[BUF_SEG]
.rowloop:
    mov  cx,bp
    mov  cl,ch
    xor  ch,ch
    shr  cx,1                ; cells = w/2
.cell:
    lodsw                    ; AL=p00 AH=p01
    mov  bx,[si+158]         ; BL=p10 BH=p11
    and  ax,0x0F0F
    and  bx,0x0F0F
    mov  dh,al               ; bg sentinel = fg
    mov  dl,8
    cmp  ah,al
    jne  .n01
    or   dl,4
    jmp  .p10
.n01:
    mov  dh,ah
.p10:
    cmp  bl,al
    jne  .n10
    or   dl,2
    jmp  .p11
.n10:
    cmp  dh,al
    jne  .p11
    mov  dh,bl
.p11:
    cmp  bh,al
    jne  .n11
    or   dl,1
    jmp  .enc
.n11:
    cmp  dh,al
    jne  .enc
    mov  dh,bh
.enc:
    ; patterns 11-14 render with fg/bg swapped (contiguous range)
    cmp  dl,11
    jb   .noswp
    cmp  dl,14
    ja   .noswp
    xchg al,dh
.noswp:
    push ax
    mov  al,dl
    and  al,7                ; pattern index 0-7
.tblimm:
    mov  bx,0                ; patched at init: &pattbl (absolute)
    cs   xlatb               ; AL = glyph
    mov  bl,al
    pop  ax
    push cx
    mov  cl,4
    shl  dh,cl
    pop  cx
    or   al,dh               ; attr = bg<<4 | fg
    mov  ah,al
    mov  al,bl
    stosw
    loop .cell
    mov  dx,bp
    mov  dl,dh
    xor  dh,dh               ; DX = w
    sub  si,dx
    sub  si,320
    sub  di,dx
    sub  di,160
    dec  bp
    test bp,0x00FF
    jnz  .rowloop
    pop  es
    pop  ds
    pop  bp
    pop  di
    pop  si
    ret

; ---------------- entry 6: draw window fill ----------------
; AH=x AL=bottom y BL=w BH=h DL=color.  Whole-cell writes.
drawwndw:
    push si
    push di
    push bp
    test ah,1
    jz   .xok
    dec  ah
    inc  bl
.xok:
    inc  bl
    and  bl,0xFE
    push dx                  ; save color
    mov  cl,ah               ; CL = x (safe copy)
    mov  dx,[PIC_RELOC]
    add  dl,al
    adc  dh,0                ; DX = bottom screen row
    mov  si,dx               ; SI = bottom screen row
    sub  dl,bh
    sbb  dh,0
    inc  dx
    shr  dx,1                ; DX = top cell row
    mov  bp,si
    shr  bp,1                ; BP = bottom cell row
    push bp                  ; save bottom cell row
    sub  bp,dx
    inc  bp                  ; BP = rows
    mov  ax,bp
    mov  ah,bl               ; pack BP = w<<8 | rows
    mov  bp,ax
    pop  ax                  ; AX = bottom cell row
    mov  dx,160
    mul  dx
    mov  di,ax
    mov  al,cl
    xor  ah,ah
    add  di,ax               ; DI = bottom cell row * 160 + x
    pop  dx                  ; DL = color
    mov  al,dl
    and  al,0x0F
    mov  ah,al
    mov  cl,4
    shl  ah,cl
    or   ah,al               ; attr = color<<4 | color
    mov  al,0xDB
    push es
    mov  es,[VRAM_SEG]
.row:
    mov  cx,bp
    mov  cl,ch
    xor  ch,ch
    shr  cx,1                ; cells
    push di
    rep  stosw
    pop  di
    sub  di,160
    dec  bp
    test bp,0x00FF
    jnz  .row
    pop  es
    pop  bp
    pop  di
    pop  si
    ret


; ---------------- INT 10h hook (text in tweaked mode) ----------------
; The interpreter prints via INT10: WrtChar (AH=9/0A), ScrollUp (AH=6),
; set cursor (AH=2, chained).  BIOS text routines are useless in our
; 2-scanline-row mode, so we render chars ourselves (HGC driver pattern).
hook_int10:
    push es
    call get_base
    cmp  byte [cs:bx+hooked], 1
    je   .done
    mov  byte [cs:bx+hooked], 1
    lea  ax,[bx+vint10]      ; absolute offset of handler
    xor  cx,cx
    mov  es,cx
    cli
    mov  cx,[es:0x40]
    mov  [cs:bx+oldvec],cx
    mov  cx,[es:0x42]
    mov  [cs:bx+oldvec+2],cx
    mov  [es:0x40],ax
    mov  [es:0x42],cs
    sti
.done:
    pop  es
    ret

unhook_int10:
    push es
    call get_base
    cmp  byte [cs:bx+hooked], 0
    je   .done
    mov  byte [cs:bx+hooked], 0
    xor  cx,cx
    mov  es,cx
    cli
    mov  cx,[cs:bx+oldvec]
    mov  [es:0x40],cx
    mov  cx,[cs:bx+oldvec+2]
    mov  [es:0x42],cx
    sti
.done:
    pop  es
    ret


; ---- INT 10h handler ----
vint10:
    cmp  ah,0x09
    je   .wrtchar
    cmp  ah,0x0A
    je   .wrtchar
    cmp  ah,0x06
    je   .scroll
    ; chain to previous handler
    push bp
    push bx
    call get_base
    mov  bp,bx
    pop  bx
    pushf
    call far [cs:bp+oldvec]
    pop  bp
    iret

.wrtchar:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp
    mov  dh,bl               ; color (get_base clobbers BX)
    call get_base
    mov  bp,bx               ; BP = overlay base
    and  dh,0x0F             ; attr byte (fg on black) into scratch
    mov  [cs:bp+attrtmp],dh
    ; glyph source -> DS:SI  (no CL shifts: CX = repeat count)
    xor  ah,ah
    test al,0x80
    jnz  .hi
    shl  ax,1
    shl  ax,1
    shl  ax,1
    mov  si,ax
    add  si,0xFA6E
    mov  dx,0xF000
    mov  ds,dx
    jmp  .gsrc
.hi:
    and  al,0x7F
    shl  ax,1
    shl  ax,1
    shl  ax,1
    push ax
    xor  ax,ax
    mov  ds,ax
    lds  si,[0x007C]         ; INT 1F: font for chars 128-255
    pop  ax
    add  si,ax
.gsrc:
    ; cursor from BDA -> DI = row*640 + col*4
    mov  dx,0x40
    mov  es,dx
    mov  dx,[es:0x50]        ; DL=col DH=row
    mov  di,0xB800
    mov  es,di
    xor  ax,ax
    mov  al,dh
    push dx
    mov  dx,640
    mul  dx                  ; AX = row*640
    pop  dx
    mov  di,ax
    xor  ax,ax
    mov  al,dl
    shl  ax,1
    shl  ax,1                ; col*4
    add  di,ax
    lea  bx,[bp+textlut]     ; BX = absolute LUT ptr (for cs xlatb)
.rep:
    push di
    push si
    mov  dl,4                ; 4 cell rows per char
.krow:
    lodsb
    mov  dh,al               ; DH = glyph row A
    lodsb                    ; AL = glyph row B
    push ax                  ; save row B
    ; left cell: idx = (A & F0) | (B >> 4)
    mov  ah,al
    mov  al,dh
    and  al,0xF0
    shr  ah,1
    shr  ah,1
    shr  ah,1
    shr  ah,1
    or   al,ah
    cs   xlatb               ; AL = cell char
    mov  ah,[cs:bp+attrtmp]
    stosw
    ; right cell: idx = (A << 4) | (B & 0F)
    pop  ax                  ; AL = row B
    and  al,0x0F
    mov  ah,al
    mov  al,dh
    shl  al,1
    shl  al,1
    shl  al,1
    shl  al,1
    or   al,ah
    cs   xlatb
    mov  ah,[cs:bp+attrtmp]
    stosw
    add  di,160-4            ; next cell row
    dec  dl
    jnz  .krow
    pop  si
    pop  di
    add  di,4                ; next char cell column (repeat count)
    loop .rep
    pop  bp
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

.scroll:
    ; AL=lines BH=fill color CH,CL=UL row,col DH,DL=LR row,col (text coords)
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp
    mov  si,0xB800
    mov  es,si
    mov  ds,si
    mov  bp,ax               ; BP.lo = lines
    ; window dims (text): DH = rows, DL = cols
    sub  dh,ch
    inc  dh
    sub  dl,cl
    inc  dl
    ; DI = top-left = CH*640 + CL*4
    push dx
    xor  ah,ah
    mov  al,ch
    mov  dx,640
    mul  dx                  ; AX = top row * 640
    pop  dx
    xor  ch,ch               ; CX = UL col
    shl  cx,1
    shl  cx,1                ; col*4
    add  ax,cx
    mov  di,ax
    ; CL = scroll amount in text rows (0 or >win -> whole window)
    mov  cx,bp
    or   cl,cl
    jnz  .s0
    mov  cl,dh
.s0:
    cmp  cl,dh
    jbe  .s1
    mov  cl,dh
.s1:
    shl  cl,1
    shl  cl,1                ; CL = scroll cell rows
    mov  al,dh
    shl  al,1
    shl  al,1                ; AL = window cell rows
    sub  al,cl               ; AL = cell rows to copy
    mov  ah,dl
    shl  ah,1                ; AH = words per row
    ; BP = scroll offset in bytes = CL*160
    push ax
    xor  ah,ah
    mov  al,cl
    mov  bp,160
    mul  bp
    mov  bp,ax
    pop  ax
    mov  si,di
    add  si,bp               ; source = dest + scroll*160
    mov  dl,al               ; DL = copy row count
    or   dl,dl
    jz   .fill
.crow:
    push cx
    push si
    push di
    mov  cl,ah
    xor  ch,ch
    rep  movsw
    pop  di
    pop  si
    pop  cx
    add  si,160
    add  di,160
    dec  dl
    jnz  .crow
.fill:
    ; blank CL cell rows at DI: char 0xDB, attr = fill color
    mov  dl,ah               ; DL = words per row
    mov  al,bh
    and  al,0x0F
    mov  ah,al
    push cx
    mov  cl,4
    shl  ah,cl
    pop  cx
    or   ah,al
    mov  al,0xDB
    or   cl,cl
    jz   .sdone
.frow:
    push cx
    push di
    mov  cl,dl
    xor  ch,ch
    rep  stosw
    pop  di
    pop  cx
    add  di,160
    dec  cl
    jnz  .frow
.sdone:
    pop  bp
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- text glyph LUT ----------------
textlut:
%include "text_lut.inc"

    times 1456-($-$$) db 0   ; arena guard: graf region is 0x5B0 bytes
