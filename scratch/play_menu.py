"""Drive the Pirates! menus by writing the key-state bitmap directly.

WHY (traced through the decompiled C, not guessed):
  FUN_0050_3593 is the menu selection loop. It commits a choice with:
        if ((*(uint *)0x3bfa & 0x10) == 0) { [0x93db] = row...; return; }
  i.e. SELECT happens when bit 4 of [0x3bfa] goes LOW.

  [0x3bfa] is produced by the input dispatcher FUN_0050_24bd, which for the
  keyboard device calls FUN_0050_24a5:
        return *(uint *)0x3c0e & 0x1f ^ 0xffff;
  so bit 4 of [0x3bfa] is low exactly when bit 4 of [0x3c0e] is SET.

  [0x3c0e] is the key-state bitmap, written ONLY by the game's INT 09h ISR --
  and that ISR only runs its key-handling path when [0x3c0c]==1, which
  FUN_0050_0592 sets only for the JOYSTICK option. With keyboard selected the
  ISR chains to BIOS, [0x3c0e] stays 0, and no selection can ever be made
  through INT 16h. That is why every Enter was swallowed.

  Fix: poke [0x3c0e] directly. Bit 4 = fire/select, bits 0-3 = direction.
"""
import sys, struct, time, os, shutil, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

OUT='scratch/out/menu'
shutil.rmtree(OUT,ignore_errors=True); os.makedirs(OUT,exist_ok=True)

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 60_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
mode=[4]; shots=[0]
def snap(uc,tag):
    fb=bytes(uc.mem_read(0xB8000,0x4000))
    img=Image.new('RGB',(320,200)); px=img.load()
    for y in range(200):
        base=(0x2000 if y&1 else 0)+(y>>1)*80
        for xb in range(80):
            b=fb[base+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    shots[0]+=1
    img.resize((640,400),Image.NEAREST).save(f'{OUT}/{shots[0]:03d}_{tag}.png')
    print(f'  [snap {shots[0]:03d}_{tag}]',flush=True)

pend=[None]; ki=[0]
# Splash x2, CGA, 2 drives, keyboard -- then the family name. The name prompt
# (FUN_0050_40bd) reads ASCII through INT 16h, unlike the sword menus which
# read the [0x3c0e] bitmap; Enter (0x0d) terminates it.
KEYS=[' ',' ','1','2','2']+list('DRAKE')+['\r']*40

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

DS=0x117B
KEYSTATE=DS*16+0x3c0e
SELBIT=0x10                 # bit 4 = fire/select
MENU_LOOP=0x50*16+0x3593
NAME_ENTRY=0x50*16+0x40bd
fire=[0]; hits=collections.Counter()
ic=[0]; nt=[25000]; nk=[3_000_000]; nfire=[0]
def hc(uc,addr,size,user):
    ic[0]+=1
    if addr==MENU_LOOP:
        hits['menu']+=1
        if hits['menu']==1:
            print(f'  menu loop FUN_0050_3593 entered at {ic[0]//1000000}M',flush=True)
            snap(uc,'menu_entered')
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C); return
    if addr==NAME_ENTRY and not hits['name']:
        hits['name']=1
        print(f'  *** NAME ENTRY (FUN_0050_40bd) at {ic[0]//1000000}M',flush=True)
        snap(uc,'name_entry')
    if ic[0]>=nk[0]:
        nk[0]=ic[0]+3_000_000
        # Config keys flow immediately; the name characters only once the
        # name prompt is actually running.
        if pend[0] is None and ki[0]<len(KEYS):
            if ki[0]<5 or hits['name']:
                pend[0]=KEYS[ki[0]]; ki[0]+=1
    # Once the menu loop is running, pulse the SELECT bit in the key-state
    # bitmap: hold it for a while, then release, like a real key press.
    if hits['menu'] and ic[0]>=nfire[0]:
        nfire[0]=ic[0]+2_000_000
        fire[0]^=1
        try:
            cur=struct.unpack('<H',uc.mem_read(KEYSTATE,2))[0]
            new=(cur|SELBIT) if fire[0] else (cur&~SELBIT)
            uc.mem_write(KEYSTATE,struct.pack('<H',new))
            if fire[0]:
                print(f'  [0x3c0e] |= 0x10  (SELECT) at {ic[0]//1000000}M',flush=True)
                snap(uc,f'fire{ic[0]//1000000}M')
        except Exception: pass
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
snap(mu,'final')
print(f'\ninstrs={ic[0]:,} menu_loop_hits={hits["menu"]} t={time.time()-t0:.0f}s')
ds=mu.reg_read(UC_X86_REG_DS)
for nm,off in (('[0x93db] selection',0x93db),('[0x3c0e] keystate',0x3c0e),
               ('[0x3bfa] input',0x3bfa),('[0x463f] res byte',0x463f)):
    try:
        v=struct.unpack('<H',mu.mem_read(DS*16+off,2))[0]
        print(f'  {nm:22s} = {v:#06x}')
    except Exception: pass
