import sys, os
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image

d = open('assets/pirates_1.img','rb').read()
os.makedirs('scratch/out', exist_ok=True)

# EGA 16-colour default palette
EGA = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
       (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]

def render_4bpp(data, width_px, path, scale=1):
    """2 pixels per byte, linear."""
    bpr = width_px // 2
    rows = len(data) // bpr
    if rows < 4: return None
    img = Image.new('RGB', (width_px, rows))
    px = img.load()
    for y in range(rows):
        base = y * bpr
        for xb in range(bpr):
            b = data[base + xb]
            px[xb*2,   y] = EGA[b >> 4]
            px[xb*2+1, y] = EGA[b & 15]
    if scale != 1:
        img = img.resize((width_px*scale, rows*scale), Image.NEAREST)
    img.save(path)
    return rows

def render_1bpp(data, width_px, path):
    """8 pixels per byte, mono -- CGA planar / coastline masks."""
    bpr = width_px // 8
    rows = len(data) // bpr
    if rows < 4: return None
    img = Image.new('RGB', (width_px, rows))
    px = img.load()
    for y in range(rows):
        base = y * bpr
        for xb in range(bpr):
            b = data[base + xb]
            for bit in range(8):
                v = 255 if (b >> (7-bit)) & 1 else 0
                px[xb*8+bit, y] = (v, v, v)
    img.save(path)
    return rows

REGIONS = [
    ('a_2c000', 0x2c000, 0x30000),
    ('b_28000', 0x28000, 0x2b000),
    ('c_24000', 0x23800, 0x26000),
    ('d_55800', 0x55800, 0x59600),
    ('e_1c000', 0x1c000, 0x22000),
]

for name, lo, hi in REGIONS:
    seg = d[lo:hi]
    for w in (160, 320, 288, 256, 640):
        p = f'scratch/out/{name}_4bpp_w{w}.png'
        r = render_4bpp(seg, w, p)
        if r: print(f'{name} 4bpp w={w:3d} rows={r}')
    for w in (320, 640):
        p = f'scratch/out/{name}_1bpp_w{w}.png'
        r = render_1bpp(seg, w, p)
        if r: print(f'{name} 1bpp w={w:3d} rows={r}')
print('done')
