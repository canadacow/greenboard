import sys
sys.stdout.reconfigure(encoding='utf-8')

SPT, HEADS = 8, 2          # boot uses AL=8 sectors/track, toggles head 0/1
SEC = 512

def chs_off(cyl, head, sec):
    """CHS -> linear file offset for an 8 sect/track, 2 head image."""
    return ((cyl * HEADS + head) * SPT + (sec - 1)) * SEC

print('=== Pirates! disk 1 load map (from boot sector trace) ===\n')

# Stage 1: title/loader graphics. ch(cyl)=0x26=38, dh toggles head, 4 chunks of 0x1000
# bx starts 0, +0x1000 each -> ES:BX with ES=0xB800 : direct CGA framebuffer write!
print('Stage 1  -> CGA framebuffer B800:0000 (title screen, 4 x 4KB)')
cyl, head = 0x26, 0
bx = 0
for i in range(4):
    off = chs_off(cyl, head, 1)
    print(f'   chunk {i}: CHS c={cyl} h={head} s=1  file 0x{off:06x}  -> B800:{bx:04X}')
    bx += 0x1000
    head += 1
    if head >= HEADS:
        head = 0
        cyl += 1
print()

# Stage 2: main program. cyl counter at [1] from 1..0x15 (21), 8 sectors per read,
# two reads per cylinder (head 0 at bx=0, head 1 at bx=0x1000), staged via 2D80
# then block-copied to [2] which starts at 0x50 and advances 0x200 per cylinder.
print('Stage 2  -> program image, staged at 2D80, copied to growing dest seg')
dest = 0x50
total = 0
rows = []
for cylv in range(1, 0x16):
    o0 = chs_off(cylv, 0, 1)
    o1 = chs_off(cylv, 1, 1)
    rows.append((cylv, o0, o1, dest))
    dest += 0x200
    total += 2 * SPT * SEC
for cylv, o0, o1, dst in rows:
    print(f'   cyl {cylv:2d}: file 0x{o0:06x} (h0) + 0x{o1:06x} (h1) -> {dst:04X}:0000  (phys 0x{dst*16:05X})')
print(f'\n   total loaded: {total} bytes ({total/1024:.0f} KB)')
print(f'   program spans file 0x{chs_off(1,0,1):06x} .. 0x{chs_off(0x15,1,1)+SPT*SEC:06x}')
print(f'   memory 0x{0x50*16:05X} .. 0x{(0x50+0x200*0x15)*16:05X}')
print()
print('Entry: relocation loop adds 0x50 to 16 words at 0050:0000, then LJMP 0050:0020')
