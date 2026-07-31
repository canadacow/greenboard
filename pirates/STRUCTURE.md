# Pirates! (MicroProse, 1987) -- structure

Facts recovered from execution traces running on the Bench 5150 emulator.
Addresses are physical (20-bit) unless a segment is named. Segment-relative
offsets are given as `SEG:off` where the segment register value is known.

Two traces are referenced:

| trace | adapter | notes |
|---|---|---|
| `bench_trace_cga.bcfg` | CGA | source of the CGA sections below |
| `bench_trace.bcfg` | EGA | adds the `PLNW` plane-write section |

Sections describing the frame pipeline, sprites and tile graphics are
adapter-specific and say which build they came from. Boot/load, the world map
decoder and audio are shared.

---

## Boot and load

PC booter. No DOS.

| stage | location | notes |
|---|---|---|
| boot sector | `0000:7C00` | loaded by BIOS `INT 13h` |
| get-IP stub | `07C06`-`07C22` | `call` to next instruction, `pop si`, relocates itself |
| second stage | `0023C`-`00395` | far-jumped to via `ljmp 0x20:0x3c` |

### Second-stage loader (`0023C`-`00395`)

1. Copies the INT 1Eh floppy parameter table to `0020:002C` and repoints the
   vector at the copy.
2. Checks `F000:FFFE` for `0xFF` and `F000:C000` for `0x21`. If either fails,
   rewrites the BIOS equipment word at `0040:0010` (clear `0x30`, set `0x10`)
   to force 40-column colour.
3. `INT 10h AX=0004` -- CGA 320x200 4-colour.
4. Reads `0040:0065`, ORs bit 2, writes to port `3D8h`. Final mode value `0x2E`
   (40-col, graphics, BW bit set).
5. Loads 4 tracks directly into CGA VRAM at `B800:0000` starting at cylinder
   0x26, 8 sectors per read, alternating heads -- the title screen appears
   progressively while loading.
6. Main image: 8KB per pass into a staging buffer at `2D80:0000`, then
   `rep movsb` to a running destination segment starting at `0x50`. Track
   counter at `[1]`, destination segment at `[2]`, limit `0x15` at `[6]`.
   Track 4 is skipped.
7. Adds `0x50` to each of 16 words at `0050:0000` (segment fixups), then
   `ljmp 0x50:0x20`.

Total loaded: 163,840 bytes to `00500`-`284FF`, written by the `rep movsb` at
`0035C`.

Drive number is never set; `DL` retains the BIOS boot drive throughout.

---

## Frame pipeline (CGA)

Three chained buffers:

```
0x28000  composed viewport (16KB, CGA layout)
   |  rep movsw at 01C13, CX=0x2000 words
   v
0x30000  back buffer -- sprites drawn here
   |  rep movsw at 01CDE, 64 words per scanline
   v
0xB8000  CGA video memory
```

Main loop is `0DD40` (585 instructions). Per-frame render chain at `0E7E4`:

| call site | target | role |
|---|---|---|
| `0E7E4` | `0262B` | |
| `0E7E7` | `0AF40` | |
| `0E7EA` | `0A340` | sprite blitter |
| `0E7ED` | `01C5B` | viewport -> back buffer copy |
| `0E7F0` | `030E0` | |
| `0E7F3` | `02C93` | |

Loop exit condition: `[0x401d]` non-zero. Its value is stored to `[0x6475]`
on exit -- a scene selector.

---

## World map

The Caribbean is stored compressed as two levels: an array of 8x8 block
indices, and a library of blocks holding 2 bits per tile.

### Decoder (`0AB0D`-`0AB4F`)

```
0AB0D  mov al, [0x647a]        ; world Y
0AB10  and ax, 0xf8
0AB13  mov bx, ax
0AB15  shl ax, 1
0AB17  shl ax, 1
0AB19  add bx, ax              ; bx = (Y & 0xF8) * 5 = (Y>>3) * 40
0AB1B  mov al, [0x6479]        ; world X
0AB1E  xor ah, ah
0AB20  shr ax, 1
0AB22  shr ax, 1               ; si = X >> 2
0AB24  mov si, ax
0AB26  mov al, [bx+si+0x608c]  ; L1: block index
0AB2A  mov cl, 3
0AB2C  shl ax, cl              ; block * 8
0AB2E  mov bx, ax
0AB30  mov al, [0x647a]
0AB33  and ax, 7               ; row within block
0AB36  mov si, ax
0AB38  mov al, [bx+si+0x588c]  ; L2: block row byte
0AB3C  mov cl, [0x6479]
0AB40  and cl, 3               ; column within byte
0AB43  xor cl, 3
0AB46  shl cl, 1
0AB48  je  0AB4C
0AB4A  shr al, cl
0AB4C  and ax, 3               ; terrain class 0..3
0AB4F  ret
```

