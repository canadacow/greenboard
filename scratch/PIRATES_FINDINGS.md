# Pirates! (1986, DOS) — Verified Findings

Everything below was verified either by executing a Python port of the game's
own code, or by reading instruction bytes directly from the disk image.
Claims that rested on inference have been removed.

Target: `assets/pirates_1.img`, `assets/pirates_2.img`.
Both are 368,640 bytes = 40 cylinders × 2 heads × 9 sectors × 512.

Disk 1 contains `0-PIRATE GAME DISK` at offset `0x200`.
Disk 2 contains `1-PIRATE GAME DISK` at offset `0x200`.

---

## Boot chain

Verified by `scratch/boot_port.py`, a literal instruction-by-instruction port
of the boot sector that runs in Python and loads the disk image.

Boot sector, disassembled from disk offset `0`:

```asm
7C00  cli
7C01  mov ax, 0x20
7C04  mov ss, ax
7C06  mov sp, 0x200
7C09  sti
7C0A  mov ax, 0x20
7C0D  mov es, ax
7C0F  mov ax, cs
7C11  mov ds, ax
7C13  call 0x7c16          ; pushes 0x7C16
7C16  pop si               ; position-independent addressing
7C17  add si, 0x11
7C1B  xor di, di
7C1D  mov cx, 0x200
7C20  rep movsb            ; relocate 512 bytes to 0020:0000
7C22  ljmp 0x20:0x3c
```

Loads performed, as recorded by the port:

| What | Source | Destination |
|---|---|---|
| Title screen | cyl 38 h0 s1, cyl 38 h1 s1, cyl 39 h0 s1, cyl 39 h1 s1 (8 sectors each) | `B800:0000`, `:1000`, `:2000`, `:3000` |
| Stage 2 | cyl 1–21, 8 sectors per head, both heads | `0050:0000` onward, via `2D80` staging |

Stage-2 loader: each cylinder is read to `2D80:0000` (head 0) and `2D80:1000`
(head 1), then `0x2000` bytes are copied to a destination segment starting at
`0x50` and advancing by `0x200` per cylinder. Entry is `0050:0020`.

The port performs 44 disk reads in total.

## Runtime segment table

After the boot loader relocates 16 words at `0050:0000` by `+0x50`:

```
cs:[00] = 117B
cs:[02] = 1038
cs:[04] = 1EB6
cs:[06] = 27FE
cs:[08] = 26BE
cs:[0A] = 0050
cs:[0C] = 0050
cs:[0E] = 0050
```

`main` at `0050:0080`, read from ported memory:

```asm
0080  mov cx, cs:[0]       ; DS = ES = 117B
0085  mov ds, cx
0087  mov es, cx
0089  mov cx, cs:[8]       ; SS = 26BE
008E  mov ss, cx
0090  mov cx, 0x13fc
0093  mov sp, cx
```

`DS = 0x117B` is therefore derived by execution, not assumed.

---

## Self-modifying code

`main` writes INT opcodes into its own code stream:

```asm
0095  mov al, 0xCC
0097  inc al               ; -> 0xCD
0099  mov cs:[0x16a], al
009D  mov cs:[0x147], al
...
00E6  mov al, 0x13
00E8  mov cs:[0x16b], al
00EC  mov cs:[0x148], al
```

The resulting byte pairs are `CD 13` = `INT 13h`. On disk those addresses hold
`00 07`. Disassembly of that region before `main` runs does not reflect the
executed instructions.

---

## Title screen

`scratch/show_title.py` runs the port and decodes `B800:0000` as CGA mode 4
(320×200, 2 bits per pixel, 4 pixels per byte, even scanlines at offset `0`,
odd at `0x2000`, 80 bytes per scanline). The result is the Pirates! title
screen: 10,607 of 16,384 bytes nonzero. Output at `scratch/out/port_title.png`.

---

## Resource system

`load_resource` at `0050:2820`, resource index in `AX`:

