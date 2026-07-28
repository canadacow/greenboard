"""Re-import stage2 with the CORRECT base so data addresses are right.

THE BUG: I imported stage2.bin at 0050:0000. But image byte 0 is physical
0x500, and the code's DS-relative displacements (e.g. mov bx,[si+0x136b]) are
offsets within the DATA segment, not within this block. Ghidra therefore
rendered [si+0x136b] as &DAT_0050_0e6b -- exactly 0x500 low -- and EVERY data
address in the decompilation is shifted by 0x500.

FIX: the program is a flat real-mode image whose byte 0 sits at physical
0x500. Load it at 0000:0500 so that segment:offset arithmetic matches the
hardware, i.e. DS:136B for DS=117B resolves to 0x117B*16+0x136B.
"""
import os, sys
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_12.1.2_PUBLIC')
import pyghidra
pyghidra.start()

from ghidra.app.util.importer import MessageLog
from ghidra.program.model.lang import LanguageID
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.base.project import GhidraProject
from ghidra.program.model.symbol import SourceType
from ghidra.program.util import DefaultLanguageService
from java.io import File

PROJ_DIR  = os.path.abspath('scratch/ghidra_proj2')
PROJ_NAME = 'pirates2'
BIN       = os.path.abspath('scratch/out/stage2.bin')
os.makedirs(PROJ_DIR, exist_ok=True)

monitor = ConsoleTaskMonitor()
project = GhidraProject.createProject(PROJ_DIR, PROJ_NAME, False)
lang = DefaultLanguageService.getLanguageService().getLanguage(
    LanguageID('x86:LE:16:Real Mode'))
program = project.importProgram(File(BIN), lang, lang.getDefaultCompilerSpec())
if program is None:
    print('IMPORT FAILED'); sys.exit(1)

tx = program.startTransaction('rebase')
try:
    af  = program.getAddressFactory()
    mem = program.getMemory()
    blk = mem.getBlocks()[0]
    # Physical 0x500 == 0000:0500. Using segment 0000 keeps every
    # segment:offset the code uses resolvable at its true linear address.
    target = af.getAddress('0000:0500')
    print(f'moving {blk.getStart()} -> {target}')
    mem.moveBlock(blk, target, monitor)
    blk.setName('stage2')
    blk.setExecute(True); blk.setRead(True); blk.setWrite(True)
    st = program.getSymbolTable()
    st.createLabel(af.getAddress('0000:0520'), 'entry_stage2', SourceType.USER_DEFINED)
    st.addExternalEntryPoint(af.getAddress('0000:0520'))
finally:
    program.endTransaction(tx, True)

project.saveAs(program, '/', program.getName(), True)
print('\nimported at 0000:0500 -- data displacements now resolve correctly.')
print('  e.g. mov bx,[si+0x136b] with DS=117B  ->  linear 0x117B*16+0x136B')
project.close(program); project.close()