| item | address (DS=`117B`) | notes |
|---|---|---|
| L1 block index array | `DS:608C` = `0x1783C` | 40 bytes per block row |
| L2 block library | `DS:588C` = `0x1703C` | 8 bytes per block, one per row |
| world X | `[0x6479]` = `0x17C29` | observed values 0..255 |
| world Y | `[0x647a]` = `0x17C2A` | observed values 0..226 |

Row stride of 40 is confirmed both by the arithmetic and by recorded register
values (`BX = 0x0320` = 800 at block row 20). One L1 entry covers 8x8 tiles;
one L2 byte holds 4 tiles at 2 bits each, most significant field first
(`(X & 3) ^ 3`).

Rendering the L1 array at stride 40 produces Caribbean geography matching the
real coastlines. Strides of 56 and 64 do not.

L2 addresses observed in use reach `0x17797`, which is block 362, so the block
library holds more than 256 entries.

Block index `0x0F` is the most common entry, appearing 625 times in the L1
array; the tiles it decodes to are ocean.

The L1 array is meaningful for rows 0-24. From row 25 the byte statistics
change (no `0x0F` entries, higher distinct-value counts), indicating different
data follows.

### Terrain classes

The decoder returns 2 bits. The caller maps that to a tile byte through a
4-entry table:

```
0AC58  mov bx, ax
0AC5A  mov al, [bx - 0x3f5f]   ; 4 entries, 0x1D851..0x1D854
0AC5E  or  al, 0x50
0AC60  mov [0xc09d], al
```

Counts over the 160x200 map:

| class | tiles | meaning |
|---|---|---|
| 3 | 25302 | ocean |
| 2 | 6145 | land |
| 1 | 523 | shallows / coastal water |
| 0 | 30 | cities |

The 30 class-0 tiles each fall in a **different** L1 block index -- no block
index repeats -- and they sit on coastlines, clustered along the Spanish Main
(y 160-190) and the Greater Antilles (y 90-110). Cities are therefore encoded
directly in the 2-bit terrain array, not overlaid separately.

Class-0 tile coordinates as observed:

```
( 65,  4) ( 75, 33) ( 80, 47) ( 65, 49) ( 85, 50) ( 60, 64) ( 78, 79)
( 26, 90) ( 86, 91) (  4, 92) (107, 93) (102,103) (127,103) ( 17,104)
(111,104) ( 83,110) ( 44,160) (113,165) ( 99,166) (139,167) ( 94,168)
(137,172) (149,172) (125,173) (105,174) (121,174) ( 89,178) (108,183)
( 72,184) ( 71,189)
```

### Map renderer (`0ABAA`-`0AD28`)

`0ABAA` is the entry point. It executed 4 times in the 32M-instruction CGA
trace and 4 times in the 30M-instruction EGA trace, called from `0CABC`, once
per viewport redraw. It reads its window origin from `[0x93b9]` and `[0x93bb]`
and manages all other state internally.

Setup:

```
0ABAA  mov ax, [0x93b9]        ; window origin X
0ABAD  mov [0x6479], al
0ABB0  mov ax, [0x93bb]        ; window origin Y
0ABB3  mov [0x647a], al
0ABB6  mov al, [0x6479]
0ABB9  mov [0xc09e], al        ; X reset value for each row
0ABBC  mov byte ptr [0x6475], 0x0b   ; 11 cells per row
0ABC1  mov al, [0x647a]
0ABC4  push ax
0ABC5  dec byte ptr [0x647a]         ; probe the row above
0ABC9  cmp byte ptr [0x647a], 0xff
0ABCE  jne 0ABD5
```

Priming pass (`0ABD5`-`0ABF6`): walks one row calling the class decoder and
seeding a 16-entry per-column parity table at `[si - 0x6c42]`
(`0x1AB6E`-`0x1AB7D`):

