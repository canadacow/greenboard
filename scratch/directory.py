"""Decode the Pirates! resource directory from disk, per the loader code.

Bootstrap (0050:2834, after load_resource(0)):
    si = 0x48C8 ; cx = 0x8A          ; 138 entries
    do { [si] += 0x48C8 ; si += 2 } while(--cx)
so resource 0 loads a block of 138 RELATIVE word pointers to DS:48C8 and
fixes them up to absolute DS offsets.

Descriptor fields (0050:2866-0x2882):
    [bx+0] = sector count
    [bx+4] = cylinder (+ [0x3bc8] disk bias)
    [bx+6] = sector       (>=10 means head 1, minus 9)
    [bx+8] = dest segment
    [bx+A] = dest offset

The directory block was located on disk by scoring: its pointers, taken as
file offsets, yield descriptors with valid CHS. Decode it and list every
resource: where it lives on disk and where it loads in memory.
"""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
d = open('assets/pirates_1.img', 'rb').read()
SEC, SPT, HEADS = 512, 9, 2

def chs(c, h, s):
    return ((c * HEADS + h) * SPT + (s - 1)) * SEC

BASE = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x187A4
N = 138

ptrs = struct.unpack_from(f'<{N}H', d, BASE)
print(f'directory at file {BASE:#08x}, {N} entries')
print(f'first pointers: ' + ' '.join(f'{p:04x}' for p in ptrs[:12]))
print()
print(f'{"idx":>4} {"ptr":>6} {"cnt":>4} {"cyl":>4} {"sec":>4} {"h":>2}  '
      f'{"dest":>10}  {"file":>9}  bytes')
print('-' * 72)

rows = []
for i, p in enumerate(ptrs):
    q = BASE + p
    if q + 12 > len(d):
        continue
    cnt = struct.unpack_from('<H', d, q)[0] & 0xFF
    cyl = struct.unpack_from('<H', d, q + 4)[0]
    sec = struct.unpack_from('<H', d, q + 6)[0]
    seg = struct.unpack_from('<H', d, q + 8)[0]
    off = struct.unpack_from('<H', d, q + 0xA)[0]
    if not (0 < cnt <= 200 and cyl <= 40 and 1 <= sec <= 18):
        continue
    head, s = (1, sec - 9) if sec >= 10 else (0, sec)
    fo = chs(cyl, head, s)
    rows.append((i, p, cnt, cyl, sec, head, seg, off, fo))
    print(f'{i:>4} {p:#06x} {cnt:>4} {cyl:>4} {sec:>4} {head:>2}  '
          f'{seg:04X}:{off:04X}  {fo:#08x}  {cnt*SEC:>6}')

print(f'\n{len(rows)} valid resources')
segs = {}
for r in rows:
    segs.setdefault(r[6], []).append(r[0])
print('\ndestination segments:')
for s in sorted(segs):
    print(f'  {s:04X}: resources {segs[s][:14]}')
