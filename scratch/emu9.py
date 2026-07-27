"""Pirates! booter emulation with MicroProse disk copy-protection support.

Protection mechanism (as reverse-engineered in src/isa/isa_fdc.cpp):
  Disk 1 ("0-PIRATE GAME DISK" label at 0x200) has track C=4 H=0 misformatted.
  The game issues a READ of C=4 H=0 S=1 with an oversized sector-size code
  (N != 2, i.e. not 512 bytes). On real media this never matches a sector ID:
  the uPD765 streams the 512 bytes of sector data followed by format gap
  filler (0x43 = 'C') into the buffer, then reports ND ("no data", ST1=0x04).
  The game checks for the gap filler AND requires the error. A naive emulator
  that returns success -- or that fails without transferring the filler --
  flunks the check.

  Through INT 13h this surfaces as: read returns CF=1, AH=0x04, but the
  destination buffer has been filled (512 bytes data, then 0x43 padding).
"""
import sys, struct, time
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D = {0: bytearray(open('assets/pirates_1.img','rb').read()),
     1: bytearray(open('assets/pirates_2.img','rb').read())}
SEC, SPT, HEADS = 512, 9, 2
PROT = D[0][0x200:0x208] == b'0-PIRATE'

MAX_INSTR = int(sys.argv[1]) if len(sys.argv) > 1 else 30_000_000
MAX_SECS = 90

mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, bytes(D[0][:512]))
mu.mem_write(0x410, struct.pack('<H', 0x0021))
mu.mem_write(0x413, struct.pack('<H', 640))
mu.mem_write(0x465, bytes([0x29]))
mu.mem_write(0xFFFFE, bytes([0xFE]))

# Keys: 1=CGA, 1=one floppy, 2=keyboard, then Enters
KEYS = [(0x02,0x31), (0x02,0x31), (0x03,0x32)] + [(0x1C,0x0D)]*40
kq = list(KEYS)
log, mode, stats = [], [4], {'13':0, '16':0, 'prot':0}
t0 = time.time()

def hook_intr(uc, intno, user):
    ax = uc.reg_read(UC_X86_REG_AX); ah = (ax>>8)&0xFF; al = ax&0xFF
    cf = 0
    if intno == 0x13:
        stats['13'] += 1
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF
        dest=(es*16+bx)&0xFFFFF
        if ah == 0x02:
            img = D.get(drv & 1, D[0])
            prot_hit = (PROT and (drv & 1) == 0 and cyl == 4 and head == 0 and sec == 1)
            src = ((cyl*HEADS+head)*SPT+(sec-1))*SEC
            if prot_hit:
                stats['prot'] += 1
                # Stream 512 bytes of (zeroed) sector data + 0x43 gap filler,
                # then report ND. AL is the requested sector count; the
                # protection read is oversized, so pad generously.
                nbytes = max(al, 1) * SEC
                buf = bytes(img[src:src+SEC]).ljust(SEC, b'\x00')
                buf += b'\x43' * (nbytes - len(buf))
                if dest + len(buf) <= 0x110000:
                    uc.mem_write(dest, buf)
                uc.reg_write(UC_X86_REG_AX, (0x04 << 8) | al)
                cf = 1
                print(f'  *** PROTECTION READ C=4 H=0 S=1 n={al} -> {es:04X}:{bx:04X}: '
                      f'{SEC}B data + {nbytes-SEC}B gap fill (0x43), return ND/CF=1', flush=True)
                fl = uc.reg_read(UC_X86_REG_EFLAGS)
                uc.reg_write(UC_X86_REG_EFLAGS, fl | 1)
                return
            data = bytes(img[src:src+al*SEC])
            if data and dest+len(data) <= 0x110000:
                uc.mem_write(dest, data)
            log.append((drv,cyl,head,sec,al,dest,src))
            print(f'  INT13 rd drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d} '
                  f'-> {es:04X}:{bx:04X} file=0x{src:06x}', flush=True)
            uc.reg_write(UC_X86_REG_AX, al)
        elif ah == 0x03:
            img = D.get(drv & 1, D[0])
            src = ((cyl*HEADS+head)*SPT+(sec-1))*SEC
            data = bytes(uc.mem_read(dest, al*SEC))
            img[src:src+len(data)] = data
            print(f'  INT13 WR drv={drv} c={cyl} h={head} s={sec} n={al}', flush=True)
            uc.reg_write(UC_X86_REG_AX, al)
        else:
            uc.reg_write(UC_X86_REG_AX, al)
    elif intno == 0x10:
        if ah == 0x00:
            mode[0] = al; print(f'  INT10 video mode {al:#x}', flush=True)
        elif ah == 0x0F:
            uc.reg_write(UC_X86_REG_AX, (0x28<<8)|mode[0])
    elif intno == 0x16:
        stats['16'] += 1
        sc, asc = kq.pop(0) if kq else (0x1C, 0x0D)
        if ah in (0x00,0x10):
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|asc)
        elif ah in (0x01,0x11):
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|asc)
            uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)&~0x40)
            return
    elif intno == 0x1A:
        t = int((time.time()-t0)*18.2)&0xFFFFFF
        uc.reg_write(UC_X86_REG_CX,(t>>16)&0xFFFF); uc.reg_write(UC_X86_REG_DX,t&0xFFFF)
    fl = uc.reg_read(UC_X86_REG_EFLAGS)
    uc.reg_write(UC_X86_REG_EFLAGS, (fl|1) if cf else (fl & ~1))

mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d: 0, None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d: None, None,1,0,UC_X86_INS_OUT)

ic=[0]
def hook_code(uc, addr, size, user):
    ic[0]+=1
    if (ic[0] & 0xFFFFF)==0 and time.time()-t0 > MAX_SECS:
        print('  [watchdog]', flush=True); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook_code)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)

print(f'=== booting (protected disk: {PROT}) ===', flush=True)
try:
    mu.emu_start(0x7C00, 0xFFFF0, 0, MAX_INSTR)
    print('ended')
except UcError as e:
    print(f'STOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')

print(f'\ninstrs={ic[0]:,} INT13={stats["13"]} INT16={stats["16"]} '
      f'protection_reads={stats["prot"]} mode={mode[0]:#x} elapsed={time.time()-t0:.1f}s')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
open('scratch/out/cga.bin','wb').write(bytes(mu.mem_read(0xB8000,0x4000)))
print('RAM -> scratch/out/ram.bin, CGA -> scratch/out/cga.bin')
