; cga8tsr.asm -- SCGA.COM v2: PCjr-style video mode 8 (160x200x16) for the
; IBM 5150 + CGA, rendered via ANSI-from-Hell tweaked text mode.
;
; TSR hooks INT 10h.  While mode 8 is active it services:
;   AH=00 AL=08  enter mode 8 (tweaked 80x100-cell text mode, 2-line rows)
;   AH=00 other  deactivate, chain to BIOS
;   AH=06/07     scroll up / down, AL=0 clears (40x25 text coords);
;                shadow kept consistent
;   AH=09/0A     write char at cursor (CX = count, BL = color, bit7 = XOR)
;   AH=0C        write pixel (CX=x 0-159, DX=y 0-199, AL=color, bit7=XOR)
;   AH=0D        read pixel (returns AL)
;   AH=0E        teletype (BL = color/bit7 XOR, CR/LF handled, scrolls)
;   everything else chains to the BIOS (AH=0F reports mode 8 via BDA).
;
; Pixel truth: 16000-byte shadow, 2 px/byte, high nibble = even x,
; offset = y*80 + x/2.  Each 2x2-px cell renders as one text cell via
; the pattern encoder (pattbl).  Plain text renders sharp through the
; E 1:1 glyph LUT and mirrors a coarse (AGI-px) approximation into the
; shadow; XOR text operates in shadow pixel space and re-encodes its
; cells, composing correctly over graphics.
;
; Double-install guarded by the 'SC8!' signature before the handler.
;
; Assemble:  nasm -f bin -o SCGA.COM cga8tsr.asm

BITS 16
CPU 8086
ORG 0x100

start:  jmp install

; ================== resident data ==================
active  db 0
oldvec  dd 0
attrtmp db 0
xorflag db 0
shptr   dw 0
s_col   db 0
s_row   db 0
sdown   db 0
s_fill  db 0
s_ulrow db 0
s_ulcol db 0
s_rows  db 0
s_cols  db 0
s_lines db 0
s_wb    db 0
f_x     dw 0
f_y     dw 0
f_x2    dw 0
f_y2    dw 0
f_col   db 0
f_byte  db 0
f_word  dw 0
p_e     db 0
p_o     db 0
ab_seg  dw 0
ab_x    dw 0
ab_x2   dw 0
ab_y    dw 0
ab_yb   dw 0
ab_p    dw 0
ab_w    dw 0

crtc_tbl db 4,0x7F, 5,0x06, 6,0x64, 7,0x70, 9,0x01

pattbl  db 0x85,0xB1,0xDD,0xFB,0x1F,0x95,0x11,0xDB  ; pattern 8-15 glyphs

e11lut:
%include "text_lut_e11.inc"

; ================== INT 10h handler ==================
        db 'SC8!'            ; install signature (handler-4)
handler:
    cmp  ah,0x00
    je   .modeset
    cmp  byte [cs:active],0
    je   .chain
    cmp  ah,0x06
    je   .scrollup
    cmp  ah,0x07
    je   .scrolldn
    cmp  ah,0x09
    je   .wrtchar
    cmp  ah,0x0A
    je   .wrtchar
    cmp  ah,0x0C
    je   .setpx
    cmp  ah,0x0D
    je   .getpx
    cmp  ah,0x0E
    je   .tty
    cmp  ah,0xF8
    je   .fillrect
    cmp  ah,0xF9
    je   .patfill
    cmp  ah,0xFA
    je   .agiblit
.chain:
    jmp  far [cs:oldvec]

; ---------------- mode set ----------------
.modeset:
    cmp  al,0x08
    je   .enter8
    mov  byte [cs:active],0
    jmp  .chain
.enter8:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    mov  ax,0x0003           ; real BIOS mode 3 first (clears, sets regs)
    pushf
    call far [cs:oldvec]
    mov  dx,0x3D8
    mov  al,0x09             ; 80-col, video on, blink off
    out  dx,al
    mov  si,crtc_tbl         ; 2-scanline rows, 100 shown, vsync 112
    mov  cx,5
    mov  dx,0x3D4