```
0ABD5  call 0AB0D
0ABD8  mov bx, ax
0ABDA  mov al, [0x6479]
0ABDD  and ax, 0xf
0ABE0  mov si, ax
0ABE2  mov al, [bx - 0x3f5f]
0ABE6  and al, 8
0ABE8  xor al, 8
0ABEA  mov [si - 0x6c42], al
0ABEE  inc byte ptr [0x6479]
0ABF2  dec byte ptr [0x6475]
0ABF6  jns 0ABD5
```

Main loop setup and bounds:

```
0ABF8  pop ax
0ABF9  mov [0x647a], al
0ABFC  mov al, [0xc09e]
0ABFF  mov [0x6479], al
0AC02  mov word ptr [0xc09f], 0x54
0AC08  mov byte ptr [0x6476], 0x0e   ; 14 rows
0AC0D  mov ax, 0x8ab1
0AC10  mov [0x9a57], ax              ; working array cursor
0AC13  mov byte ptr [0x6475], 0x0b   ; 11 cells per row
0AC18  dec byte ptr [0x6479]         ; probe X-1 for row parity
0AC1C  call 0AB0D
0AC21  mov al, [bx - 0x3f5f]
0AC25  and al, 8
0AC27  xor al, 8
0AC29  mov [0x93ce], al
0AC2C  inc byte ptr [0x6479]
0AC30  call 0AC55                    ; one cell
0AC33  inc byte ptr [0x6479]
0AC37  dec byte ptr [0x6475]
0AC3B  jne 0AC30
0AC3D  add word ptr [0x9a57], 0x2c
0AC42  mov al, [0xc09e]
0AC45  mov [0x6479], al
0AC48  inc byte ptr [0x647a]
0AC4C  dec byte ptr [0x6476]
0AC50  jne 0AC13
```

Window is **11 cells wide by 14 rows**. Each cell produces 4 tile bytes wide
by 2 rows tall, giving the 44 x 28 working array (25 rows used).

Note `0AC2C` and `0AC33`: X is incremented both before entering the inner loop
and inside it.

### Per-cell routine (`0AC55`-`0ACFC`)

```
0AC55  call 0AB0D              ; terrain class 0..3
0AC58  mov bx, ax
0AC5A  mov al, [bx - 0x3f5f]   ; class -> tile byte, 4 entries
0AC5E  or  al, 0x50
0AC60  mov [0xc09d], al
0AC63  and al, 8
0AC65  mov [0x93bd], al
0AC68  mov bl, [0x6479]
0AC6C  and bx, 0xf
0AC70  mov al, [bx - 0x6c42]   ; per-column parity table
0AC74  cmp al, [0x93bd]
0AC78  jne 0AC7F
0AC7A  xor byte ptr [0xc09d], 0x30
0AC7F  mov al, [0x93bd]
0AC82  xor al, 8
0AC84  mov [bx - 0x6c42], al   ; flip this column's parity
0AC88  mov al, [0x647a]
0AC8D  shl ax, 1
0AC8F  add ax, 0xbe90
0AC92  mov [0x9a5b], ax        ; variant cursor = 0xBE90 + Y*2
0AC95  mov dx, 0
0AC98  mov si, 3               ; four tiles, high index to low
0AC9B  mov bx, [0x9a5b]
0AC9F  mov al, [bx + si]
0ACA1  and al, 3
0ACA3  or  al, [0xc09d]
0ACA7  mov bx, [0x9a57]
0ACAB  mov [bx + si], al
0ACAD  dec si
0ACAE  jns 0AC9B
0ACB0  mov al, [0x93bd]
0ACB3  cmp al, [0x93ce]
0ACB7  jne 0ACCF
0ACB9  inc si                  ; extra cell when row parity matches
0ACBA  mov al, [0xc09d]
0ACBD  and al, 0x10
0ACBF  je  0ACC3
0ACC1  mov al, 1
0ACC3  or  al, [0xc09d]
0ACC7  or  al, 0x30
0ACC9  mov bx, [0x9a57]
0ACCD  mov [bx + si], al
0ACCF  mov al, [0xc09d]
0ACD2  and al, 0xf
0ACD4  or  al, 0x50
0ACD6  mov [0xc09d], al
0ACD9  add word ptr [0x9a57], 0x2c
0ACDE  mov al, [0x6479]
0ACE3  add word ptr [0x9a5b], ax
0ACE7  inc dx
0ACE8  cmp dx, 2
0ACEB  jne 0AC98
0ACED  mov ax, [0xc09f]
0ACF0  sub word ptr [0x9a57], ax
0ACF4  mov al, [0x93bd]
0ACF7  xor al, 8
0ACF9  mov [0x93ce], al
0ACFC  ret
```

