"""Find the copy-protection quiz's SOURCE DATA.

At ~49M instrs the prompt is composed into DS:0x96F3 as
    "PANAMA .. 1660, MR. INCOGNITO"
So: watch memory WRITES into that buffer, and for each write record the
instruction doing it plus the source registers. That leads back to the table
holding city/date pairs -- the fleet & silver-train schedule.

Also hook 0050:3FBD (the check) and log the full path taken.
"""
import sys, struct, time, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 130_000_000
md=Cs(CS_ARCH_X86,CS_MODE_16)

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

mode=[4]; pend=[None]; ki=[0]
KEYS=[' ',' ','1','2','2']+list('DRAKE')+['\r']*60
DS=0x117B

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

# Watch writes into the compose buffer that holds "<CITY> in <YEAR>, MR. <NAME>"
BUF_LO=DS*16+0x96F0
BUF_HI=DS*16+0x9760
writers=collections.Counter()
srcinfo={}
def hook_mem(uc,access,addr,size,value,user):
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    writers[(cs,ip)]+=1
    if (cs,ip) not in srcinfo:
        ds=uc.reg_read(UC_X86_REG_DS); si=uc.reg_read(UC_X86_REG_SI)
        # Read the SOURCE bytes LIVE -- the static image holds different data
        # at these addresses, which has misled me repeatedly.
        try: src=bytes(uc.mem_read(ds*16+si,48))
        except Exception: src=b''
        srcinfo[(cs,ip)]=dict(si=si,di=uc.reg_read(UC_X86_REG_DI),
                              bx=uc.reg_read(UC_X86_REG_BX),ds=ds,
                              val=value,src=src)
mu.hook_add(UC_HOOK_MEM_WRITE,hook_mem,None,BUF_LO,BUF_HI)

CHECK=0x50*16+0x3fbd
path=[]
MENU_LOOP=0x50*16+0x3593
hits=collections.Counter()
ic=[0]; nt=[25000]; nk=[3_000_000]; nfire=[0]
def hc(uc,addr,size,user):
    ic[0]+=1
    if addr==MENU_LOOP: hits['menu']=1
    if 0x50*16+0x3fbd <= addr <= 0x50*16+0x4020 and len(path)<200:
        path.append(addr-0x50*16)
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
            cur=struct.unpack('<H',uc.mem_read(DS*16+0x3c0e,2))[0]
            uc.mem_write(DS*16+0x3c0e,struct.pack('<H',cur^0x10))
        except Exception: pass
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)

print(f'instrs={ic[0]:,} t={time.time()-t0:.0f}s')
print(f'\nwrites into the prompt buffer DS:96F0-9760:')
for (cs,ip),n in writers.most_common(12):
    s=srcinfo[(cs,ip)]
    txt=''.join(chr(c) if 32<=c<127 else ('|' if c==0xff else '.') for c in s['src'])
    print(f'  {cs:04X}:{ip:04X} x{n:<5d} SI={s["si"]:04X} DI={s["di"]:04X} DS={s["ds"]:04X}')
    print(f'        src bytes LIVE: {txt}')
print(f'\nexecution path through 0x3FBD..0x4020 (first 60):')
seen=[]
for a in path:
    if not seen or seen[-1]!=a: seen.append(a)
print('  '+' -> '.join(f'{a:04X}' for a in seen[:60]))
buf=bytes(mu.mem_read(DS*16+0x96F0,0x60))
print('\nprompt buffer contents:')
print(' ',''.join(chr(c) if 32<=c<127 else ('|' if c==0xff else '.') for c in buf))
