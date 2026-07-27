"""Import the Pirates! stage-2 image into a Ghidra project as 16-bit real mode.

pyghidra-mcp has no flag for raw/headerless binaries, so we do the import
ourselves with bundled pyghidra and hand the finished project to the server.

Facts established earlier by tracing the boot sector:
  - stage 2 is 168KB, loaded to segment 0x50 offset 0 (physical 0x500)
  - entry point is 0050:0020
  - the first 16 words at 0050:0000 are a segment table the boot loader
    relocates by +0x50; word[0] = the game's data segment (0x117B at runtime)
"""
import os, sys
sys.stdout.reconfigure(encoding='utf-8')
os.environ.setdefault('GHIDRA_INSTALL_DIR', r'C:\dev\ghidra\ghidra_11.4.2_PUBLIC')

import pyghidra
pyghidra.start()

from ghidra.app.util.importer import MessageLog
from ghidra.program.model.lang import LanguageID, CompilerSpecID
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.base.project import GhidraProject
from ghidra.program.model.symbol import SourceType
from java.io import File

PROJ_DIR  = os.path.abspath('scratch/ghidra_proj')
PROJ_NAME = 'pirates'
BIN       = os.path.abspath('scratch/out/stage2.bin')
os.makedirs(PROJ_DIR, exist_ok=True)

monitor = ConsoleTaskMonitor()
project = GhidraProject.createProject(PROJ_DIR, PROJ_NAME, False)
print(f'project: {PROJ_DIR}\\{PROJ_NAME}.gpr')

lang_id = LanguageID('x86:LE:16:Real Mode')
from ghidra.program.util import DefaultLanguageService
lang = DefaultLanguageService.getLanguageService().getLanguage(lang_id)
cspec = lang.getDefaultCompilerSpec()
print(f'language: {lang.getLanguageID()}  cspec: {cspec.getCompilerSpecID()}')

program = project.importProgram(File(BIN), lang, cspec)
if program is None:
    print('IMPORT FAILED'); sys.exit(1)

tx = program.startTransaction('setup')
try:
    af = program.getAddressFactory()
    space = af.getDefaultAddressSpace()
    mem = program.getMemory()
    blk = mem.getBlocks()[0]
    # Rebase so addresses read as 0050:xxxx like the live game.
    target = af.getAddress('0050:0000')
    print(f'moving block {blk.getName()} {blk.getStart()} -> {target}')
    mem.moveBlock(blk, target, monitor)
    blk.setName('stage2')
    blk.setExecute(True); blk.setRead(True); blk.setWrite(True)

    # Entry point: LJMP target from the boot sector.
    entry = af.getAddress('0050:0020')
    program.getSymbolTable().createLabel(entry, 'entry_stage2', SourceType.USER_DEFINED)
    program.getSymbolTable().addExternalEntryPoint(entry)

    # Label the input routines we identified by emulation, so they are easy
    # to find in the decompiler.
    for off, name in (('0050:08a0','key_read_char'),
                      ('0050:0896','key_read_wrapper'),
                      ('0050:09e0','kbd_flush_buffer'),
                      ('0050:2740','sel_poll'),
                      ('0050:0a7d','glyph_blit_cga'),
                      ('0050:0a9d','glyph_cell_write'),
                      ('0050:12a0','timer_isr_int1c'),
                      ('0050:26e0','kbd_isr_int09'),
                      ('0050:2820','load_resource'),
                      ('0050:28be','disk_read_sector')):
        try:
            program.getSymbolTable().createLabel(af.getAddress(off), name,
                                                 SourceType.USER_DEFINED)
        except Exception as e:
            print(f'  label {off} {name}: {e}')
finally:
    program.endTransaction(tx, True)

# importProgram() returns a program that is not yet in the project, so
# save() has no location -- use saveAs() to place it in the root folder.
project.saveAs(program, '/', program.getName(), True)
project.close(program)
project.close()
print('\nimported OK -- open with:')
print(f'  pyghidra-mcp --project-path {PROJ_DIR}\\{PROJ_NAME}.gpr')
