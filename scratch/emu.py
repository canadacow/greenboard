"""Emulate the Pirates! booter under Unicorn with a fake BIOS.
Logs every INT 13h (disk read) so we learn exactly which sectors the game
loads, where they go, and when -- no guessing about segments."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

DISK1 = open('assets/pirates_1.img', 'rb').read()
DISK2 = open('assets/pirates_2.img', 'rb').read()
SEC = 512
SPT, HEADS = 9, 2

def chs(disk, c, h, s):
    return ((c*HEADS + h)*SPT + (s-1))*SEC

MEM = 1024*1024 + 0x10000
mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, MEM)

# BIOS data area basics
mu.mem_write(0x410, struct.pack('<H', 0x0021))   # equipment: 1 floppy, 80x25 colour
mu.mem_write(0x413, struct.pack('<H', 640))      # KB of memory
mu.mem_write(0x465, bytes([0x29]))               # CGA mode reg shadow

# Put boot sector at 0000:7C00
mu.mem_write(0x7C00, DISK1[:512])

# IVT: point every vector at a stub that we intercept via IN-instruction trick.
# Simpler: install 'CF3' (iret) stubs and hook interrupts natively.
STUB = 0xF0000
for v in range(256):
    mu.mem_write(v*4, struct.pack('<HH', 0x0100 + v*2, 0xF000))
# each stub: just IRET (we intercept before execution via UC_HOOK_INTR)
mu.mem_write(0xF0100, b'\xCF' * 512)

reads = []
disk_sel = [0]

def hook_intr(uc, intno, user):
    ah = (uc.reg_read(UC_X86_REG_AX) >> 8) & 0xFF
    al = uc.reg_read(UC_X86_REG_AX) & 0xFF
    if intno == 0x13:
        cx = uc.reg_read(UC_X86_REG_CX); dx = uc.reg_read(UC_X86_REG_DX)
        bx = uc.reg_read(UC_X86_REG_BX); es = uc.reg_read(UC_X86_REG_ES)
        cyl = ((cx >> 8) & 0xFF) | ((cx & 0xC0) << 2)
        sec = cx & 0x3F
        head = (dx >> 8) & 0xFF
        drive = dx & 0xFF
        if ah == 0x02:
            src = chs(drive, cyl, head, sec)
            data = (DISK2 if drive & 1 else DISK1)[src: src + al*SEC]
            dest = (es*16 + bx) & 0xFFFFF
            if data and dest + len(data) <= MEM:
                uc.mem_write(dest, data)
            reads.append((cyl, head, sec, al, es, bx, src, dest))
            print(f'  INT13 READ  c={cyl:2d} h={head} s={sec:2d} n={al:2d} -> {es:04X}:{bx:04X} '
                  f'(phys {dest:06X})  file 0x{src:06x}')
            uc.reg_write(UC_X86_REG_AX, 0x0000)
            # clear CF
            fl = uc.reg_read(UC_X86_REG_EFLAGS) & ~1
            uc.reg_write(UC_X86_REG_EFLAGS, fl)
        elif ah == 0x00:
            uc.reg_write(UC_X86_REG_AX, 0)
            uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS) & ~1)
        else:
            uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS) & ~1)
    elif intno == 0x10:
        if ah == 0x0F:
            uc.reg_write(UC_X86_REG_AX, 0x0004)
        # else ignore
    elif intno == 0x16:
        uc.reg_write(UC_X86_REG_AX, 0x0000)
    elif intno == 0x1A:
        uc.reg_write(UC_X86_REG_CX, 0)
        uc.reg_write(UC_X86_REG_DX, 0)
    # emulate IRET manually: pop ip, cs, flags
    sp = uc.reg_read(UC_X86_REG_SP); ss = uc.reg_read(UC_X86_REG_SS)
    base = ss*16 + sp
    ip, cs, fl = struct.unpack('<HHH', uc.mem_read(base, 6))
    uc.reg_write(UC_X86_REG_SP, (sp + 6) & 0xFFFF)
    uc.reg_write(UC_X86_REG_CS, cs)
    uc.reg_write(UC_X86_REG_IP, ip)

mu.hook_add(UC_HOOK_INTR, hook_intr)

# I/O ports: swallow OUTs, return 0 for INs
def hook_in(uc, port, size, user): return 0
def hook_out(uc, port, size, value, user): return
mu.hook_add(UC_HOOK_INSN, hook_in, None, 1, 0, UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, hook_out, None, 1, 0, UC_X86_INS_OUT)

count = [0]
def hook_code(uc, addr, size, user):
    count[0] += 1

mu.hook_add(UC_HOOK_CODE, hook_code)

mu.reg_write(UC_X86_REG_CS, 0)
mu.reg_write(UC_X86_REG_IP, 0x7C00)
mu.reg_write(UC_X86_REG_SS, 0)
mu.reg_write(UC_X86_REG_SP, 0x7000)
mu.reg_write(UC_X86_REG_DX, 0x0000)

print('=== booting ===')
try:
    mu.emu_start(0x7C00, 0xFFFFF, 0, 20_000_000)
except UcError as e:
    cs = mu.reg_read(UC_X86_REG_CS); ip = mu.reg_read(UC_X86_REG_IP)
    print(f'\nstopped: {e} at {cs:04X}:{ip:04X} after {count[0]} instrs')

print(f'\ntotal INT13 reads: {len(reads)}')
mu.mem_write(0, b'')  # noop
open('scratch/out/ram_after_boot.bin','wb').write(bytes(mu.mem_read(0, 0x100000)))
print('dumped RAM to scratch/out/ram_after_boot.bin')
