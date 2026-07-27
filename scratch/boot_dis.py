import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

d = open('assets/pirates_1.img', 'rb').read()
boot = d[:512]

md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = False

print('=== pirates_1.img boot sector, 0000:7C00 ===')
for i in md.disasm(boot, 0x7C00):
    print(f'{i.address:04X}:  {i.bytes.hex():<14s} {i.mnemonic:<7s} {i.op_str}')
    if i.address - 0x7C00 > 400:
        break
print()
print('last 16 bytes:', boot[-16:].hex())