```asm
2820  mov [0x3b84], ax
2823  call 0x2844          ; res_fetch
2826  cmp byte [0x11a3], 0 ; retry while error
282B  jne 0x2823
282D  cmp word [0x3b84], 0
2832  jne 0x2843
2834  mov si, 0x48c8       ; resource 0 only:
2837  mov cx, 0x8a         ;   relocate 138 word pointers
283A  mov ax, si
283C  add [si], ax
283E  add si, 2
2841  loop 0x283c
2843  ret
```

`res_fetch` at `0050:2844`:

```asm
2855  mov si, [0x3b84]
2859  shl si, 1
285B  mov bx, [si + 0x136b]   ; descriptor pointer table
285F  mov ax, [bx]
2866  mov [0x11b8], al        ; sector count
2869  mov ax, [bx + 4]
286C  add ax, [0x3bc8]
2870  mov [0x11ae], al        ; cylinder
2873  mov ax, [bx + 6]
2876  mov [0x11af], al        ; sector
2879  mov ax, [bx + 8]
287C  mov [0x11b2], ax        ; destination segment
287F  mov ax, [bx + 0xa]
2882  mov [0x11b4], ax        ; destination offset
2890  cmp byte [0x11af], 0xa  ; sector >= 10 -> head 1, subtract 9
```

Descriptor pointer table is at `DS:0x136B`. Read from ported memory, the first
eight pointers are `11bb 11c7 11d3 11df 11eb 11f7 1203 120f` — stride 12,
matching the field layout above.

`res_fetch` and `load_resource` are ported in `scratch/splash_port.py`;
`load_resource(0x20)` completes with error byte `[0x11A3] = 0`.

---

## Text encoding

Lowercase letters are stored XOR `0xE0`. Uppercase, digits and punctuation are
plain ASCII. `0x0D` is a line break; `0xFF` terminates a record.

Verified: bytes at disk offset `0x04DA00` begin
`59 8f 95 8e 87 20 81 8e 84 20 90 8f 8f 92 2c 20`.
Applying XOR `0xE0` to bytes ≥ `0x80` yields "Young and poor, " and the full
record decodes to the opening narrative text shown by the game.

Strings located on disk 1 using this encoding:

| String | Offset |
|---|---|
| `carefully consult` | `0x04F104` |
| `January` | `0x04F12D` |
| `precise` | `0x04F20C` |
| `Early in the month` | `0x04F228` |
| `Late in the month` | `0x04F23E` |
| `Treasure Fleet` | `0x04CE86` |
| `arrives at` | `0x04D098` |
| `do you know` | `0x04DB94` |
| `Silver Train` | `0x04FFAE` |

Decoding `0x04F0E0` onward yields the month list (`"January"` … `"December"`)
and the follow-up prompt with `Early in the month.` / `Late in the month.`

---

## City tables

`0050:3F58` computes a city record address:

```asm
3F58  mov al, [0x47c0]
3F5B  and ax, 0x3f          ; index, bits 0-5
3F5E  mov bx, 0x18          ; 24 bytes per record
3F61  mul bx
3F63  add ax, 0x4240        ; table base
3F66  mov [0x9a7f], ax
```

and selects between two strings on bit 6 of the same byte:

```asm
3F72  mov al, [0x47c0]
3F75  and al, 0x40
3F77  je 0x3f7f
3F79  mov si, 0x211c
3F7C  call 0x33ff
3F7F  ...                    ; otherwise si = 0x2140
```

`FUN_0050_4271` copies 12 bytes from record offset `+0x0C`:

```c
puVar3 = (undefined1 *)(*(int *)0x9a7f + 0xc);
for (12 bytes) DAT_0050_91f3[i] = *puVar3++;
```

Six 1024-byte blocks on disk 1 contain 24-byte records — a 12-byte
space-padded name followed by 12 data bytes, beginning at `+0x0C` in each
block:

| File offset |
|---|
| `0x054000` |
| `0x054400` |
| `0x054800` |
| `0x054C00` |
| `0x055000` |
| `0x055400` |

Example records from the block at `0x055400`:

```
ANTIGUA        0a 9c 92 01 03 12 1b 0f 9b 00 00 00
BARBADOS       00 22 6f 01 00 02 0a 00 32 00 00 00
BELIZE         00 83 03 01 00 03 08 02 4b 00 00 00
BERMUDA        10 1a 5a 00 03 18 15 14 6e 00 00 00
```

