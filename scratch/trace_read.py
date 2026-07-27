import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = True

# Find the start of the function containing 0x28be by walking back to a preceding RET/prologue.
# Then disassemble the whole read routine.
print('=== disk read routine (around 0x2850-0x2900) ===')
for ins in md.disasm(img[0x2840:0x2910], 0x2840):
    print(f'  {ins.address:05X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}')
print()

# Locate near-call targets across the whole image to find who calls the sector reader.
def find_calls_to(target):
    """near call rel16 = E8 disp; target = addr_after + disp"""
    out = []
    for i in range(len(img) - 3):
        if img[i] == 0xE8:
            disp = int.from_bytes(img[i+1:i+3], 'little', signed=True)
            if (i + 3 + disp) & 0xFFFF == target & 0xFFFF:
                out.append(i)
    return out

# Which routine is the "load sectors" wrapper? Try the function starting near 0x2880
for t in (0x2880, 0x2884, 0x288c, 0x2890):
    c = find_calls_to(t)
    if c:
        print(f'callers of 0x{t:05x}: ' + ', '.join(f'0x{x:05x}' for x in c[:20]))
