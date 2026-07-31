"""Decode the Pirates! sprite atlas from a trace and render a contact sheet.

The pointer table at ES:0040 = 0x1EBA0 holds one word per sprite, an offset
within segment 1EB6. Each record is:

    +0  hotspot X   +1  hotspot Y   +2  width   +3  height
    +4  width*height pixel bytes

There is no count field. The table length follows from the data: every record
ends exactly where the next begins, and that chain holds for 129 slots before
the dimensions stop being plausible. The 129-word table also ends precisely
where the first record starts.

In the EGA build the pixel bytes carry colour directly -- each is a doubled
nybble whose low half is an EGA colour index, with 0x00 transparent. (In the
CGA build the same field is a stencil; see pirates/STRUCTURE.md.)

Usage:
    python scripts/sprite_atlas.py [trace.bcfg] [out.png]
"""

import json
import os
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bcfg_fast import FastTrace

SEG = 0x1EB60          # sprite atlas segment 1EB6
TAB = 0x1EBA0          # pointer table, ES:0040
AT = 26_000_000        # any instant after the atlas has loaded


def load(path, at=AT):
    """Walk the table and return [(slot, hx, hy, w, h, pixels), ...]."""
    t = FastTrace(path)
    mem = np.array(t.snapshot(at), dtype=np.uint8)

    def w16(a):
        return int(mem[a]) | (int(mem[a + 1]) << 8)

    out = []
    prev_end = None
    for i in range(400):
        a = SEG + w16(TAB + i * 2)
        if a + 4 >= len(mem):
            break
        hx, hy, w, h = (int(mem[a]), int(mem[a + 1]),
                        int(mem[a + 2]), int(mem[a + 3]))
        if not (1 <= w <= 64 and 1 <= h <= 64):
            break
        # Records are packed with no padding; a gap means we are past the end.
        if prev_end is not None and a != prev_end:
            break
        out.append((i, hx, hy, w, h, mem[a + 4:a + 4 + w * h].reshape(h, w)))
        prev_end = a + 4 + w * h
    return t, mem, out


def ega_palette(t, at=29_000_000):
    """Attribute-controller palette, replayed from the port log.

    Every EGA register is write-only, so the port log is the only record. The
    attribute controller shares one port with an index/data flip-flop that a
    status read resets, so reads matter as well as writes.
    """
    po = t.ports
    attr, ai, flip = {}, 0, False
    for r in po[po["instr"] <= at]:
        pt, v, isw = int(r["port"]), int(r["data"]), int(r["is_write"])
        if pt in (0x3BA, 0x3DA):
            if not isw:
                flip = False
            continue
        if not isw:
            continue
        if pt == 0x3C0:
            if not flip:
                ai = v & 0x1F
            else:
                attr[ai] = v
            flip = not flip

    # 320x200 is a 15.7 kHz frame, so the monitor decodes RGBI. Transcribed
    # from ega_color() in src/display/ega_display.cpp.
    def rgb(c):
        idx = (((c >> 4) & 1) << 3) | (c & 7)
        i = (idx >> 3) & 1
        r = ((idx >> 2) & 1) * 0xAA + i * 0x55
        g = ((idx >> 1) & 1) * 0xAA + i * 0x55
        b = ((idx >> 0) & 1) * 0xAA + i * 0x55
        if idx == 6:
            g = 0x55
        return (r, g, b)

    return np.array([rgb(attr.get(i, 0)) for i in range(16)], dtype=np.uint8)


# 3x5 digits for the slot labels -- no font dependency.
DIGITS = {
    "0": ["111", "101", "101", "101", "111"], "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"], "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"], "7": ["111", "001", "010", "010", "010"],
    "8": ["111", "101", "111", "101", "111"], "9": ["111", "101", "111", "001", "111"],
}


def png(path, img):
    h, w = img.shape[:2]
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += img[y].tobytes()

    def chunk(tag, d):
        return (struct.pack(">I", len(d)) + tag + d
                + struct.pack(">I", zlib.crc32(tag + d) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
                + chunk(b"IEND", b""))


def contact_sheet(sprites, pal, cols=12, scale=3):
    maxw = max(s[3] for s in sprites)
    maxh = max(s[4] for s in sprites)
    pad, label = 3, 7
    cw, chh = maxw + pad * 2, maxh + pad * 2 + label
    rows = (len(sprites) + cols - 1) // cols
    W, H = cols * cw, rows * chh

    # Checkerboard, so transparent is distinguishable from a black pixel --
    # colour index 0 is not black in this palette.
    yy, xx = np.mgrid[0:H, 0:W]
    canvas = np.where((((yy >> 2) + (xx >> 2)) & 1)[..., None], 40, 60).astype(np.uint8)
    canvas = np.repeat(canvas, 3, axis=2) if canvas.shape[2] == 1 else canvas

    for (i, hx, hy, w, h, px) in sprites:
        cy, cx = divmod(i, cols)
        ox, oy = cx * cw + pad, cy * chh + pad
        sub = canvas[oy:oy + h, ox:ox + w]
        opaque = px != 0
        sub[opaque] = pal[(px & 0x0F)[opaque]]
        tx, ty = ox, oy + maxh + 1
        for chr_ in str(i):
            for r in range(5):
                for c in range(3):
                    if DIGITS[chr_][r][c] == "1" and ty + r < H and tx + c < W:
                        canvas[ty + r, tx + c] = (200, 200, 200)
            tx += 4

    return np.repeat(np.repeat(canvas, scale, 0), scale, 1)


def main():
    trace = sys.argv[1] if len(sys.argv) > 1 else "bench_trace.bcfg"
    out = sys.argv[2] if len(sys.argv) > 2 else "pirates/sprite_atlas.png"

    t, mem, sprites = load(trace)
    print("%d sprites, largest %dx%d"
          % (len(sprites), max(s[3] for s in sprites), max(s[4] for s in sprites)))

    pal = ega_palette(t)
    png(out, contact_sheet(sprites, pal))
    print("wrote %s" % out)

    def w16(a):
        return int(mem[a]) | (int(mem[a + 1]) << 8)

    meta = [{"slot": i, "hotspot": [hx, hy], "w": w, "h": h,
             "addr": "%05X" % (SEG + w16(TAB + i * 2))}
            for (i, hx, hy, w, h, _) in sprites]
    js = os.path.splitext(out)[0] + ".json"
    with open(js, "w") as f:
        json.dump(meta, f, indent=1)
    print("wrote %s" % js)


if __name__ == "__main__":
    main()
