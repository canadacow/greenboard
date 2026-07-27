"""Decompile named functions from the Pirates! project, and show xrefs."""
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
af=program.getAddressFactory()
fm=program.getFunctionManager()

ifc=DecompInterface(); ifc.openProgram(program)

targets = sys.argv[1:] or ['0050:2740','0050:252b','0050:08a0']
for t in targets:
    a=af.getAddress(t)
    fn=fm.getFunctionContaining(a)
    if fn is None:
        print(f'\n### {t}: no function'); continue
    print(f'\n{"="*70}\n### {t}  {fn.getName()}  [{fn.getEntryPoint()} - {fn.getBody().getMaxAddress()}]')
    refs=program.getReferenceManager().getReferencesTo(fn.getEntryPoint())
    callers=[]
    for r in refs:
        cf=fm.getFunctionContaining(r.getFromAddress())
        callers.append(f'{r.getFromAddress()}' + (f' ({cf.getName()})' if cf else ''))
    if callers: print('CALLED BY: '+', '.join(callers[:12]))
    res=ifc.decompileFunction(fn,90,monitor)
    if res.decompileCompleted():
        print(res.getDecompiledFunction().getC())
    else:
        print('  decompile failed:',res.getErrorMessage())

project.close(program); project.close()
