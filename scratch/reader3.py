import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

print('=== the third reader: 0x09B0-0x0A60 ===')
for ins in md.disasm(img[0x9b0:0xa60],0x9b0):
    mark=''
    if ins.address in (0x9e2,0x9e8): mark=' <<< INT16'
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}{mark}')
print()
def callers_of(t):
    out=[]
    for i in range(len(img)-3):
        if img[i]==0xE8:
            d=int.from_bytes(img[i+1:i+3],'little',signed=True)
            if ((i+3+d)&0xFFFF)==t: out.append(i)
    return out
for t in (0x9b0,0x9c0,0x9d0,0x9d8,0x9e0):
    c=callers_of(t)
    if c: print(f'callers of 0x{t:05x}: '+', '.join(f'0x{x:05x}' for x in c[:12]))
