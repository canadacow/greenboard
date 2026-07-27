"""Find Pirates' text-printing routine.

Earlier we found the menu display-list at image 0x135d0:
  \xff <col> <row> <attr> "GRAPHICS CONFIGURATION" \xff ...
So some routine walks that list. Also 0x32cc / 0x32da were called with
si = string pointers in the disk-swap prompt code -- those are prime
candidates for 'print string'.
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

for name,addr,ln in (('0x32cc (print?)',0x32cc,0x50),
                     ('0x32da (print?)',0x32da,0x60),
                     ('0x3334',0x3334,0x40)):
    print(f'=== {name} ===')
    for ins in md.disasm(img[addr:addr+ln],addr):
        print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
    print()

def callers_of(target):
    out=[]
    for i in range(len(img)-3):
        if img[i]==0xE8:
            d=int.from_bytes(img[i+1:i+3],'little',signed=True)
            if ((i+3+d)&0xFFFF)==target: out.append(i)
    return out
for t in (0x32cc,0x32da):
    c=callers_of(t)
    print(f'callers of 0x{t:05x}: {len(c)} -> ' + ', '.join(f'0x{x:05x}' for x in c[:16]))
