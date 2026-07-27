"""Find the EXACT instruction the game is stuck on at the drive-config screen.

No assumptions: run, then histogram the executed addresses in the last N
million instructions, and dump the tight loop with full register/memory
context so we can see what condition never becomes true.
"""
import sys, struct, time, collections, os
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
md=Cs(CS_ARCH_X86,CS_MODE_16)
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 40_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0,b'\xCC'*0x100000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0xB8000,b'\x00'*0x8000); mu.mem_write(0xA0000,b'\x00'*0x10000)
mu.mem_write(0x410,struct.pack('<H',0x0021)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

mode=[4]
def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==3: return
    if intno==0x13:
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF; dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            im=D.get(drv&1,D[0]); src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            if PROT and (drv&1)==0 and cyl==4 and head==0 and sec==1:
                nb=max(al,1)*SEC
                buf=bytes(im[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*(nb-SEC)
                if dest+len(buf)<=0x110000: uc.mem_write(dest,buf)
                uc.reg_write(UC_X86_REG_AX,(0x04<<8)|al)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1); return
            data=bytes(im[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00: mode[0]=al
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        # ALWAYS empty -- we want to see what it does while waiting
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
        uc.reg_write(UC_X86_REG_AX,0); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

ports=collections.Counter()
def hin(uc,port,size,user):
    ports[port]+=1
    if port==0x3DA: return 0x09
    return 0
mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,hin,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

hot=collections.Counter(); ic=[0]; TAIL=MAX-3_000_000
def hc(uc,addr,size,user):
    ic[0]+=1
    if ic[0]>TAIL:
        hot[(uc.reg_read(UC_X86_REG_CS),uc.reg_read(UC_X86_REG_IP))]+=1
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
print(f'instrs={ic[0]:,} t={time.time()-t0:.0f}s  IN ports={dict(ports)}')
print('\n=== hottest addresses (the stuck loop) ===')
for (cs,ip),c in hot.most_common(30):
    lin=(cs*16+ip)&0xFFFFF
    try:
        ins=next(md.disasm(bytes(mu.mem_read(lin,10)),lin),None)
        t=f'{ins.mnemonic} {ins.op_str}' if ins else '?'
    except Exception: t='?'
    print(f'  {cs:04X}:{ip:04X} x{c:<7d} {t}')

# Dump the loop body around the hottest address
if hot:
    (cs,ip),_=hot.most_common(1)[0]
    lo=max(0,(cs*16+ip)-0x60)
    print(f'\n=== disassembly around hottest {cs:04X}:{ip:04X} ===')
    for ins in md.disasm(bytes(mu.mem_read(lo,0xE0)),lo):
        mark=' <<<' if ins.address==(cs*16+ip) else ''
        print(f'  {ins.address:05X}: {ins.mnemonic:<7s} {ins.op_str}{mark}')
