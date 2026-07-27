"""Decompile EVERY function in the Pirates! stage-2 program to one file,
plus a separate index of function -> callers. Read the file, don't re-run
Ghidra for each question.

Output:
  scratch/out/pirates_all.c      - all 523 decompiled functions
  scratch/out/pirates_index.txt  - address, name, size, callers, callees
"""
import os, sys, time
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
fm=program.getFunctionManager(); rm=program.getReferenceManager()
ifc=DecompInterface(); ifc.openProgram(program)

os.makedirs('scratch/out',exist_ok=True)
fns=sorted(fm.getFunctions(True), key=lambda f: f.getEntryPoint().getOffset())
print(f'decompiling {len(fns)} functions...')

t0=time.time()
cf=open('scratch/out/pirates_all.c','w',encoding='utf-8')
idx=open('scratch/out/pirates_index.txt','w',encoding='utf-8')
cf.write('/* Pirates! (1986) IBM PC booter -- stage 2, decompiled.\n'
         '   Loaded at 0050:0000 (physical 0x500), 168KB, entry 0050:0020.\n'
         '   Language: x86:LE:16:Real Mode */\n')
idx.write(f'{"addr":<14}{"name":<28}{"size":>7}  callers\n{"-"*90}\n')

ok=fail=0
for i,fn in enumerate(fns):
    ep=fn.getEntryPoint()
    callers=set()
    for r in rm.getReferencesTo(ep):
        c=fm.getFunctionContaining(r.getFromAddress())
        callers.add(c.getName() if c else str(r.getFromAddress()))
    size=fn.getBody().getNumAddresses()
    idx.write(f'{str(ep):<14}{fn.getName():<28}{size:>7}  {",".join(sorted(callers)[:10])}\n')

    cf.write(f'\n/* {"="*70}\n * {ep}  {fn.getName()}  ({size} bytes)\n')
    if callers: cf.write(f' * called by: {", ".join(sorted(callers)[:14])}\n')
    cf.write(f' */\n')
    res=ifc.decompileFunction(fn,120,monitor)
    if res.decompileCompleted():
        c=res.getDecompiledFunction().getC()
        cf.write('\n'.join(l for l in c.splitlines() if l.strip())+'\n')
        ok+=1
    else:
        cf.write(f'/* DECOMPILE FAILED: {res.getErrorMessage()} */\n'); fail+=1
    if (i+1)%100==0: print(f'  {i+1}/{len(fns)}  ({time.time()-t0:.0f}s)')

cf.close(); idx.close()
print(f'\ndone in {time.time()-t0:.0f}s: {ok} ok, {fail} failed')
for p in ('scratch/out/pirates_all.c','scratch/out/pirates_index.txt'):
    print(f'  {p}  {os.path.getsize(p):,} bytes')
project.close(program); project.close()
