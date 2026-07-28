"""Verify the rebase fixed data addresses: 0050:285B should now decompile with
0x136b (not DAT_..._0e6b). Analyze and decompile the loader."""
import os, sys, time
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()
from ghidra.base.project import GhidraProject
from ghidra.program.flatapi import FlatProgramAPI
from ghidra.app.decompiler import DecompInterface
from ghidra.app.plugin.core.analysis import AutoAnalysisManager
from ghidra.util.task import ConsoleTaskMonitor

monitor=ConsoleTaskMonitor()
project=GhidraProject.openProject(os.path.abspath('scratch/ghidra_proj2'),'pirates2',True)
program=project.openProgram('/','stage2.bin',False)
af=program.getAddressFactory(); fm=program.getFunctionManager()
api=FlatProgramAPI(program,monitor)

tx=program.startTransaction('analyze')
t0=time.time()
try:
    # stage2 byte 0 is at 0x500, so an old image offset X is now 0x500+X.
    for off,name in ((0x0520,'entry_stage2'),(0x0580,'main'),
                     (0x2d20,'load_resource'),(0x2d44,'res_fetch'),
                     (0x2dbe,'disk_read'),(0x42dd,'script_interp'),
                     (0x44bd,'protection_check'),(0x45bd,'name_entry')):
        a=af.getAddress(f'0000:{off:04x}')
        try:
            api.disassemble(a); api.createFunction(a,name)
        except Exception as e: print(f'  seed {off:#06x}: {e}')
    mgr=AutoAnalysisManager.getAnalysisManager(program)
    mgr.initializeOptions(); mgr.reAnalyzeAll(None); mgr.startAnalysis(monitor)
finally:
    program.endTransaction(tx,True)
print(f'analysis {time.time()-t0:.0f}s, functions={fm.getFunctionCount()}')

ifc=DecompInterface(); ifc.openProgram(program)
# The descriptor fetch: old image 0x2844 -> now 0x500+0x2844 = 0x2d44
a=af.getAddress('0000:2d44')
fn=fm.getFunctionContaining(a)
if fn:
    res=ifc.decompileFunction(fn,120,monitor)
    if res.decompileCompleted():
        c=res.getDecompiledFunction().getC()
        print('\n=== descriptor fetch, rebased ===')
        for line in c.splitlines():
            if line.strip(): print(' ',line)
        print('\nCONTAINS 0x136b:', '0x136b' in c, '  contains 0x0e6b:', '0xe6b' in c)
else:
    print('no function at 0000:2d44')
project.save(program); project.close(program); project.close()
