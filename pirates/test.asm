0023C  33c0  xor ax, ax
0023E  8ed8  mov ds, ax
00240  be7800  mov si, 0x78
00243  ad  lodsw ax, word ptr [si]
00244  8bd8  mov bx, ax
00246  ad  lodsw ax, word ptr [si]
00247  8ed8  mov ds, ax
00249  8bf3  mov si, bx
0024B  b82000  mov ax, 0x20
0024E  8ec0  mov es, ax
00250  bf2c00  mov di, 0x2c
00253  b91000  mov cx, 0x10
00256  f3a4  rep movsb byte ptr es:[di], byte ptr [si]
00257  a4  movsb byte ptr es:[di], byte ptr [si]
00258  b82000  mov ax, 0x20
0025B  8ed8  mov ds, ax
0025D  33c0  xor ax, ax
0025F  8ec0  mov es, ax
00261  bf7800  mov di, 0x78
00264  b82c00  mov ax, 0x2c
00267  ab  stosw word ptr es:[di], ax
00268  b82000  mov ax, 0x20
0026B  ab  stosw word ptr es:[di], ax
0026C  b800f0  mov ax, 0xf000
0026F  8ec0  mov es, ax
00271  26803efeffff  cmp byte ptr es:[0xfffe], 0xff
00272  803efeffff  cmp byte ptr [0xfffe], 0xff
00277  7508  jne 0x281
00279  26803e00c021  cmp byte ptr es:[0xc000], 0x21
0027A  803e00c021  cmp byte ptr [0xc000], 0x21
0027F  7412  je 0x293
00281  b84000  mov ax, 0x40
00284  8ec0  mov es, ax
00286  bf1000  mov di, 0x10
00289  268125cf00  and word ptr es:[di], 0xcf
0028A  8125cf00  and word ptr [di], 0xcf
0028E  26810d1000  or word ptr es:[di], 0x10
0028F  810d1000  or word ptr [di], 0x10
00293  b80400  mov ax, 4
00296  cd10  int 0x10
00298  b84000  mov ax, 0x40
0029B  8ec0  mov es, ax
0029D  26a06500  mov al, byte ptr es:[0x65]
0029E  a06500  mov al, byte ptr [0x65]
002A1  0c04  or al, 4
002A3  bad803  mov dx, 0x3d8
002A6  ee  out dx, al
002A7  bf0400  mov di, 4
002AA  b800b8  mov ax, 0xb800
002AD  8ec0  mov es, ax
002AF  33db  xor bx, bx
002B1  32d2  xor dl, dl
002B3  b526  mov ch, 0x26
002B5  b600  mov dh, 0
002B7  b101  mov cl, 1
002B9  be0500  mov si, 5
002BC  b80802  mov ax, 0x208
002BF  cd13  int 0x13
002C1  7306  jae 0x2c9
002C9  81c30010  add bx, 0x1000
002CD  4f  dec di
002CE  740b  je 0x2db
002D0  fec6  inc dh
002D2  80e601  and dh, 1
002D5  75e0  jne 0x2b7
002D7  fec5  inc ch
002D9  ebda  jmp 0x2b5
002DB  c70602005000  mov word ptr [2], 0x50
002E1  c70604000000  mov word ptr [4], 0
002E7  c606010001  mov byte ptr [1], 1
002EC  c606060015  mov byte ptr [6], 0x15
002F1  be0a00  mov si, 0xa
002F4  b8802d  mov ax, 0x2d80
002F7  8ec0  mov es, ax
002F9  33db  xor bx, bx
002FB  33d2  xor dx, dx
002FD  b101  mov cl, 1
002FF  8a2e0100  mov ch, byte ptr [1]
00303  b008  mov al, 8
00305  b402  mov ah, 2
00307  cd13  int 0x13
00309  7311  jae 0x31c
0031C  be0a00  mov si, 0xa
0031F  b8802d  mov ax, 0x2d80
00322  8ec0  mov es, ax
00324  bb0010  mov bx, 0x1000
00327  ba0001  mov dx, 0x100
0032A  b101  mov cl, 1
0032C  8a2e0100  mov ch, byte ptr [1]
00330  b008  mov al, 8
00332  b402  mov ah, 2
00334  cd13  int 0x13
00336  7311  jae 0x349
00349  33ff  xor di, di
0034B  a10200  mov ax, word ptr [2]
0034E  8ec0  mov es, ax
00350  33f6  xor si, si
00352  bb802d  mov bx, 0x2d80
00355  1e  push ds
00356  8edb  mov ds, bx
00358  b90020  mov cx, 0x2000
0035B  f3a4  rep movsb byte ptr es:[di], byte ptr [si]
0035C  a4  movsb byte ptr es:[di], byte ptr [si]
0035D  1f  pop ds
0035E  810602000002  add word ptr [2], 0x200
00364  fe060100  inc byte ptr [1]
00368  803e010004  cmp byte ptr [1], 4
0036D  7504  jne 0x373
0036F  fe060100  inc byte ptr [1]
00373  a00100  mov al, byte ptr [1]
00376  3a060600  cmp al, byte ptr [6]
0037A  7f03  jg 0x37f
0037C  e972ff  jmp 0x2f1
0037F  33ff  xor di, di
00381  b600  mov dh, 0
00383  b85000  mov ax, 0x50
00386  8ed8  mov ds, ax
00388  b91000  mov cx, 0x10
0038B  818500005000  add word ptr [di], 0x50
00391  47  inc di
00392  47  inc di
00393  e2f6  loop 0x38b
00395  ea20005000  ljmp 0x50:0x20