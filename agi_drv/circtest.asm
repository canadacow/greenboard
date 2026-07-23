; circtest.asm -- CIRCTEST.COM: exercise the SCGA mode-8 TSR.
; Sets 160x200x16 mode 8, draws a circle via INT 10h write-pixel,
; labels it via INT 10h text, waits for a key, restores mode 3.
;
; The circle is computed in a 320-wide virtual space (x doubled) so it
; comes out roughly round on screen; radius 80 virtual = 40px x 80px.
;
; Assemble:  nasm -f bin -o CIRCTEST.COM circtest.asm

BITS 16
CPU 8086
ORG 0x100

CENTER_X equ 80              ; pixel coords
CENTER_Y equ 100
RADIUS   equ 80              ; virtual (double-x) units
CIRCOL   equ 14              ; yellow
TXTCOL   equ 15              ; white

start:
    mov  ax,0x0008           ; enter mode 8 (via TSR)
    int  0x10
    mov  ah,0x0F
    int  0x10
    cmp  al,0x08
    jne  notsr
    ; midpoint circle in virtual space: x=0, y=R, d=1-R
    xor  si,si               ; SI = vx
    mov  di,RADIUS           ; DI = vy
    mov  bp,1-RADIUS         ; BP = d
.loop:
    cmp  si,di
    jg   .done
    call plot8
    or   bp,bp
    js   .dneg
    mov  ax,si               ; d >= 0: d += 2(x-y)+5, y--
    sub  ax,di
    shl  ax,1
    add  ax,5
    add  bp,ax
    dec  di
    jmp  .step
.dneg:
    mov  ax,si               ; d < 0: d += 2x+3
    shl  ax,1
    add  ax,3
    add  bp,ax
.step:
    inc  si
    jmp  .loop
.done:
    ; label: "CIRCLE" centered at text row 12
    mov  ah,0x02
    xor  bh,bh
    mov  dx,0x0C11           ; row 12, col 17
    int  0x10
    mov  si,msg
.t:
    lodsb
    or   al,al
    jz   .wait
    mov  ah,0x0E
    mov  bl,TXTCOL
    xor  bh,bh
    int  0x10
    jmp  .t
.wait:
    xor  ah,ah               ; wait for key
    int  0x16
    mov  ax,0x0003           ; back to text mode (TSR deactivates)
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

; plot the 8 octant points for virtual (SI, DI)
plot8:
    mov  ax,si
    mov  bx,di
    call plotv               ; ( vx,  vy)
    mov  ax,si
    neg  ax
    mov  bx,di
    call plotv               ; (-vx,  vy)
    mov  ax,si
    mov  bx,di
    neg  bx
    call plotv               ; ( vx, -vy)
    mov  ax,si
    neg  ax
    mov  bx,di
    neg  bx
    call plotv               ; (-vx, -vy)
    mov  ax,di
    mov  bx,si
    call plotv               ; ( vy,  vx)
    mov  ax,di
    neg  ax
    mov  bx,si
    call plotv               ; (-vy,  vx)
    mov  ax,di
    mov  bx,si
    neg  bx
    call plotv               ; ( vy, -vx)
    mov  ax,di
    neg  ax
    mov  bx,si
    neg  bx
    call plotv               ; (-vy, -vx)
    ret

; plot virtual point: AX = vx (signed), BX = vy (signed)
; pixel x = vx/2, pixel y = vy*5/6  (160x200 on 4:3: px aspect 5:3)
plotv:
    push bx
    push cx
    push dx
    sar  ax,1                ; pixel x offset = vx/2
    add  ax,CENTER_X
    mov  cx,ax
    mov  dx,bx               ; DX = vy sign carrier
    or   bx,bx
    jns  .pos
    neg  bx
.pos:
    mov  ax,bx
    shl  ax,1
    shl  ax,1
    add  ax,bx               ; |vy| * 5
    mov  bl,6
    div  bl                  ; AL = |vy| * 5 / 6
    xor  ah,ah
    or   dx,dx
    jns  .apply
    neg  ax
.apply:
    add  ax,CENTER_Y
    mov  dx,ax
    mov  al,CIRCOL
    mov  ah,0x0C
    xor  bh,bh
    int  0x10
    pop  dx
    pop  cx
    pop  bx
    ret

msg    db "CIRCLE",0
errmsg db "SCGA TSR not loaded (mode 8 unavailable).",13,10,"$"
