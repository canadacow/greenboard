# Pirates! (1986, DOS) — Verified Findings

Everything below was verified either by executing a Python port of the game's
own boot code, or by reading instruction bytes directly from the disk image.

Target: `assets/pirates_1.img`, `assets/pirates_2.img`.
Both are 368,640 bytes = 40 cylinders × 2 heads × 9 sectors × 512.

Disk 1 contains `0-PIRATE GAME DISK` at offset `0x200`.
Disk 2 contains `1-PIRATE GAME DISK` at offset `0x200`.

---

## Boot chain

Verified by `scratch/boot_port.py`, an instruction-by-instruction port of the
boot sector that runs in Python and loads the disk image.

The boot sector relocates itself 512 bytes to `0020:0000` using a
position-independent `call`/`pop si` sequence, then jumps to `0020:003C`.

Loads performed, as recorded by the port:

| What | Source | Destination |
|---|---|---|
| Title screen | cyl 38 h0 s1, cyl 38 h1 s1, cyl 39 h0 s1, cyl 39 h1 s1 (8 sectors each) | `B800:0000`, `:1000`, `:2000`, `:3000` |
| Stage 2 | cyl 1–21, 8 sectors per head, both heads | `0050:0000` onward, via `2D80` staging |

Stage-2 loader: each cylinder is read to `2D80:0000` (head 0) and `2D80:1000`
(head 1), then `0x2000` bytes are copied to a destination segment starting at
`0x50` and advancing by `0x200` per cylinder. Entry is `0050:0020`.

The port performs 44 disk reads in total.

The stage-2 read of cylinder 4 fails; the destination segment still advances.

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

`main` at `0050:0080` sets `DS` and `ES` from `cs:[0]` (= `117B`), `SS` from
`cs:[8]` (= `26BE`), and `SP` to `0x13FC`.

`DS = 0x117B` is therefore derived by execution, not assumed.

`main` also writes INT opcode bytes into its own code stream at `cs:[0x147]`,
`cs:[0x148]`, `cs:[0x16A]` and `cs:[0x16B]`. Those addresses hold different
values in the disk image, so a disassembly of that region taken before `main`
runs does not reflect the instructions that execute.

---

## Title screen

`scratch/show_title.py` runs the port and decodes `B800:0000` as CGA mode 4
(320×200, 2 bits per pixel, 4 pixels per byte, even scanlines at offset `0`,
odd at `0x2000`, 80 bytes per scanline). The result is the Pirates! title
screen: 10,607 of 16,384 bytes nonzero. Output at `scratch/out/port_title.png`.

---

## Resource system

`load_resource` at `0050:2820` takes a resource index in `AX`, stores it at
`[0x3B84]`, and calls `res_fetch` at `0050:2844`, retrying while the error
byte `[0x11A3]` is nonzero. When the index is zero it additionally relocates
138 word pointers at `DS:0x48C8` by adding `0x48C8` to each.

`res_fetch` indexes a pointer table at `DS:0x136B` by the resource number
doubled, then reads fields from the descriptor it points at:

| Descriptor offset | Field | Copied to |
|---|---|---|
| `+0` | sector count | `[0x11B8]` |
| `+4` | cylinder (plus `[0x3BC8]`) | `[0x11AE]` |
| `+6` | sector | `[0x11AF]` |
| `+8` | destination segment | `[0x11B2]` |
| `+0xA` | destination offset | `[0x11B4]` |

A sector value of 10 or more selects head 1 and has 9 subtracted.

Read from ported memory, the first eight pointers at `DS:0x136B` are
`11bb 11c7 11d3 11df 11eb 11f7 1203 120f` — stride 12, matching the field
layout above.

`res_fetch` and `load_resource` are ported in `scratch/splash_port.py`;
`load_resource(0x20)` completes with error byte `[0x11A3] = 0`.

---

## Text encoding

Lowercase letters are stored XOR `0xE0`. Uppercase, digits and punctuation are
plain ASCII. `0x0D` is a line break; `0xFF` terminates a record.

Verified against disk offset `0x04DA00`, whose bytes decode under this scheme
to the opening narrative text the game displays.

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

Decoding from `0x04F0E0` yields the twelve month names followed by the
`Early in the month.` / `Late in the month.` prompt.

---

## City tables

Six 1024-byte regions on disk 1 contain 24-byte records: a 12-byte
space-padded name followed by 12 data bytes, beginning at offset `+0x0C`
within each region.

| File offset |
|---|
| `0x054000` |
| `0x054400` |
| `0x054800` |
| `0x054C00` |
| `0x055000` |
| `0x055400` |

Names include ANTIGUA, BARBADOS, BELIZE, BERMUDA, CAMPECHE, CARACAS,
CARTAGENA, CORO, CUMANA, GIBRALTAR, HAVANA, MARACAIBO, MARGARITA, PANAMA,
PUERTO BELLO, RIO DE HACHA, SAN JUAN, SANTA MARTA, SANTIAGO, TORTUGA,
TRINIDAD, VERA CRUZ. The regions differ in which cities they contain.

Each region's records terminate with two `FLORIDA CHNL` entries, after which
28 bytes follow. Those 28-byte tails differ between regions and all end with
the same three bytes. Their meaning has not been determined.

---

## Year calculation

At `0050:41DB` the era index is multiplied by `0x14` (20) and `0x618` (1560)
is added, with a further value from `[0x9A9D]` added before the result is
passed to a decimal formatter.

