"""How does the name prompt read characters?

0050:2740 is the poll wrapper: INT16 AH=1 peek, AH=0 get, then tests AL for
0xE0 / 0x16 / 'V' / SPACE and LOOPS on anything else -- it never returns a
typed character. So text entry must use a different reader.

Dump what follows 0x2783 (the 0x16 branch) and 0x2806 (called from the
selector), and find the code that stores typed characters into a buffer.
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

for name,a,n in (('0x2783 (the 0x16 branch)',0x2783,0x40),
                 ('0x2806 (selector helper)',0x2806,0x60),
                 ('0x27C0-0x2820',0x27c0,0x60)):
    print(f'=== {name} ===')
    for ins in md.disasm(img[a:a+n],a):
        print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
    print()

# find the "Your name?" prompt string and who references it
i=img.find(b'family name')
print(f'"family name" at 0x{i:05x}' if i>=0 else 'not found')
i2=img.find(b'Your name')
print(f'"Your name" at 0x{i2:05x}' if i2>=0 else 'not found')
