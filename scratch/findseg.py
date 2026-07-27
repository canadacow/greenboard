import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
md = Cs(CS_ARCH_X86, CS_MODE_16)

# The routine starts:  mov ax, cs:[0] ; mov ds, ax ; mov ax, cs:[8] ; mov es, ax
# So segment values live in a table at the START of the code segment (image offset 0 area).
print('=== first 32 words of image (segment table) ===')
for i in range(0, 32):
    v = struct.unpack_from('<H', img, i*2)[0]
    print(f'  [{i*2:02x}] = 0x{v:04x}  ({v})', end='')
    if (i+1) % 4 == 0: print()
print()

# But note the relocation loop in boot added 0x50 to 16 words at 0050:0000.
# Our raw image is PRE-relocation. Apply +0x50 to first 16 words to get runtime segs.
print('=== after boot relocation (+0x50 to first 16 words) ===')
segs = []
for i in range(16):
    v = struct.unpack_from('<H', img, i*2)[0]
    r = (v + 0x50) & 0xFFFF
    segs.append(r)
    print(f'  word[{i}] raw=0x{v:04x} -> seg 0x{r:04x} (phys 0x{r*16:06X}, image off 0x{r*16-0x500:06X})')
