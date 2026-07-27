import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== 0x1280-0x1300 : suspected timer tick handler ===')
for ins in md.disasm(img[0x1280:0x1300],0x1280):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== 0x1200-0x1240 : IVT install? ===')
for ins in md.disasm(img[0x1200:0x1250],0x1200):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
