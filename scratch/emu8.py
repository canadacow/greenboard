"""Booter emulation with keyboard input + watchdog.

Key fixes:
  1. UC_HOOK_INTR fires BEFORE the CPU pushes flags/CS/IP -- do NOT emulate IRET.
  2. Enter at CS=0x07C0 so 'mov ax,cs' is correct.
  3. INT 16h feeds a scripted keystroke queue (Enter / '1'), then falls back to
     Enter forever, so title/menu waits always advance.
  4. Watchdog: instruction budget + wall-clock cap so it can never hang.
"""
import sys, struct, time
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D = {0: open('assets/pirates_1.img','rb').read(),
     1: open('assets/pirates_2.img','rb').read()}
SEC, SPT, HEADS = 512, 9, 2

MAX_INSTR = int(sys.argv[1]) if len(sys.argv) > 1 else 40_000_000
MAX_SECS  = 60

mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, D[0][:512])
mu.mem_write(0x410, struct.pack('<H', 0x0021))
mu.mem_write(0x413, struct.pack('<H', 640))
mu.mem_write(0x465, bytes([0x29]))
mu.mem_write(0xFFFFE, bytes([0xFE]))

# scancode, ascii
KEYS = [(0x1C,0x0D), (0x02,0x31), (0x1C,0x0D), (0x1C,0x0D), (0x02,0x31), (0x1C,0x0D)]
kq = list(KEYS)
reads, mode, stats = [], [4], {'int16':0, 'int13':0}
t0 = time.time()

def hook_intr(uc, intno, user):
    ax = uc.reg_read(UC_X86_REG_AX); ah = (ax>>8)&0xFF; al = ax&0xFF
    cf = 0
    if intno == 0x13:
        stats['int13'] += 1
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF
        if ah == 0x02:
            img = D.get(drv & 1, D[0])
            src = ((cyl*HEADS+head)*SPT+(sec-1))*SEC
            data = img[src:src+al*SEC]
            dest = (es*16+bx) & 0xFFFFF
            if data and dest+len(data) <= 0x110000:
                uc.mem_write(dest, data)
            reads.append((drv,cyl,head,sec,al,dest,src))
            print(f'  INT13 drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d} '
                  f'-> {es:04X}:{bx:04X} phys={dest:05X} file=0x{src:06x}', flush=True)
        uc.reg_write(UC_X86_REG_AX, al)
    elif intno == 0x10:
        if ah == 0x00:
            mode[0] = al
            print(f'  INT10 set video mode {al:#x}', flush=True)
        elif ah == 0x0F:
            uc.reg_write(UC_X86_REG_AX, (0x28<<8)|mode[0])
    elif intno == 0x16:
        stats['int16'] += 1
        sc, asc = kq.pop(0) if kq else (0x1C, 0x0D)
        if ah in (0x00, 0x10):
            uc.reg_write(UC_X86_REG_AX, (sc<<8)|asc)
        elif ah in (0x01, 0x11):
            uc.reg_write(UC_X86_REG_AX, (sc<<8)|asc)
            uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS) & ~0x40)
            return
        elif ah == 0x02:
            uc.reg_write(UC_X86_REG_AX, (ax & 0xFF00))
    elif intno == 0x1A:
        t = int((time.time()-t0)*18.2) & 0xFFFFFF
        uc.reg_write(UC_X86_REG_CX, (t>>16)&0xFFFF)
        uc.reg_write(UC_X86_REG_DX, t & 0xFFFF)
    fl = uc.reg_read(UC_X86_REG_EFLAGS)
    uc.reg_write(UC_X86_REG_EFLAGS, (fl | 1) if cf else (fl & ~1))

mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d: 0, None, 1, 0, UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d: None, None, 1, 0, UC_X86_INS_OUT)

ic = [0]
def hook_code(uc, addr, size, user):
    ic[0] += 1
    if (ic[0] & 0xFFFFF) == 0 and time.time() - t0 > MAX_SECS:
        print('  [watchdog: wall-clock cap]', flush=True)
        uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook_code)

mu.reg_write(UC_X86_REG_CS, 0x07C0); mu.reg_write(UC_X86_REG_IP, 0)
mu.reg_write(UC_X86_REG_SS, 0); mu.reg_write(UC_X86_REG_SP, 0x7000)
mu.reg_write(UC_X86_REG_DX, 0)

print(f'=== booting (budget {MAX_INSTR:,} instrs / {MAX_SECS}s) ===', flush=True)
try:
    mu.emu_start(0x7C00, 0xFFFF0, 0, MAX_INSTR)
    print('emulation ended (budget reached or halt)')
except UcError as e:
    print(f'\nSTOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')

print(f'\ninstrs={ic[0]:,}  INT13={stats["int13"]}  INT16={stats["int16"]}  mode={mode[0]:#x}')
print(f'elapsed {time.time()-t0:.1f}s')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0, 0x100000)))
print('RAM -> scratch/out/ram.bin')
