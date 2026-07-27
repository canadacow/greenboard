import sys
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image

d = open('assets/pirates_1.img', 'rb').read()
fb = bytearray(0x4000)
for i, off in enumerate((0x55800, 0x56a00, 0x57c00, 0x58e00)):
    fb[i*0x1000:(i+1)*0x1000] = d[off:off+0x1000]

CGA1 = [(0,0,0), (85,255,255), (255,85,255), (255,255,255)]
CGA0 = [(0,0,0), (85,255,85), (255,85,85), (255,255,85)]

def decode(pal, path):
    img = Image.new('RGB', (320, 200))
    px = img.load()
    for y in range(200):
        base = (0x2000 if (y & 1) else 0) + (y >> 1) * 80
        for xb in range(80):
            b = fb[base + xb]
            for p in range(4):
                px[xb*4+p, y] = pal[(b >> (6-2*p)) & 3]
    img.resize((640, 400), Image.NEAREST).save(path)

decode(CGA1, 'scratch/out/t2_pal1.png')
decode(CGA0, 'scratch/out/t2_pal0.png')
print('ok')
