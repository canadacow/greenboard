"""Every INT 16h site in the loaded program image, with the AH set before it.
The live code sits +0x500 from these image offsets (image[0] = phys 0x500)."""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image

img=build_image('assets/pirates_1.img')
md=Cs(CS_ARCH_X86,CS_MODE_16)

sites=[i for i in range(len(img)-1) if img[i]==0xCD and img[i+1]==0x16]
print(f'{len(sites)} INT 16h sites in the {len(img)} byte image\n')
for off in sites:
    lo=max(0,off-24)
    ah=None
    for ins in md.disasm(img[lo:off+2],lo):
        o=ins.op_str
        if ins.mnemonic=='mov' and o.startswith('ah,'): ah=o.split(',')[1].strip()
        elif ins.mnemonic=='mov' and o.startswith('ax,'): ah=o.split(',')[1].strip()+' (ax)'
    print(f'  image 0x{off:05X}   live 0050:{off:04X}   AH={ah}')
