import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
BASE = 0x500  # phys address of image[0]

SEC = 512
def off9(cyl, head, sec):
    return ((cyl*2 + head)*9 + (sec-1))*SEC

def img_off(seg, off):
    return seg*16 + off - BASE

# DS for the load routine = word[0] + 0x50 = 0x117b
DS = 0x117b
print(f'DS = 0x{DS:04x} -> image offset of DS:0000 = 0x{img_off(DS,0):06x}')
print()

# table of pointers at DS:0x136b
tbl = img_off(DS, 0x136b)
print(f'pointer table at image 0x{tbl:06x}')
print(f'{"idx":>3} {"ptr":>6} | {"cnt":>4} {"cyl":>4} {"sec":>4} {"dseg":>6} {"doff":>6}   filepos    size')
found = 0
for i in range(96):
    p = tbl + i*2
    if p+2 > len(img): break
    v = struct.unpack_from('<H', img, p)[0]
    if v == 0: continue
    d = img_off(DS, v)
    if d < 0 or d + 12 > len(img): continue
    cnt, _, cyl, sec, dseg, doff = struct.unpack_from('<HHHHHH', img, d)
    c8, s = cnt & 0xff, sec
    if not (0 < c8 <= 255): continue
    if cyl > 45 or not (1 <= s <= 18): continue
    head = 0
    ss = s
    if ss >= 10: head, ss = 1, ss-9
    fp = off9(cyl, head, ss)
    print(f'{i:3d} 0x{v:04x} | {c8:4d} {cyl:4d} {s:4d} 0x{dseg:04x} 0x{doff:04x}  0x{fp:06x}  {c8*512:6d}')
    found += 1
print(f'\n{found} plausible descriptors')