.crtc:
    mov  ax,[cs:si]
    inc  si
    inc  si
    out  dx,al
    inc  dx
    mov  al,ah
    out  dx,al
    dec  dx
    loop .crtc
    mov  al,10               ; hide hardware cursor (R10 bit5)
    out  dx,al
    inc  dx
    mov  al,0x20
    out  dx,al
    mov  ax,0xB800           ; clear VRAM: space on black
    mov  es,ax
    xor  di,di
    mov  ax,0x0020
    mov  cx,8000
    rep  stosw
    push cs                  ; clear shadow
    pop  es
    mov  di,shadow
    xor  ax,ax
    mov  cx,8000
    rep  stosw
    mov  ax,0x40             ; BDA: mode 8, 40 cols, cursor home
    mov  ds,ax
    mov  byte [0x49],0x08
    mov  word [0x4A],40
    mov  word [0x50],0
    mov  byte [cs:active],1
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- write pixel ----------------
; AL = color (bit7 = XOR), CX = x, DX = y
.setpx:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    cmp  cx,160
    jae  .px_done
    cmp  dx,200
    jae  .px_done
    call shadow_addr         ; SI = shadow byte for (CX,DX)
    mov  ah,[cs:si]
    test cl,1
    jnz  .px_odd
    mov  bl,al               ; even x = high nibble
    push cx
    mov  cl,4
    shl  bl,cl
    pop  cx
    and  bl,0xF0
    test al,0x80
    jz   .px_e_set
    xor  ah,bl
    jmp  .px_store
.px_e_set:
    and  ah,0x0F
    or   ah,bl
    jmp  .px_store
.px_odd:
    mov  bl,al
    and  bl,0x0F
    test al,0x80
    jz   .px_o_set
    xor  ah,bl
    jmp  .px_store
.px_o_set:
    and  ah,0xF0
    or   ah,bl
.px_store:
    cmp  ah,[cs:si]          ; unchanged pixel: skip store + re-encode
    je   .px_done
    mov  [cs:si],ah
    call encode_cell
.px_done:
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- read pixel ----------------
; CX = x, DX = y -> AL
.getpx:
    push bx
    push si
    push cx
    cmp  cx,160
    jae  .gp_zero
    cmp  dx,200
    jae  .gp_zero
    call shadow_addr
    mov  al,[cs:si]
    test cl,1
    jnz  .gp_odd
    push cx
    mov  cl,4
    shr  al,cl
    pop  cx
.gp_odd:
    and  al,0x0F
    jmp  .gp_out
.gp_zero:
    xor  al,al
.gp_out:
    pop  cx
    pop  si
    pop  bx
    iret

; ---------------- write char (AH=9/0A) ----------------
; AL = char, BL = color (bit7 = XOR), CX = count, cursor from BDA
.wrtchar:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    jcxz .wc_done
    push ax
    mov  ax,0x40
    mov  ds,ax
    mov  dx,[0x50]           ; DL = col, DH = row
    pop  ax
.wc_rep:
    cmp  dl,40
    jae  .wc_done
    cmp  dh,25
    jae  .wc_done
    push cx
    push dx
    call render_at
    pop  dx
    pop  cx
    inc  dl
    loop .wc_rep
.wc_done:
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- teletype (AH=0E) ----------------
.tty:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    push ax
    mov  ax,0x40
    mov  ds,ax
    mov  dx,[0x50]
    pop  ax
    cmp  al,0x0D
    jne  .tty_notcr
    xor  dl,dl
    jmp  .tty_store
.tty_notcr:
    cmp  al,0x0A
    jne  .tty_notlf
    inc  dh
    jmp  .tty_wrap
.tty_notlf:
    cmp  al,0x08             ; backspace: cursor left, no draw
    jne  .tty_notbs
    or   dl,dl
    jz   .tty_store
    dec  dl
    jmp  .tty_store
.tty_notbs:
    cmp  al,0x07             ; BEL: ignore
    je   .tty_store
.tty_char:
    push dx
    call render_at
    pop  dx
    inc  dl
    cmp  dl,40
    jb   .tty_store
    xor  dl,dl
    inc  dh
.tty_wrap:
    cmp  dh,25
    jb   .tty_store
    mov  dh,24               ; Sierra PutChar semantics: never scroll,
                             ; clamp at the bottom row
.tty_store:
    push ax
    mov  ax,0x40
    mov  ds,ax
    mov  [0x50],dx
    pop  ax
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- scroll (AH=6 up / AH=7 down) ----------------
.scrollup:
    push ax
    mov  byte [cs:sdown],0
    pop  ax
    jmp  .scr_go
.scrolldn:
    push ax
    mov  byte [cs:sdown],1
    pop  ax
.scr_go:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    call scroll_core
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- fill rect (AH=F8h, SCGA private) ----------------
; AL = color, CX = x, DX = y, SI = width, DI = height.
; Shadow filled nibble-exact; interior cells written as solid words,
; edge cells re-encoded from shadow.
.fillrect:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    cmp  cx,160
    jae  .fr_done
    cmp  dx,200
    jae  .fr_done
    and  al,0x0F
    mov  [cs:f_col],al
    mov  [cs:f_x],cx
    mov  [cs:f_y],dx
    mov  bx,cx
    add  bx,si
    dec  bx
    cmp  bx,160
    jb   .frx
    mov  bx,159
