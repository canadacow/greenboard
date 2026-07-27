"""Decompile the entire Pirates! input subsystem to a file, with xrefs.

Chain established from the decompiler:
  FUN_0050_24bd  = input dispatcher. Clears [0x3bf8], branches on
                   [0x3b72]/[0x3b74] (the CONTROL CONFIGURATION choice) to one
                   of three device readers, and stores the result in [0x3bfa].
      [0x3b72]==1 -> FUN_0050_2536
      [0x3b72]< 2 -> FUN_0050_24a5   (this one calls sel_poll)
      else        -> FUN_0050_2582
"""
import os, sys
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()

from ghidra.base.project import GhidraProject
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

PROJ_DIR=os.path.abspath('scratch/ghidra_proj')
monitor=ConsoleTaskMonitor()
project=GhidraProject.openProject(PROJ_DIR,'pirates',True)
program=project.openProgram('/','stage2.bin',True)
af=program.getAddressFactory(); fm=program.getFunctionManager()
rm=program.getReferenceManager()
ifc=DecompInterface(); ifc.openProgram(program)

TARGETS=['0050:24bd','0050:24a5','0050:2536','0050:2582','0050:2440',
         '0050:2740','0050:08a0','0050:0896','0050:2806','0050:10fc']

out=[]
for t in TARGETS:
    a=af.getAddress(t)
    fn=fm.getFunctionContaining(a)
    if fn is None:
        out.append(f'\n### {t}: NO FUNCTION\n'); continue
    hdr=f'\n{"="*74}\n### {t}  {fn.getName()}  [{fn.getEntryPoint()} .. {fn.getBody().getMaxAddress()}]'
    callers=[]
    for r in rm.getReferencesTo(fn.getEntryPoint()):
        cf=fm.getFunctionContaining(r.getFromAddress())
        nm=cf.getName() if cf else '?'
        callers.append(f'{r.getFromAddress()}({nm})')
    hdr+='\nCALLED BY: '+(', '.join(sorted(set(callers))[:14]) if callers else '(none)')
    out.append(hdr)
    res=ifc.decompileFunction(fn,120,monitor)
    if res.decompileCompleted():
        c=res.getDecompiledFunction().getC()
        c='\n'.join(l for l in c.splitlines() if l.strip())   # drop blank lines
        out.append(c)
    else:
        out.append('  DECOMPILE FAILED: '+str(res.getErrorMessage()))

txt='\n'.join(out)
open('scratch/out/input_decomp.txt','w',encoding='utf-8').write(txt)
print(txt)
project.close(program); project.close()
