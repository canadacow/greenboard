; show.asm -- SHOW.COM: SCGA mode-8 showcase.
; Five screens, any key advances, ESC quits:
;   1  16-color bars with XOR hex labels
;   2  pattern-class zoo: solid / vertical / horizontal / checker bands
;   3  concentric circles (aspect-corrected)
;   4  16-ray starburst
;   5  color bands + normal text + XOR text + scroll up/down exercise
;
; Assemble:  nasm -f bin -o SHOW.COM show.asm

BITS 16
CPU 8086
ORG 0x100

start:
    call mode8
    jc   notsr
    call scr_bars
    call waitkey
    jz   quit
    call mode8
    call scr_zoo
    call waitkey
    jz   quit
    call mode8
    call scr_circles
    call waitkey
    jz   quit
    call mode8
    call scr_rays
    call waitkey
    jz   quit
    call mode8
    call scr_text
    call waitkey
quit:
    mov  ax,0x0003
    int  0x10
    mov  ax,0x4C00
    int  0x21

notsr:
    mov  ax,0x0003
    int  0x10
    mov  dx,errmsg
    mov  ah,9
    int  0x21
    mov  ax,0x4C01
    int  0x21

; enter mode 8; CF set if unavailable
mode8:
    mov  ax,0x0008
    int  0x10
    mov  ah,0x0F
    int  0x10
    cmp  al,0x08
    jne  .bad
    clc
    ret
.bad:
    stc
    ret

; wait for key; ZF set if ESC
waitkey:
    xor  ah,ah
    int  0x16
    cmp  al,27
    ret

; plot pixel: AL = color, CX = x, DX = y
pixel:
    push ax
    push bx
    mov  ah,0x0C
    xor  bh,bh
    int  0x10
    pop  bx
    pop  ax
    ret

; put string: SI = text, DL = col, DH = row, BL = color
puts:
    push ax
    push bx
    push dx
    mov  ah,0x02
    xor  bh,bh
    int  0x10
.c: lodsb
    or   al,al
    jz   .done
    mov  ah,0x0E
    xor  bh,bh
    int  0x10
    jmp  .c
.done:
    pop  dx
    pop  bx
    pop  ax
    ret

; ================= screen 1: 16 color bars =================
scr_bars:
    xor  cx,cx               ; x
.bx:
    mov  ax,cx
    mov  bl,10
    div  bl                  ; AL = x/10 = color
    mov  bl,al
    xor  dx,dx               ; y
.by:
    mov  al,bl
    call pixel
    inc  dx
    cmp  dx,200
    jb   .by
    inc  cx
    cmp  cx,160
    jb   .bx
    ; XOR hex labels centered on each bar at row 12
    xor  si,si               ; i
.lb:
    mov  ax,si
    shl  ax,1                ; 2i
    mov  bx,ax
    shl  ax,1
    shl  ax,1                ; 8i
    add  ax,bx               ; 10i
    add  ax,3
    shr  ax,1
    shr  ax,1                ; col = (10i+3)/4
    mov  dl,al
    mov  dh,12
    mov  ah,0x02
    xor  bh,bh
    int  0x10
    mov  ax,si
    cmp  al,10
    jb   .dig
    add  al,7
.dig:
    add  al,'0'
    mov  ah,0x09
    mov  bl,0x8F             ; XOR white -> inverts the bar color
    xor  bh,bh
    mov  cx,1
    int  0x10
    inc  si
    cmp  si,16
    jb   .lb
    ret

; ================= screen 2: pattern-class zoo =================
; bands of 50 rows: solid / 1px vstripes / 1px hstripes / checker,
; 8 color-pair columns of 20 px
pairA db 1,2,3,4,5,6,7,8
pairB db 9,10,11,12,13,14,15,0

scr_zoo:
    xor  dx,dx               ; y
.y:
    xor  cx,cx               ; x
