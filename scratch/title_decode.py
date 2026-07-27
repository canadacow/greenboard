import sys
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image

d = open('assets/pirates_1.img', 'rb').read()

# Stage 1 loads 4 x 4KB chunks to B800:0000,1000,2000,3000.
# CGA 320x200x2bpp uses only 0x0000-0x3FFF: even scanlines at 0x0000, odd at 0x2000.
fb = bytearray(0x4000)
for i, off in enumerate((0x4c000, 0x4d000, 0x4e000, 0x4f000)):
    fb[i*0x1000:(i+1)*0x1000] = d[off:off+0x1000]

CGA1 = [(0,0,0), (0,170,170), (170,0,170), (170,170,170)]        # cyan/magenta
CGA0 = [(0,0,0), (0,170,0), (170,0,0), (170,85,0)]               # green/red/brown

def decode(pal, path):
    img = Image.new('RGB', (320, 200))
    px = img.load()
    for y in range(200):
        base = (0x2000 if (y & 1) else 0) + (y >> 1) * 80
        for xb in range(80):
            b = fb[base + xb]
            for p in range(4):
                px[xb*4 + p, y] = pal[(b >> (6 - 2*p)) & 3]
    img.resize((640, 400), Image.NEAREST).save(path)

decode(CGA1, 'scratch/out/title_pal1.png')
decode(CGA0, 'scratch/out/title_pal0.png')
print('wrote scratch/out/title_pal1.png and title_pal0.png')
