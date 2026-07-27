"""What writes [0x3c08] and [0x3c02]? Those gate the menu input path.
Search the image for instructions referencing those addresses."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img')
md=Cs(CS_ARCH_X86,CS_MODE_16); md.detail=True

TARGETS={0x3c08:'input_enable?',0x3c02:'?',0x3bc3:'?',0x3c10:'?',0x3c14:'rng',0x3b98:'?'}
found={t:[] for t in TARGETS}
# brute-force disassemble from many offsets and collect refs
seen=set()
for start in range(0, len(img), 0x40):
    for ins in md.disasm(img[start:start+0x80], start):
        if ins.address in seen: continue
        seen.add(ins.address)
        for t in TARGETS:
            if f'0x{t:x}' in ins.op_str:
                found[t].append((ins.address, ins.mnemonic, ins.op_str))

for t,name in TARGETS.items():
    lst=found[t]
    writes=[x for x in lst if x[1] in ('mov','and','or','xor','inc','dec') and x[2].startswith(f'word ptr [0x{t:x}]') or x[2].startswith(f'byte ptr [0x{t:x}]')]
    print(f'\n=== [0x{t:04x}] {name}: {len(lst)} refs ===')
    for a,m,o in lst[:14]:
        tag=' WRITE' if o.startswith(f'word ptr [0x{t:x}]') or o.startswith(f'byte ptr [0x{t:x}]') else ''
        print(f'  0x{a:05x}: {m:<6s} {o}{tag}')