.x:
    ; band p = y/50, group g = x/20
    mov  ax,dx
    mov  bl,50
    div  bl
    mov  bh,al               ; BH = band
    mov  ax,cx
    mov  bl,20
    div  bl                  ; AL = group
    push bx
    mov  bx,ax
    and  bx,7
    mov  ah,[pairA+bx]       ; AH = color A
    mov  al,[pairB+bx]       ; AL = color B
    pop  bx
    ; select by class
    cmp  bh,0
    je   .useA
    cmp  bh,1
    jne  .n1
    test cl,1                ; vertical stripes
    jz   .useA
    jmp  .useB
.n1:
    cmp  bh,2
    jne  .n2
    test dl,1                ; horizontal stripes
    jz   .useA
    jmp  .useB
.n2:
    mov  bl,cl               ; checker
    xor  bl,dl
    test bl,1
    jz   .useA
.useB:
    jmp  .plot
.useA:
    mov  al,ah
.plot:
    call pixel
    inc  cx
    cmp  cx,160
    jb   .x
    inc  dx
    cmp  dx,200
    jb   .y
    ret

; ================= screen 3: concentric circles =================
CENTER_X equ 80
CENTER_Y equ 100

circol  db 0                 ; current circle color

scr_circles:
    mov  byte [circol],9
    mov  ax,95
    call circle
    mov  byte [circol],11
    mov  ax,75
    call circle
    mov  byte [circol],13
    mov  ax,55
    call circle
    mov  byte [circol],14
    mov  ax,35
    call circle
    mov  si,scgamsg
    mov  dl,18
    mov  dh,12
    mov  bl,0x8F
    call puts
    ret

; midpoint circle, AX = virtual radius (x doubled space)
circle:
    push si
    push di
    push bp
    xor  si,si               ; vx
    mov  di,ax               ; vy
    mov  bp,1
    sub  bp,ax               ; d = 1 - r
.loop:
    cmp  si,di
    jg   .done
    call plot8
    or   bp,bp
    js   .dneg
    mov  ax,si
    sub  ax,di
    shl  ax,1
    add  ax,5
    add  bp,ax
    dec  di
    jmp  .step
.dneg:
    mov  ax,si
    shl  ax,1
    add  ax,3
    add  bp,ax
.step:
    inc  si
    jmp  .loop
.done:
    pop  bp
    pop  di
    pop  si
    ret

plot8:
    mov  ax,si
    mov  bx,di
    call plotv
    mov  ax,si
    neg  ax
    mov  bx,di
    call plotv
    mov  ax,si
    mov  bx,di
    neg  bx
    call plotv
    mov  ax,si
    neg  ax
    mov  bx,di
    neg  bx
    call plotv
    mov  ax,di
    mov  bx,si
    call plotv
    mov  ax,di
    neg  ax
    mov  bx,si
    call plotv
    mov  ax,di
    mov  bx,si
    neg  bx
    call plotv
    mov  ax,di
    neg  ax
    mov  bx,si
    neg  bx
    call plotv
    ret

; virtual point: AX = vx, BX = vy (signed); x = vx/2, y = vy*5/6
plotv:
    push bx
    push cx
    push dx
    sar  ax,1
    add  ax,CENTER_X
    mov  cx,ax
    mov  dx,bx
    or   bx,bx
    jns  .pos
    neg  bx
.pos:
    mov  ax,bx
    shl  ax,1
    shl  ax,1
    add  ax,bx               ; |vy|*5
    mov  bl,6
    div  bl
    xor  ah,ah
    or   dx,dx
    jns  .apply
    neg  ax
.apply:
    add  ax,CENTER_Y
    mov  dx,ax
    mov  al,[circol]
    call pixel
    pop  dx
    pop  cx
    pop  bx
    ret

; ================= screen 4: starburst =================
raytab:
    dw 128,100
    dw 124,130
    dw 114,156
    dw 98,173
    dw 80,179
    dw 62,173
    dw 46,156
    dw 36,130
    dw 32,100
    dw 36,70
    dw 46,44
    dw 62,27
    dw 80,21
    dw 98,27
    dw 114,44
    dw 124,70

lx1 dw 0
ly1 dw 0
lcol db 0

scr_rays:
    xor  si,si               ; ray index
.r:
    mov  ax,si
    and  al,0x0F
    inc  al                  ; color 1..15 cycling
    cmp  al,16
    jb   .c
    mov  al,1