| item | address (DS=`117B`) | notes |
|---|---|---|
| class -> tile byte | `[bx - 0x3f5f]` = `0x1D851`-`0x1D854` | values `{00, 0C, 04, 08}` |
| per-column parity | `[bx - 0x6c42]` = `0x1AB6E`-`0x1AB7D` | 16 entries, flipped each visit |
| variant source | `DS:BE90` = `0x1D640` | `+ Y*2`, `+= X` per pass |
| current tile byte | `[0xc09d]` = `0x1D84D` | carried between cells |
| column parity | `[0x93bd]` = `0x1AB6D` | |
| row parity | `[0x93ce]` = `0x1AB7E` | |
| working cursor | `[0x9a57]` | `+= 0x2C` per pass, `-= [0xc09f]` at end |
| row-back constant | `[0xc09f]` | set to `0x54` at `0AC02` |

Observed tile bytes in a rendered viewport: `0x50`-`0x5E` and `0x64`-`0x79`.

Observed values during one redraw: `[0x93bd]` = `0x08` and `[0x93ce]` = `0x00`
throughout, so the `0ACB3` comparison never matched and the extra-cell write at
`0ACCD` did not execute.

### Reproducing the renderer

`runmap.py` interprets these instructions directly over a memory image taken
from a trace snapshot, rather than reimplementing the rule. Running `0ABAA`
with `[0x93b9]`/`[0x93bb]` set to the recorded window origin (145, 166)
reproduces all 1100 bytes of the game's working array at `0x1A261` exactly, and
the interpreter's instruction path matches the recorded execution path for
every step there is trace data to compare against.

Reimplementations of the tile rule as a formula reached 88% byte agreement and
were sensitive to which terms were included; the failures were concentrated in
column 0 of each row, which is the state established by the priming pass at
`0ABD5`.

---

## Viewport

### Layout

40 columns x 25 rows, one byte per tile. Bounds are explicit in the render
loop at `01B7B`:

```
01B87  mov  word ptr [0x3b7a], 0     ; row = 0
01B8D  mov  word ptr [0x3b78], 0     ; col = 0
01B93  mov  si, [0x3b7a]
01B97  shl  si, 1
01B99  mov  bx, [si + 0xa26]         ; row pointer table
01B9D  add  bx, [0x3b78]             ; + column
01BA1  mov  al, [bx]                 ; tile index
...
01BDD  cmp  word ptr [0x3b78], 0x28  ; 40 columns
01BE8  cmp  word ptr [0x3b7a], 0x19  ; 25 rows
```

Rows are **not** contiguous. `DS:0x0A26` holds a table of 25 word pointers,
one per row.

| item | address (DS=`117B`) | notes |
|---|---|---|
| row pointer table | `DS:0A26` = `0x121D6` | 25 words |
| row data | `0x1A761` onward | rows 0x28 bytes apart in the observed sample |
| row counter | `[0x3b7a]` | 0..24 |
| column counter | `[0x3b78]` | 0..39 |

Observed tile values cluster in `0x54`-`0x5B`, with sparse other values.

### Redraw frequency

The terrain is not redrawn per frame. It is redrawn only when the view
scrolls. Three redraws occurred in a 32M-instruction trace, at instruction
counts 20,493,218 / 24,410,075 / 30,154,427, each writing ~32,320 bytes over
16,352 distinct addresses of `0x28000`-`0x2BFFF` within roughly 85,000
instructions. Between redraws only sprites are drawn.

The row-pointer values change between redraws, which is what moves the
viewport across the world.

### Tile graphics (CGA)

16 bytes per tile, base `0x1B830` (`SI = 0xA080` with `DS = 117B`).
Index scaling is `shl ax, 4` in the blitter.

Renderer `019ED`-`01A32`, fully unrolled, eight `lodsw`/`mov` pairs:

| source word | destination offset |
|---|---|
| 1 | `di + 0x0000` |
| 2 | `di + 0x2000` |
| 3 | `di + 0x0050` |
| 4 | `di + 0x2050` |
| 5 | `di + 0x00A0` |
| 6 | `di + 0x20A0` |
| 7 | `di + 0x00F0` |
| 8 | `di + 0x20F0` |

