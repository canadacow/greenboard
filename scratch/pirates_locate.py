import sys, collections
sys.stdout.reconfigure(encoding='utf-8')

d = open('assets/pirates_1.img','rb').read()

# Score every 512-byte sector on the WHOLE disk for "packed 4bpp graphics" character:
#  - high fraction of bytes whose two nibbles are equal (dd,77,22,ff,00...) => solid-colour pixel pairs
#  - but NOT mostly zero (that's just empty space)
def score(blk):
    if not blk: return None
    zero = blk.count(0) / len(blk)
    same = sum(1 for b in blk if (b >> 4) == (b & 15)) / len(blk)
    return zero, same

rows = []
for off in range(0, len(d), 512):
    blk = d[off:off+512]
    z, s = score(blk)
    rows.append((off, z, s))

# Report contiguous stretches that are graphics-like: same-nibble heavy, not zero-dominated
print('sectors with same-nibble >=35% and zeros <60%:')
runs = []
cur = None
for off, z, s in rows:
    good = s >= 0.35 and z < 0.60
    if good and cur is None: cur = off
    elif not good and cur is not None:
        runs.append((cur, off)); cur = None
if cur is not None: runs.append((cur, len(d)))
for a, b in runs:
    if b - a >= 1024:
        blk = d[a:b]
        z, s = score(blk)
        print(f'  0x{a:06x}-0x{b:06x}  {b-a:6d} bytes  zeros={z:.2f} samenib={s:.2f}')
print()

# Also: where are the densest non-zero areas overall?
print('per-8KB nonzero density across disk:')
for off in range(0, len(d), 8192):
    blk = d[off:off+8192]
    nz = 1 - blk.count(0)/len(blk)
    same = sum(1 for b in blk if (b>>4)==(b&15))/len(blk)
    bar = '#' * int(nz*40)
    print(f'  0x{off:06x} nz={nz:.2f} same={same:.2f} {bar}')