.frx:
    mov  [cs:f_x2],bx
    cmp  bx,cx
    jl   .fr_done
    mov  bx,dx
    add  bx,di
    dec  bx
    cmp  bx,200
    jb   .fry
    mov  bx,199
.fry:
    mov  [cs:f_y2],bx
    cmp  bx,dx
    jl   .fr_done
    mov  al,[cs:f_col]       ; f_byte = c | c<<4;  f_word = solid cell
    mov  ah,al
    push cx
    mov  cl,4
    shl  ah,cl
    pop  cx
    or   al,ah
    mov  [cs:f_byte],al
    mov  ah,al
    mov  al,0xDB
    mov  [cs:f_word],ax
    ; --- shadow fill, row by row ---
    push cs
    pop  es
    mov  dx,[cs:f_y]
.fr_row:
    mov  cx,[cs:f_x]
    call shadow_addr         ; SI = shadow ptr for (CX,DX)
    mov  di,si
    mov  bx,[cs:f_x2]
    test cl,1                ; left partial pixel (odd x)?
    jz   .fr_mid
    mov  al,[es:di]
    and  al,0xF0
    or   al,[cs:f_col]
    mov  [es:di],al
    inc  di
    inc  cx
    cmp  cx,bx
    jg   .fr_next
.fr_mid:
    mov  ax,bx
    sub  ax,cx
    inc  ax                  ; remaining pixel count
    shr  ax,1                ; full bytes
    push cx
    mov  cx,ax
    mov  al,[cs:f_byte]
    rep  stosb
    pop  cx
    mov  ax,bx
    sub  ax,cx
    inc  ax
    test al,1                ; trailing partial (even x2, odd count)?
    jz   .fr_next
    mov  al,[es:di]
    and  al,0x0F
    mov  ah,[cs:f_col]
    push cx
    mov  cl,4
    shl  ah,cl
    pop  cx
    or   al,ah
    mov  [es:di],al
.fr_next:
    inc  dx
    cmp  dx,[cs:f_y2]
    jle  .fr_row
    ; --- cell pass ---
    mov  ax,0xB800
    mov  es,ax
    mov  dx,[cs:f_y]
    and  dl,0xFE             ; cell-top scanline
.fc_row:
    mov  di,dx               ; DI = (y>>1)*160 + (x & ~1)
    shr  di,1
    mov  ax,di
    shl  ax,1
    shl  ax,1
    shl  ax,1
    shl  ax,1
    shl  ax,1                ; *32
    push cx
    mov  cl,7
    shl  di,cl               ; *128
    pop  cx
    add  di,ax
    mov  ax,[cs:f_x]
    and  al,0xFE
    add  di,ax
    mov  cx,[cs:f_x]
    and  cl,0xFE
.fc_cell:
    ; fully covered? y>=f_y, y+1<=f_y2, x>=f_x, x+1<=f_x2
    cmp  dx,[cs:f_y]
    jl   .fc_part
    mov  ax,dx
    inc  ax
    cmp  ax,[cs:f_y2]
    jg   .fc_part
    cmp  cx,[cs:f_x]
    jl   .fc_part
    mov  ax,cx
    inc  ax
    cmp  ax,[cs:f_x2]
    jg   .fc_part
    mov  ax,[cs:f_word]
    stosw
    jmp  .fc_next
.fc_part:
    call encode_cell         ; partial: truth from shadow
    add  di,2
.fc_next:
    add  cx,2
    cmp  cx,[cs:f_x2]
    jle  .fc_cell
    add  dx,2
    cmp  dx,[cs:f_y2]
    jle  .fc_row
.fr_done:
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- pattern fill (AH=F9h, SCGA private) ----------------
; AL = color A, BL = color B, BH = type (0 = 1px vstripes,
; 1 = 1px hstripes, 2 = 1px checker), CX = x, DX = y, SI = w, DI = h.
; Coordinates are cell-aligned (x,y,w,h even); odd bits are masked.
; Each type has a constant cell word and constant per-row-parity shadow
; bytes, so the whole region fills at rep-stos speed.
.patfill:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    and  al,0x0F
    mov  [cs:f_col],al       ; A
    mov  al,bl
    and  al,0x0F
    mov  [cs:p_o],al         ; B (temporary home)
    mov  [cs:attrtmp],bh     ; type
    ; clip rect -> f_x/f_y/f_x2/f_y2, aligned to cells
    cmp  cx,160
    jae  .pf_done
    cmp  dx,200
    jae  .pf_done
    and  cl,0xFE
    and  dl,0xFE
    mov  [cs:f_x],cx
    mov  [cs:f_y],dx
    mov  bx,cx
    add  bx,si
    dec  bx
    cmp  bx,160
    jb   .pfx
    mov  bx,159
