"""Look for disk-based copy protection evidence.

Two things to check:
 1. Which INT13 functions besides read(02) does the code use?
    - AH=04 verify, AH=05 format, AH=02 with odd sector numbers,
      AH=00 reset, and reads of sectors that don't exist in a normal 9-spt image.
 2. Do the .img files even contain the weak/extra sectors? A plain 368640-byte
    image has NO room for non-standard sectors -- so if the protection reads
    e.g. sector 10+ on a track, the image can't hold it and the check must be
    satisfied some other way (or the image is already cracked).
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
md = Cs(CS_ARCH_X86, CS_MODE_16)

print('=== every INT 13h site with preceding AH/AL setup ===')
sites = [i for i in range(len(img)-1) if img[i] == 0xCD and img[i+1] == 0x13]
for off in sites:
    lo = max(0, off - 60)
    ctx = list(md.disasm(img[lo:off+2], lo))
    ah = al = None
    for ins in ctx:
        if ins.mnemonic == 'mov' and ins.op_str.startswith('ah,'):
            ah = ins.op_str.split(',')[1].strip()
        if ins.mnemonic == 'mov' and ins.op_str.startswith('al,'):
            al = ins.op_str.split(',')[1].strip()
        if ins.mnemonic == 'mov' and ins.op_str.startswith('ax,'):
            ah = al = ins.op_str.split(',')[1].strip() + ' (ax)'
    print(f'  0x{off:05x}:  AH={ah}  AL={al}')
print()

# Look for the classic protection signature: comparing returned AH error code
# against a specific value (e.g. 0x10 CRC error, 0x04 sector not found, 0x02 addr mark)
print('=== comparisons against disk error codes near INT13 sites ===')
for off in sites:
    hi = min(len(img), off + 80)
    for ins in md.disasm(img[off:hi], off):
        if ins.mnemonic == 'cmp' and any(f'0x{v:x}' in ins.op_str for v in (0x10,0x04,0x02,0x03,0x20,0x40)):
            print(f'  after 0x{off:05x}: {ins.address:05X}: {ins.mnemonic} {ins.op_str}')
        if ins.mnemonic in ('ret','retf'): break
print()
print('image sizes:', len(open('assets/pirates_1.img','rb').read()),
      len(open('assets/pirates_2.img','rb').read()))
print('368640 = 40cyl x 2head x 9sec x 512 -> standard, no room for extra/weak sectors')
