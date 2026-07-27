"""Booter emulation -- correct interrupt handling.

Critical detail: Unicorn's UC_HOOK_INTR fires when the INT instruction executes,
BEFORE the CPU has pushed flags/CS/IP. So there is nothing to 'IRET' from.
The handler must simply set result registers and let execution continue at the
instruction after INT -- which Unicorn does automatically once the hook returns.
Earlier versions popped a non-existent stack frame and jumped to garbage.
"""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D = {0: open('assets/pirates_1.img','rb').read(),
     1: open('assets/pirates_2.img','rb').read()}
SEC, SPT, HEADS = 512, 9, 2

mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, D[0][:512])
mu.mem_write(0x410, struct.pack('<H',0x0021))
mu.mem_write(0x413, struct.pack('<H',640))
mu.mem_write(0x465, bytes([0x29]))
mu.mem_write(0xFFFFE, bytes([0xFE]))

reads, mode = [], [4]
def hook_intr(uc, intno, user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    cf_clear = True
    if intno == 0x13:
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF
        if ah == 0x02:
            img=D.get(drv&1, D[0])
            src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            data=img[src:src+al*SEC]
            dest=(es*16+bx)&0xFFFFF
            if data and dest+len(data)<=0x110000:
                uc.mem_write(dest,data)
            reads.append((drv,cyl,head,sec,al,dest,src))
            print(f'  INT13 drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d} '
                  f'-> {es:04X}:{bx:04X} phys={dest:05X} file=0x{src:06x}')
        uc.reg_write(UC_X86_REG_AX, al)
    elif intno == 0x10:
        if ah==0x00: mode[0]=al
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x03<<8)|mode[0])
    elif intno == 0x16:
        if ah in (0x00,0x10): uc.reg_write(UC_X86_REG_AX,0x011B)
        else: cf_clear=False; uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
    elif intno == 0x1A:
        uc.reg_write(UC_X86_REG_CX,0); uc.reg_write(UC_X86_REG_DX,0)
    if cf_clear:
        uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS) & ~1)
    # NO manual IRET -- hook fires pre-push; Unicorn resumes after the INT.

mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d:0, None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d:None, None,1,0,UC_X86_INS_OUT)

ic=[0]
mu.hook_add(UC_HOOK_CODE, lambda u,a,s,d: ic.__setitem__(0, ic[0]+1))

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)

print('=== booting ===')
try:
    mu.emu_start(0x7C00, 0xFFFF0, 0, 300_000_000)
    print('ended')
except UcError as e:
    print(f'\nSTOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X} after {ic[0]} instrs')

print(f'\nINT13 reads: {len(reads)}  instrs: {ic[0]}  mode: {mode[0]:#x}')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
print('RAM -> scratch/out/ram.bin')
