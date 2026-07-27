import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
md = Cs(CS_ARCH_X86, CS_MODE_16)

print('=== the AH=3 (WRITE) routine at 0x2900-0x29d0 ===')
for ins in md.disasm(img[0x2920:0x29d0], 0x2920):
    print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== strings in the program image ===')
import re
cur, start = [], None
for i, b in enumerate(img):
    if 32 <= b < 127:
        if start is None: start = i
        cur.append(chr(b))
    else:
        if start is not None and len(cur) >= 6:
            s = ''.join(cur)
            if re.search(r'[A-Za-z]{4}', s):
                print(f'  0x{start:05x}: {s[:78]}')
        cur, start = [], None
