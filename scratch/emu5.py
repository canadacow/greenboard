import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

D1 = open('assets/pirates_1.img','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_16)
mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, D1[:512])
mu.mem_write(0x410, struct.pack('<H',0x0021))
mu.mem_write(0x465, bytes([0x29]))
for v in range(256):
    mu.mem_write(v*4, struct.pack('<HH',0xFF53,0xF000))
mu.mem_write(0xFFF53, b'\xCF')

n=[0]
def hook_code(uc,addr,size,user):
    n[0]+=1
    if n[0] < 60 or n[0] % 1 == 0:
        if 560 <= n[0] <= 640:
            cs,ip = uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP)
            try:
                i = next(md.disasm(bytes(uc.mem_read(addr,10)),addr),None)
                t = f'{i.mnemonic:<7s} {i.op_str}' if i else '??'
            except Exception: t='??'
            print(f'{n[0]:4d} {cs:04X}:{ip:04X} lin={addr:05X} {t:<32s} ax={uc.reg_read(UC_X86_REG_AX):04X} es={uc.reg_read(UC_X86_REG_ES):04X} bx={uc.reg_read(UC_X86_REG_BX):04X} cx={uc.reg_read(UC_X86_REG_CX):04X} dx={uc.reg_read(UC_X86_REG_DX):04X}')
    if n[0] > 700: uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook_code)

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX)
    print(f'   >>> INT {intno:02X} ax={ax:04X} cx={uc.reg_read(UC_X86_REG_CX):04X} dx={uc.reg_read(UC_X86_REG_DX):04X} es={uc.reg_read(UC_X86_REG_ES):04X} bx={uc.reg_read(UC_X86_REG_BX):04X}')
    ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==0x13 and ah==0x02:
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F; head=(dx>>8)&0xFF
        src=((cyl*2+head)*9+(sec-1))*512
        dest=(uc.reg_read(UC_X86_REG_ES)*16+uc.reg_read(UC_X86_REG_BX))&0xFFFFF
        uc.mem_write(dest, D1[src:src+al*512])
        print(f'       READ c={cyl} h={head} s={sec} n={al} file 0x{src:06x} -> phys {dest:05X}')
        uc.reg_write(UC_X86_REG_AX, al)
    if intno==0x10 and ah==0x0F: uc.reg_write(UC_X86_REG_AX,0x0304)
    uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)&~1)
    sp=uc.reg_read(UC_X86_REG_SP); ss=uc.reg_read(UC_X86_REG_SS)
    ip,cs,fl=struct.unpack('<HHH', uc.mem_read(ss*16+sp,6))
    uc.reg_write(UC_X86_REG_SP,(sp+6)&0xFFFF)
    uc.reg_write(UC_X86_REG_CS,cs); uc.reg_write(UC_X86_REG_IP,ip)
mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d:0,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
try: mu.emu_start(0x7C00, 0xFFFFF, 0, 0)
except UcError as e: print('ERR',e, f'{mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
print('instrs', n[0])
