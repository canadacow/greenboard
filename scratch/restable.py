import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')

# Loader at 0x2855: si = [0x3b84]; si <<= 1; bx = [si + 0x136b]  -> table of WORD pointers at 0x136b
# Descriptor at bx:  +0 count(sectors, low byte used), +4 cyl(+[0x3bc8]), +6 sector, +8 seg, +0xA off
TBL = 0x136b
print('=== resource pointer table at 0x136B ===')
ptrs = []
for i in range(64):
    p = TBL + i*2
    if p + 2 > len(img): break
    v = struct.unpack_from('<H', img, p)[0]
    ptrs.append(v)

# Descriptors live in the same segment; plausible ones point inside the image
def rd(off, n=2):
    return struct.unpack_from('<H', img, off)[0] if off+2 <= len(img) else None

print(f'{"idx":>3} {"ptr":>6} | {"cnt":>4} {"cyl":>4} {"sec":>4} {"seg":>6} {"off":>6}  filepos')
SEC=512
def off9(cyl, head, sec):
    return ((cyl*2 + head)*9 + (sec-1))*SEC

for i, v in enumerate(ptrs):
    if v == 0 or v + 12 > len(img):
        continue
    cnt = rd(v)
    cyl = rd(v+4)
    sec = rd(v+6)
    seg = rd(v+8)
    offs = rd(v+10)
    # sanity: counts and CHS in range for a 40cyl/2head/9spt disk
    if cnt is None or not (0 < (cnt & 0xff) <= 200): continue
    if cyl is None or cyl > 45: continue
    if sec is None or not (1 <= sec <= 18): continue
    head = 0
    s = sec
    if s >= 10:
        head, s = 1, s - 9
    fp = off9(cyl, head, s)
    print(f'{i:3d} 0x{v:04x} | {cnt&0xff:4d} {cyl:4d} {sec:4d} 0x{seg:04x} 0x{offs:04x}  0x{fp:06x}  ({(cnt&0xff)*512} bytes)')
