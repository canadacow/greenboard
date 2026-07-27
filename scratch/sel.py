"""What key does the sword-selector menu at 0050:274A accept?"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== 0x2700-0x27C0 : selector input handler ===')
for ins in md.disasm(img[0x2700:0x27c0],0x2700):
    mark=' <<< INT16' if ins.address in (0x2742,0x2748) else ''
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}{mark}')
