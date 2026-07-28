# Pirates! (1986, DOS) — Reverse Engineering Findings

Target: `assets/pirates_1.img`, `assets/pirates_2.img` (360KB, 40 cyl × 2 heads × 9 sectors × 512).

Disk 1 is labelled `0-PIRATE GAME DISK` at offset `0x200`; disk 2 is `1-PIRATE GAME DISK`.

---

## Boot chain

| Stage | Source | Destination | Notes |
|---|---|---|---|
| Boot sector | disk offset `0` | `0000:7C00`, relocates to `0020:0000` | 512 bytes, `ljmp 0020:003C` |
| Title screen | cyl 38–39, sectors 1–8, both heads | `B800:0000`–`B800:3FFF` | 4 × 8 sectors, 16KB |
| Stage 2 | cyl 1–21, 8 sectors × 2 heads | `0050:0000` via `2D80` staging | 168KB, entry `0050:0020` |

Stage-2 loader detail: each cylinder is read to `2D80:0000` (head 0) and `2D80:1000`
(head 1), then `0x2000` bytes are copied to a destination segment that starts at
`0x50` and advances by `0x200` per cylinder.

**Cylinder 4 is skipped.** It is the misformatted protection track; the read fails
but the destination segment still advances, leaving a `0x2000` hole in memory.

After loading, 16 words at `0050:0000` are relocated by `+0x50`, producing the
runtime segment table.

## Runtime segment table (`0050:0000`, post-relocation)

```
cs:[00] = 117B   data segment (DS)
cs:[02] = 1038   second code segment
cs:[04] = 1EB6
cs:[06] = 27FE
cs:[08] = 26BE   stack segment (SS)
cs:[0A] = 0050
cs:[0C] = 0050
cs:[0E] = 0050
```

`main` (`0050:0080`) sets `DS = ES = cs:[0]` = `117B`, `SS = cs:[8]` = `26BE`, `SP = 0x13FC`.

## Executed segments

From an instruction-level trace, only four code segments ever execute:

| CS | share of instructions | DS in force |
|---|---|---|
| `0050` | ~83% | `117B` (mostly), `3000`, `27FE` |
| `1038` | ~17% | `1038` |
| `0020` | 161 instrs | `2D80` (boot relocation) |
| `27FE` | 1 instr | `27FE` |

---

## Self-modifying code

`main` patches INT opcodes into its own code stream before use:

```asm
0095  mov al, 0xCC
0097  inc al                  ; -> 0xCD (INT opcode)
0099  mov cs:[0x16A], al
009D  mov cs:[0x147], al
...
00E6  mov al, 0x13
00E8  mov cs:[0x16B], al
00EC  mov cs:[0x148], al      ; together: CD 13 = INT 13h
```

Those bytes are `00 07` on disk. Static disassembly of that region is not
meaningful until `main` runs.

---

## Resource system

`load_resource` (`0050:2820`), index in `AX`:

```
[0x3b84] = AX
res_fetch()                       ; 0050:2844
if AX == 0: relocate 138 word pointers at DS:48C8 by +0x48C8
```

`res_fetch` (`0050:2844`) reads the descriptor pointer table at **`DS:0x136B`**:

```asm
2855  mov si, [0x3b84]
2859  shl si, 1
285B  mov bx, [si + 0x136b]     ; descriptor pointer
```

Descriptor field layout (byte offsets from `bx`):

| Offset | Field |
|---|---|
| `+0` | sector count |
| `+4` | cylinder (plus disk bias `[0x3bc8]`) |
| `+6` | sector — if ≥ 10, head 1 and subtract 9 |
| `+8` | destination segment |
| `+0xA` | destination offset |

A synthetic descriptor sits at `DS:0x135F`, immediately before the table.
`FUN_0000_094a` writes `[0x1363]` = cylinder, `[0x1365]` = sector,
`[0x1369]` = destination — exactly `bx+4`, `bx+6`, `bx+0xA` for `bx = 0x135F`.
This loads the video-driver overlay without a directory entry (start cylinder
6, 11 or 16 selected by reading port `0x200`; 5 cylinders × 2 sectors).

### Decoded resources

| idx | cnt | cyl | sec | destination | file offset | bytes |
|---|---|---|---|---|---|---|
| 0 | 3 | 0 | 5 | `05B4:48C8` | `0x000800` | 1536 |
| 4 | 2 | 37 | 7 | `0400:4240` | `0x054000` | 1024 |
| 6 | 2 | 37 | 9 | `0400:4240` | `0x054400` | 1024 |
| 7 | 2 | 37 | 11 | `0400:4240` | `0x054800` | 1024 |
| 8 | 2 | 37 | 13 | `0400:4240` | `0x054C00` | 1024 |
| 9 | 2 | 37 | 15 | `0400:4240` | `0x055000` | 1024 |
| 10 | 2 | 37 | 17 | `0400:4240` | `0x055400` | 1024 |
| 18 | 4 | 0 | 9 | `0780:4E8C` | `0x001000` | 2048 |
| 19 | 1 | 34 | 1 | `0200:68B1` | `0x04C800` | 512 |
| 28 | 6 | 0 | 3 | `0BE8:588C` | `0x000400` | 3072 |
| 31 | 4 | 0 | 1 | `0794:4130` | `0x000000` | 2048 |