`0x50` is the CGA row pitch (80 bytes); `0x2000` is the odd-scanline bank. So
a tile is 8 pixels wide (2 bytes at 2bpp) by 8 scanlines, written as four row
pairs across both interleave banks.

Destination address: `[si + 0x3c6f]` indexed by row, plus column x 2.

---

## Adapter paths

Three tile renderers exist. Selection is via `[0x3b98]`.

| renderer | tile size | scaling | adapter |
|---|---|---|---|
| `019ED` | 16 bytes | `shl ax, 4` | CGA |
| `01A33` | 32 bytes | `shl ax, 5` | 16-colour, 4 banks, word writes |
| `01AAB` | 32 bytes | `shl ax, 5` | EGA, 4 banks, byte writes |

Sprite blitters follow the same split: `0A4F6` (CGA), `0A438` (16-colour),
`0A5C6` (EGA).

Comparing CFG nodes between the two traces:

| set | count |
|---|---|
| CGA trace nodes | 7931 |
| EGA trace nodes | 8307 |
| shared | 6843 |
| CGA only | 1088 |
| EGA only | 1464 |

`0ABAA` (the map renderer) appears in both. `019ED` and `0A4F6` appear only in
the CGA trace; `01AAB`, `0A5C6` and `01C4C` only in the EGA trace. `01A33` and
`0A438` appear in neither, so the 16-colour path did not execute in either
trace.

---

## Tile graphics (EGA)

32 bytes per tile, base `0x1B830` -- the same base as CGA, with index scaling
`shl ax, 5` instead of `shl ax, 4`. Tiles `0x50`-`0x79` therefore occupy
`0x1C230`-`0x1C76F`, confirmed by the `READ` records of the blitter's `lodsb`
instructions.

Renderer `01AAB`-`01B7A`, fully unrolled, 32 `lodsb`/`mov` pairs. Destination
offsets in source order:

| bytes | destination offsets |
|---|---|
| 1-8 | `0x0000` `0x0028` `0x0050` `0x0078` `0x00A0` `0x00C8` `0x00F0` `0x0118` |
| 9-16 | `0x2000` + the same eight |
| 17-24 | `0x4000` + the same eight |
| 25-32 | `0x6000` + the same eight |

`0x28` is 40 bytes, the EGA row pitch at 320 pixels wide. `0x2000` is the
plane stride. So byte `k` of a tile is plane `k / 8`, row `k % 8`, eight
pixels MSB-first -- one byte per plane per row.

Setup is identical in form to the CGA renderer:

```
01AAB  mov bx, [0x3b7a]        ; row
01AAF  shl bx, 1
01AB1  mov di, [bx + 0x3c6f]   ; row pointer table
01AB5  add di, [0x3b78]        ; + column (1 byte per tile, not 2)
01AB9  mov cl, 5
01ABB  shl ax, cl              ; tile index * 32
01ABD  add si, ax
```

Decoding the bank this way and comparing against a frame reconstructed from
the trace's plane records: 947 of 1000 8x8 cells match a tile exactly. The 53
non-matching cells are the positions where ship and cloud sprites overlap the
terrain.

### Screen push (`01C30`-`01C5A`)

The EGA build composes a full 4-plane frame in main RAM and copies it to the
card one plane at a time, rather than writing planes during tile drawing.

```
01C30  mov ax, [0xc]           ; ES = video segment
01C33  mov es, ax
01C35  mov ax, 0xff08          ; GC bit mask = FF
01C38  mov dx, 0x3ce
01C3B  out dx, ax
01C3C  xor si, si
01C3E  mov ah, 1               ; plane mask, shifted left each pass
01C40  mov al, 2               ; Sequencer Map Mask index
01C42  mov dx, 0x3c4
01C45  out dx, ax
01C46  xor di, di
01C48  mov cx, 0x1000
01C4B  rep movsw               ; 8KB into the selected plane
01C4D  shl ah, 1
01C4F  cmp ah, 0x10
01C52  jb  01C40
```

`01C4C` is the highest-frequency plane-writing instruction in the EGA trace
(20,398,080 plane records past instruction 25,000,000). Source segment is
`27FE`, the same staging buffer the CGA build uses as its composed viewport.

### Video mode

Registers replayed from the port log at instruction 29,000,000:

