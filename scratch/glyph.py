"""Find the glyph blitter -- the routine that draws characters into the CGA
framebuffer. In mode 4 there is no BIOS text, so the game has its own.

Earlier at 0x12E8 we saw:
    mov es,bx / mov bx,[0x3b7a] / shl bx,1 / add di,[bx+0x3c6f]
    mov bx,[0x3b78] / shl bx,1 / shl bx,1 / add di,bx / mov cl,5
That is: di = rowtable[y]*? + x*4  -- a classic char-cell address calc.
Dump the whole routine and find its callers.
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img'); md=Cs(CS_ARCH_X86,CS_MODE_16)

print('=== 0x12C8-0x1400 : suspected glyph blitter ===')
for ins in md.disasm(img[0x12e8:0x13a0],0x12e8):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()
def callers_of(target):
    out=[]
    for i in range(len(img)-3):
        if img[i]==0xE8:
            d=int.from_bytes(img[i+1:i+3],'little',signed=True)
            if ((i+3+d)&0xFFFF)==target: out.append(i)
    return out
for t in (0x12e8,0x12f0,0x1300):
    c=callers_of(t)
    if c: print(f'callers of 0x{t:05x}: '+', '.join(f'0x{x:05x}' for x in c[:20]))
print()
print('cursor vars: [0x3b78]=col? [0x3b7a]=row?  rowtable at 0x3c6f')