.c:
    mov  [lcol],al
    mov  bx,si
    shl  bx,1
    shl  bx,1                ; *4
    mov  ax,[raytab+bx]
    mov  [lx1],ax
    mov  ax,[raytab+bx+2]
    mov  [ly1],ax
    call line
    inc  si
    cmp  si,16
    jb   .r
    ret

; Bresenham line from (CENTER_X,CENTER_Y) to ([lx1],[ly1]) in [lcol]
ldx  dw 0
ldy  dw 0
lsx  dw 0
lsy  dw 0
lerr dw 0
lcx  dw 0
lcy  dw 0

line:
    push ax
    push bx
    push cx
    push dx
    mov  ax,CENTER_X
    mov  [lcx],ax
    mov  ax,CENTER_Y
    mov  [lcy],ax
    ; dx = |x1-x0|, sx = sign
    mov  ax,[lx1]
    sub  ax,[lcx]
    mov  bx,1
    or   ax,ax
    jns  .dx1
    neg  ax
    mov  bx,-1
.dx1:
    mov  [ldx],ax
    mov  [lsx],bx
    ; dy = -|y1-y0|, sy = sign
    mov  ax,[ly1]
    sub  ax,[lcy]
    mov  bx,1
    or   ax,ax
    jns  .dy1
    neg  ax
    mov  bx,-1
.dy1:
    neg  ax
    mov  [ldy],ax
    mov  [lsy],bx
    mov  ax,[ldx]
    add  ax,[ldy]
    mov  [lerr],ax
.pl:
    mov  cx,[lcx]
    mov  dx,[lcy]
    mov  al,[lcol]
    call pixel
    mov  ax,[lcx]
    cmp  ax,[lx1]
    jne  .go
    mov  ax,[lcy]
    cmp  ax,[ly1]
    je   .done
.go:
    mov  ax,[lerr]
    shl  ax,1                ; e2
    cmp  ax,[ldy]
    jl   .skipx
    mov  bx,[ldy]
    add  [lerr],bx
    mov  bx,[lsx]
    add  [lcx],bx
.skipx:
    cmp  ax,[ldx]
    jg   .pl
    mov  bx,[ldx]
    add  [lerr],bx
    mov  bx,[lsy]
    add  [lcy],bx
    jmp  .pl
.done:
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; ================= screen 5: text + scroll =================
scr_text:
    ; four color bands via AH=6 clear-fill
    mov  ax,0x0600
    mov  bh,1                ; blue rows 0-5
    xor  cx,cx
    mov  dx,0x0527
    int  0x10
    mov  ax,0x0600
    mov  bh,4                ; red rows 6-11
    mov  cx,0x0600
    mov  dx,0x0B27
    int  0x10
    mov  ax,0x0600
    mov  bh,2                ; green rows 12-17
    mov  cx,0x0C00
    mov  dx,0x1127
    int  0x10
    mov  ax,0x0600
    mov  bh,6                ; brown rows 18-24
    mov  cx,0x1200
    mov  dx,0x1827
    int  0x10
    ; normal text (fg on black cells)
    mov  si,normmsg
    mov  dl,2
    mov  dh,2
    mov  bl,15
    call puts
    ; XOR text on each band
    mov  si,xormsg
    mov  dl,2
    mov  dh,8
    mov  bl,0x8F
    call puts
    mov  si,xormsg
    mov  dl,2
    mov  dh,14
    mov  bl,0x8F
    call puts
    mov  si,xormsg
    mov  dl,2
    mov  dh,20
    mov  bl,0x8F
    call puts
    ; scroll exercise: middle region up 2 then down 1
    mov  ax,0x0602
    mov  bh,5                ; magenta blanks
    mov  cx,0x0605
    mov  dx,0x1122
    int  0x10
    mov  ax,0x0701
    mov  bh,3                ; cyan blank
    mov  cx,0x0605
    mov  dx,0x1122
    int  0x10
    ret

scgamsg db "SCGA",0
normmsg db "NORMAL TEXT (fg on black)",0
xormsg  db "XOR TEXT 8F INVERTS BAND",0
errmsg  db "SCGA TSR not loaded (mode 8 unavailable).",13,10,"$"