24 valid descriptors total.

---

## Text encoding

Strings are stored with **lowercase letters XOR `0xE0`**; uppercase, digits and
punctuation are plain ASCII. `\r` (`0x0D`) is a line break, `0xFF` terminates a
record.

This is why plain-text searches for game strings fail on the disk images.

Decoding `0x04DA00` (cyl 34 head 1 sector 1) yields the opening narrative
verbatim, beginning "Young and poor, you seek your fortune in the New World."

Located strings:

| String | Disk 1 offset |
|---|---|
| `do you know` | `0x04DB94` |
| `Treasure Fleet` | `0x04CE86` |
| `Silver Train` | `0x04FFAE` |
| `arrives at` | `0x04D098` |
| `January` | `0x04F12D` |
| `Early in the month` | `0x04F228` |
| `Late in the month` | `0x04F23E` |
| `carefully consult` | `0x04F104` |
| `precise` | `0x04F20C` |

The quiz prompt at `0x04DB60` decodes as a template with runtime-inserted
fields:

```
...Brethren of the Coast.  "Aye mate," they
reply, "but do you know when
                                  <- city
arrives at
                                  <- date
?"
```

---

## City tables

Six blocks, one per era, each 1024 bytes, all loading to `DS:0x4240`:

| Era | File offset |
|---|---|
| 1560 | `0x054000` |
| 1580 | `0x054400` |
| 1600 | `0x054800` |
| 1620 | `0x054C00` |
| 1640 | `0x055000` |
| 1660 | `0x055400` |

Record format: 24 bytes — 12-byte space-padded name, then 12 data bytes.
Records begin at `+0x0C` in each block. 41 cities per era.

Indexed by `FUN_0050_3f58`:

```asm
3F58  mov al, [0x47c0]
3F5B  and ax, 0x3f          ; city index, bits 0-5
3F5E  mov bx, 0x18          ; 24 bytes per record
3F61  mul bx
3F63  add ax, 0x4240        ; table base
```

`[0x47c0]` bit 6 selects Treasure Fleet vs Silver Train.

Sample (1660 era):

```
ANTIGUA        0a 9c 92 01 03 12 1b 0f 9b 00 00 00
BARBADOS       00 22 6f 01 00 02 0a 00 32 00 00 00
BELIZE         00 83 03 01 00 03 08 02 4b 00 00 00
BERMUDA        10 1a 5a 00 03 18 15 14 6e 00 00 00
```

Each block's city list terminates with two `FLORIDA CHNL` records, followed by
28 bytes of numeric data. Those tails vary systematically between eras:

```
1620: 18 18 1c 05 05 14 14 14 17 17 17 01 04 0f 18 1c 17 17 05 05 03 25 25 0d 0d 27 28 29
1640: 19 19 1d 05 05 13 13 13 18 18 18 01 04 0e 19 1d 18 18 05 05 03 25 25 0c 0c 27 28 29
1660: 0e 1a 1d 06 06 14 14 14 19 19 19 01 05 1a 1d 19 19 06 06 04 25 25 0c 0c 0c 27 28 29
```

Most values increment by 1 between successive eras; every block ends `27 28 29`.
The encoding has not been resolved — a linear `month × 2 + half` mapping does not
reproduce the printed manual's schedule.

---

## Year calculation

```asm
41D3  mov al, [0x475a]
41D8  mov [0x9a1f], ax      ; era index
41DB  mov bx, 0x14          ; 20
41DE  mul bx
41E0  add ax, 0x618         ; 1560
```

`year = 1560 + 20 × era`. Era 5 → 1660, matching the on-screen prompt.

---

## Bytecode interpreter

`0050:3DDF` is a byte-stream interpreter:

```
PC = [0x154d]
b  = *PC++
[0x9a81] = sign_extend(b)      ; sign is a modifier flag
[0x9a87] = abs(b)              ; opcode
dispatch:
    0x61        -> call 0x3FBD, loop
    0x00 / 0xFF -> ret
    0x32..0x37  -> scene = 0..5, draw
    0x29        -> menu 3x9    ; [0x4740]  = pick*2
    0x2A        -> menu 12x12  ; [0x4740] += pick ; call 0x3FA5
    0x06        -> menu 5x1
    default     -> [0x9a83] = opcode ; call 0x4604 (show text)
if [0x9a81] < 0 before a menu -> call 0x460E
goto loop
```