| register | value | meaning |
|---|---|---|
| Misc Output | `0x23` | colour base `3Dx`, RAM enable |
| Sequencer 1 | `0x0B` | bit 3 set: dot clock halved (320 wide) |
| Sequencer 4 | `0x06` | bit 2 set: sequential addressing |
| GC 5 | `0x00` | write mode 0, odd/even off |
| GC 6 | `0x05` | graphics mode, window `B8000` 32K |
| CRTC 1 | `0x27` | 40 characters displayed |
| CRTC 18 | `0xC7` | 200 lines displayed |
| CRTC 19 | `0x14` | offset 20 words = 40 bytes per row |

Frame total is under 300 lines, so the monitor runs at 15.7 kHz and decodes
RGBI rather than 6-bit rgbRGB (see `ega_color()` in
`src/display/ega_display.cpp`).

Attribute palette registers observed: `11 20 02 00 20 20 06 07 10 11 12 13 14
00 16 17`. Colour indices present in a rendered frame: 0, 2, 6, 7, 8, 9, 10,
11, 13, 14, 15.

---

## Sprites (CGA)

Separate system from the terrain tiles.

### Atlas

| item | address | notes |
|---|---|---|
| pointer table | `ES:0040` = `0x1EBA0` | one word per sprite, offset within segment `1EB6` |
| sprite records | `0x1ECA2` onward | packed contiguously, no padding |

Record format:

```
+0  byte  hotspot X
+1  byte  hotspot Y
+2  byte  width
+3  byte  height
+4  ...   width*height bytes, one byte per pixel
```

Record size is `4 + width*height`. Verified: slot 25 pointer `0x1C1E`,
slot 26 pointer `0x1CD2`, difference 180 = `4 + 16*11`.

The atlas is loaded from disk by the boot loader (`0035C`), not generated.

### Atlas extent

The table has no count field. Its length follows from the data: every record
ends exactly where the next one begins, and that chain holds unbroken for 129
slots. Slot 129's pointer yields dimensions 163x94, which is not a sprite.

| item | value |
|---|---|
| slots | 129 |
| pointer table | `0x1EBA0`-`0x1ECA2` (129 words) |
| sprite records | `0x1ECA2`-`0x26BDF` (32,573 bytes) |
| largest sprite | 46 x 42 |

The 129-word table ends precisely where the first record starts, so the table
size is fixed by the layout rather than inferred.

### Pixel encoding

In the CGA build the pixel bytes are a stencil: observed values `0x00`
(transparent), `0x55`, `0xFF`, with colour supplied from `es:[0x408d]`, split
into four pre-shifted 2-bit fields at `0A502`-`0A51E` and stored at
`[0x1c]`-`[0x1f]` for the four pixel positions within a CGA byte.

In the EGA build the bytes carry colour directly. Every value across all 129
sprites is a doubled nybble, and the low nybble is an EGA colour index:

| value | count | value | count |
|---|---|---|---|
| `00` | 18911 | `88` | 192 |
| `11` | 826 | `BB` | 75 |
| `22` | 808 | `CC` | 344 |
| `33` | 455 | `DD` | 6123 |
| `44` | 93 | `EE` | 10 |
| `66` | 253 | `FF` | 2298 |
| `77` | 1669 | | |

`0x00` is transparent, 18,911 of 32,057 pixel bytes.

### Slot contents (EGA trace)

| slots | contents |
|---|---|
| 0-12 | cursor arrow, sword, smoke puffs, small icons |
| 13-18 | clouds, six variants |
| 19-51 | ships, several hull classes with roughly eight headings each |
| 52-55, 84-87 | full-body figures |
| 56-75, 88-107 | sword and arm poses, held in separate slots from the bodies |
| 76, 108 | figure with arms raised |
| 77-83 | legs, blue breeches |
| 109-115 | legs, green breeches, same poses as 77-83 |

Duel figures are composited from separate body, leg and arm slots rather than
stored as whole frames.

The CGA-trace slot numbers differ: clouds were observed at 25-30 and ships at
48-62 there.

Sprite data regions touched in one CGA trace: `1ECA6`-`1EDA1`,
`1FC50`-`1FF13`, `20782`-`20BA5` (clouds), `21709`-`21C00` (ships),
`21E7C`-`22233`.

`pirates/sprite_atlas.json` holds slot, hotspot, dimensions and address for
all 129 entries; `scripts/sprite_atlas.py` regenerates it and the contact
sheet from a trace.

