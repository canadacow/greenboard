"""What does the sector-9 scan expect?

A 9-sectors-per-track image HAS sector 9 on every track, so reads succeed.
But the game reads S=9 on tracks 0..N repeatedly and then stops -- classic
signature of a protection routine measuring which tracks have a readable
sector 9, or reading a sector that on the ORIGINAL media is formatted
differently (e.g. N!=2, or missing on certain tracks).

Check what's actually stored at each track's sector 9 in the image, and
whether the code compares the returned data against something.
"""
import sys, collections
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image

d=open('assets/pirates_1.img','rb').read()
SEC=512
def off(c,h,s): return ((c*2+h)*9+(s-1))*SEC

print('=== sector 9 of each track (first 16 bytes + entropy) ===')
for c in range(0,12):
    for h in (0,1):
        o=off(c,h,9)
        blk=d[o:o+SEC]
        distinct=len(set(blk))
        print(f'  c={c:2d} h={h}: off=0x{o:06x} distinct={distinct:3d} {blk[:16].hex()}')
print()

img=build_image('assets/pirates_1.img')
md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== code around the INT13 read site 0x28be (checks after read) ===')
for ins in md.disasm(img[0x28fa:0x2990],0x28fa):
    print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