Each block's records terminate with two `FLORIDA CHNL` entries. 28 bytes follow.
Those 28-byte tails differ between blocks:

```
0x054C00: 18 18 1c 05 05 14 14 14 17 17 17 01 04 0f 18 1c 17 17 05 05 03 25 25 0d 0d 27 28 29
0x055000: 19 19 1d 05 05 13 13 13 18 18 18 01 04 0e 19 1d 18 18 05 05 03 25 25 0c 0c 27 28 29
0x055400: 0e 1a 1d 06 06 14 14 14 19 19 19 01 05 1a 1d 19 19 06 06 04 25 25 0c 0c 0c 27 28 29
```

All three end with `27 28 29`. The meaning of these bytes has not been
determined.

---

## Year calculation

```asm
41D3  mov al, [0x475a]
41D6  xor ah, ah
41D8  mov [0x9a1f], ax
41DB  mov bx, 0x14
41DE  mul bx
41E0  add ax, 0x618
41E3  add ax, [0x9a9d]
41E7  call 0xf24            ; decimal formatter
```

`0x618` = 1560, `0x14` = 20.

`0050:352F` is a decimal formatter: repeated subtraction against a table of
powers of ten at `[0x3B88]`, emitting one digit per place.

---

## Bytecode interpreter

`0050:3DDF`:

```asm
3DDF  mov word [0x93e9], 0
3DE5  call 0x3f58
3DE8  mov al, [0x4740]
3DEB  xor ah, ah
3DED  mov [0x93db], ax
3DF0  cmp ax, 0x10
3DF3  jbe 0x3e00
3DF5  mov si, [0x154f]        ; out of range -> entry 0
3DF9  mov [0x154d], si
3DFD  jmp 0x3e11
3E00  mov bx, [0x93db]
3E04  shl bx, 1
3E06  mov si, [bx + 0x154f]   ; program table
3E0A  mov [0x154d], si
3E0E  call 0x460e
3E11  mov si, [0x154d]        ; fetch loop
3E15  mov al, [si]
3E17  inc word [0x154d]
3E1B  cwde
3E1C  mov [0x9a81], ax        ; signed value
3E1F  or ax, ax
3E21  jns 0x3e25
3E23  neg ax
3E25  mov [0x9a87], ax        ; opcode = abs(byte)
3E28  cmp ax, 0x61
3E2B  jne 0x3e32
3E2D  call 0x3fbd
3E30  jmp 0x3e11
3E32  cmp word [0x9a81], 0
3E37  je 0x3e40
3E39  cmp word [0x9a81], -1
3E3E  jne 0x3e41
3E40  ret                     ; halt on 0 or -1
3E41  cmp word [0x9a87], 0x32
...                            ; 0x32..0x37 set [0x93e7] = 0..5
3EBF  cmp word [0x9a87], 0x29
3EC6  mov word [0x93d1], 3
3ECC  mov word [0x93d3], 9
3ED2  cmp word [0x9a87], 0x2a
3ED9  mov word [0x93d1], 0xc
...
3F02  call 0x45f8
3F05  cmp word [0x9a87], 0x29
3F0C  mov ax, [0x93db]
3F0F  shl ax, 1
3F11  mov [0x4740], al
3F14  cmp word [0x9a87], 0x2a
3F1B  mov ax, [0x93db]
3F1E  add [0x4740], al
3F22  call 0x3fa5
```

Structure: program counter at `[0x154D]`, one byte fetched and the pointer
advanced, opcode taken as the absolute value with the sign retained separately
as a flag, dispatch by comparison chain, halt on `0` or `-1`. There is no
operand stack.

A 17-entry pointer table at `DS:0x154F` is indexed by `[0x4740]`. Read from
live memory the entries are:

```
1444 1450 145e 1470 1481 1491 14a2 14b2 14c1 14d1 14e0 14f1 1500 1510 151f 152e 153e
```

---

## Copy protection

### Disk check

