"""Fix bad function boundaries in the Pirates! project and re-decompile.

FUN_1000_0e99 was a phantom: Ghidra split mid-instruction, inside the
'call 0x505f' at 1000:0E9A. The real routine starts at 1000:0E8F and is a
buffered stream reader:

    read_bytes(dest, count):
        if [0xc73d] > 0x1ff: refill_buffer(); [0xc73d] = 0
        al = buf[0x68b1 + [0xc73d]++]
        *dest++ = al
        while (--count)

This script deletes the bogus functions, re-disassembles from the true
starts, recreates the functions, and dumps the decompiled result.
"""
import os, sys
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()

from ghidra.base.project import GhidraProject
from ghidra.program.flatapi import FlatProgramAPI
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.symbol import SourceType
from ghidra.util.task import ConsoleTaskMonitor

PROJ=os.path.abspath('scratch/ghidra_proj')
monitor=ConsoleTaskMonitor()
project=GhidraProject.openProject(PROJ,'pirates',True)
program=project.openProgram('/','stage2.bin',False)
af=program.getAddressFactory(); fm=program.getFunctionManager()
api=FlatProgramAPI(program,monitor)

# (bad_entry_to_delete, true_entry, name)
FIXES=[('1000:0e99','1000:0e8f','read_bytes'),
       (None,       '1000:0e71','read_into_cursor'),
       (None,       '1000:0ebe','nibble_expand'),
       (None,       '1000:505f','refill_buffer'),
       (None,       '1000:1ccf','huff_decode_sym'),
       (None,       '1000:1bf9','decode_image_rect')]

tx=program.startTransaction('fix boundaries')
try:
    # 1. remove the phantom functions
    for bad,_,_ in FIXES:
        if not bad: continue
        a=af.getAddress(bad)
        f=fm.getFunctionAt(a)
        if f:
            print(f'deleting phantom function at {bad}')
            fm.removeFunction(a)
        api.clearListing(a, a.add(4))

    # 2. re-disassemble and recreate at the true entries
    for _,good,name in FIXES:
        a=af.getAddress(good)
        f=fm.getFunctionAt(a)
        if f is None:
            api.disassemble(a)
            f=api.createFunction(a,name)
            print(f'created {name} at {good}')
        else:
            f.setName(name, SourceType.USER_DEFINED)
            print(f'renamed existing -> {name} at {good}')
finally:
    program.endTransaction(tx,True)

# 3. decompile the fixed set
ifc=DecompInterface(); ifc.openProgram(program)
out=[]
for _,good,name in FIXES:
    a=af.getAddress(good)
    fn=fm.getFunctionContaining(a)
    if fn is None:
        out.append(f'\n### {good} {name}: STILL NO FUNCTION'); continue
    out.append(f'\n{"="*72}\n### {good}  {fn.getName()}  '
               f'[{fn.getEntryPoint()} .. {fn.getBody().getMaxAddress()}]')
    res=ifc.decompileFunction(fn,120,monitor)
    if res.decompileCompleted():
        c=res.getDecompiledFunction().getC()
        out.append('\n'.join(l for l in c.splitlines() if l.strip()))
    else:
        out.append('  FAILED: '+str(res.getErrorMessage()))
txt='\n'.join(out)
open('scratch/out/decoder_fixed.txt','w',encoding='utf-8').write(txt)
print(txt)

# The program was opened read-write from '/stage2.bin', so it already has a
# location -- use save(). saveAs() to the SAME path fails with
# FileInUseException because our own open handle still holds that file.
project.save(program)
project.close(program); project.close()
