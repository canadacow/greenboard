111E3  8d1e8500  lea bx, [0x85]
111E7  eb58  jmp 0x11241
1123C  c3  ret
11241  891ed70a  mov word ptr [0xad7], bx
11245  e8e1ff  call 0x11229
11248  75f2  jne 0x1123c
1124A  8b1ed70a  mov bx, word ptr [0xad7]
1124E  8b07  mov ax, word ptr [bx]
11250  3d0000  cmp ax, 0
11253  74e7  je 0x1123c
11255  8bf0  mov si, ax
11257  43  inc bx
11258  43  inc bx
11259  8b17  mov dx, word ptr [bx]
1125B  43  inc bx
1125C  43  inc bx
1125D  8af2  mov dh, dl
1125F  b202  mov dl, 2
11261  8b0f  mov cx, word ptr [bx]
11263  43  inc bx
11264  43  inc bx
11265  891ed70a  mov word ptr [0xad7], bx
11269  8af9  mov bh, cl
1126B  b101  mov cl, 1
1126D  32db  xor bl, bl
1126F  bdff02  mov bp, 0x2ff
11272  80ff00  cmp bh, 0
11275  7506  jne 0x1127d
11277  81f50002  xor bp, 0x200
1127B  b764  mov bh, 0x64
1127D  33c0  xor ax, ax
1127F  8bf8  mov di, ax
11281  80ea01  sub dl, 1
11284  d1d0  rcl ax, 1
11286  f7d8  neg ax
11288  22c6  and al, dh
1128A  02d0  add dl, al
1128C  8bf8  mov di, ax
1128E  33c0  xor ax, ax
11290  80e901  sub cl, 1
11293  d1d0  rcl ax, 1
11295  f7d8  neg ax
11297  22c5  and al, ch
11299  02c8  add cl, al
1129B  0bf8  or di, ax
1129D  33c0  xor ax, ax
1129F  80eb01  sub bl, 1
112A2  d1d0  rcl ax, 1
112A4  2bf0  sub si, ax
112A6  749d  je 0x11245
112A8  f7d8  neg ax
112AA  22c7  and al, bh
112AC  02d8  add bl, al
112AE  0bc7  or ax, di
112B0  b04c  mov al, 0x4c
112B2  e661  out 0x61, al
112B4  23c5  and ax, bp
112B6  0ac4  or al, ah
112B8  e661  out 0x61, al
112BA  8bc1  mov ax, cx
112BC  8b0edb0a  mov cx, word ptr [0xadb]
112C0  e2fe  loop 0x112c0
112C2  8bc8  mov cx, ax
112C4  ebb7  jmp 0x1127d