`src/isa/isa_fdc.cpp` in this repository implements it: a read of C=4 H=0 S=1
with sector-size code `N != 2` must stream the sector's 512 bytes followed by
format gap filler (`0x43`), then report ST0 `0x40` / ST1 `0x04`. At INT 13h
this surfaces as `AH=0x04`, `CF=1`.

Disk 1 offset `0x009000` (C=4 H=0 S=1) is 512 zero bytes.

The boot loader's stage-2 read of cylinder 4 fails; the destination segment
still advances.

### Manual lookup

`0050:3FBD`:

```asm
3FD4  cmp byte [0x3bc6], 2
3FD9  jne 0x3fe3
3FDB  mov bx, 0x100
3FDE  mov cx, 0x4f66
3FE1  jmp 0x3fea
3FE3  xor bx, bx
3FE5  mov cx, 0x2104
3FE8  mov dh, 1
3FEA  mov word [0x1440], 5    ; retry counter
3FF0  mov ax, 0x201
3FF5  jae 0x4000
3FF7  dec word [0x1440]
3FFB  jne 0x3ff0
3FFD  jmp 0x4014
4000  lcall 0x10da, 0x36e0
4005  mov cx, cs:[0]
400A  mov ds, cx
400C  mov es, cx
400E  cmp ax, [0x15a3]
4012  je 0x4019
4014  mov byte [0x473d], 4
4019  call 0x460e
```

`0050:4616`:

```c
if (*(char *)0x473d < '\x04') return;
do { c = *(char *)*(undefined2 *)0x154d; *(int *)0x154d += 1; } while (c != '\0');
```

Answer accumulation is at `3F05`–`3F22` above: opcode `0x29` stores
`selection × 2` into `[0x4740]`; opcode `0x2A` adds its selection to the same
byte.

A scan of all `cmp`, `test` and `sub` instructions in the 172KB stage-2 image
found no instruction reading `[0x4740]`.

---

## Graphics

Character cells are written by `0050:0A9D`, identified by hooking memory writes
to `B8000`–`BBFFF` and observing that it was the only routine producing
character-shaped patterns. It writes `es:[di]`, `+0x2000`, `+0x50`, `+0x2050`,
`+0xA0`, `+0x20A0`, `+0xF0`, `+0x20F0` — CGA interleaved banks with 80-byte
scanlines.

`0050:0A00` dispatches on `[0x3B98]` to `0xA7D`, `0x12E8` or `0xBCD`, with the
character code in `AL` and a font pointer `si = 0xA080`.

`[0x3B78]` and `[0x3B7A]` hold cursor column and row.

Compressed image decoder, disassembled at these addresses:

| Address | Behaviour |
|---|---|
| `1000:0E8F` | reads `n` bytes from a 512-byte buffered window, refilling via `1000:505F` when the cursor `[0xC73D]` exceeds `0x1FF` |
| `1000:0E71` | calls the above into the cursor `[0xC73A]`, then advances it |
| `1000:0EBE` | reads a byte, spreads its high and low nibbles, writes to `[bx]` and `[bx+0x10]` |
| `1000:1CCF` | walks a tree MSB-first from a node index, returning `-1 - node` at a negative node |
| `1000:1BF9` | per-row loop; row address from a table at `0x3E31` plus `x >> 1`; toggles `[0xC730]` per pixel (two pixels per byte); an escape value at `[0xC735]` introduces a repeat count accumulated while decoded symbols equal `0xFF`; subtracts a bias at `[0xC73C]` |

---

## Input

Three INT 16h sites exist in stage 2, found by scanning for `CD 16`:

| Peek (AH=1) | Get (AH=0) |
|---|---|
| `0x08A7` | `0x08AE` |
| `0x09E2` | `0x09E8` |
| `0x2742` | `0x2748` |

`0050:08A0`:

