import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== 0x2a60-0x2b20 : disk/drive select + cylinder bias ===')
for ins in md.disasm(img[0x2a60:0x2b20],0x2a60):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== 0x2bb5-0x2c40 : the disk-error / swap-prompt path ===')
for ins in md.disasm(img[0x2bb5:0x2c40],0x2bb5):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
