import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== 0x0a00-0x0a60 : caller 1 context ===')
for ins in md.disasm(img[0xa00:0xa60],0xa00):
    print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== 0x0cc0-0x0d10 : caller 2 context (print char?) ===')
for ins in md.disasm(img[0xcc0:0xd10],0xcc0):
    print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== 0x12c0-0x12ea : entry to blitter ===')
for ins in md.disasm(img[0x12c0:0x12ea],0x12c0):
    print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