.pfx:
    or   bl,1                ; x2 odd: whole cells
    mov  [cs:f_x2],bx
    cmp  bx,cx
    jl   .pf_done
    mov  bx,dx
    add  bx,di
    dec  bx
    cmp  bx,200
    jb   .pfy
    mov  bx,199
.pfy:
    or   bl,1                ; y2 odd: whole cells
    mov  [cs:f_y2],bx
    cmp  bx,dx
    jl   .pf_done
    ; build per-parity shadow bytes and the constant cell word
    mov  al,[cs:f_col]       ; A
    mov  ah,al
    push cx
    mov  cl,4
    shl  ah,cl               ; AH = A<<4
    pop  cx
    mov  bl,[cs:p_o]         ; B
    mov  bh,bl
    push cx
    mov  cl,4
    shl  bh,cl               ; BH = B<<4
    pop  cx
    mov  dl,[cs:attrtmp]     ; type
    cmp  dl,1
    je   .pf_h
    cmp  dl,2
    je   .pf_c
    ; type 0: vertical stripes -- bytes AB/AB, cell 0xDD attr (B<<4)|A
    mov  dh,ah
    or   dh,bl
    mov  [cs:p_e],dh
    mov  [cs:p_o],dh
    mov  dh,bh
    or   dh,al
    mov  dl,0xDD
    jmp  .pf_w
.pf_h:
    ; type 1: horizontal stripes -- bytes AA/BB, cell 0x1F attr (A<<4)|B
    mov  dh,ah
    or   dh,al
    mov  [cs:p_e],dh
    mov  dh,bh
    or   dh,bl
    mov  [cs:p_o],dh
    mov  dh,ah
    or   dh,bl
    mov  dl,0x1F
    jmp  .pf_w
.pf_c:
    ; type 2: checker -- bytes AB/BA, cell 0xB1 attr (B<<4)|A
    mov  dh,ah
    or   dh,bl
    mov  [cs:p_e],dh
    mov  dh,bh
    or   dh,al
    mov  [cs:p_o],dh
    mov  dh,bh
    or   dh,al
    mov  dl,0xB1
.pf_w:
    mov  al,dl
    mov  ah,dh
    mov  [cs:f_word],ax
    ; shadow fill
    push cs
    pop  es
    mov  dx,[cs:f_y]
.pf_row:
    mov  cx,[cs:f_x]
    call shadow_addr
    mov  di,si
    mov  ax,[cs:f_x2]
    sub  ax,cx
    inc  ax
    shr  ax,1                ; bytes per row
    mov  cx,ax
    mov  al,[cs:p_e]
    test dl,1
    jz   .pf_even
    mov  al,[cs:p_o]
.pf_even:
    rep  stosb
    inc  dx
    cmp  dx,[cs:f_y2]
    jle  .pf_row
    ; cell fill
    mov  ax,0xB800
    mov  es,ax
    mov  dx,[cs:f_y]
.pf_crow:
    mov  di,dx               ; DI = (y>>1)*160 + x
    shr  di,1
    mov  ax,di
    shl  ax,1
    shl  ax,1
    shl  ax,1
    shl  ax,1
    shl  ax,1                ; *32
    push cx
    mov  cl,7
    shl  di,cl               ; *128
    pop  cx
    add  di,ax
    add  di,[cs:f_x]
    mov  ax,[cs:f_x2]
    sub  ax,[cs:f_x]
    inc  ax
    shr  ax,1                ; words per cell row
    mov  cx,ax
    mov  ax,[cs:f_word]
    rep  stosw
    add  dx,2
    cmp  dx,[cs:f_y2]
    jle  .pf_crow
.pf_done:
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ---------------- AGI blit (AH=FAh, SCGA private) ----------------
; Blit a rect of an AGI-style source buffer (1 byte/px, low nibble =
; color, 160 bytes/row) into the mode-8 screen.
;   ES = source segment, DL = x (0-159), DH = vertical offset added to
;   y (AGI picReloc), AL = bottom y, BL = width, BH = height.
; Phase 1 packs source pixels into the shadow; phase 2 encodes every
; touched cell from shadow (edges compose with existing content).
.agiblit:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    sti
    cld
    mov  [cs:ab_seg],es
    test dl,1                ; align left edge / width to even
    jz   .ab_x
    dec  dl
    inc  bl
