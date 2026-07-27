"""Where exactly is the menu loop, and what condition is it testing?
Disassemble around the key-routine callers 0x7e1/0x7ec/0x860."""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

print('=== 0x0780-0x0896 : the wait-for-input helpers ===')
for ins in md.disasm(img[0x780:0x896],0x780):
    print(f'  {ins.address:04X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