```asm
08A0  xor ax, ax
08A2  mov [0x8a2], ax
08A5  mov ah, 1
08A7  int 0x16
08A9  je 0x8c7               ; empty -> return 0
08AB  mov ax, 0
08AE  int 0x16
08B0  cmp ax, 0
08B5  cmp al, 0
08B7  jne 0x8c9
08B9  mov al, ah             ; extended key
08C0  mov al, [si + 0x8e4]   ; translation table
08C9  and ax, 0xff
08CC  cmp ax, 0x61
08D1  cmp ax, 0x7b
08D6  and ax, 0xdf           ; fold lowercase to uppercase
08DB  mov [0x8a2], ax
```

`0050:09E0` peeks and gets in a loop until the buffer reports empty, discarding
what it reads.

`0050:2740` peeks; if a key is present it gets one and compares `AL` against
`0xE0`, `0x16`, then (when `[0x9AA9]` is zero) `0x56`, `0x76` and `0x20`,
looping on anything else. It returns when the peek reports no key.

`FUN_0050_24A5`:

```c
return *(uint *)0x3c0e & 0x1f ^ 0xffff;
```

`FUN_0050_3593` commits a menu selection when bit 4 of `[0x3BFA]` is clear:

```c
if ((*(uint *)0x3bfa & 0x10) == 0) {
    *(int *)0x93db = (*(int *)0x3b7e - *(int *)0x1427) + *(int *)0x1425;
    return;
}
```

The INT 09h handler at `0050:26E0`:

```asm
26ED  cmp word [0x3c0c], 1
26F2  je 0x26fd
26F4  pop bx / pop ax / pop ds / popf
26F8  ljmp cs:[0x26dc]        ; chain to previous handler
26FD  in al, 0x60
26FF  mov bl, al
2701  and bx, 0x7f
2705  shl bx, 1
2707  cmp word [bx + 0xa60], 0
270C  je 0x26f4
270E  test al, 0x80
2712  mov ax, [bx + 0xa60]
2716  or [0x3c0e], ax         ; make
271D  mov ax, [bx + 0xa60]
2721  xor ax, 0xffff
2724  and [0x3c0e], ax        ; break
2734  mov al, 0x20
2736  mov dx, 0x20
2739  out dx, al              ; EOI
```

`FUN_0050_0592` sets `[0x3C0C] = 1` in one branch:

```asm
    cVar2 = FUN_0050_07e9();
    if (cVar2 == '1') {
      *(undefined2 *)0x3bb0 = 0;
      FUN_0050_0610();
      *(undefined2 *)0x3c0c = 1;
      return;
    }
    *(undefined2 *)0x3b72 = 0;
```

`FUN_0050_40BD` reads with `FUN_0050_0860`, accepts `0x08` as backspace,
returns on `0x0D`, and stores characters in the range `0x20`–`0x5F` (up to 9)
into `[0x104B]`.

Timer: `0050:128D` writes a handler address to `0000:0070` (IVT vector `0x1C`).
The handler at `0050:12A0` increments `[0x8A0]` and decrements `[0x3C02]`,
`[0x3C04]`, `[0x3C06]`, `[0x3C08]` and `[0x3C0A]` when each is nonzero.

---

## Ghidra project configuration

Stage-2 image byte 0 corresponds to physical address `0x500`.

Loading the image at `0050:0000` causes DS-relative displacements to be
reported `0x500` too low. The instruction bytes `8b 9c 6b 13` at `0x285B`
decode as `mov bx, [si + 0x136b]`; with that base Ghidra rendered the operand
as `&DAT_0050_0e6b`, and `0x136b - 0x0e6b = 0x500`.

Loading at `0000:0500` renders the same operand as `DAT_0000_136b`.

Project: `scratch/ghidra_proj2`. Decompilation of 517 functions:
`scratch/out/pirates_fixed.c`.

---

## Files

| File | Purpose |
|---|---|
| `scratch/boot_port.py` | boot sector ported to Python; runs and loads the disk |
| `scratch/show_title.py` | renders the title screen from ported memory |
| `scratch/stage2_port.py` | `main` prologue; shows the patched INT opcodes |
| `scratch/splash_port.py` | `res_fetch` and `load_resource` ported |
| `scratch/ghidra_reimport.py` | Ghidra import at the correct base |
| `scratch/out/pirates_fixed.c` | full decompilation |
| `scratch/out/port_title.png` | title screen rendered by the port |
