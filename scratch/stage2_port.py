"""Continue the port: stage 2 entry, executing from ported memory only.

Rule after the boot-sector lesson: once the port has loaded memory, NEVER
read bytes from anywhere else. No build_image, no raw disk offsets, no
hand-translated addresses. Port a block, run it, read c.mem.

0050:0020  jmp 0x80              (entry -- thunk table sits at 0x23..0x7F)
0050:0080  main:
    mov cx, cs:[0]   ; DS = data segment  (117B)
    mov ds, cx
    mov es, cx
    mov cx, cs:[8]   ; SS = stack segment (26BE)
    mov ss, cx
    mov sp, 0x13fc
    mov al, 0xcc / inc al -> 0xCD, stored to cs:[0x16a] and cs:[0x147]
        (patches two INT opcodes into the code stream)
    mov [0x3bc6], dh ; boot drive
    ; BIOS ROM signature probe at F000:FFFE / F000:C000
    mov [0x3b98], 0xffff
    call 0x67 / 0x1136 / 0x128d
    ...
"""
import sys
sys.path.insert(0, 'scratch')
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
from boot_port import CPU, run_boot, run_reloc, run_title, run_stage2
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

md = Cs(CS_ARCH_X86, CS_MODE_16)


def boot_all():
    """Run everything the boot sector does, returning the loaded machine."""
    c = CPU()
    run_boot(c)
    run_reloc(c)
    run_title(c)
    seg, off = run_stage2(c)
    return c, seg, off


def dis(c, seg, off, n=0x40, label=''):
    """Disassemble straight out of ported memory."""
    a = c.lin(seg, off)
    code = bytes(c.mem[a:a + n])
    if label:
        print(f'=== {label}  ({seg:04X}:{off:04X}) ===')
    for i in md.disasm(code, off):
        print(f'  {seg:04X}:{i.address:04X}  {i.bytes.hex():<12s} '
              f'{i.mnemonic:<7s} {i.op_str}')


def run_main_prologue(c):
    """0050:0080 -- set up segments and patch the two INT opcodes.

    The patch is the interesting part: main writes 0xCD (the INT opcode)
    into cs:[0x16A] and cs:[0x147]. Those bytes are 0 on disk, which is why
    a static disassembly of that region is meaningless until this runs.
    """
    cs = 0x0050
    c.cx = c.rw(cs, 0)          # cs:[0] -> data segment
    c.ds = c.es = c.cx
    c.cx = c.rw(cs, 8)          # cs:[8] -> stack segment
    c.ss = c.cx
    c.cx = 0x13FC
    c.sp = c.cx
    c.al = 0xCC
    c.al = (c.al + 1) & 0xFF    # inc al -> 0xCD = INT
    c.wb(cs, 0x16A, c.al)
    c.wb(cs, 0x147, c.al)
    c.wb(c.ds, 0x3BC6, c.dh)    # boot drive
    # BIOS ROM signature probe
    if c.rb(0xF000, 0xFFFE) == 0xFF and c.rb(0xF000, 0xC000) == 0x21:
        c.wb(c.ds, 0x3BC3, 1)
    c.ww(c.ds, 0x3B98, 0xFFFF)
    return cs


if __name__ == '__main__':
    c, seg, off = boot_all()
    print(f'boot complete: entry {seg:04X}:{off:04X}')
    print(f'segment table: ' + ' '.join(f'{c.rw(0x50, i*2):04X}' for i in range(8)))
    print()

    cs = run_main_prologue(c)
    print(f'after main prologue: DS={c.ds:04X} ES={c.es:04X} '
          f'SS={c.ss:04X} SP={c.sp:04X}')
    print(f'  patched cs:[0x16A] = {c.rb(cs,0x16A):#04x}  '
          f'cs:[0x147] = {c.rb(cs,0x147):#04x}')
    print()

    # Now those patched sites disassemble correctly -- from ported memory.
    dis(c, 0x0050, 0x0140, 0x40, 'patched region around cs:0x147')
    print()
    dis(c, 0x0050, 0x0165, 0x20, 'patched region around cs:0x16A')
