"""[0x68b1] holds the disk label byte the game compares to '0'/'1'.
Resource index 0x1e is loaded right before. Where does it land, and does our
emulated read put the label byte where the game expects?"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== 0x2820-0x2850 : load-resource-by-index entry ===')
for ins in md.disasm(img[0x2820:0x2850],0x2820):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
# strings near 0x3944-0x3990 used by the swap prompt
print('=== prompt strings 0x3940-0x39a0 ===')
print(repr(img[0x3940:0x39a0]))
print()
print('=== what is at 0x68b0 region in the image (label buffer) ===')
print(repr(img[0x68a0:0x68e0]))
