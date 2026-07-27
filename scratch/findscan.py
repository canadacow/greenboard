"""Find the routine issuing the sector-9 scan.
It is NOT the descriptor loader (that reads from the table). Something sets
sector=9 explicitly and walks cylinders. Look for code writing 9 to the
sector field [0x11af] or building CX with sector 9."""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

seen=set(); hits=[]
for start in range(0,len(img),0x40):
    for ins in md.disasm(img[start:start+0x80],start):
        if ins.address in seen: continue
        seen.add(ins.address)
        o=ins.op_str
        if '0x11af' in o or '0x11ad' in o or '0x11ae' in o or '0x3bc8' in o:
            hits.append((ins.address,ins.mnemonic,o))
print('=== refs to CHS scratch vars [0x11ad cyl-head] [0x11ae cyl] [0x11af sec] [0x3bc8 cyl bias] ===')
for a,m,o in sorted(set(hits)):
    print(f'  0x{a:05x}: {m:<6s} {o}')