Programs are selected from a 17-entry pointer table at `DS:0x154F`, indexed by
`[0x4740]`:

```asm
3DE8  mov al, [0x4740]
3DF0  cmp ax, 0x10
3DF3  jbe 0x3E00
3DF5  mov si, [0x154f]        ; out of range -> entry 0
3E00  mov bx, [0x93db]
3E04  shl bx, 1
3E06  mov si, [bx + 0x154f]
3E0A  mov [0x154d], si
```

Table contents verified live: `1444 1450 145e 1470 1481 1491 14a2 14b2 14c1
14d1 14e0 14f1 1500 1510 151f 152e 153e`.

All 17 programs share one structure:

```
TEXT  SCENE  TEXT  MENU_EARLY_LATE  MENU_MONTH  -TEXT  CHECK  -SCENE  TEXT  HALT
```

Opcodes `0x29` and `0x2A` are adjacent in all 17. `CHECK` (`0x61`) appears in 15
of 17; programs 0 and 1 omit it. There is no operand stack and no control flow
other than halt.

---

## Copy protection

### Disk check

Track 4, head 0, sector 1 on disk 1 is misformatted. The game issues a read with
sector-size code `N=4` (2048 bytes) rather than `N=2`. Correct behaviour is to
stream 512 bytes of sector data followed by 1536 bytes of format gap filler
(`0x43`), then report "no data" — `ST0=0x40`, `ST1=0x04`, surfacing at INT 13h as
`AH=0x04`, `CF=1`.

This matches the implementation in `src/isa/isa_fdc.cpp` in this repository.

### Manual lookup

`FUN_0050_3FBD` (`0050:3FBD`):

```asm
3FEA  mov word [0x1440], 5     ; retry counter
3FF0  mov ax, 0x201
3FF5  jae 0x4000
3FF7  dec word [0x1440]
3FFB  jne 0x3ff0
3FFD  jmp 0x4014               ; give up -> penalty
4000  lcall 0x10da, 0x36e0     ; compute expected answer -> AX
400E  cmp ax, [0x15a3]         ; player's selection
4012  je 0x4019
4014  mov byte [0x473d], 4     ; penalty flag
```

`FUN_0050_4616` skips script bytes while `[0x473d] >= 4`:

```c
if (*(char *)0x473d < '\x04') return;
do { c = *[0x154d]; [0x154d]++; } while (c != '\0');
```

Answer accumulation, from the interpreter:

```asm
3F05  cmp [0x9a87], 0x29
3F0F  shl ax, 1
3F11  mov [0x4740], al         ; Early/Late × 2
3F14  cmp [0x9a87], 0x2a
3F1E  add [0x4740], al         ; += month
3F22  call 0x3fa5
```

No `cmp`, `test` or `sub` instruction anywhere in the 172KB image reads
`[0x4740]`; the value is used only as the script-table index.

`[0x15a3]` was observed holding `0x1234` at the comparison, and segment `0x1000`
was never entered during a 50M-instruction trace, so `FUN_1000_4480` did not
execute on that path.

---

## Graphics

Screen text is drawn by a glyph blitter, not BIOS. Identified by hooking memory
writes to `B8000`–`BBFFF`:

- `0050:0A9D` — the only routine writing character cells. Writes `es:[di]`,
  `+0x2000`, `+0x50`, `+0x2050`, `+0xA0`, `+0x20A0`, `+0xF0`, `+0x20F0`
  (CGA interleaved banks, 80-byte scanlines).
- `0050:0A00` — mode dispatcher, `AL` = character code, font at `si = 0xA080`,
  routes to `0xA7D` (CGA), `0x12E8`, or `0xBCD` depending on `[0x3b98]`.
- Cursor position: `[0x3b78]` = column, `[0x3b7a]` = row.
- Glyphs are 16 bytes each; `char = (SI − font_base) / 16`.

Compressed images use Huffman coding with an RLE escape, 4 bits per pixel:

| Routine | Function |
|---|---|
| `1000:0E8F` | `read_bytes(dest, n)` — buffered stream, 512-byte window |
| `1000:0E71` | read into the global cursor `[0xC73A]` |
| `1000:0EBE` | nibble expansion — spreads a byte's nibbles to `[bx]` and `[bx+0x10]` |
| `1000:505F` | refill the 512-byte buffer from disk |
| `1000:1CCF` | decode one Huffman symbol; tree at `0x67B1`, bitstream cursor `[0xC73D]` |
| `1000:1BF9` | decode a 4bpp image rectangle; row table at `0x3E31` |

