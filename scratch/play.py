"""Pirates! booter -- driven through the real input path.

The game's key routine (0050:0896) is:
    INT 16h AH=01 (peek); ZF -> no key, return 0
    INT 16h AH=00 (get);  AL=ASCII (AH=scancode)
    lowercase a-z folded to uppercase
So: report EMPTY on peek most of the time, and only present a key when we
actually want to send one. Each key is consumed exactly once.

Screens are captured whenever the framebuffer changes materially, so we can
watch the game advance: config menu -> name entry -> map.
"""
import sys, struct, time, os, hashlib
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

os.makedirs('scratch/out/play', exist_ok=True)
D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX_INSTR=int(sys.argv[1]) if len(sys.argv)>1 else 200_000_000
MAX_SECS=300

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0,b'\xCC'*0x100000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0xB8000,b'\x00'*0x8000); mu.mem_write(0xA0000,b'\x00'*0x10000)
mu.mem_write(0x410,struct.pack('<H',0x0021)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
mode=[4]; shot=[0]; lasthash=['']
def grab(uc):
    fb=bytes(uc.mem_read(0xB8000,0x4000))
    img=Image.new('RGB',(320,200)); px=img.load()
    for y in range(200):
        base=(0x2000 if y&1 else 0)+(y>>1)*80
        for xb in range(80):
            b=fb[base+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    return img, hashlib.md5(fb).hexdigest()
def snap(uc,tag):
    img,h=grab(uc)
    shot[0]+=1
    nm=f'scratch/out/play/{shot[0]:03d}_{tag}.png'
    img.resize((640,400),Image.NEAREST).save(nm)
    print(f'  [snap {nm}]',flush=True)
    lasthash[0]=h
    return nm

# EGA(3), 2 drives(2), keyboard(2), then Enter to proceed
SCRIPT=list('322')+['\r']*30
si=[0]; pending=[None]; sent=[0]
stats={'13':0,'peek':0,'get':0,'prot':0}
t0=time.time()
# release a key every N instructions so the game has time to redraw
NEXT=[4_000_000]; GAP=4_000_000

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==3: return
    if intno==0x13:
        stats['13']+=1
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF; dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            img=D.get(drv&1,D[0]); src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            if PROT and (drv&1)==0 and cyl==4 and head==0 and sec==1:
                stats['prot']+=1
                nb=max(al,1)*SEC
                buf=bytes(img[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*(nb-SEC)
                if dest+len(buf)<=0x110000: uc.mem_write(dest,buf)
                uc.reg_write(UC_X86_REG_AX,(0x04<<8)|al)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1)
                print('  *** PROTECTION READ -> ND + gap fill',flush=True); return
            data=bytes(img[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
            print(f'  INT13 drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d}',flush=True)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00:
            if mode[0]!=al:
                mode[0]=al; print(f'  INT10 mode {al:#x}',flush=True)
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        if ah in (0x01,0x11):
            stats['peek']+=1
            if pending[0] is None:
                uc.reg_write(UC_X86_REG_EFLAGS, fl|0x40)   # ZF=1: empty
            else:
                c=pending[0]
                uc.reg_write(UC_X86_REG_AX,(0x1C<<8)|ord(c))
                uc.reg_write(UC_X86_REG_EFLAGS, fl&~0x40)
            return
        if ah in (0x00,0x10):
            stats['get']+=1
            c=pending[0] if pending[0] is not None else '\r'
            pending[0]=None
            uc.reg_write(UC_X86_REG_AX,(0x1C<<8)|ord(c))
            return
        uc.reg_write(UC_X86_REG_AX,0); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

ic=[0]
def hc(uc,addr,size,user):
    ic[0]+=1
    n=ic[0]
    if n>=NEXT[0]:
        NEXT[0]=n+GAP
        img,h=grab(uc)
        if h!=lasthash[0]:
            snap(uc,f'i{n//1000000}M')
        if pending[0] is None and si[0]<len(SCRIPT):
            pending[0]=SCRIPT[si[0]]; si[0]+=1; sent[0]+=1
            k=pending[0]
            print(f"  [key '{k if k!=chr(13) else 'ENTER'}' queued at {n//1000000}M]",flush=True)
    if (n&0x3FFFFF)==0 and time.time()-t0>MAX_SECS:
        print('  [watchdog]',flush=True); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
print(f'=== play (EGA/2drv/kbd, protected={PROT}) ===',flush=True)
try:
    mu.emu_start(0x7C00,0xFFFF0,0,MAX_INSTR); print('ended (budget)')
except UcError as e:
    print(f'STOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
snap(mu,'final')
print(f"\ninstrs={ic[0]:,} INT13={stats['13']} peek={stats['peek']} get={stats['get']} "
      f"prot={stats['prot']} keys={sent[0]} mode={mode[0]:#x} t={time.time()-t0:.0f}s")
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
