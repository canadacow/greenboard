1002A  56  push si
1002B  51  push cx
1002C  8bfd  mov di, bp
1002E  32ff  xor bh, bh
10030  8a1c  mov bl, byte ptr [si]
10032  d0eb  shr bl, 1
10034  d0eb  shr bl, 1
10036  d0eb  shr bl, 1
10038  d0eb  shr bl, 1
1003A  06  push es
1003B  2e8e060000  mov es, word ptr cs:[0]
1003C  8e060000  mov es, word ptr [0]
10040  268aa763d3  mov ah, byte ptr es:[bx - 0x2c9d]
10041  8aa763d3  mov ah, byte ptr [bx - 0x2c9d]
10045  80e4c0  and ah, 0xc0
10048  8a1c  mov bl, byte ptr [si]
1004A  80e30f  and bl, 0xf
1004D  268a8773d3  mov al, byte ptr es:[bx - 0x2c8d]
1004E  8a8773d3  mov al, byte ptr [bx - 0x2c8d]
10052  2430  and al, 0x30
10054  0ae0  or ah, al
10056  8a5c01  mov bl, byte ptr [si + 1]
10059  d0eb  shr bl, 1
1005B  d0eb  shr bl, 1
1005D  d0eb  shr bl, 1
1005F  d0eb  shr bl, 1
10061  268a8763d3  mov al, byte ptr es:[bx - 0x2c9d]
10062  8a8763d3  mov al, byte ptr [bx - 0x2c9d]
10066  240c  and al, 0xc
10068  0ae0  or ah, al
1006A  8a5c01  mov bl, byte ptr [si + 1]
1006D  80e30f  and bl, 0xf
10070  268a8773d3  mov al, byte ptr es:[bx - 0x2c8d]
10071  8a8773d3  mov al, byte ptr [bx - 0x2c8d]
10075  2403  and al, 3
10077  0ac4  or al, ah
10079  07  pop es
1007A  aa  stosb byte ptr es:[di], al
1007B  83c602  add si, 2
1007E  e2b0  loop 0x10030
10080  8bfd  mov di, bp
10082  81c70020  add di, 0x2000
10086  59  pop cx
10087  5e  pop si
10088  81c60020  add si, 0x2000
1008C  56  push si
1008D  51  push cx
1008E  8a1c  mov bl, byte ptr [si]
10090  d0eb  shr bl, 1
10092  d0eb  shr bl, 1
10094  d0eb  shr bl, 1
10096  d0eb  shr bl, 1
10098  06  push es
10099  2e8e060000  mov es, word ptr cs:[0]
1009A  8e060000  mov es, word ptr [0]
1009E  268aa783d3  mov ah, byte ptr es:[bx - 0x2c7d]
1009F  8aa783d3  mov ah, byte ptr [bx - 0x2c7d]
100A3  80e4c0  and ah, 0xc0
100A6  8a1c  mov bl, byte ptr [si]
100A8  80e30f  and bl, 0xf
100AB  268a8793d3  mov al, byte ptr es:[bx - 0x2c6d]
100AC  8a8793d3  mov al, byte ptr [bx - 0x2c6d]
100B0  2430  and al, 0x30
100B2  0ae0  or ah, al
100B4  8a5c01  mov bl, byte ptr [si + 1]
100B7  d0eb  shr bl, 1
100B9  d0eb  shr bl, 1
100BB  d0eb  shr bl, 1
100BD  d0eb  shr bl, 1
100BF  268a8783d3  mov al, byte ptr es:[bx - 0x2c7d]
100C0  8a8783d3  mov al, byte ptr [bx - 0x2c7d]
100C4  240c  and al, 0xc
100C6  0ae0  or ah, al
100C8  8a5c01  mov bl, byte ptr [si + 1]
100CB  80e30f  and bl, 0xf
100CE  268a8793d3  mov al, byte ptr es:[bx - 0x2c6d]
100CF  8a8793d3  mov al, byte ptr [bx - 0x2c6d]
100D3  2403  and al, 3
100D5  0ac4  or al, ah
100D7  07  pop es
100D8  aa  stosb byte ptr es:[di], al
100D9  83c602  add si, 2
100DC  e2b0  loop 0x1008e
100DE  83c550  add bp, 0x50
100E1  59  pop cx
100E2  5e  pop si
100E3  81c60020  add si, 0x2000
100E7  b80808  mov ax, 0x808
100EA  c3  ret