.ab_x:
    inc  bl
    and  bl,0xFE
    mov  cl,dl
    xor  ch,ch
    mov  [cs:ab_x],cx
    mov  cl,bl
    mov  [cs:ab_w],cx
    add  cx,[cs:ab_x]
    dec  cx
    mov  [cs:ab_x2],cx       ; x + w - 1
    mov  cl,dh
    xor  ch,ch
    mov  [cs:ab_p],cx
    mov  cl,al
    mov  [cs:ab_yb],cx       ; bottom y (buffer coords)
    xor  ah,ah               ; AX = bottom y (word)
    mov  dl,bh
    xor  dh,dh               ; DX = h (P already stashed)
    sub  ax,dx
    inc  ax                  ; word math: 167-168+1 = 0, not 256
    mov  [cs:ab_y],ax        ; top y
    ; --- phase 1: pack source rows into shadow ---
    mov  dx,[cs:ab_y]
.ab_row:
    ; SI = src: y*160 + x
    mov  si,dx
    shl  si,1
    shl  si,1
    shl  si,1
    shl  si,1
    shl  si,1                ; y*32
    mov  ax,si
    shl  ax,1
    shl  ax,1                ; y*128
    add  si,ax               ; y*160
    add  si,[cs:ab_x]
    ; DI = shadow + (y+p)*80 + x/2
    mov  ax,dx
    add  ax,[cs:ab_p]
    mov  di,ax
    shl  di,1
    shl  di,1
    shl  di,1
    shl  di,1                ; *16
    mov  ax,di
    shl  ax,1
    shl  ax,1                ; *64
    add  di,ax               ; *80
    mov  ax,[cs:ab_x]
    shr  ax,1
    add  di,ax
    add  di,shadow
    mov  cx,[cs:ab_w]
    shr  cx,1                ; output bytes
    mov  ds,[cs:ab_seg]
    push cs
    pop  es
.ab_pack:
    lodsw                    ; AL = left px, AH = right px
    and  ax,0x0F0F
    push cx
    mov  cl,4
    shl  al,cl
    pop  cx
    or   al,ah               ; hi = left, lo = right
    stosb
    loop .ab_pack
    inc  dx
    cmp  dx,[cs:ab_yb]
    jle  .ab_row
    ; --- phase 2: encode touched cells ---
    mov  dx,[cs:ab_y]
    add  dx,[cs:ab_p]
    and  dl,0xFE             ; top cell scanline
    mov  bx,[cs:ab_yb]
    add  bx,[cs:ab_p]        ; bottom screen row
.ab_crow:
    mov  cx,[cs:ab_x]
.ab_cell:
    call encode_cell
    add  cx,2
    cmp  cx,[cs:ab_x2]
    jle  .ab_cell
    add  dx,2
    cmp  dx,bx
    jle  .ab_crow
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; ================== helpers ==================

; SI = shadow + DX*80 + CX>>1   (preserves AX, CX, DX)
shadow_addr:
    push bx
    mov  si,dx
    shl  si,1
    shl  si,1
    shl  si,1
    shl  si,1                ; y*16
    mov  bx,si
    shl  bx,1
    shl  bx,1                ; y*64
    add  si,bx               ; y*80
    mov  bx,cx
    shr  bx,1
    add  si,bx
    add  si,shadow
    pop  bx
    ret

; re-encode the text cell containing pixel (CX, DX) from shadow to VRAM.
; Preserves all registers.
encode_cell:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    and  dl,0xFE             ; DX = cell top scanline
    mov  di,dx               ; VRAM DI = (y>>1)*160 + (x & ~1)
    shr  di,1
    mov  ax,di
    shl  ax,1
    shl  ax,1
    shl  ax,1
    shl  ax,1
    shl  ax,1                ; cellrow*32
    push cx
    mov  cl,7
    shl  di,cl               ; cellrow*128
    pop  cx
    add  di,ax               ; cellrow*160
    mov  bx,cx
    and  bl,0xFE
    xor  bh,bh
    add  di,bx
    mov  ax,0xB800
    mov  es,ax
    call shadow_addr         ; SI = cell's first shadow byte
    mov  al,[cs:si]          ; hi = p00, lo = p01
    mov  bl,[cs:si+80]       ; hi = p10, lo = p11
    mov  ah,al
    push cx
    mov  cl,4
    shr  ah,cl               ; AH = p00
    pop  cx
    and  al,0x0F             ; AL = p01
    mov  bh,bl
    push cx
    mov  cl,4
    shr  bh,cl               ; BH = p10
    pop  cx
    and  bl,0x0F             ; BL = p11
    mov  dh,ah               ; classify: fg=AH, DH=bg, DL=pattern
    mov  dl,8
    cmp  al,ah
    jne  .n01
    or   dl,4
    jmp  .p10
.n01:
    mov  dh,al
.p10:
    cmp  bh,ah
    jne  .n10
    or   dl,2
    jmp  .p11
.n10:
    cmp  dh,ah
    jne  .p11
    mov  dh,bh
.p11:
    cmp  bl,ah
    jne  .n11
    or   dl,1
    jmp  .enc