`decode_image_rect` details: nibble toggle at `[0xC730]` (two pixels per byte),
RLE escape value at `[0xC735]`, run length accumulated while each decoded symbol
is `0xFF`, run value at `[0xC734]`, bias subtracted at `[0xC73C]`.

---

## Input

Three INT 16h peek/get pairs exist in stage 2:

| Peek | Get | Use |
|---|---|---|
| `0x08A7` | `0x08AE` | general character read |
| `0x09E2` | `0x09E8` | buffer flush (discards until empty) |
| `0x2742` | `0x2748` | poll wrapper |

`key_read_char` (`0050:08A0`): peek; if empty return 0; otherwise get, translate
extended keys via a table at `0x8E4`, fold lowercase to uppercase, store result
at `[0x8A2]`.

Menu selection does not use INT 16h. `FUN_0050_3593` commits a choice when bit 4
of `[0x3BFA]` is low:

```c
if ((*(uint *)0x3bfa & 0x10) == 0) { [0x93db] = row; return; }
```

`[0x3BFA]` is produced by the input dispatcher `FUN_0050_24BD`, which branches on
`[0x3B72]`/`[0x3B74]` (the CONTROL CONFIGURATION choice) to one of three device
readers. The keyboard reader `FUN_0050_24A5` is:

```c
return *(uint *)0x3c0e & 0x1f ^ 0xffff;
```

`[0x3C0E]` is a key-state bitmap written only by the game's INT 09h ISR
(`0050:26E0`), which processes keys only when `[0x3C0C] == 1`. `FUN_0050_0592`
sets that flag only when JOYSTICK is selected; with KEYBOARD selected the ISR
chains to the previous handler.

Name entry (`FUN_0050_40BD`) uses the ordinary INT 16h path: backspace `0x08`,
accept `0x20`–`0x5F` up to 9 characters into `[0x104B]`, terminate on `0x0D`.

Timers: the game installs a handler at IVT vector `0x1C` (`0050:128D` writes to
`0000:0070`). The ISR at `0050:12A0` decrements five countdowns at `[0x3C02]`,
`[0x3C04]`, `[0x3C06]`, `[0x3C08]`, `[0x3C0A]`. Menu loops block on these.
The game also hooks INT 08h at `1038:0B80`, which increments a frame flag at
`[0x1038:0AEA]` that the main loop spins on, EOIs the PIC, and chains onward.

---

## Tooling

### Python port (`scratch/boot_port.py`)

Instruction-by-instruction transliteration of the boot sector. Provides an 8086
state class (registers with 8-bit halves, 1MB memory, `rep movsb`, INT 13h) and
four routines: `run_boot`, `run_reloc`, `run_title`, `run_stage2`.

Reproduces: entry `0050:0020`, 44 disk reads, cylinder 4 skipped, and the
segment table `117B 1038 1EB6 27FE 26BE 0050 0050 0050`.

`scratch/show_title.py` renders `B800:0000` from ported memory as CGA mode 4 and
produces the title screen image (10607/16384 bytes nonzero).

`scratch/splash_port.py` ports `res_fetch` and `load_resource`; `load_resource(0x20)`
completes with error code 0 and the descriptor pointers read from live memory as
`11bb 11c7 11d3 11df 11eb 11f7 1203 120f` (stride 12).

### Ghidra

Project `scratch/ghidra_proj2`, program loaded at `0000:0500`.

The image byte 0 corresponds to physical `0x500`. Loading at `0050:0000` causes
Ghidra to report every DS-relative displacement `0x500` too low — the
instruction `8b 9c 6b 13` (`mov bx,[si+0x136b]`) rendered as `&DAT_0050_0e6b`.
Loading at `0000:0500` renders it as `DAT_0000_136b`.

Full decompilation: `scratch/out/pirates_fixed.c` (517 functions).

### `build_image` caveat

`scratch/stage2.py:build_image()` reconstructs stage 2 by concatenating
cylinders. It does not reproduce memory layout exactly. Ported memory
(`c.mem`) is the reliable source.

---

## Emulation harness (`scratch/*.py`, Unicorn-based)

Boots and runs the game to the opening sword duel. Configuration reached by
sending SPACE (dismiss splash), then `1` (CGA), `2` (two drives), `2` (keyboard);
menus driven by setting bit 4 of `[0x3C0E]`; name entry via INT 16h.

Requirements established for a working boot:
- Enter at `CS=0x07C0`, `IP=0` so `mov ax, cs` yields the correct segment.
- `UC_HOOK_INTR` fires before the CPU pushes flags/CS/IP; do not emulate IRET
  inside the hook.
- Equipment word at `0x410` must be `0x0061` for two floppy drives.
- IRQ0 must be delivered to whichever of INT 08h / INT 1Ch is actually hooked.
- The protection read must deliver 2048 bytes (512 data + 1536 × `0x43`) then
  `AH=0x04`, `CF=1`.
