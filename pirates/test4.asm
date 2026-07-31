0A3CC  2ea10000  mov ax, word ptr cs:[0]
0A3CD  a10000  mov ax, word ptr [0]
0A3D0  8ed8  mov ds, ax
0A3D2  2ea10400  mov ax, word ptr cs:[4]
0A3D3  a10400  mov ax, word ptr [4]
0A3D6  8ec0  mov es, ax
0A3D8  a18340  mov ax, word ptr [0x4083]
0A3DB  26a30800  mov word ptr es:[8], ax
0A3DC  a30800  mov word ptr [8], ax
0A3DF  a18540  mov ax, word ptr [0x4085]
0A3E2  26a30a00  mov word ptr es:[0xa], ax
0A3E3  a30a00  mov word ptr [0xa], ax
0A3E6  8b368740  mov si, word ptr [0x4087]
0A3EA  d1e6  shl si, 1
0A3EC  268b9c4000  mov bx, word ptr es:[si + 0x40]
0A3ED  8b9c4000  mov bx, word ptr [si + 0x40]
0A3F1  268a07  mov al, byte ptr es:[bx]
0A3F2  8a07  mov al, byte ptr [bx]
0A3F4  32e4  xor ah, ah
0A3F6  03068940  add ax, word ptr [0x4089]
0A3FA  26a30200  mov word ptr es:[2], ax
0A3FB  a30200  mov word ptr [2], ax
0A3FE  268a4701  mov al, byte ptr es:[bx + 1]
0A3FF  8a4701  mov al, byte ptr [bx + 1]
0A402  32e4  xor ah, ah
0A404  03068b40  add ax, word ptr [0x408b]
0A408  26a30000  mov word ptr es:[0], ax
0A409  a30000  mov word ptr [0], ax
0A40C  268a4702  mov al, byte ptr es:[bx + 2]
0A40D  8a4702  mov al, byte ptr [bx + 2]
0A410  32e4  xor ah, ah
0A412  26a30400  mov word ptr es:[4], ax
0A413  a30400  mov word ptr [4], ax
0A416  268a4703  mov al, byte ptr es:[bx + 3]
0A417  8a4703  mov al, byte ptr [bx + 3]
0A41A  26a30600  mov word ptr es:[6], ax
0A41B  a30600  mov word ptr [6], ax
0A41E  83c304  add bx, 4
0A421  26891e0c00  mov word ptr es:[0xc], bx
0A422  891e0c00  mov word ptr [0xc], bx
0A426  833e983b01  cmp word ptr [0x3b98], 1
0A42B  7405  je 0xa432
0A42D  7706  ja 0xa435
0A42F  e9c400  jmp 0xa4f6
0A4F6  2ea10400  mov ax, word ptr cs:[4]
0A4F7  a10400  mov ax, word ptr [4]
0A4FA  8ed8  mov ds, ax
0A4FC  2ea10000  mov ax, word ptr cs:[0]
0A4FD  a10000  mov ax, word ptr [0]
0A500  8ec0  mov es, ax
0A502  26a18d40  mov ax, word ptr es:[0x408d]
0A503  a18d40  mov ax, word ptr [0x408d]
0A506  8ae0  mov ah, al
0A508  24c0  and al, 0xc0
0A50A  a21c00  mov byte ptr [0x1c], al
0A50D  8ac4  mov al, ah
0A50F  2430  and al, 0x30
0A511  a21d00  mov byte ptr [0x1d], al
0A514  8ac4  mov al, ah
0A516  240c  and al, 0xc
0A518  a21e00  mov byte ptr [0x1e], al
0A51B  80e403  and ah, 3
0A51E  88261f00  mov byte ptr [0x1f], ah
0A522  8b1e0000  mov bx, word ptr [0]
0A526  d1e3  shl bx, 1
0A528  a10200  mov ax, word ptr [2]
0A52B  d1e8  shr ax, 1
0A52D  d1e8  shr ax, 1
0A52F  260387a13c  add ax, word ptr es:[bx + 0x3ca1]
0A530  0387a13c  add ax, word ptr [bx + 0x3ca1]
0A534  a30e00  mov word ptr [0xe], ax
0A537  26a1109d  mov ax, word ptr es:[0x9d10]
0A538  a1109d  mov ax, word ptr [0x9d10]
0A53B  8ec0  mov es, ax
0A53D  813e0000c800  cmp word ptr [0], 0xc8
0A543  7203  jb 0xa548
0A545  eb76  jmp 0xa5bd
0A548  8b3e0e00  mov di, word ptr [0xe]
0A54C  8b0e0400  mov cx, word ptr [4]
0A550  8b1e0c00  mov bx, word ptr [0xc]
0A554  8b2e0200  mov bp, word ptr [2]
0A558  8bf5  mov si, bp
0A55A  81e60300  and si, 3
0A55E  3b2e0800  cmp bp, word ptr [8]
0A562  7224  jb 0xa588
0A564  3b2e0a00  cmp bp, word ptr [0xa]
0A568  731e  jae 0xa588
0A56A  8a07  mov al, byte ptr [bx]
0A56C  22842400  and al, byte ptr [si + 0x24]
0A570  7416  je 0xa588
0A572  3a841800  cmp al, byte ptr [si + 0x18]
0A576  7504  jne 0xa57c
0A578  8a841c00  mov al, byte ptr [si + 0x1c]
0A57C  268a25  mov ah, byte ptr es:[di]
0A57D  8a25  mov ah, byte ptr [di]
0A57F  22a42000  and ah, byte ptr [si + 0x20]
0A583  0ae0  or ah, al
0A585  268825  mov byte ptr es:[di], ah
0A586  8825  mov byte ptr [di], ah
0A588  45  inc bp
0A589  46  inc si
0A58A  81e60300  and si, 3
0A58E  7501  jne 0xa591
0A590  47  inc di
0A591  43  inc bx
0A592  e2ca  loop 0xa55e
0A594  813e0e000020  cmp word ptr [0xe], 0x2000
0A59A  7208  jb 0xa5a4
0A59C  812e0e00b01f  sub word ptr [0xe], 0x1fb0
0A5A2  eb06  jmp 0xa5aa
0A5A4  81060e000020  add word ptr [0xe], 0x2000
0A5AA  ff060000  inc word ptr [0]
0A5AE  a10400  mov ax, word ptr [4]
0A5B1  01060c00  add word ptr [0xc], ax
0A5B5  ff0e0600  dec word ptr [6]
0A5B9  7402  je 0xa5bd
0A5BB  eb80  jmp 0xa53d
0A5BD  2ea10000  mov ax, word ptr cs:[0]
0A5BE  a10000  mov ax, word ptr [0]
0A5C1  8ed8  mov ds, ax
0A5C3  8ec0  mov es, ax
0A5C5  c3  ret