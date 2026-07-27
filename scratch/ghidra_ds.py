"""Find where the ISR's scancode->bit table really lives, using Ghidra's own
address arithmetic instead of my hand-rolled base math (which has been wrong
repeatedly).

The ISR does:  mov bx,[0x3c0e-ish] ... [bx + 0xa60]
so 0xa60 is a DS-relative offset. Ghidra knows the program is at 0050:0000,
so DS:0xa60 for DS=0x117B is linear 0x117B*16 + 0xa60.  In the program's own
address space (base 0x50:0), that is offset (0x117B-0x50)*16 + 0xa60.
Print the bytes both ways and let the data decide.
"""
import os, sys, struct
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()
from ghidra.base.project import GhidraProject

project=GhidraProject.openProject(os.path.abspath('scratch/ghidra_proj'),'pirates',True)
program=project.openProgram('/','stage2.bin',True)
af=program.getAddressFactory(); mem=program.getMemory()

def read(addr_str,n):
    a=af.getAddress(addr_str)
    b=bytearray(n)
    mem.getBytes(a,b)
    return bytes(b)

print('memory blocks:')
for b in mem.getBlocks():
    print(f'  {b.getName()}  {b.getStart()} .. {b.getEnd()}  size={b.getSize()}')
print()

# Try the table at several plausible segments.
for seg in ('117b','1038','0050','1eb6','27fe','26be'):
    try:
        d=read(f'{seg}:0a60',64)
    except Exception as e:
        print(f'{seg}:0a60  unreadable'); continue
    words=struct.unpack('<32H',d)
    nz=sum(1 for w in words if w)
    single=sum(1 for w in words if w and (w & (w-1))==0)   # exactly one bit
    print(f'{seg}:0a60  nonzero={nz:2d} single-bit={single:2d}  '
          f'{" ".join(f"{w:04x}" for w in words[:12])}')
print('\n(a real scancode->bit table should be mostly single-bit values)')
project.close(program); project.close()
