"""Full INT 09h ISR. Entry per the live IVT is 0050:26E0."""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== INT 09h ISR at 0x26E0 ===')
for ins in md.disasm(img[0x26e0:0x2740],0x26e0):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== scancode->bit table at 0xa60 (first 64 words) ===')
import struct
for r in range(4):
    row=[f'{struct.unpack("<H",img[0xa60+ (r*16+i)*2:0xa60+(r*16+i)*2+2])[0]:04x}' for i in range(16)]
    print(f'  [{r*16:02x}] '+' '.join(row))
