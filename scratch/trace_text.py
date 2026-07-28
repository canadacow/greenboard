"""Find where the narrative text comes from.

"Young and poor, you seek your fortune in the New World..." is a TEXT opcode
in the bytecode script -- the interpreter at 0050:3DDF falls through to
'call 0x4604' with the opcode in [0x9a83]. So 0x4604 is 'show text N'.

Rather than search for the string (it is not stored as plain ASCII on either
disk), hook 0x4604, record N and the pointer it resolves, and dump the bytes
it reads from. That identifies the string table and its encoding.
"""
import sys, struct, time, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 130_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

mode=[4]; pend=[None]; ki=[0]
KEYS=[' ',' ','1','2','2']+list('DRAKE')+['\r']*60

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

SHOW_TEXT=0x50*16+0x4604      # interpreter's "show text N"
INTERP   =0x50*16+0x3ddf
MENU_LOOP=0x50*16+0x3593
# 0050:32CC / 32DA copy strings into the compose buffer -- watch SI there too.
COPY1=0x50*16+0x32cc
COPY2=0x50*16+0x32da

events=[]; hits=collections.Counter()
ic=[0]; nt=[25000]; nk=[3_000_000]; nfire=[0]
def hc(uc,addr,size,user):
    ic[0]+=1
    if addr==MENU_LOOP: hits['menu']=1
    if addr==INTERP: hits['interp']+=1
    if addr in (COPY1,COPY2) and len(events)<60:
        ds=uc.reg_read(UC_X86_REG_DS); si=uc.reg_read(UC_X86_REG_SI)
        try:
            raw=bytes(uc.mem_read(ds*16+si,48))
        except Exception:
            raw=b''
        txt=''.join(chr(c) if 32<=c<127 else ('|' if c==0xff else '.') for c in raw)
        events.append((ic[0],'COPY',ds,si,txt))
    if addr==SHOW_TEXT and len(events)<60:
        ds=uc.reg_read(UC_X86_REG_DS)
        n=struct.unpack('<H',uc.mem_read(ds*16+0x9a83,2))[0]
        events.append((ic[0],'TEXT',ds,n,''))
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C); return
    if ic[0]>=nk[0]:
        nk[0]=ic[0]+3_000_000
        if pend[0] is None and ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1
    if hits['menu'] and ic[0]>=nfire[0]:
        nfire[0]=ic[0]+2_000_000
        try:
            cur=struct.unpack('<H',uc.mem_read(0x117b*16+0x3c0e,2))[0]
            uc.mem_write(0x117b*16+0x3c0e,struct.pack('<H',cur^0x10))
        except Exception: pass
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)

print(f'instrs={ic[0]:,} t={time.time()-t0:.0f}s  interpreter entries={hits["interp"]}')
print(f'\n{len(events)} text events:')
for n,kind,ds,a,txt in events[:50]:
    if kind=='COPY':
        print(f'  {n//1000000:3d}M COPY  DS={ds:04X} SI={a:04X}  {txt}')
    else:
        print(f'  {n//1000000:3d}M TEXT  op={a}')
