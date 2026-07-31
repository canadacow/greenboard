0DD40  c706acc01300  mov word ptr [0xc0ac], 0x13
0DD46  b8e000  mov ax, 0xe0
0DD49  e89430  call 00DE0
0DD4C  e8664f  call 02CB5
0DD4F  eb03  jmp 0DD54
0DD51  e8644f  call 02CB8
0DD54  b80300  mov ax, 3
0DD57  a30a3c  mov word ptr [0x3c0a], ax
0DD5A  c7061d400000  mov word ptr [0x401d], 0
0DD60  833e0b4000  cmp word ptr [0x400b], 0
0DD65  7509  jne 0DD70
0DD67  e8340c  call 0E99E
0DD6A  e8ec0b  call 0E959
0DD6D  e8860e  call 0EBF6
0DD70  833e0b4001  cmp word ptr [0x400b], 1
0DD75  7506  jne 0DD7D
0DD77  c7061d400400  mov word ptr [0x401d], 4
0DD7D  833e0b4002  cmp word ptr [0x400b], 2
0DD82  7500  jne 0DD84
0DD84  ff060b40  inc word ptr [0x400b]
0DD88  ff062140  inc word ptr [0x4021]
0DD8C  813e2140ee02  cmp word ptr [0x4021], 0x2ee
0DD92  7e06  jle 0DD9A
0DD9A  a1453b  mov ax, word ptr [0x3b45]
0DD9D  a3e33f  mov word ptr [0x3fe3], ax
0DDA0  f726fb3f  mul word ptr [0x3ffb]
0DDA4  0306e93f  add ax, word ptr [0x3fe9]
0DDA8  32e4  xor ah, ah
0DDAA  a3e93f  mov word ptr [0x3fe9], ax
0DDAD  a1433b  mov ax, word ptr [0x3b43]
0DDB0  f7d8  neg ax
0DDB2  a30140  mov word ptr [0x4001], ax
0DDB5  e811cc  call 0A9C9
0DDB8  3d0000  cmp ax, 0
0DDBB  7e23  jle 0DDE0
0DDBD  833ee33f00  cmp word ptr [0x3fe3], 0
0DDC2  751c  jne 0DDE0
0DDE0  833ef93f00  cmp word ptr [0x3ff9], 0
0DDE5  7e1e  jle 0DE05
0DE05  833e1f4000  cmp word ptr [0x401f], 0
0DE0A  7e21  jle 0DE2D
0DE2D  a1e93f  mov ax, word ptr [0x3fe9]
0DE30  050800  add ax, 8
0DE33  2b06f33f  sub ax, word ptr [0x3ff3]
0DE37  b104  mov cl, 4
0DE39  d3e8  shr ax, cl
0DE3B  250f00  and ax, 0xf
0DE3E  058c56  add ax, 0x568c
0DE41  8b1eef3f  mov bx, word ptr [0x3fef]
0DE45  b104  mov cl, 4
0DE47  d3e3  shl bx, cl
0DE49  03d8  add bx, ax
0DE4B  8a07  mov al, byte ptr [bx]
0DE4D  98  cwde
0DE4E  d1e0  shl ax, 1
0DE50  d1e0  shl ax, 1
0DE52  8b1ef53f  mov bx, word ptr [0x3ff5]
0DE56  d1fb  sar bx, 1
0DE58  d1fb  sar bx, 1
0DE5A  2bc3  sub ax, bx
0DE5C  833ef73f00  cmp word ptr [0x3ff7], 0
0DE61  7e06  jle 0DE69
0DE69  b103  mov cl, 3
0DE6B  d3e0  shl ax, cl
0DE6D  a34d3b  mov word ptr [0x3b4d], ax
0DE70  b80300  mov ax, 3
0DE73  2b06fb3f  sub ax, word ptr [0x3ffb]
0DE77  f726fd3f  mul word ptr [0x3ffd]
0DE7B  bb0800  mov bx, 8
0DE7E  2bd8  sub bx, ax
0DE80  a14d3b  mov ax, word ptr [0x3b4d]
0DE83  e87158  call 036F7
0DE86  a30940  mov word ptr [0x4009], ax
0DE89  c7067a3b1500  mov word ptr [0x3b7a], 0x15
0DE8F  c706783b0d00  mov word ptr [0x3b78], 0xd
0DE95  d1f8  sar ax, 1
0DE97  d1f8  sar ax, 1
0DE99  b102  mov cl, 2
0DE9B  e832cb  call 0A9D0
0DE9E  a1e93f  mov ax, word ptr [0x3fe9]
0DEA1  a30140  mov word ptr [0x4001], ax
0DEA4  833e094000  cmp word ptr [0x4009], 0
0DEA9  7d0f  jge 0DEBA
0DEBA  8b0e0940  mov cx, word ptr [0x4009]
0DEBE  a10140  mov ax, word ptr [0x4001]
0DEC1  e8c6ce  call 0AD8A
0DEC4  0106e53f  add word ptr [0x3fe5], ax
0DEC8  8b0e0940  mov cx, word ptr [0x4009]
0DECC  a10140  mov ax, word ptr [0x4001]
0DECF  e8d3ce  call 0ADA5
0DED2  2906e73f  sub word ptr [0x3fe7], ax
0DED6  a1e53f  mov ax, word ptr [0x3fe5]
0DED9  2b061340  sub ax, word ptr [0x4013]
0DEDD  b105  mov cl, 5
0DEDF  d3f8  sar ax, cl
0DEE1  a34d3b  mov word ptr [0x3b4d], ax
0DEE4  a30340  mov word ptr [0x4003], ax
0DEE7  bb2000  mov bx, 0x20
0DEEA  ba3801  mov dx, 0x138
0DEED  e8b0ca  call 0A9A0
0DEF0  3d0000  cmp ax, 0
0DEF3  7503  jne 0DEF8
0DEF8  a14d3b  mov ax, word ptr [0x3b4d]
0DEFB  a29f40  mov byte ptr [0x409f], al
0DEFE  8826a740  mov byte ptr [0x40a7], ah
0DF02  a1e73f  mov ax, word ptr [0x3fe7]
0DF05  2b061540  sub ax, word ptr [0x4015]
0DF09  b106  mov cl, 6
0DF0B  d3f8  sar ax, cl
0DF0D  a30540  mov word ptr [0x4005], ax
0DF10  a34d3b  mov word ptr [0x3b4d], ax
0DF13  bb3200  mov bx, 0x32
0DF16  bac400  mov dx, 0xc4
0DF19  e884ca  call 0A9A0
0DF1C  3d0000  cmp ax, 0
0DF1F  7503  jne 0DF24
0DF24  a14d3b  mov ax, word ptr [0x3b4d]
0DF27  a2af40  mov byte ptr [0x40af], al
0DF2A  a1e93f  mov ax, word ptr [0x3fe9]
0DF2D  050800  add ax, 8
0DF30  b104  mov cl, 4
0DF32  d3e8  shr ax, cl
0DF34  250f00  and ax, 0xf
0DF37  052000  add ax, 0x20
0DF3A  8b36ef3f  mov si, word ptr [0x3fef]
0DF3E  0284c240  add al, byte ptr [si + 0x40c2]
0DF42  a29740  mov byte ptr [0x4097], al
0DF45  833e274000  cmp word ptr [0x4027], 0
0DF4A  7e3f  jle 0DF8B
0DF8B  a01541  mov al, byte ptr [0x4115]
0DF8E  833e274000  cmp word ptr [0x4027], 0
0DF93  7403  je 0DF98
0DF98  a2b940  mov byte ptr [0x40b9], al
0DF9B  c6069b4009  mov byte ptr [0x409b], 9
0DFA0  c6069d4009  mov byte ptr [0x409d], 9
0DFA5  833e0d4000  cmp word ptr [0x400d], 0
0DFAA  7403  je 0DFAF
0DFAF  b80000  mov ax, 0
0DFB2  a2a340  mov byte ptr [0x40a3], al
0DFB5  a2ab40  mov byte ptr [0x40ab], al
0DFB8  a2a540  mov byte ptr [0x40a5], al
0DFBB  a2ad40  mov byte ptr [0x40ad], al
0DFBE  833e493b00  cmp word ptr [0x3b49], 0
0DFC3  7f03  jg 0DFC8
0DFC5  eb74  jmp 0E03B
0E03B  e99301  jmp 0E1D1
0E1D1  a15d40  mov ax, word ptr [0x405d]
0E1D4  e8e4c7  call 0A9BB
0E1D7  29065d40  sub word ptr [0x405d], ax
0E1DB  a11b40  mov ax, word ptr [0x401b]
0E1DE  b107  mov cl, 7
0E1E0  d3e8  shr ax, cl
0E1E2  f7260940  mul word ptr [0x4009]
0E1E6  a30940  mov word ptr [0x4009], ax
0E1E9  8b0e0940  mov cx, word ptr [0x4009]
0E1ED  a1e93f  mov ax, word ptr [0x3fe9]
0E1F0  e897cb  call 0AD8A
0E1F3  0306e53f  add ax, word ptr [0x3fe5]
0E1F7  2b064340  sub ax, word ptr [0x4043]
0E1FB  a30340  mov word ptr [0x4003], ax
0E1FE  8b0e0940  mov cx, word ptr [0x4009]
0E202  a1e93f  mov ax, word ptr [0x3fe9]
0E205  e89dcb  call 0ADA5
0E208  8bd8  mov bx, ax
0E20A  a1e73f  mov ax, word ptr [0x3fe7]
0E20D  2bc3  sub ax, bx
0E20F  2b064540  sub ax, word ptr [0x4045]
0E213  a30540  mov word ptr [0x4005], ax
0E216  8b0e0340  mov cx, word ptr [0x4003]
0E21A  a10540  mov ax, word ptr [0x4005]
0E21D  f7d8  neg ax
0E21F  e88acb  call 0ADAC
0E222  d1f8  sar ax, 1
0E224  a30740  mov word ptr [0x4007], ax
0E227  a10340  mov ax, word ptr [0x4003]
0E22A  e89cc7  call 0A9C9
0E22D  a30140  mov word ptr [0x4001], ax
0E230  a34d3b  mov word ptr [0x3b4d], ax
0E233  a10540  mov ax, word ptr [0x4005]
0E236  e890c7  call 0A9C9
0E239  3b060140  cmp ax, word ptr [0x4001]
0E23D  7e04  jle 0E243
0E23F  87064d3b  xchg word ptr [0x3b4d], ax
0E243  d1e8  shr ax, 1
0E245  03064d3b  add ax, word ptr [0x3b4d]
0E249  a31b40  mov word ptr [0x401b], ax
0E24C  8b1e2740  mov bx, word ptr [0x4027]
0E250  b109  mov cl, 9
0E252  d3e3  shl bx, cl
0E254  81c3f401  add bx, 0x1f4
0E258  3bc3  cmp ax, bx
0E25A  7d06  jge 0E262
0E262  a10b40  mov ax, word ptr [0x400b]
0E265  25ff00  and ax, 0xff
0E268  7542  jne 0E2AC
0E2AC  813e1b403421  cmp word ptr [0x401b], 0x2134
0E2B2  7e06  jle 0E2BA
0E2BA  833e274000  cmp word ptr [0x4027], 0
0E2BF  7403  je 0E2C4
0E2C4  a10b40  mov ax, word ptr [0x400b]
0E2C7  251f00  and ax, 0x1f
0E2CA  7403  je 0E2CF
0E2CC  e93f01  jmp 0E40E
0E2CF  8b1e0b40  mov bx, word ptr [0x400b]
0E2D3  81e33f00  and bx, 0x3f
0E2D7  b104  mov cl, 4
0E2D9  d3e3  shl bx, cl
0E2DB  a11b40  mov ax, word ptr [0x401b]
0E2DE  2bc3  sub ax, bx
0E2E0  8bd8  mov bx, ax
0E2E2  d1eb  shr bx, 1
0E2E4  d1eb  shr bx, 1
0E2E6  03c3  add ax, bx
0E2E8  b10c  mov cl, 0xc
0E2EA  d3e8  shr ax, cl
0E2EC  bb1b00  mov bx, 0x1b
0E2EF  f7e3  mul bx
0E2F1  a34d3b  mov word ptr [0x3b4d], ax
0E2F4  a1f53f  mov ax, word ptr [0x3ff5]
0E2F7  d1e8  shr ax, 1
0E2F9  40  inc ax
0E2FA  3b065540  cmp ax, word ptr [0x4055]
0E2FE  7e05  jle 0E305
0E300  83064d3b09  add word ptr [0x3b4d], 9
0E305  a1f53f  mov ax, word ptr [0x3ff5]
0E308  d1e0  shl ax, 1
0E30A  3b065540  cmp ax, word ptr [0x4055]
0E30E  7e05  jle 0E315
0E315  a1eb3f  mov ax, word ptr [0x3feb]
0E318  d1e8  shr ax, 1
0E31A  40  inc ax
0E31B  3b065740  cmp ax, word ptr [0x4057]
0E31F  7d05  jge 0E326
0E321  83064d3b03  add word ptr [0x3b4d], 3
0E326  a1eb3f  mov ax, word ptr [0x3feb]
0E329  d1e0  shl ax, 1
0E32B  3b065740  cmp ax, word ptr [0x4057]
0E32F  7d05  jge 0E336
0E336  a15940  mov ax, word ptr [0x4059]
0E339  f7265f40  mul word ptr [0x405f]
0E33D  b103  mov cl, 3
0E33F  d3e8  shr ax, cl
0E341  a30140  mov word ptr [0x4001], ax
0E344  a1ed3f  mov ax, word ptr [0x3fed]
0E347  d1e8  shr ax, 1
0E349  40  inc ax
0E34A  3b060140  cmp ax, word ptr [0x4001]
0E34E  7d04  jge 0E354
0E350  ff064d3b  inc word ptr [0x3b4d]
0E354  a1ed3f  mov ax, word ptr [0x3fed]
0E357  d1e0  shl ax, 1
0E359  3b060140  cmp ax, word ptr [0x4001]
0E35D  7d04  jge 0E363
0E363  833e4d3b00  cmp word ptr [0x3b4d], 0
0E368  750e  jne 0E378
0E378  bb8c56  mov bx, 0x568c
0E37B  031e4d3b  add bx, word ptr [0x3b4d]
0E37F  81c38000  add bx, 0x80
0E383  8a07  mov al, byte ptr [bx]
0E385  32e4  xor ah, ah
0E387  a32b40  mov word ptr [0x402b], ax
0E38A  833e1d4000  cmp word ptr [0x401d], 0
0E38F  7518  jne 0E3A9
0E391  a12b40  mov ax, word ptr [0x402b]
0E394  250100  and ax, 1
0E397  350100  xor ax, 1
0E39A  3b066140  cmp ax, word ptr [0x4061]
0E39E  7406  je 0E3A6
0E3A6  a36140  mov word ptr [0x4061], ax
0E3A9  a12b40  mov ax, word ptr [0x402b]
0E3AC  b106  mov cl, 6
0E3AE  d3e0  shl ax, cl
0E3B0  03060740  add ax, word ptr [0x4007]
0E3B4  a34d3b  mov word ptr [0x3b4d], ax
0E3B7  833e2b4001  cmp word ptr [0x402b], 1
0E3BC  741e  je 0E3DC
0E3DC  2b064740  sub ax, word ptr [0x4047]
0E3E0  98  cwde
0E3E1  e8e5c5  call 0A9C9
0E3E4  3d4000  cmp ax, 0x40
0E3E7  7e06  jle 0E3EF
0E3E9  81064d3b8000  add word ptr [0x3b4d], 0x80
0E3EF  a14d3b  mov ax, word ptr [0x3b4d]
0E3F2  2b06f33f  sub ax, word ptr [0x3ff3]
0E3F6  98  cwde
0E3F7  e8cfc5  call 0A9C9
0E3FA  3d5f00  cmp ax, 0x5f
0E3FD  7e06  jle 0E405
0E3FF  81064d3b8000  add word ptr [0x3b4d], 0x80
0E405  a14d3b  mov ax, word ptr [0x3b4d]
0E408  25f000  and ax, 0xf0
0E40B  a34940  mov word ptr [0x4049], ax
0E40E  a14940  mov ax, word ptr [0x4049]
0E411  2b064740  sub ax, word ptr [0x4047]
0E415  98  cwde
0E416  e8a2c5  call 0A9BB
0E419  f7264140  mul word ptr [0x4041]
0E41D  01064740  add word ptr [0x4047], ax
0E421  a14740  mov ax, word ptr [0x4047]
0E424  050800  add ax, 8
0E427  2b06f33f  sub ax, word ptr [0x3ff3]
0E42B  b104  mov cl, 4
0E42D  d3e8  shr ax, cl
0E42F  250f00  and ax, 0xf
0E432  058c56  add ax, 0x568c
0E435  8b1e5b40  mov bx, word ptr [0x405b]
0E439  b104  mov cl, 4
0E43B  d3e3  shl bx, cl
0E43D  03d8  add bx, ax
0E43F  8a07  mov al, byte ptr [bx]
0E441  98  cwde
0E442  833e754000  cmp word ptr [0x4075], 0
0E447  7501  jne 0E44A
0E449  48  dec ax
0E44A  d1e0  shl ax, 1
0E44C  d1e0  shl ax, 1
0E44E  8b1e5540  mov bx, word ptr [0x4055]
0E452  d1fb  sar bx, 1
0E454  d1fb  sar bx, 1
0E456  2bc3  sub ax, bx
0E458  833e614000  cmp word ptr [0x4061], 0
0E45D  7e06  jle 0E465
0E465  b103  mov cl, 3
0E467  d3e0  shl ax, cl
0E469  a34d3b  mov word ptr [0x3b4d], ax
0E46C  b80300  mov ax, 3
0E46F  2b064140  sub ax, word ptr [0x4041]
0E473  f726fd3f  mul word ptr [0x3ffd]
0E477  bb0800  mov bx, 8
0E47A  2bd8  sub bx, ax
0E47C  a14d3b  mov ax, word ptr [0x3b4d]
0E47F  e87552  call 036F7
0E482  a30940  mov word ptr [0x4009], ax
0E485  c7067a3b1500  mov word ptr [0x3b7a], 0x15
0E48B  c706783b2100  mov word ptr [0x3b78], 0x21
0E491  a10940  mov ax, word ptr [0x4009]
0E494  d1f8  sar ax, 1
0E496  d1f8  sar ax, 1
0E498  b102  mov cl, 2
0E49A  e833c5  call 0A9D0
0E49D  a14740  mov ax, word ptr [0x4047]
0E4A0  a30140  mov word ptr [0x4001], ax
0E4A3  833e094000  cmp word ptr [0x4009], 0
0E4A8  7d0f  jge 0E4B9
0E4B9  8b0e0940  mov cx, word ptr [0x4009]
0E4BD  a10140  mov ax, word ptr [0x4001]
0E4C0  e8c7c8  call 0AD8A
0E4C3  01064340  add word ptr [0x4043], ax
0E4C7  8b0e0940  mov cx, word ptr [0x4009]
0E4CB  a10140  mov ax, word ptr [0x4001]
0E4CE  e8d4c8  call 0ADA5
0E4D1  29064540  sub word ptr [0x4045], ax
0E4D5  a14740  mov ax, word ptr [0x4047]
0E4D8  050800  add ax, 8
0E4DB  b104  mov cl, 4
0E4DD  d3e8  shr ax, cl
0E4DF  250f00  and ax, 0xf
0E4E2  052000  add ax, 0x20
0E4E5  8b365b40  mov si, word ptr [0x405b]
0E4E9  0284c240  add al, byte ptr [si + 0x40c2]
0E4ED  a29940  mov byte ptr [0x4099], al
0E4F0  a14340  mov ax, word ptr [0x4043]
0E4F3  2b061340  sub ax, word ptr [0x4013]
0E4F7  b105  mov cl, 5
0E4F9  d3f8  sar ax, cl
0E4FB  a34d3b  mov word ptr [0x3b4d], ax
0E4FE  bb2000  mov bx, 0x20
0E501  ba3801  mov dx, 0x138
0E504  e899c4  call 0A9A0
0E507  3d0000  cmp ax, 0
0E50A  7503  jne 0E50F
0E50F  a14d3b  mov ax, word ptr [0x3b4d]
0E512  a2a140  mov byte ptr [0x40a1], al
0E515  8826a940  mov byte ptr [0x40a9], ah
0E519  a2a640  mov byte ptr [0x40a6], al
0E51C  8826ae40  mov byte ptr [0x40ae], ah
0E520  a14540  mov ax, word ptr [0x4045]
0E523  2b061540  sub ax, word ptr [0x4015]
0E527  b106  mov cl, 6
0E529  d3f8  sar ax, cl
0E52B  a34d3b  mov word ptr [0x3b4d], ax
0E52E  bb3200  mov bx, 0x32
0E531  bac400  mov dx, 0xc4
0E534  e869c4  call 0A9A0
0E537  3d0000  cmp ax, 0
0E53A  7503  jne 0E53F
0E53F  a14d3b  mov ax, word ptr [0x3b4d]
0E542  a2b140  mov byte ptr [0x40b1], al
0E545  a2b640  mov byte ptr [0x40b6], al
0E548  a01241  mov al, byte ptr [0x4112]
0E54B  a2b740  mov byte ptr [0x40b7], al
0E54E  c6069a4009  mov byte ptr [0x409a], 9
0E553  c6069c4009  mov byte ptr [0x409c], 9
0E558  833e4b4000  cmp word ptr [0x404b], 0
0E55D  7403  je 0E562
0E55F  e9a600  jmp 0E608
0E562  833e574000  cmp word ptr [0x4057], 0
0E567  7f03  jg 0E56C
0E56C  b000  mov al, 0
0E56E  a2a440  mov byte ptr [0x40a4], al
0E571  a2ac40  mov byte ptr [0x40ac], al
0E574  a2a240  mov byte ptr [0x40a2], al
0E577  a2aa40  mov byte ptr [0x40aa], al
0E57A  833e274000  cmp word ptr [0x4027], 0
0E57F  7e14  jle 0E595
0E595  a10740  mov ax, word ptr [0x4007]
0E598  2b064740  sub ax, word ptr [0x4047]
0E59C  98  cwde
0E59D  e829c4  call 0A9C9
0E5A0  2d4000  sub ax, 0x40
0E5A3  e823c4  call 0A9C9
0E5A6  3d0600  cmp ax, 6
0E5A9  7c03  jl 0E5AE
0E5AB  eb58  jmp 0E605
0E5AE  833e5d4000  cmp word ptr [0x405d], 0
0E5B3  7403  je 0E5B8
0E5B8  c7064b40d0ff  mov word ptr [0x404b], 0xffd0
0E5BE  a10740  mov ax, word ptr [0x4007]
0E5C1  2b064740  sub ax, word ptr [0x4047]
0E5C5  98  cwde
0E5C6  e8f2c3  call 0A9BB
0E5C9  b106  mov cl, 6
0E5CB  d3e0  shl ax, cl
0E5CD  03064740  add ax, word ptr [0x4047]
0E5D1  a35140  mov word ptr [0x4051], ax
0E5D4  a14340  mov ax, word ptr [0x4043]
0E5D7  a34d40  mov word ptr [0x404d], ax
0E5DA  a14540  mov ax, word ptr [0x4045]
0E5DD  a34f40  mov word ptr [0x404f], ax
0E5E0  c70653401f4e  mov word ptr [0x4053], 0x4e1f
0E5E6  b84800  mov ax, 0x48
0E5E9  8b1e5f40  mov bx, word ptr [0x405f]
0E5ED  d1e3  shl bx, 1
0E5EF  d1e3  shl bx, 1
0E5F1  2bc3  sub ax, bx
0E5F3  a35d40  mov word ptr [0x405d], ax
0E5F6  c70601400100  mov word ptr [0x4001], 1
0E5FC  e8aa02  call 0E8A9
0E5FF  a01b41  mov al, byte ptr [0x411b]
0E602  a2ba40  mov byte ptr [0x40ba], al
0E605  e98a01  jmp 0E792
0E608  a14b40  mov ax, word ptr [0x404b]
0E60B  051800  add ax, 0x18
0E60E  e8b8c3  call 0A9C9
0E611  2d1800  sub ax, 0x18
0E614  e8b2c3  call 0A9C9
0E617  a30140  mov word ptr [0x4001], ax
0E61A  8b1e5340  mov bx, word ptr [0x4053]
0E61E  b107  mov cl, 7
0E620  d3eb  shr bx, cl
0E622  3bc3  cmp ax, bx
0E624  7e04  jle 0E62A
0E626  891e0140  mov word ptr [0x4001], bx
0E62A  a14d40  mov ax, word ptr [0x404d]
0E62D  2b061340  sub ax, word ptr [0x4013]
0E631  b105  mov cl, 5
0E633  d3f8  sar ax, cl
0E635  a2a440  mov byte ptr [0x40a4], al
0E638  8826ac40  mov byte ptr [0x40ac], ah
0E63C  a2a240  mov byte ptr [0x40a2], al
0E63F  8826aa40  mov byte ptr [0x40aa], ah
0E643  a14f40  mov ax, word ptr [0x404f]
0E646  2b061540  sub ax, word ptr [0x4015]
0E64A  b106  mov cl, 6
0E64C  d3f8  sar ax, cl
0E64E  a2b440  mov byte ptr [0x40b4], al
0E651  2b060140  sub ax, word ptr [0x4001]
0E655  a2b240  mov byte ptr [0x40b2], al
0E658  833e4b4000  cmp word ptr [0x404b], 0
0E65D  7e18  jle 0E677
0E65F  a14b40  mov ax, word ptr [0x404b]
0E662  40  inc ax
0E663  250300  and ax, 3
0E666  a34b40  mov word ptr [0x404b], ax
0E669  050900  add ax, 9
0E66C  a29a40  mov byte ptr [0x409a], al
0E66F  c6069c4080  mov byte ptr [0x409c], 0x80
0E674  e91b01  jmp 0E792
0E677  ff064b40  inc word ptr [0x404b]
0E67B  b98000  mov cx, 0x80
0E67E  a15140  mov ax, word ptr [0x4051]
0E681  e806c7  call 0AD8A
0E684  01064d40  add word ptr [0x404d], ax
0E688  b98000  mov cx, 0x80
0E68B  a15140  mov ax, word ptr [0x4051]
0E68E  e814c7  call 0ADA5
0E691  29064f40  sub word ptr [0x404f], ax
0E695  a14d40  mov ax, word ptr [0x404d]
0E698  2b06e53f  sub ax, word ptr [0x3fe5]
0E69C  e82ac3  call 0A9C9
0E69F  a30140  mov word ptr [0x4001], ax
0E6A2  a34d3b  mov word ptr [0x3b4d], ax
0E6A5  a14f40  mov ax, word ptr [0x404f]
0E6A8  2b06e73f  sub ax, word ptr [0x3fe7]
0E6AC  e81ac3  call 0A9C9
0E6AF  3b060140  cmp ax, word ptr [0x4001]
0E6B3  7e04  jle 0E6B9
0E6B5  87064d3b  xchg word ptr [0x3b4d], ax
0E6B9  d1e8  shr ax, 1
0E6BB  03064d3b  add ax, word ptr [0x3b4d]
0E6BF  a30140  mov word ptr [0x4001], ax
0E6C2  3b065340  cmp ax, word ptr [0x4053]
0E6C6  7f03  jg 0E6CB
0E6C8  e9c100  jmp 0E78C
0E6CB  c7064b400100  mov word ptr [0x404b], 1
0E6D1  a01f41  mov al, byte ptr [0x411f]
0E6D4  a2ba40  mov byte ptr [0x40ba], al
0E6D7  a17540  mov ax, word ptr [0x4075]
0E6DA  b106  mov cl, 6
0E6DC  d3e0  shl ax, cl
0E6DE  054000  add ax, 0x40
0E6E1  39065340  cmp word ptr [0x4053], ax
0E6E5  7c03  jl 0E6EA
0E6E7  e99000  jmp 0E77A
0E77A  c70601400200  mov word ptr [0x4001], 2
0E780  e82601  call 0E8A9
0E783  c70653400000  mov word ptr [0x4053], 0
0E789  eb07  jmp 0E792
0E78C  a10140  mov ax, word ptr [0x4001]
0E78F  a35340  mov word ptr [0x4053], ax
0E792  a10b40  mov ax, word ptr [0x400b]
0E795  250100  and ax, 1
0E798  7500  jne 0E79A
0E79A  833e20c700  cmp word ptr [0xc720], 0
0E79F  741e  je 0E7BF
0E7BF  833e22c700  cmp word ptr [0xc722], 0
0E7C4  741e  je 0E7E4
0E7E4  e8443e  call 0262B
0E7E7  e856c7  call 0AF40
0E7EA  e853bb  call 0A340
0E7ED  e86b34  call 01C5B
0E7F0  e8ed48  call 030E0
0E7F3  e89d44  call 02C93
0E7F6  833e0b4002  cmp word ptr [0x400b], 2
0E7FB  750c  jne 0E809
0E7FD  c7061d400400  mov word ptr [0x401d], 4
0E803  c706f73fffff  mov word ptr [0x3ff7], 0xffff
0E809  833e1d4000  cmp word ptr [0x401d], 0
0E80E  7503  jne 0E813
0E810  e93ef5  jmp 0DD51
0E813  a11d40  mov ax, word ptr [0x401d]
0E816  a27564  mov byte ptr [0x6475], al
0E819  a1ed3f  mov ax, word ptr [0x3fed]
0E81C  03062340  add ax, word ptr [0x4023]
0E820  a34348  mov word ptr [0x4843], ax
0E823  a1eb3f  mov ax, word ptr [0x3feb]
0E826  03062540  add ax, word ptr [0x4025]
0E82A  a24548  mov byte ptr [0x4845], al
0E82D  a1f53f  mov ax, word ptr [0x3ff5]
0E830  25f000  and ax, 0xf0
0E833  0306ef3f  add ax, word ptr [0x3fef]
0E837  a25448  mov byte ptr [0x4854], al
0E83A  c7060340ffff  mov word ptr [0x4003], 0xffff
0E840  c606c047ff  mov byte ptr [0x47c0], 0xff
0E845  a12740  mov ax, word ptr [0x4027]
0E848  8b1e2b40  mov bx, word ptr [0x402b]
0E84C  81e30200  and bx, 2
0E850  03c3  add ax, bx
0E852  8b1e5540  mov bx, word ptr [0x4055]
0E856  81e3f000  and bx, 0xf0
0E85A  03c3  add ax, bx
0E85C  3d0000  cmp ax, 0
0E85F  7403  je 0E864
0E864  b80100  mov ax, 1
0E867  8a1e4f48  mov bl, byte ptr [0x484f]
0E86B  32ff  xor bh, bh
0E86D  4b  dec bx
0E86E  83fb00  cmp bx, 0
0E871  7f03  jg 0E876
0E876  a3e33f  mov word ptr [0x3fe3], ax
0E879  891e553b  mov word ptr [0x3b55], bx
0E87D  8b1ee33f  mov bx, word ptr [0x3fe3]
0E881  8a875448  mov al, byte ptr [bx + 0x4854]
0E885  250700  and ax, 7
0E888  a30140  mov word ptr [0x4001], ax
0E88B  3b060340  cmp ax, word ptr [0x4003]
0E88F  7e0a  jle 0E89B
0E891  a10140  mov ax, word ptr [0x4001]
0E894  a30340  mov word ptr [0x4003], ax
0E897  881ec047  mov byte ptr [0x47c0], bl
0E89B  ff06e33f  inc word ptr [0x3fe3]
0E89F  a1e33f  mov ax, word ptr [0x3fe3]
0E8A2  3b06553b  cmp ax, word ptr [0x3b55]
0E8A6  76d5  jbe 0E87D
0E8A8  c3  ret