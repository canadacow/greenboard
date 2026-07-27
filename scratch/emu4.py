"""Full booter emulation. Key fix: enter at CS=0x07C0, IP=0x0000 so that
'mov ax,cs' yields 0x07C0 and the self-relocation math works."""
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
for v in range(256):
    mu.mem_write(v*4, struct.pack('<HH',0xFF53,0xF000))
mu.mem_write(0xFFF53, b'\xCF')

reads = []
def hook_intr(uc, intno, user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==0x13:
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF
        if ah in (0x02,0x03,0x04):
            img=D.get(drv&1,D[0])
            src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            dest=(es*16+bx)&0xFFFFF
            if ah==0x02:
                data=img[src:src+al*SEC]
                if data: uc.mem_write(dest,data)
                reads.append((cyl,head,sec,al,es,bx,src,dest))
                print(f'  INT13 rd c={cyl:2d} h={head} s={sec:2d} n={al:2d} -> {es:04X}:{bx:04X} '
                      f'phys {dest:05X}  file 0x{src:06x}')
            uc.reg_write(UC_X86_REG_AX, al)
        elif ah==0x00:
            uc.reg_write(UC_X86_REG_AX,0)
        uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)&~1)
        return
    elif intno==0x10:
        if ah==0x0F: uc.reg_write(UC_X86_REG_AX,0x0304)
    elif intno==0x16:
        if ah in (0x00,0x10): uc.reg_write(UC_X86_REG_AX,0x011B)
        else: uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
    elif intno==0x1A:
        uc.reg_write(UC_X86_REG_CX,0); uc.reg_write(UC_X86_REG_DX,0)
    sp=uc.reg_read(UC_X86_REG_SP); ss=uc.reg_read(UC_X86_REG_SS)
    ip,cs,fl=struct.unpack('<HHH', uc.mem_read(ss*16+sp,6))
    uc.reg_write(UC_X86_REG_SP,(sp+6)&0xFFFF)
    uc.reg_write(UC_X86_REG_CS,cs); uc.reg_write(UC_X86_REG_IP,ip)

mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d:0, None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d:None, None,1,0,UC_X86_INS_OUT)

ic=[0]
def hc(uc,a,s,u): ic[0]+=1
mu.hook_add(UC_HOOK_CODE, hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0x0000)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0x0000)

print('=== booting (CS=07C0:0000) ===')
try:
    mu.emu_start(0x7C00, 0, 0, 60_000_000)
    print('halted normally')
except UcError as e:
    print(f'\nSTOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X} after {ic[0]} instrs')

print(f'\ntotal INT13 reads: {len(reads)}, instrs {ic[0]}')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
print('RAM -> scratch/out/ram.bin')