.n11:
    cmp  dh,ah
    jne  .enc
    mov  dh,bl
.enc:
    cmp  dl,11               ; patterns 11-14 render fg/bg swapped
    jb   .noswp
    cmp  dl,14
    ja   .noswp
    xchg ah,dh
.noswp:
    mov  al,dl
    and  al,7
    mov  bx,pattbl
    cs   xlatb               ; AL = cell glyph
    push cx
    mov  cl,4
    shl  dh,cl
    pop  cx
    or   dh,ah               ; attr = bg<<4 | fg
    mov  ah,dh
    stosw
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; render one 40-col char: AL = char, BL = color (bit7 = XOR),
; DL = col, DH = row.  Clobbers AX,BX,CX,DX,SI,DI,DS,ES.
render_at:
    mov  [cs:s_col],dl
    mov  [cs:s_row],dh
    mov  bh,bl
    and  bh,0x7F             ; full attr: bg (bits 4-6) << 4 | fg
    mov  [cs:attrtmp],bh
    mov  bh,bl
    and  bh,0x80             ; bit 7 = XOR draw
    mov  [cs:xorflag],bh
    push ax
    mov  al,dh               ; t = row*640
    xor  ah,ah
    mov  cl,7
    shl  ax,cl               ; row*128
    mov  di,ax
    shl  ax,1
    shl  ax,1                ; row*512
    add  di,ax               ; DI = row*640
    mov  al,dl
    xor  ah,ah
    shl  ax,1                ; col*2
    mov  si,di
    add  si,ax
    add  si,shadow
    mov  [cs:shptr],si       ; shadow ptr = shadow + row*640 + col*2
    shl  ax,1                ; col*4
    add  di,ax               ; DI = VRAM = row*640 + col*4
    pop  ax
    xor  ah,ah               ; glyph -> DS:SI
    test al,0x80
    jnz  .hi
    shl  ax,1
    shl  ax,1
    shl  ax,1
    mov  si,ax
    add  si,0xFA6E
    mov  bx,0xF000
    mov  ds,bx
    jmp  .go
.hi:
    and  al,0x7F
    shl  ax,1
    shl  ax,1
    shl  ax,1
    push ax
    xor  ax,ax
    mov  ds,ax
    lds  si,[0x007C]         ; INT 1F font
    pop  ax
    add  si,ax
.go:
    cmp  byte [cs:xorflag],0
    jne  render_xor
    mov  ax,0xB800
    mov  es,ax
    mov  cx,4                ; 4 cell rows
.k:
    lodsb
    mov  dh,al               ; glyph row A
    call sh_row
    lodsb
    mov  dl,al               ; glyph row B
    call sh_row
    mov  al,dh               ; left cell: (A & F0) | (B >> 4)
    and  al,0xF0
    mov  bh,dl
    shr  bh,1
    shr  bh,1
    shr  bh,1
    shr  bh,1
    or   al,bh
    mov  bx,e11lut
    cs   xlatb
    mov  ah,[cs:attrtmp]
    stosw
    mov  al,dh               ; right cell: (A << 4) | (B & 0F)
    shl  al,1
    shl  al,1
    shl  al,1
    shl  al,1
    mov  bh,dl
    and  bh,0x0F
    or   al,bh
    mov  bx,e11lut
    cs   xlatb
    mov  ah,[cs:attrtmp]
    stosw
    add  di,160-4
    loop .k
    ret

render_xor:
    mov  cx,8                ; XOR the glyph into shadow pixel space
.xr:
    lodsb
    call xr_row
    loop .xr
    mov  al,[cs:s_col]       ; re-encode the char's 8 cells
    xor  ah,ah
    shl  ax,1
    shl  ax,1
    mov  cx,ax               ; x0 = col*4
    mov  al,[cs:s_row]
    xor  ah,ah
    shl  ax,1
    shl  ax,1
    shl  ax,1
    mov  dx,ax               ; y0 = row*8
    mov  bl,4
.ec:
    call encode_cell
    add  cx,2
    call encode_cell
    sub  cx,2
    add  dx,2
    dec  bl
    jnz  .ec
    ret

; sh_row: AL = glyph row byte; writes the coarse 4-px approximation
; (fg where either glyph dot of the pair is set, else bg) as 2 shadow
; bytes at [cs:shptr]; shptr += 80.  Preserves registers.
sh_row:
    push ax
    push bx
    push cx
    push dx
    mov  cl,4
    mov  bl,[cs:attrtmp]
    and  bl,0x0F             ; fg
    mov  bh,bl
    shl  bh,cl               ; fg<<4
    mov  dl,[cs:attrtmp]
    shr  dl,cl               ; bg
    mov  dh,dl
    shl  dh,cl
    or   dl,dh               ; DL = bg | bg<<4 (default byte)
    ; byte 0 in AH
    mov  ah,dl
    test al,0xC0
    jz   .q1
    and  ah,0x0F
    or   ah,bh
