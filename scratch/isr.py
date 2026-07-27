"""1038:0B94 increments [0xAEA]. Which vector is that ISR attached to?
Dump the handler and scan the live IVT for anything pointing into seg 1038."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
ram=open('scratch/out/ram.bin','rb').read()
md=Cs(CS_ARCH_X86,CS_MODE_16); SEG=0x1038; base=SEG*16

print('=== ISR at 1038:0B70-0BD0 ===')
for ins in md.disasm(ram[base+0xB70:base+0xBD0],0xB70):
    mark=' <<<' if ins.address==0xB94 else ''
    print(f'  {SEG:04X}:{ins.address:04X}: {ins.mnemonic:<7s} {ins.op_str}{mark}')
print()
print('=== live IVT entries (non-stub) ===')
for v in range(0x100):
    off,seg=struct.unpack('<HH', ram[v*4:v*4+4])
    if seg==0xF000 and 0xE000<=off<0xE100:   # our default stub
        continue
    if seg==0 and off==0: continue
    print(f'  INT {v:02X}h -> {seg:04X}:{off:04X}' + ('   <-- seg 1038!' if seg==SEG else ''))
