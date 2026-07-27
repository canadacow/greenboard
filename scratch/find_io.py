import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
md = Cs(CS_ARCH_X86, CS_MODE_16)

# 1. Where are INT 13h (disk) and INT 10h (video) invoked?
print('=== INT instructions in stage2 ===')
hits = {}
i = 0
while True:
    i = img.find(b'\xcd', i)
    if i < 0 or i + 1 >= len(img):
        break
    vec = img[i+1]
    if vec in (0x10, 0x13, 0x16, 0x1a, 0x21):
        hits.setdefault(vec, []).append(i)
    i += 1
for vec in sorted(hits):
    lst = hits[vec]
    print(f'  INT {vec:02X}h: {len(lst)} sites -> ' + ', '.join(f'0x{o:05x}' for o in lst[:20]))
print()

# 2. Disassemble around each INT 13h to see the CHS setup
print('=== context around INT 13h sites ===')
for off in hits.get(0x13, [])[:6]:
    lo = max(0, off - 48)
    print(f'--- near image offset 0x{off:05x} (seg0050:{off:04X}) ---')
    for ins in md.disasm(img[lo:off+4], lo):
        mark = ' <<<' if ins.address == off else ''
        print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}{mark}')
    print()
