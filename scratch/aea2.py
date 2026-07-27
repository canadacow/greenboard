"""Disassemble the live spin from the RAM dump (not the static image)."""
import sys, collections
sys.stdout.reconfigure(encoding='utf-8')
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
ram=open('scratch/out/ram.bin','rb').read()
md=Cs(CS_ARCH_X86,CS_MODE_16)
SEG=0x1038; base=SEG*16
print(f'=== live code at {SEG:04X}:0C30-0CB0 (phys {base+0xC30:05X}) ===')
for ins in md.disasm(ram[base+0xC30:base+0xCB0], 0xC30):
    mark=' <<< SPIN' if ins.address in (0xC4F,0xC54) else ''
    print(f'  {SEG:04X}:{ins.address:04X}: {ins.bytes.hex():<12s} {ins.mnemonic:<7s} {ins.op_str}{mark}')
print()
print(f'[{SEG:04X}:0AEA] current value = {ram[base+0xAEA]:#04x}')
print()
print('=== who writes [0xaea]? scan whole segment (64KB) in RAM ===')
seen=set(); hits=[]
for start in range(0,0x10000,0x40):
    p=base+start
    if p+0x80>len(ram): break
    for ins in md.disasm(ram[p:p+0x80], start):
        if ins.address in seen: continue
        seen.add(ins.address)
        if '0xaea' in ins.op_str:
            hits.append((ins.address,ins.mnemonic,ins.op_str))
for a,m,o in sorted(set(hits))[:30]:
    print(f'  {SEG:04X}:{a:04X}: {m:<6s} {o}')