### Blitter (`0A340`-`0A5C5`)

Sprite ID comes from `[0x4087]`; screen origin from `[0x4089]` / `[0x408b]`.

```
0A3E6  mov si, [0x4087]        ; sprite ID
0A3EA  shl si, 1
0A3EC  mov bx, es:[si + 0x40]  ; pointer table lookup
0A3F1  mov al, es:[bx]         ; header
```

Scanline start addresses come from a 200-word lookup table at
`es:[bx + 0x3ca1]`, indexed by Y doubled. X contributes `X >> 2` (four pixels
per byte).

Inner loop (`0A55E`-`0A592`) per pixel:

- X clipping against `[8]` and `[0xa]`
- mask source byte by pixel position via `[si + 0x24]`; zero skips
- colour substitution: compare against `[si + 0x18]`, replace with `[si + 0x1c]`
- read-modify-write destination, preserving other pixels via `[si + 0x20]`
- `si` cycles 0-3, `di` advances when it wraps

Scanline advance at `0A594`: if offset >= `0x2000` subtract `0x1fb0`, else add
`0x2000` -- the CGA bank interleave.

### Entity table

Sprite placement is driven by a table walked once per frame.

```
0CF4E  mov ax, [0x3fe3]     ; entity index
0CF51  inc ax
0CF54  shl ax, 3            ; x8
0CF58  shl ax, 1            ; x16
0CF5A  add bx, ax           ; = index * 24
0CF5C  add bx, 0x47c0       ; table base
0CF60  mov [0x400d], bx     ; record pointer
```

| item | address (DS=`117B`) | notes |
|---|---|---|
| entity table | `DS:47C0` = `0x15F70` | 24 bytes per record |
| entity count | `[0x3b55]` | |
| loop index | `[0x3fe3]` | |
| record pointer | `[0x400d]` | set per iteration |

First four bytes of a record are world X and Y as words. Map coordinates
scale to world coordinates as `X * 256 + 0x80` and `Y * 128 + 0x40`
(`0D138`-`0D153`), so tiles are 256 x 128 world units.

Player position bytes at `[0x4841]` / `[0x4842]` (`0x15FF1` / `0x15FF2`).
These are saved to `[0x3ffd]` / `[0x3fff]` and restored at `0D0DE`, so they
double as scratch for whichever entity is being drawn.

Another 24-byte-record table exists at `[0x4240]`, indexed via
`mov ax, 0x18 / mul` at `042B6`.

---

## Audio

PC speaker, bit-banged. Three phase accumulators, two carrying pitch and one
acting as the note timer.

### Player (`11241`-`112C4`)

Entry stubs set a tune pointer and jump to `11241`. One observed:
`111E3  lea bx, [0x85]`.

Note fetch (`1124A`-`11265`):

```
mov bx, [0x0ad7]   ; song pointer
mov ax, [bx]       ; word 0 -> SI, duration; zero terminates
inc bx / inc bx
mov dx, [bx]       ; word 1
mov dh, dl         ; voice A period = low byte
mov dl, 2          ; seed counter A
inc bx / inc bx
mov cx, [bx]       ; word 2
mov bh, cl         ; voice B period = low byte
mov cl, 1          ; seed counter B
xor bl, bl         ; seed counter C
inc bx / inc bx
mov [0x0ad7], bx   ; advance
```

Record is 6 bytes: duration word, then two words whose **low bytes** are the
voice periods. The high bytes are not read.

| item | address (DS=`1038`) | notes |
|---|---|---|
| song pointer | `[0x0ad7]` = `0x10E57` | advances 6 bytes per note |
| tempo divisor | `[0x0adb]` = `0x10E5B` | observed value 1 |
| tune (intro) | `DS:0085` = `0x10405` | 7 records, all silent, duration 30 |
| tune (main) | `DS:0583` = `0x10903` | 34 records |

### Mixing loop (`1127D`-`112C4`)

Three identical accumulator blocks, one per voice:

```
sub  dl, 1      ; decrement counter
rcl  ax, 1      ; borrow -> AX bit 0
neg  ax         ; 0 or 0xFFFF -- branchless mask
and  al, dh     ; reload period if it underflowed
add  dl, al
```

Voice C (`bl`/`bh`) also decrements the duration in `SI` on borrow; when `SI`
reaches zero the next note is fetched. `bh` reloads to `0x64` at `112 7B`.

