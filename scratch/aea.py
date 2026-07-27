"""Who sets [0xAEA] in segment 0x1038? That segment is 'word[1]+0x50' from the
relocated segment table = 0x1038, i.e. image offset 0x1038*16 - 0x500."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

SEG=0x1038
base=SEG*16-0x500          # image offset of SEG:0000
print(f'segment {SEG:04X} -> image offset 0x{base:06x}')
print()
print('=== code around SEG:0C40-0CB0 (the spin) ===')
for ins in md.disasm(img[base+0xC30:base+0xCB0], 0xC30):
    print(f'  {ins.address:04X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== all refs to [0xaea] within this segment ===')
seen=set()
for start in range(0, 0x8000, 0x40):
    off=base+start
    if off+0x80>len(img): break
    for ins in md.disasm(img[off:off+0x80], start):
        if ins.address in seen: continue
        seen.add(ins.address)
        if '0xaea' in ins.op_str:
            print(f'  {SEG:04X}:{ins.address:04X}: {ins.mnemonic:<6s} {ins.op_str}')
