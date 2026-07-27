import sys
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

d = open('assets/pirates_1.img', 'rb').read()
boot = d[:512]
md = Cs(CS_ARCH_X86, CS_MODE_16)

print('=== tail of boot sector from 0x7D94 ===')
for i in md.disasm(boot[0x194:], 0x7D94):
    print(f'{i.address:04X}:  {i.bytes.hex():<14s} {i.mnemonic:<7s} {i.op_str}')
print()

# The relocated copy runs at 0020:0000. Boot copies 0x200 bytes from 7C16+0x11 = 7C27.
# So offset 0x27 in the sector becomes 0020:0000, and entry is ljmp 0x20:0x3c
print('=== relocated view: 0020:0000 == file offset 0x27, entry at 0020:003C ===')
reloc = boot[0x27:]
for i in md.disasm(reloc, 0x0000):
    if i.address < 0x3c:
        continue
    print(f'{i.address:04X}:  {i.bytes.hex():<14s} {i.mnemonic:<7s} {i.op_str}')
    if i.address > 0x120:
        break