Output: voices OR together, are masked by `BP` (`0x2FF`, with bit 9 toggled
periodically), and are written to port `61h` twice per pass -- `mov al, 0x4c`
then the computed value. Bit 1 of port `61h` is the speaker line.

Both voices OR onto a single bit, so the speaker output is their combined
square-wave sum, not two independent channels.

An abort-check stub is called at `11245`. The byte at `0x11229` is
self-modified between `C3` (`ret`) and `90` (`nop`) -- the only code byte in
the trace observed to change.

### Timing

The player's tick rate was measured from records where both voice periods are
equal (both voices toggle together, so speaker transitions map to one period
unambiguously). Seven such notes give 78,288-85,148 Hz, spread ~8%.

Separately, port `61h` write pairs occur roughly every 31 instructions.

The main tune runs 10.0 seconds of emulated time at 4.77 MHz.

### Other audio path

PIT channel 2 (port `42h`) is written 662 times by `1038:0DCE`, separate from
the speaker bit-banging. Counter 0 is reprogrammed to `0x4DAE` (60 Hz) by
`1038:0C0C` and restored to the BIOS default by `1038:0D05`.

---

## Segments observed

| segment | base | contents |
|---|---|---|
| `0020` | `0x00200` | second-stage loader |
| `0050` | `0x00500` | main program code |
| `117B` | `0x117B0` | game state, map row table, tile graphics |
| `1038` | `0x10380` | audio player and song data |
| `1EB6` | `0x1EB60` | sprite atlas |
| `27FE` | `0x27FE0` | composed viewport buffer; EGA plane staging source |
| `3000` | `0x30000` | back buffer |
| `B800` | `0xB8000` | CGA video memory; EGA window in the observed mode |

The segment table is at `CS:0000` with `CS = 0050`. Values read from the EGA
trace:

| offset | value | base |
|---|---|---|
| `0` | `117B` | `0x117B0` |
| `2` | `1038` | `0x10380` |
| `4` | `1EB6` | `0x1EB60` |
| `6` | `27FE` | `0x27FE0` |
| `8` | `26BE` | `0x26BE0` |
| `A` | `B800` | `0xB8000` |
| `C` | `3000` | `0x30000` |

`01B7B` loads `DS` from `cs:[0]` and `ES` from `cs:[6]` before dispatching to
a tile renderer, so all three renderers read tiles from the same base.

In the EGA trace the video BIOS at `C0000` also executes (`C007F`-`C03DE`
among others); the CGA trace has no nodes there.

---

## Trace facts

### EGA plane capture

CPU-side write records cannot reconstruct EGA video memory: the byte on the
bus is transformed by set/reset, the ALU against the latches, the bit mask and
the plane mask before it lands, and write mode 1 discards the CPU data
entirely. The `PLNW` section records the post-pipeline byte per plane instead,
written from `ISA_EGA::on_mmio_write`.

Record layout (20 bytes on disk): `instr` u64, `off` u32, `cs` u16, `ip` u16,
`plane` u8, `data` u8, pad u16. `off` is an offset within the plane, not a
physical address, because the CPU-visible window moves with Graphics Misc bits
2-3.

`FastTrace.plane_snapshot(instr)` in `scripts/bcfg_fast.py` returns a
`(4, 65536)` array by scatter, the same way `snapshot()` builds the flat 1 MB
image.

### Trace sizes

| section | CGA trace | EGA trace |
|---|---|---|
| writes | 43,502,059 | 13,805,944 |
| reads | 19,912,723 | 16,502,146 |
| ports | 533,185 | 1,867,736 |
| states | 28,123,616 | 26,540,542 |
| exec | -- | 30,744,903 |
| planes | -- | 52,189,975 |

Plane write distribution across the four planes in the EGA trace:
13,507,748 / 13,163,246 / 12,501,348 / 13,017,633.

### Full-map render

`pirates/map_tiles.npy` (400 x 640 tile bytes, produced by interpreting
`0ABAA`) is adapter-independent, since `0ABAA` is shared. Rendering it through
the CGA bank at 16-byte stride gives `pirates/map_full.png`; through the EGA
bank at 32-byte stride gives `pirates/map_ega.png`. Both are 5120 x 3200.

Colour indices used across the whole EGA map: 0, 2, 6, 8, 10, 13, 14.
