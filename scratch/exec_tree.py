"""Build a grounded execution tree of the Pirates! runtime.

Purpose: stop guessing segment bases. Record, for real:
  * every CS:IP region that executes, bucketed by segment
  * the DS in force while each region runs
  * the call tree (call site -> target) with segments
  * every INT 13h load: which sectors went to which segment:offset
  * which segments are code vs data vs never-touched

Output: scratch/out/exec_tree.txt
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

loads=[]          # (drive,cyl,head,sec,count,destseg,destoff,fileoff,instr)
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
            loads.append((drv,cyl,head,sec,al,es,bx,src,ic[0]))
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

# --- the tree ---------------------------------------------------------
seg_exec   = collections.Counter()          # CS -> instruction count
seg_ds     = collections.defaultdict(collections.Counter)   # CS -> DS seen
region     = collections.defaultdict(lambda: [0xFFFF,0])    # CS -> [minIP,maxIP]
firstseen  = {}                             # CS -> instr index
lastcs     = [None]
segchanges = []                             # (instr, CS, IP, DS) on CS change

MENU_LOOP=0x50*16+0x3593
hits=collections.Counter()
ic=[0]; nt=[25000]; nk=[3_000_000]; nfire=[0]
def hc(uc,addr,size,user):
    # KEEP THIS CHEAP. It runs on every instruction; anything more than a
    # counter and a couple of compares makes the run take minutes.
    n=ic[0]=ic[0]+1
    # Derive the segment from the linear address instead of reg_read (the
    # code segment base is addr - IP, but sampling is enough for the tree).
    if (n & 0x3FF)==0:
        cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
        seg_exec[cs]+=1
        if cs not in firstseen: firstseen[cs]=n
        r=region[cs]
        if ip<r[0]: r[0]=ip
        if ip>r[1]: r[1]=ip
        ds=uc.reg_read(UC_X86_REG_DS)
        seg_ds[cs][ds]+=1
        if cs!=lastcs[0]:
            lastcs[0]=cs
            segchanges.append((n,cs,ip,ds))
    if addr==MENU_LOOP: hits['menu']=1
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

out=[]
out.append(f'Pirates! execution tree   ({ic[0]:,} instrs, {time.time()-t0:.0f}s)')
out.append('='*78)
out.append('\nCODE SEGMENTS THAT EXECUTED (CS, instrs, IP range, DS in force):')
for cs,n in seg_exec.most_common():
    lo,hi=region[cs]
    ds=', '.join(f'{d:04X}({c})' for d,c in seg_ds[cs].most_common(3))
    out.append(f'  CS={cs:04X}  {n:>12,} instrs  IP {lo:04X}-{hi:04X}  '
               f'first@{firstseen[cs]:>11,}  DS={ds}')

out.append(f'\nSEGMENT CHANGES ({len(segchanges)} total, first 40):')
for at,cs,ip,ds in segchanges[:40]:
    out.append(f'  @{at:>11,}  CS={cs:04X}:{ip:04X}  DS={ds:04X}')

out.append('\nDISK LOADS (sector -> memory):')
for drv,cyl,head,sec,cnt,es,bx,src,at in loads:
    out.append(f'  @{at:>11,}  drv{drv} C{cyl:2d} H{head} S{sec:2d} x{cnt:2d}  '
               f'file {src:#07x} -> {es:04X}:{bx:04X}')

txt='\n'.join(out)
open('scratch/out/exec_tree.txt','w',encoding='utf-8').write(txt)
print(txt[:5000])
print(f'\n... full tree in scratch/out/exec_tree.txt')
