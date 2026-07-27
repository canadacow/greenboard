import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image

img=build_image('assets/pirates_1.img')
md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== key input routine at 0050:0890-0940 (image 0x890) ===')
for ins in md.disasm(img[0x890:0x940],0x890):
    print(f'  {ins.address:04X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== the caller / menu logic 0x2700-0x2790 ===')
for ins in md.disasm(img[0x2700:0x2790],0x2700):
    print(f'  {ins.address:04X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