.q1:
    test al,0x30
    jz   .q2
    and  ah,0xF0
    or   ah,bl
.q2:
    ; byte 1 in DH
    mov  dh,dl
    test al,0x0C
    jz   .q3
    and  dh,0x0F
    or   dh,bh
.q3:
    test al,0x03
    jz   .q4
    and  dh,0xF0
    or   dh,bl
.q4:
    mov  bx,[cs:shptr]
    mov  [cs:bx],ah
    mov  [cs:bx+1],dh
    add  bx,80
    mov  [cs:shptr],bx
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; xr_row: AL = glyph row byte; XORs fg into covered shadow pixels at
; [cs:shptr]; shptr += 80.  Preserves registers.
xr_row:
    push ax
    push bx
    push cx
    push di
    mov  bl,[cs:attrtmp]
    and  bl,0x0F             ; XOR uses fg only
    mov  bh,bl
    mov  cl,4
    shl  bh,cl               ; fg<<4
    mov  di,[cs:shptr]
    test al,0xC0
    jz   .x1
    xor  [cs:di],bh
.x1:
    test al,0x30
    jz   .x2
    xor  [cs:di],bl
.x2:
    test al,0x0C
    jz   .x3
    xor  [cs:di+1],bh
.x3:
    test al,0x03
    jz   .x4
    xor  [cs:di+1],bl
.x4:
    add  di,80
    mov  [cs:shptr],di
    pop  di
    pop  cx
    pop  bx
    pop  ax
    ret

; scroll_core: AL=lines (0=clear), BH=fill color, CH/CL=UL row/col,
; DH/DL=LR row/col (40x25 text coords).  [cs:sdown]: 0=up, 1=down.
; Moves cells and keeps the shadow consistent.
scroll_core:
    push bp
    mov  [cs:s_fill],bh
    mov  [cs:s_ulrow],ch
    mov  [cs:s_ulcol],cl
    sub  dh,ch
    inc  dh                  ; rows
    sub  dl,cl
    inc  dl                  ; cols
    mov  [cs:s_rows],dh
    mov  [cs:s_cols],dl
    mov  bl,al
    or   bl,bl
    jnz  .c0
    mov  bl,dh
.c0:
    cmp  bl,dh
    jbe  .c1
    mov  bl,dh
.c1:
    mov  [cs:s_lines],bl
    mov  ax,0xB800
    mov  es,ax
    mov  ds,ax
    mov  al,[cs:s_ulrow]     ; DI = ulrow*640 + ulcol*4
    xor  ah,ah
    mov  dx,640
    mul  dx
    mov  di,ax
    mov  al,[cs:s_ulcol]
    xor  ah,ah
    shl  ax,1
    shl  ax,1
    add  di,ax
    mov  bl,[cs:s_lines]     ; BL = scroll cell rows
    shl  bl,1
    shl  bl,1
    mov  al,[cs:s_rows]
    shl  al,1
    shl  al,1
    sub  al,bl
    mov  dl,al               ; DL = copy cell rows
    mov  dh,[cs:s_cols]
    shl  dh,1                ; DH = words per cell row
    mov  al,bl
    xor  ah,ah
    mov  bp,160
    mul  bp
    mov  bp,ax               ; BP = scroll distance bytes
    cmp  byte [cs:sdown],0
    jne  .down
    mov  si,di               ; --- up: copy top-down ---
    add  si,bp
    or   dl,dl
    jz   .ufill
.uc:
    mov  cl,dh
    xor  ch,ch
    push si
    push di
    rep  movsw
    pop  di
    pop  si
    add  si,160
    add  di,160
    dec  dl
    jnz  .uc
.ufill:
    call fill_rows           ; DI = first blank row
    jmp  .shadow
.down:
    push di                  ; save window top (fill start)
    mov  al,dl               ; last row = top + (copy+scroll-1)*160
    add  al,bl
    dec  al
    xor  ah,ah
    push dx
    mov  dx,160
    mul  dx
    pop  dx
    add  di,ax
    mov  si,di
    sub  si,bp
    or   dl,dl
    jz   .dfill
.dc:
    mov  cl,dh
    xor  ch,ch
    push si
    push di
    rep  movsw
    pop  di
    pop  si
    sub  si,160
    sub  di,160
    dec  dl
    jnz  .dc
.dfill:
    pop  di                  ; window top
    call fill_rows
.shadow:
    call scroll_shadow
    pop  bp
    ret

