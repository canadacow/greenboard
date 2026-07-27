"""Is the name-entry routine even reached, and what key does it see?

FUN_0050_40bd (0050:40bd) is the "What is your family name?" reader:
    [0x3c0c] = 0                      keyboard mode off while typing
    loop: c = FUN_0050_0860()         read one key
          if c == 0x08  -> backspace
          if c == 0x0d  -> [0x3c0c]=1; return     <-- ENTER IS ACCEPTED
          if 0x20 <= c <= 0x5f and len < 9 -> append
So Enter works here. Hook it, plus FUN_0050_0860's return, and log what
characters actually arrive.
"""
import sys, struct, time, os, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 120_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

mode=[4]; pend=[None]; ki=[0]
NAME='DRAKE'
KEYS=[' ',' ','1','2','2']+['\r']*10+list(NAME)+['\r']*80

def do_int(uc,vec):
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    ss=uc.reg_read(UC_X86_REG_SS); sp=(uc.reg_read(UC_X86_REG_SP)-6)&0xFFFF
    uc.mem_write(ss*16+sp,struct.pack('<HHH',ip,cs,fl)); uc.reg_write(UC_X86_REG_SP,sp)
    nip,ncs=struct.unpack('<HH',uc.mem_read(vec*4,4))
    uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x200)
    uc.reg_write(UC_X86_REG_CS,ncs); uc.reg_write(UC_X86_REG_IP,nip)

i16get=[0]
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
                sc=0x1C if pend[0]=='\r' else 0x39
                uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(pend[0]))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            c=pend[0] or '\r'; pend[0]=None
            i16get[0]+=1
            sc=0x1C if c=='\r' else 0x39
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(c)); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

# Trace the whole chain from main down to name entry:
#   FUN_0050_0080 (main) -> FUN_1000_0e99-check -> FUN_0050_36e5 (game loop)
#   -> FUN_0050_3760 (new career) -> FUN_0050_3939 / FUN_0050_40bd (name)
WATCH={0x50*16+0x0080:'main_0080',
       0x50*16+0x04e0:'startup_04e0',
       0x50*16+0x0592:'config_menu_0592',
       0x50*16+0x044a:'FUN_044a',
       0x50*16+0x36e5:'game_loop_36e5',
       0x50*16+0x3760:'new_career_3760',
       0x50*16+0x3939:'FUN_3939',
       0x50*16+0x40bd:'NAME_ENTRY_40bd',
       0x50*16+0x0860:'key_0860',
       0x50*16+0x2820:'load_resource',
       0x50*16+0x28be:'disk_read_int13'}
NAME_ENTRY=0x50*16+0x40bd
KEY_0860  =0x50*16+0x0860
hits=collections.Counter()
resload=[]
chars=[]
ic=[0]; nt=[25000]; nk=[3_000_000]
def hc(uc,addr,size,user):
    ic[0]+=1
    if addr in WATCH:
        nm=WATCH[addr]
        hits[nm]+=1
        if nm=='load_resource':
            ax=uc.reg_read(UC_X86_REG_AX)
            ds=uc.reg_read(UC_X86_REG_DS)
            err=0
            try: err=uc.mem_read(ds*16+0x11a3,1)[0]
            except Exception: pass
            resload.append((ic[0],ax))
            if len(resload)<=40:
                print(f'  load_resource(idx={ax:#06x}) at {ic[0]//1000000:3d}M '
                      f'lasterr={err:#04x}',flush=True)
        elif hits[nm]==1:
            print(f'  first hit: {nm:20s} at {ic[0]//1000000:3d}M instrs',flush=True)
            if nm=='new_career_3760':
                # Read the gating vars with the DS the GAME is using here,
                # not the DS left over inside the timer ISR at the end.
                ds=uc.reg_read(UC_X86_REG_DS)
                print(f'    DS={ds:04X} at new_career entry:',flush=True)
                for vn,off,sz in (('[0x463f]',0x463f,1),('[0x9a21]',0x9a21,2),
                                  ('[0x4640]',0x4640,1),('[0x9a1f]',0x9a1f,2)):
                    try:
                        raw=bytes(uc.mem_read(ds*16+off,sz))
                        v=raw[0] if sz==1 else struct.unpack('<H',raw)[0]
                        print(f'      {vn} = {v}',flush=True)
                    except Exception: pass
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C); return
    if ic[0]>=nk[0]:
        nk[0]=ic[0]+3_000_000
        if pend[0] is None and ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1
hot=collections.Counter()
def hc2(uc,addr,size,user):
    if ic[0]>MAX-1_500_000:
        hot[(uc.reg_read(UC_X86_REG_CS),uc.reg_read(UC_X86_REG_IP))]+=1
mu.hook_add(UC_HOOK_CODE,hc)
mu.hook_add(UC_HOOK_CODE,hc2)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)

print(f'\ninstrs={ic[0]:,} t={time.time()-t0:.0f}s')
print(f'INT16 gets served : {i16get[0]}')
print('\ncall chain reached:')
for a,nm in WATCH.items():
    print(f'  {nm:20s} {hits[nm]:8d}')
ds=mu.reg_read(UC_X86_REG_DS)
try:
    buf=bytes(mu.mem_read(ds*16+0x104b,12))
    print(f'name buffer [0x104b]: {buf!r}')
except Exception: pass

# FUN_0050_3760 spins in:  do { r = rng() } while ([0x9a21] <= (r & 0x1f));
# If [0x9a21] == 0 that can never terminate. It is set from
#     [0x9a21] = *(byte *)0x463f - 3      (after load_resource())
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
_md=Cs(CS_ARCH_X86,CS_MODE_16)
print('\n=== WHERE IT IS SPINNING ===')
for (cs,ip),c in hot.most_common(12):
    lin=(cs*16+ip)&0xFFFFF
    try:
        i=next(_md.disasm(bytes(mu.mem_read(lin,10)),lin),None)
        t=f'{i.mnemonic} {i.op_str}' if i else '?'
    except Exception: t='?'
    print(f'  {cs:04X}:{ip:04X} x{c:<7d} {t}')

print('\nloop-gating variables in FUN_0050_3760:')
for nm,off,sz in (('[0x9a21] rng bound',0x9a21,2),
                  ('[0x463f] source byte',0x463f,1),
                  ('[0x9a91] outer ctr',0x9a91,2),
                  ('[0x9a93] inner ctr',0x9a93,2),
                  ('[0x9a1f] era',0x9a1f,2),
                  ('[0x93db] menu pick',0x93db,2),
                  ('[0x9aa9] input gate',0x9aa9,2)):
    try:
        raw=bytes(mu.mem_read(ds*16+off,sz))
        v=raw[0] if sz==1 else struct.unpack('<H',raw)[0]
        print(f'  {nm:22s} = {v} ({v:#06x})')
    except Exception as e:
        print(f'  {nm:22s} unreadable')
