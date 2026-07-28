"""Find the fleet/train schedule table.

Known: the quiz prompt is composed into DS:96F3 as
    "<CITY> in <YEAR>, MR. <NAME>"
and the ANSWER menu shows a month + Early/Late. The correct answer must be
read from a table. Strategy: break at the check (0050:3FBD), then dump every
memory READ the code performs in the window just before it, and report which
source addresses were touched. The schedule table is whatever it read.
"""
import sys, struct, time, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 50_000_000
DS=0x117B

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

# Record data READS while the prompt is being built (0050:3F58 .. 0050:3FBD).
arm=[False]
reads=collections.Counter()
def hook_rd(uc,access,addr,size,value,user):
    if arm[0]:
        reads[addr]+=1
# NOTE: a global MEM_READ hook fires on every data read and slows the run to
# a crawl (it halted at 16M instead of reaching the quiz at ~44M). Restrict it
# to the data segment only, and enable it lazily once the prompt build starts.
mu.hook_add(UC_HOOK_MEM_READ,hook_rd,None,DS*16,DS*16+0xFFFF)

BUILD=0x50*16+0x3f58
CHECK=0x50*16+0x3fbd
INTERP=0x50*16+0x3ddf
MENU=0x50*16+0x3593
hits=collections.Counter(); done=[False]
ic=[0]; nt=[25000]; nk=[3_000_000]; nfire=[0]
def hc(uc,addr,size,user):
    n=ic[0]=ic[0]+1
    if addr==MENU: hits['menu']=1
    # 0050:3F58 runs on many paths long before the quiz. Only start recording
    # once the script interpreter (0050:3DDF) is actually running, and clear
    # the log on each new build so we keep only the final, quiz-time pass.
    if addr==INTERP: hits['interp']=1
    if addr==BUILD and hits['interp'] and not done[0]:
        reads.clear(); arm[0]=True
    if addr==CHECK and arm[0] and not done[0]:
        arm[0]=False; done[0]=True
        hits['at']=n
        uc.emu_stop(); return
    if n>=nt[0]:
        nt[0]=n+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C); return
    if n>=nk[0]:
        nk[0]=n+3_000_000
        if pend[0] is None and ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1
    if hits['menu'] and n>=nfire[0]:
        nfire[0]=n+2_000_000
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
print(f'instrs={ic[0]:,} t={time.time()-t0:.0f}s  armed_window_hit={done[0]}')

if not reads:
    print('no reads captured'); sys.exit(0)

# Group the read addresses into contiguous regions
addrs=sorted(reads)
groups=[]; start=prev=addrs[0]
for a in addrs[1:]:
    if a-prev>32:
        groups.append((start,prev)); start=a
    prev=a
groups.append((start,prev))
print(f'\n{len(reads)} distinct addresses read while composing the prompt:')
for lo,hi in groups:
    n=sum(reads[a] for a in addrs if lo<=a<=hi)
    off_lo=lo-DS*16; off_hi=hi-DS*16
    print(f'  {lo:#07x}-{hi:#07x}  (DS:{off_lo:04X}-{off_hi:04X})  {hi-lo+1:5d} bytes, {n} reads')
    raw=bytes(mu.mem_read(lo,min(hi-lo+1,64)))
    txt=''.join(chr(c^0xe0) if c>=0x80 else (chr(c) if 32<=c<127 else '.') for c in raw)
    print(f'      {raw[:32].hex()}')
    print(f'      {txt}')

# The read log showed a pointer table with a constant 0x2D stride around
# DS:0BB0 whose targets contain the narrative text -- dump those records.
print('\n=== quiz state at the check ===')
for nm,off,sz in (('[0x47c0] question sel',0x47c0,1),
                  ('[0x9a1f] era index',0x9a1f,2),
                  ('[0x15a3] expected',0x15a3,2),
                  ('[0x4740] answer acc',0x4740,1),
                  ('[0x473d] penalty',0x473d,1)):
    try:
        raw=bytes(mu.mem_read(DS*16+off,sz))
        v=raw[0] if sz==1 else struct.unpack('<H',raw)[0]
        print(f'  {nm:24s} = {v:#06x} ({v})')
    except Exception: pass

print('\n=== 45-byte records at DS:0BE3 (stride 0x2D) ===')
for i in range(20):
    off=0x0be3+i*0x2d
    raw=bytes(mu.mem_read(DS*16+off,0x2d))
    txt=''.join(chr(c^0xe0) if c>=0x80 else (chr(c) if 32<=c<127 else '.') for c in raw)
    print(f'  [{i:2d}] DS:{off:04X}  {raw[:16].hex()}  {txt[:44]}')
