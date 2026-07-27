"""Trace what the menu code does with the key it reads.
Watch the caller of the key routine (0050:0896) and the menu state variable."""
import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image

img=build_image('assets/pirates_1.img')
md=Cs(CS_ARCH_X86,CS_MODE_16)

def callers_of(target):
    out=[]
    for i in range(len(img)-3):
        if img[i]==0xE8:
            disp=int.from_bytes(img[i+1:i+3],'little',signed=True)
            if ((i+3+disp)&0xFFFF)==target: out.append(i)
    return out

print('callers of key-input 0x0896:', [f'0x{c:05x}' for c in callers_of(0x896)][:20])
print('callers of 0x08ef (clear screen):', [f'0x{c:05x}' for c in callers_of(0x8ef)][:20])
print()
# The config menu strings: find "GRAPHICS CONFIGURATION"
for pat in (b'GRAPHICS', b'CONTROL', b'JOYSTICK', b'KEYBOARD', b'TANDY'):
    i=img.find(pat)
    print(f'{pat!r} at image 0x{i:05x}' if i>=0 else f'{pat!r} not found')
print()
i=img.find(b'GRAPHICS')
print('=== bytes around the menu string table ===')
print(img[i-64:i+260])
