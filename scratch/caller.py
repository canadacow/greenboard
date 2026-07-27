"""The selector poll at image 0x2740 RETURNS normally (buffer empties, so
je 0x276D fires). Its live caller is 0050:252B -> image 0x202B. Read that."""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
# The stack showed "return to 0050:252B". CS=0050 and the image IS loaded at
# 0050:0000, so image offset == live offset. No -0x500 correction.
print('=== caller at image 0x2500-0x2570 (live 0050:2500) ===')
for ins in md.disasm(img[0x2500:0x2570],0x2500):
    mark=' <== returns here' if ins.address==0x252b else ''
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}{mark}')
