"""Run Ghidra auto-analysis on the imported Pirates! stage-2 program, seeded
with the entry point and the routines we identified by emulation."""
import os, sys, time
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()

from ghidra.base.project import GhidraProject
from ghidra.program.flatapi import FlatProgramAPI
from ghidra.app.script import GhidraScriptUtil
from ghidra.util.task import ConsoleTaskMonitor

PROJ_DIR  = os.path.abspath('scratch/ghidra_proj')
PROJ_NAME = 'pirates'
monitor = ConsoleTaskMonitor()

project = GhidraProject.openProject(PROJ_DIR, PROJ_NAME, True)
program = project.openProgram('/', 'stage2.bin', False)
print('opened:', program.getName(), program.getLanguageID())

tx = program.startTransaction('analyze')
t0=time.time()
try:
    api = FlatProgramAPI(program, monitor)
    af  = program.getAddressFactory()
    # Seed disassembly at every routine we know is real code.
    seeds = ['0050:0020','0050:08a0','0050:0896','0050:09e0','0050:2740',
             '0050:0a7d','0050:0a9d','0050:12a0','0050:26e0','0050:2820',
             '0050:28be','0050:252b','0050:2500']
    for s in seeds:
        a=af.getAddress(s)
        try:
            api.disassemble(a)
            api.createFunction(a, None)
        except Exception as e:
            print(f'  seed {s}: {e}')
    from ghidra.app.plugin.core.analysis import AutoAnalysisManager
    mgr = AutoAnalysisManager.getAnalysisManager(program)
    mgr.initializeOptions()
    mgr.reAnalyzeAll(None)
    mgr.startAnalysis(monitor)
finally:
    program.endTransaction(tx, True)

fm = program.getFunctionManager()
print(f'\nanalysis done in {time.time()-t0:.0f}s')
print('functions found:', fm.getFunctionCount())
project.save(program)
project.close(program)
project.close()
print('saved')
