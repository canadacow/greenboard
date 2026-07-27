"""Which I/O ports does the game actually read? A booter usually talks to the
8255/8042 keyboard port (0x60/0x61/0x64) directly rather than via INT 16h."""
import sys, struct, time, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT = D[0][0x200:0x208]==b'0-PIRATE'
mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0x410,struct.pack('<H',0x0021)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))

inports=collections.Counter(); outports=collections.Counter()
mode=[4]; t0=time.time()

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==0x13:
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF; dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            img=D.get(drv&1,D[0]); src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            if PROT and (drv&1)==0 and cyl==4 and head==0 and sec==1:
                nb=max(al,1)*SEC
                buf=bytes(img[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*(nb-SEC)
                uc.mem_write(dest,buf); uc.reg_write(UC_X86_REG_AX,(0x04<<8)|al)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1); return
            data=bytes(img[src:src+al*SEC])
            if data: uc.mem_write(dest,data)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00: mode[0]=al
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
        uc.reg_write(UC_X86_REG_AX,0); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

def hin(uc,port,size,user):
    inports[port]+=1
    if port==0x3DA: return 0x09    # CGA status: toggle retrace bits
    return 0
def hout(uc,port,size,value,user): outports[port]+=1

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,hin,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,hout,None,1,0,UC_X86_INS_OUT)
mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
try: mu.emu_start(0x7C00,0xFFFF0,0,20_000_000)
except UcError as e: print('stop',e)
print('IN ports:', {f'{p:#05x}':c for p,c in inports.most_common(12)})
print('OUT ports:',{f'{p:#05x}':c for p,c in outports.most_common(12)})
