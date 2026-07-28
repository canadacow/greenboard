"""Full decompilation with CORRECTED data addresses (rebased to 0000:0500)."""
import os, sys, time
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()
from ghidra.base.project import GhidraProject
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

monitor=ConsoleTaskMonitor()
project=GhidraProject.openProject(os.path.abspath('scratch/ghidra_proj2'),'pirates2',True)
program=project.openProgram('/','stage2.bin',True)
fm=program.getFunctionManager(); rm=program.getReferenceManager()
ifc=DecompInterface(); ifc.openProgram(program)
fns=sorted(fm.getFunctions(True), key=lambda f: f.getEntryPoint().getOffset())
print(f'decompiling {len(fns)} functions (rebased)...')
t0=time.time()
with open('scratch/out/pirates_fixed.c','w',encoding='utf-8') as cf:
    cf.write('/* Pirates! DOS -- decompiled with CORRECT data addresses.\n'
             '   Image byte 0 = physical 0x500; loaded at 0000:0500 so that\n'
             '   DS-relative displacements resolve to their true offsets.\n'
             '   (Previously imported at 0050:0000, which shifted every data\n'
             '   address down by 0x500 -- e.g. 0x136b appeared as 0x0e6b.) */\n')
    ok=0
    for i,fn in enumerate(fns):
        ep=fn.getEntryPoint()
        callers=set()
        for r in rm.getReferencesTo(ep):
            c=fm.getFunctionContaining(r.getFromAddress())
            callers.add(c.getName() if c else str(r.getFromAddress()))
        cf.write(f'\n/* {"="*66}\n * {ep}  {fn.getName()}\n')
        if callers: cf.write(f' * called by: {", ".join(sorted(callers)[:12])}\n')
        cf.write(' */\n')
        res=ifc.decompileFunction(fn,120,monitor)
        if res.decompileCompleted():
            cf.write('\n'.join(l for l in res.getDecompiledFunction().getC().splitlines() if l.strip())+'\n')
            ok+=1
        else:
            cf.write('/* FAILED */\n')
        if (i+1)%150==0: print(f'  {i+1}/{len(fns)} ({time.time()-t0:.0f}s)')
print(f'done: {ok}/{len(fns)} in {time.time()-t0:.0f}s -> scratch/out/pirates_fixed.c')
print('size', os.path.getsize('scratch/out/pirates_fixed.c'))
project.close(program); project.close()
