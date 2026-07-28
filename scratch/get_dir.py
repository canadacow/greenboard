"""Capture the resource directory at DS:136B, then decode every resource
from the DISK using the loader's own field layout. One measurement, then
pure Python -- no further emulation.

res_fetch (0000:2D5B):
    bx = [si + 0x136b]     ; si = resource index * 2
    count = [bx+0] ; cyl = [bx+4] + [0x3bc8] ; sec = [bx+6]
    seg   = [bx+8] ; off = [bx+0xA]
    sec >= 10  =>  head 1, sec -= 9
"""
import sys, struct, time, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
DS=0x117B
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 20_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))
mode=[4]; pend=[None]; ki=[0]
KEYS=[' ',' ','1','2','2']

def do_int(uc,vec):
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    ss=uc.reg_read(UC_X86_REG_SS); sp=(uc.reg_read(UC_X86_REG_SP)-6)&0xFFFF
    uc.mem_write(ss*16+sp,struct.pack('<HHH',ip,cs,fl)); uc.reg_write(UC_X86_REG_SP,sp)
    nip,ncs=struct.unpack('<HH',uc.mem_read(vec*4,4))
    uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x200)
    uc.reg_write(UC_X86_REG_CS,ncs); uc.reg_write(UC_X86_REG_IP,nip)

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==0x13:
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF; dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            im=D[drv&1]; src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            if PROT and (drv&1)==0 and cyl==4 and head==0 and sec==1:
                buf=bytes(im[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*1536
                if dest+len(buf)<=0x110000: uc.mem_write(dest,buf)
                uc.reg_write(UC_X86_REG_AX,0x0400)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1); return
            data=bytes(im[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00: mode[0]=al
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        if ah in (0x01,0x11):
            if pend[0] is None: uc.reg_write(UC_X86_REG_EFLAGS,fl|0x40)
            else:
                uc.reg_write(UC_X86_REG_AX,(0x39<<8)|ord(pend[0]))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            c=pend[0] or '\r'; pend[0]=None
            uc.reg_write(UC_X86_REG_AX,(0x39<<8)|ord(c)); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

FETCH=0x50*16+0x2844   # res_fetch entry (image 0x2844 == 0050:2844)
got={}
ic=[0]; nt=[25000]; nk=[3_000_000]
def hc(uc,addr,size,user):
    n=ic[0]=ic[0]+1
    if addr==FETCH and 'tbl' not in got and n>2_000_000:
        got['tbl']=bytes(uc.mem_read(DS*16+0x136b,0x120))
        got['at']=n; uc.emu_stop(); return
    if n>=nt[0]:
        nt[0]=n+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc,0x08 if v8[1]!=0xF000 else 0x1C); return
    if n>=nk[0]:
        nk[0]=n+3_000_000
        if pend[0] is None and ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1
mu.hook_add(UC_HOOK_CODE,hc)
mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
if 'tbl' not in got:
    print('res_fetch not reached'); sys.exit(1)

tbl=got['tbl']
open('scratch/out/res_table.bin','wb').write(tbl)
print(f"captured DS:136B at {got['at']:,} instrs ({time.time()-t0:.0f}s)")
ptrs=struct.unpack('<144H',tbl)
print('pointers:',' '.join(f'{p:04x}' for p in ptrs[:16]))

disk=D[0]
def chs(c,h,s): return ((c*HEADS+h)*SPT+(s-1))*SEC
print(f'\n{"idx":>4} {"ptr":>6} {"cnt":>4} {"cyl":>4} {"sec":>4}  {"dest":>10}  file      bytes')
print('-'*70)
rows=[]
for i,p in enumerate(ptrs):
    lin=DS*16+p
    try: rec=bytes(mu.mem_read(lin,12))
    except Exception: continue
    cnt=struct.unpack_from('<H',rec,0)[0]&0xFF
    cyl=struct.unpack_from('<H',rec,4)[0]
    sec=struct.unpack_from('<H',rec,6)[0]
    seg=struct.unpack_from('<H',rec,8)[0]
    off=struct.unpack_from('<H',rec,10)[0]
    if not (0<cnt<=200 and cyl<=40 and 1<=sec<=18): continue
    h,s=(1,sec-9) if sec>=10 else (0,sec)
    fo=chs(cyl,h,s)
    rows.append((i,cnt,cyl,sec,seg,off,fo))
    print(f'{i:>4} {p:#06x} {cnt:>4} {cyl:>4} {sec:>4}  {seg:04X}:{off:04X}  {fo:#08x} {cnt*SEC:>6}')
print(f'\n{len(rows)} resources')
