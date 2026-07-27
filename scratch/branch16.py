import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)
print('=== 0x2740-0x2810 : the whole selector poll incl. all branches ===')
for ins in md.disasm(img[0x2740:0x2810],0x2740):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
print('=== callers of the poll 0x2740 ===')
out=[]
for i in range(len(img)-3):
    if img[i]==0xE8:
        d=int.from_bytes(img[i+1:i+3],'little',signed=True)
        if ((i+3+d)&0xFFFF)==0x2740: out.append(i)
print('  '+', '.join(f'0x{x:05x}' for x in out[:20]))
print()
print('=== what 0x252B (the live caller) does with the result ===')
for ins in md.disasm(img[0x2000:0x2060],0x2000):
    print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}')