; effective fill color from s_fill: text-attr background nibble if
; present (0xFF -> 15, 0x70 -> 7), else the plain low-nibble color
fill_color:
    mov  al,[cs:s_fill]
    mov  ah,al
    and  al,0x0F
    push cx
    mov  cl,4
    shr  ah,cl
    pop  cx
    and  ah,0x0F
    jz   .fg
    mov  al,ah
.fg:
    ret

; fill BL cell rows at ES:DI, DH words each, with solid fill color
fill_rows:
    push ax
    push bx
    push cx
    push di
    call fill_color
    mov  ah,al
    mov  cl,4
    shl  ah,cl
    or   ah,al               ; attr = c<<4 | c
    mov  al,0xDB
    or   bl,bl
    jz   .done
.f:
    mov  cl,dh
    xor  ch,ch
    push di
    rep  stosw
    pop  di
    add  di,160
    dec  bl
    jnz  .f
.done:
    pop  di
    pop  cx
    pop  bx
    pop  ax
    ret

; mirror the scroll in shadow pixel space (from stashed parameters)
scroll_shadow:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push cs
    pop  ds
    push cs
    pop  es
    mov  al,[cs:s_ulrow]     ; BX = shadow + ulrow*640 + ulcol*2
    xor  ah,ah
    mov  dx,640
    mul  dx
    mov  bx,ax
    mov  al,[cs:s_ulcol]
    xor  ah,ah
    shl  ax,1
    add  bx,ax
    add  bx,shadow
    mov  al,[cs:s_lines]     ; DL = scroll px rows (lines*8)
    shl  al,1
    shl  al,1
    shl  al,1
    mov  dl,al
    mov  al,[cs:s_rows]      ; DH = window px rows (rows*8)
    shl  al,1
    shl  al,1
    shl  al,1
    mov  dh,al
    mov  al,[cs:s_cols]      ; stash width bytes (cols*2)
    shl  al,1
    mov  [cs:s_wb],al
    mov  al,dl               ; AX = distance = d*80
    xor  ah,ah
    push dx
    mov  dx,80
    mul  dx
    pop  dx
    mov  si,ax               ; SI = distance bytes (temp)
    mov  al,dh
    sub  al,dl               ; AL = copy px rows
    cmp  byte [cs:sdown],0
    jne  .down
    mov  di,bx               ; up: dest walks top->bottom
    add  si,bx               ; src = dest + dist
.uc:
    or   al,al
    jz   .fill               ; DI = first blank px row
    push si
    push di
    mov  cl,[cs:s_wb]
    xor  ch,ch
    rep  movsb
    pop  di
    pop  si
    add  si,80
    add  di,80
    dec  al
    jmp  .uc
.down:
    push si                  ; save distance
    push ax
    mov  al,dh               ; last px row = bx + (window-1)*80
    dec  al
    xor  ah,ah
    push dx
    mov  dx,80
    mul  dx
    pop  dx
    mov  di,bx
    add  di,ax
    pop  ax
    pop  cx                  ; CX = distance
    mov  si,di
    sub  si,cx
.dc:
    or   al,al
    jz   .dfill
    push si
    push di
    mov  cl,[cs:s_wb]
    xor  ch,ch
    rep  movsb
    pop  di
    pop  si
    sub  si,80
    sub  di,80
    dec  al
    jmp  .dc
.dfill:
    mov  di,bx               ; fill at window top
.fill:
    call fill_color          ; fill DL px rows at DI
    mov  ah,al
    mov  cl,4
    shl  ah,cl
    or   al,ah               ; byte = c | c<<4
    mov  ah,al
    or   dl,dl
    jz   .done
.f:
    push di
    mov  cl,[cs:s_wb]
    xor  ch,ch
    push ax
    rep  stosb
    pop  ax
    pop  di
    add  di,80
    dec  dl
    jnz  .f
.done:
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

resident_end:
shadow  equ resident_end
shadow_end equ shadow + 16000

; ================== transient install code ==================
install:
    mov  ax,0x3510
    int  0x21                ; ES:BX = current INT 10h
    cmp  word [es:bx-4],0x4353   ; 'SC'
    jne  .fresh
    cmp  word [es:bx-2],0x2138   ; '8!'
    jne  .fresh
    mov  dx,already
    mov  ah,9
    int  0x21
    mov  ax,0x4C01
    int  0x21
.fresh:
    mov  [oldvec],bx
    mov  [oldvec+2],es
    mov  dx,handler
    mov  ax,0x2510
    int  0x21
    mov  dx,banner
    mov  ah,9
    int  0x21
    mov  dx,(shadow_end - $$ + 0x100 + 15) >> 4
    mov  ax,0x3100
    int  0x21

banner  db "SCGA v2: 160x200x16 (mode 8) on CGA installed.",13,10,"$"
already db "SCGA already installed.",13,10,"$"