`0050:352F` is that formatter: repeated subtraction against a table of powers
of ten at `[0x3B88]`, emitting one digit per place.

---

## Bytecode interpreter

`0050:3DDF` runs a byte-stream interpreter:

- Program counter at `[0x154D]`.
- One byte is fetched and the pointer advanced.
- The byte is sign-extended and stored at `[0x9A81]`; its absolute value is
  stored at `[0x9A87]` and used as the opcode, so the sign survives as a
  separate flag.
- Dispatch is a chain of comparisons against the opcode.
- Values `0` and `-1` cause a return.

There is no operand stack.

The program to run is selected from a 17-entry pointer table at `DS:0x154F`,
indexed by `[0x4740]` after a bounds check against `0x10`; out-of-range values
fall back to entry 0. Read from live memory the entries are:

```
1444 1450 145e 1470 1481 1491 14a2 14b2 14c1 14d1 14e0 14f1 1500 1510 151f 152e 153e
```

---

## Copy protection

`src/isa/isa_fdc.cpp` in this repository implements the disk check: a read of
C=4 H=0 S=1 with sector-size code `N != 2` must stream the sector's 512 bytes
followed by format gap filler (`0x43`), then report ST0 `0x40` / ST1 `0x04`.
At INT 13h this surfaces as `AH=0x04`, `CF=1`.

Disk 1 offset `0x009000`, which is C=4 H=0 S=1, contains 512 zero bytes.

A scan of every `cmp`, `test` and `sub` instruction in the 172KB stage-2 image
found none reading `[0x4740]`.

---

## Graphics

Character cells are written by `0050:0A9D`, identified by hooking memory writes
to `B8000`–`BBFFF` and observing it was the only routine producing
character-shaped patterns. It writes at `es:[di]` and at offsets `+0x2000`,
`+0x50`, `+0x2050`, `+0xA0`, `+0x20A0`, `+0xF0`, `+0x20F0` — CGA interleaved
banks with 80-byte scanlines.

`0050:0A00` dispatches on `[0x3B98]` to one of three routines (`0xA7D`,
`0x12E8`, `0xBCD`) with the character code in `AL` and a font pointer
`si = 0xA080`.

`[0x3B78]` and `[0x3B7A]` hold cursor column and row.

Image decoding routines, as disassembled:

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

`0050:08A0` peeks; if the buffer is empty it returns zero, otherwise it gets a
key, translates extended keys through a table at `0x8E4`, folds the range
`0x61`–`0x7A` to uppercase by masking with `0xDF`, and stores the result at
`[0x8A2]`.

`0050:09E0` peeks and gets in a loop until the buffer reports empty,
discarding what it reads.

`0050:2740` peeks; if a key is present it gets one and compares `AL` against
`0xE0`, `0x16`, then — when `[0x9AA9]` is zero — `0x56`, `0x76` and `0x20`,
looping on anything else. It returns when the peek reports no key.

`FUN_0050_24A5` returns `[0x3C0E] & 0x1F ^ 0xFFFF`.

`FUN_0050_3593` commits a menu selection when bit 4 of `[0x3BFA]` is clear,
storing the result at `[0x93DB]`.

The INT 09h handler at `0050:26E0` compares `[0x3C0C]` against 1. When it does
not match, it restores registers and chains to a previous handler stored at
`cs:[0x26DC]`. When it matches, it reads port `0x60`, masks the scancode to 7
bits, doubles it, and uses it to index a table at `0xA60`; the value found
there is OR-ed into `[0x3C0E]` for a make code or masked out for a break code.
It then writes `0x20` to port `0x20`.

`FUN_0050_0592` sets `[0x3C0C] = 1` in the branch taken when the key read
returns `'1'`; the other branch clears `[0x3B72]`.

`FUN_0050_40BD` reads keys via `FUN_0050_0860`, treats `0x08` as backspace,
returns on `0x0D`, and stores characters in the range `0x20`–`0x5F` (up to 9)
into `[0x104B]`.

Timer: `0050:128D` writes a handler address to `0000:0070` (IVT vector `0x1C`).
The handler at `0050:12A0` increments `[0x8A0]` and decrements `[0x3C02]`,
`[0x3C04]`, `[0x3C06]`, `[0x3C08]` and `[0x3C0A]` when each is nonzero.

---

## Ghidra project configuration

Stage-2 image byte 0 corresponds to physical address `0x500`.

Loading the image at `0050:0000` causes DS-relative displacements to be
reported `0x500` too low. The instruction at `0x285B` reads a word from
`[si + 0x136b]`; with that base Ghidra rendered the operand as
`&DAT_0050_0e6b`, and `0x136b - 0x0e6b = 0x500`.

Loading at `0000:0500` renders the same operand as `DAT_0000_136b`.

Project: `scratch/ghidra_proj2`. Decompilation of 517 functions:
`scratch/out/pirates_fixed.c`.

---

## Files

| File | Purpose |
|---|---|
| `scratch/boot_port.py` | boot sector ported to Python; runs and loads the disk |
| `scratch/show_title.py` | renders the title screen from ported memory |
| `scratch/stage2_port.py` | `main` prologue |
| `scratch/splash_port.py` | `res_fetch` and `load_resource` ported |
| `scratch/ghidra_reimport.py` | Ghidra import at the correct base |
| `scratch/out/pirates_fixed.c` | full decompilation |
| `scratch/out/port_title.png` | title screen rendered by the port |
