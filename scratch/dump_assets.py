"""Scan both Pirates! disk images for Huffman-compressed graphics and dump
every decodable one to pirates/ as a 16-colour PNG.

Header layout, from FUN_1000_0ed7:
    read_into_cursor(0x2b)      -> 0x2b byte header
    hdr[0x20] = count of sub-objects   ([0xc73f])
    hdr[0x21] = bias                   ([0xc73c])
    hdr[0x22] = width                  ([0xc743])
    hdr[0x23] = height                 ([0xc745])
    then per sub-object records, then the Huffman tree, then the bitstream.

The tree read is:
    read_bytes(3)  -> hi/lo count word + escape byte ([0xc735])
    read_bytes(count*2) -> low bytes
    read_bytes(count)   -> hi/flag bytes
"""
import sys, os, struct
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, 'scratch')
from pirates_decode import Stream, Huff, build_tree, decode_rect
from PIL import Image

OUT = 'pirates'
os.makedirs(OUT, exist_ok=True)

# EGA/16-colour default palette (the data is 4bpp, so 16 colours is native).
EGA = [(0x00,0x00,0x00),(0x00,0x00,0xAA),(0x00,0xAA,0x00),(0x00,0xAA,0xAA),
       (0xAA,0x00,0x00),(0xAA,0x00,0xAA),(0xAA,0x55,0x00),(0xAA,0xAA,0xAA),
       (0x55,0x55,0x55),(0x55,0x55,0xFF),(0x55,0xFF,0x55),(0x55,0xFF,0xFF),
       (0xFF,0x55,0x55),(0xFF,0x55,0xFF),(0xFF,0xFF,0x55),(0xFF,0xFF,0xFF)]


def try_decode(data, off):
    """Attempt to parse a compressed image container at `off`."""
    s = Stream(data, off)
    hdr = s.bytes(0x2b)
    nsub, bias, w, h = hdr[0x20], hdr[0x21], hdr[0x22], hdr[0x23]
    if not (8 <= w <= 320 and 8 <= h <= 200):
        return None
    if nsub > 64:
        return None
    # per sub-object records: 7 bytes each, plus nested entries
    for _ in range(nsub):
        rec = s.bytes(7)
        for _ in range(rec[5] if rec[5] < 64 else 0):
            s.bytes(7)
    t = s.bytes(3)
    count = (t[0] << 8) | t[1]
    escape = t[2]
    if not (1 <= count <= 256):
        return None
    lo = s.bytes(count * 2)
    hi = s.bytes(count)
    nodes = build_tree(lo, hi)
    huff = Huff(s, nodes)
    try:
        px = decode_rect(huff, w, h, escape, bias)
    except Exception:
        return None
    # Reject flat/degenerate results
    if len(set(px)) < 3:
        return None
    return w, h, px, nsub, count


def save(px, w, h, path):
    img = Image.new('P', (w, h))
    img.putdata(px)
    pal = []
    for c in EGA:
        pal += list(c)
    img.putpalette(pal + [0] * (768 - len(pal)))
    img.resize((w * 3, h * 3), Image.NEAREST).convert('RGB').save(path)


def main():
    total = 0
    for disk in (1, 2):
        data = open(f'assets/pirates_{disk}.img', 'rb').read()
        print(f'=== scanning pirates_{disk}.img ===')
        off = 0
        step = 512
        while off < len(data) - 0x200:
            r = try_decode(data, off)
            if r:
                w, h, px, nsub, count = r
                name = f'{OUT}/d{disk}_{off:06x}_{w}x{h}.png'
                save(px, w, h, name)
                print(f'  0x{off:06x}  {w:3d}x{h:<3d} sub={nsub:2d} '
                      f'huff={count:3d}  -> {name}')
                total += 1
            off += step
    print(f'\n{total} images written to {OUT}/')


if __name__ == '__main__':
    main()
