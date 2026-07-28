"""Prove the Python port works: run it and render the CGA framebuffer.

run_title() loads 4 x 8 sectors from cylinder 0x26 straight into B800:0000,
which is CGA mode 4 memory. Decode it exactly as the hardware would:
  - 320x200, 2 bits per pixel, 4 pixels per byte
  - interleaved banks: even scanlines at 0x0000, odd at 0x2000
  - 80 bytes per scanline
"""
import sys
sys.path.insert(0, 'scratch')
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
from boot_port import CPU, run_boot, run_reloc, run_title, run_stage2
from PIL import Image

c = CPU()
run_boot(c)
run_reloc(c)
run_title(c)

fb = bytes(c.mem[c.lin(0xB800, 0): c.lin(0xB800, 0) + 0x4000])
print(f'framebuffer: {sum(1 for b in fb if b)}/{len(fb)} bytes nonzero')

# CGA palette 1, high intensity (the mode the boot sector selects)
PAL = [(0, 0, 0), (85, 255, 255), (255, 85, 255), (255, 255, 255)]

img = Image.new('RGB', (320, 200))
px = img.load()
for y in range(200):
    base = (0x2000 if (y & 1) else 0) + (y >> 1) * 80
    for xb in range(80):
        b = fb[base + xb]
        for p in range(4):
            px[xb * 4 + p, y] = PAL[(b >> (6 - 2 * p)) & 3]

img.resize((640, 400), Image.NEAREST).save('scratch/out/port_title.png')
print('wrote scratch/out/port_title.png')

# Also dump which sectors the port actually read, to show where it came from.
print('\ndisk reads performed by the port:')
for r in c.reads:
    if r[0] == 'PROT':
        print(f'  protection sector C{r[1]} H{r[2]} S{r[3]}')
    else:
        drive, cyl, head, sec, n, es, bx, src = r
        print(f'  C{cyl:2d} H{head} S{sec:2d} x{n} -> {es:04X}:{bx:04X}  file {src:#08x}')
