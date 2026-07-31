0D124  c606c0400f  mov byte ptr [0x40c0], 0xf
0D129  c606c1400f  mov byte ptr [0x40c1], 0xf
0D12E  833e074000  cmp word ptr [0x4007], 0
0D133  7403  je 0D138
0D138  a04148  mov al, byte ptr [0x4841]
0D13B  32e4  xor ah, ah
0D13D  b108  mov cl, 8
0D13F  d3e0  shl ax, cl
0D141  058000  add ax, 0x80
0D144  a3e53f  mov word ptr [0x3fe5], ax
0D147  a04248  mov al, byte ptr [0x4842]
0D14A  32e4  xor ah, ah
0D14C  b107  mov cl, 7
0D14E  d3e0  shl ax, cl
0D150  054000  add ax, 0x40
0D153  a3e73f  mov word ptr [0x3fe7], ax
0D156  c706e93f4000  mov word ptr [0x3fe9], 0x40
0D15C  c706f33fc000  mov word ptr [0x3ff3], 0xc0
0D162  81260b401f00  and word ptr [0x400b], 0x1f
0D168  c706ef3f0000  mov word ptr [0x3fef], 0
0D16E  bb0100  mov bx, 1
0D171  8a164f48  mov dl, byte ptr [0x484f]
0D175  32f6  xor dh, dh
0D177  8a875348  mov al, byte ptr [bx + 0x4853]
0D17B  250700  and ax, 7
0D17E  a30140  mov word ptr [0x4001], ax
0D181  3b06ef3f  cmp ax, word ptr [0x3fef]
0D185  7e03  jle 0D18A
0D187  a3ef3f  mov word ptr [0x3fef], ax
0D18A  43  inc bx
0D18B  3bda  cmp bx, dx
0D18D  7ee8  jle 0D177
0D18F  bb0000  mov bx, 0
0D192  8a87d040  mov al, byte ptr [bx + 0x40d0]
0D196  8887b740  mov byte ptr [bx + 0x40b7], al
0D19A  8bc3  mov ax, bx
0D19C  250100  and ax, 1
0D19F  050500  add ax, 5
0D1A2  88879740  mov byte ptr [bx + 0x4097], al
0D1A6  43  inc bx
0D1A7  83fb04  cmp bx, 4
0D1AA  72e6  jb 0D192
0D1AC  b000  mov al, 0
0D1AE  b307  mov bl, 7
0D1B0  88879f40  mov byte ptr [bx + 0x409f], al
0D1B4  88a7a740  mov byte ptr [bx + 0x40a7], ah
0D1B8  8887af40  mov byte ptr [bx + 0x40af], al
0D1BC  4b  dec bx
0D1BD  79f1  jns 0D1B0
0D1BF  b80400  mov ax, 4
0D1C2  8b1eed3f  mov bx, word ptr [0x3fed]
0D1C6  d1e3  shl bx, 1
0D1C8  03d8  add bx, ax
0D1CA  ba0500  mov dx, 5
0D1CD  0316ed3f  add dx, word ptr [0x3fed]
0D1D1  b8b400  mov ax, 0xb4
0D1D4  88879f40  mov byte ptr [bx + 0x409f], al
0D1D8  88a7a740  mov byte ptr [bx + 0x40a7], ah
0D1DC  c687af4090  mov byte ptr [bx + 0x40af], 0x90
0D1E1  43  inc bx
0D1E2  3bda  cmp bx, dx
0D1E4  7eeb  jle 0D1D1
0D1E6  c606b54096  mov byte ptr [0x40b5], 0x96
0D1EB  a01241  mov al, byte ptr [0x4112]
0D1EE  a2bb40  mov byte ptr [0x40bb], al
0D1F1  a01341  mov al, byte ptr [0x4113]
0D1F4  a2bd40  mov byte ptr [0x40bd], al
0D1F7  a2be40  mov byte ptr [0x40be], al
0D1FA  c70607400200  mov word ptr [0x4007], 2
0D200  c706f53fe703  mov word ptr [0x3ff5], 0x3e7
0D206  c3  ret