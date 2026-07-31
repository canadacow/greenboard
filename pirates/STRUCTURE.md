# Pirates! (MicroProse, 1987) -- structure

Facts recovered from execution traces of the CGA build running on the Bench
5150 emulator. Addresses are physical (20-bit) unless a segment is named.
Segment-relative offsets are given as `SEG:off` where the segment register
value is known.

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

## Frame pipeline

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

Rendering the L1 array at stride 40 produces recognisable Caribbean geography:
Cuba, Hispaniola, Puerto Rico, the Lesser Antilles, Florida, the Bahamas, the
Yucatan, and the Spanish Main. Strides of 56 and 64 produce noise.

L2 addresses observed in use reach `0x17797`, which is block 362, so the block
library holds more than 256 entries.

Block index `0x0F` is open ocean and is by far the most common entry.

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

### Features drawn over the terrain

Two mechanisms modify or add cells after the terrain class is resolved.

A 16-entry table indexed by the low nybble of world X:

```
0AC68  mov bl, [0x6479]
0AC6C  and bx, 0xf
0AC70  mov al, [bx - 0x6c42]   ; 16 entries, 0x1AB6E..0x1AB7D
0AC74  cmp al, [0x93bd]
0AC78  jne 0AC7F
0AC7A  xor byte ptr [0xc09d], 0x30
```

A conditional single-cell write, gated on a comparison of `[0x93bd]` against
`[0x93ce]`:

```
0ACB0  mov al, [0x93bd]
0ACB3  cmp al, [0x93ce]        ; 0x1AB7E
0ACB7  jne 0ACCF
0ACB9  inc si
0ACBA  mov al, [0xc09d]
0ACBD  and al, 0x10
0ACBF  je  0ACC3
0ACC1  mov al, 1
0ACC3  or  al, [0xc09d]
0ACC7  or  al, 0x30
0ACC9  mov bx, [0x9a57]
0ACCD  mov [bx + si], al       ; extra cell
```

`[0x93bd]` = `0x1AB6D`, written 616 times by `0AC65`. `[0x93ce]` = `0x1AB7E`,
written 616 times by `0ACF9` and 56 times by `0AC29`.

Cities are class 0 in the terrain array (see above), so these two mechanisms
select tile variants rather than place features.

### Variant table

The low 2 bits of the final tile byte come from a separate table, not from the
terrain array:

```
0AC88  mov al, [0x647a]        ; world Y
0AC8D  shl ax, 1
0AC8F  add ax, 0xbe90
0AC92  mov [0x9a5b], ax        ; = 0xBE90 + Y*2

0AC98  mov si, 3               ; four tiles per pass, high to low
0AC9B  mov bx, [0x9a5b]
0AC9F  mov al, [bx + si]       ; variant source byte
0ACA1  and al, 3
0ACA3  or  al, [0xc09d]        ; merge with the class byte
0ACA7  mov bx, [0x9a57]
0ACAB  mov [bx + si], al       ; -> 44-wide working array
0ACAD  dec si
0ACAE  jns 0AC9B

0ACD9  add word ptr [0x9a57], 0x2c   ; next working-array row
0ACE3  add word ptr [0x9a5b], ax     ; advance variant cursor by world X
0ACE8  cmp dx, 2                     ; two passes
```

| item | address (DS=`117B`) | notes |
|---|---|---|
| variant table | `DS:BE90` = `0x1D640` | indexed by `Y*2`, walked by X |
| working-array cursor | `[0x9a57]` | advances 0x2C (44) per row |
| variant cursor | `[0x9a5b]` | `0xBE90 + Y*2`, then `+= X` per row |

Confirmed from recorded register state: at `0AC9F` with `BX = 0xBFDC`, the
offset from the table base is 332 = 2 * 166, matching world Y = 166.

### Final tile byte

```
class      = 2 bits from L1/L2 at (X, Y)
class_byte = [0x1D851 + class]          ; {00, 0C, 04, 08}
base       = 0x50 | class_byte
variant    = [0xBE90 + Y*2 + x_offset] & 3
tile       = base | variant             ; plus the 0x30 toggle above
```

Observed tile bytes in a rendered viewport: `0x50`-`0x5E` and `0x64`-`0x79`,
consistent with this construction.

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

### Tile graphics

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

## Sprites

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

Pixel bytes are a **stencil**, not colour. Observed values are `0x00`
(transparent), `0x55`, `0xFF`. The colour comes from `es:[0x408d]`, split into
four pre-shifted 2-bit fields at `0A502`-`0A51E` and stored at `[0x1c]`-`[0x1f]`
for the four pixel positions within a CGA byte.

The atlas is loaded from disk by the boot loader (`0035C`), not generated.

### Known slot ranges

| slots | dimensions | contents |
|---|---|---|
| 25-30 | 16 wide, 10-12 tall | clouds, six variants |
| 48-62 | 9-20 wide, 12-14 tall | ship, one per heading |

Sprite data regions touched in one trace: `1ECA6`-`1EDA1`, `1FC50`-`1FF13`,
`20782`-`20BA5` (clouds), `21709`-`21C00` (ships), `21E7C`-`22233`.

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

Because both voices OR onto a single bit, the audible result is their
interference pattern rather than two separable tones.

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
| `27FE` | `0x27FE0` | composed viewport buffer |
| `3000` | `0x30000` | back buffer |
| `B800` | `0xB8000` | CGA video memory |
