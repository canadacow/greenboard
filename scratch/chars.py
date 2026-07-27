"""Character blitter is at 0050:0A7D (writes es:[di], +0x2000, +0x50 ... at
0A9D onward). Hook its ENTRY and log AL = character code, so we can see the
text the game prints, and drive the menu off real screen content."""
import sys, struct, time, os, shutil, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

shutil.rmtree('scratch/out/chars', ignore_errors=True)
os.makedirs('scratch/out/chars', exist_ok=True)
D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 80_000_000

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
    img.resize((640,400),Image.NEAREST).save(f'scratch/out/chars/{shots[0]:03d}_{tag}.png')
    print(f'  [snap {shots[0]:03d}_{tag}]',flush=True)

pend=[None]
# space to clear splashes, then CGA/2drv/keyboard, then Enter
KEYS=['1','2','2']+['\r']*100
ki=[0]
text=[]

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
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        if ah in (0x01,0x11):
            if pend[0] is None: uc.reg_write(UC_X86_REG_EFLAGS,fl|0x40)
            else:
                uc.reg_write(UC_X86_REG_AX,(0x1C<<8)|ord(pend[0]))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            c=pend[0] or '\r'; pend[0]=None
            uc.reg_write(UC_X86_REG_AX,(0x1C<<8)|ord(c)); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

# 0x0A00 is the mode-dispatching char entry (AL=char); it calls 0xa7d / 0x12e8
# / 0xbcd depending on [0x3b98]. 0x0CCE is the second entry. Hook both, plus
# the CGA leaf 0xa7d, so no matter which path draws we see the character.
BLIT_ENTRIES={0x50*16+0x0A00, 0x50*16+0x0CCE}
BLIT=0x50*16+0x0A7D
ic=[0]; nt=[25000]; sent=[0]; menu=[False]
def hc(uc,addr,size,user):
    ic[0]+=1
    if addr==BLIT:
        al=uc.reg_read(UC_X86_REG_AX)&0xFF
        if 32<=al<127:
            text.append(chr(al))
            s=''.join(text[-40:])
            # As soon as the CONFIG menu text appears, start sending keys
            if not menu[0] and 'CONFIGURATION' in ''.join(text[-400:]):
                menu[0]=True
                print(f'  [MENU TEXT SEEN: {"".join(text[-60:])!r}]',flush=True)
                snap(uc,'menu_seen')
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200: do_int(uc,0x08); return
    # Splash/copyright screens wait on "press any key" -- send SPACE until the
    # CONFIGURATION text actually appears, then send the config keys.
    if pend[0] is None and (ic[0]%3_000_000)==0:
        if not menu[0]:
            pend[0]=' '
        elif ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1; sent[0]+=1
            k=pend[0]
            print(f"  [key '{'ENTER' if k==chr(13) else k}'] text={''.join(text[-50:])!r}",flush=True)
            snap(uc,f'key{ki[0]}')
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
snap(mu,'final')
print(f'\ninstrs={ic[0]:,} chars={len(text)} keys={sent[0]} mode={mode[0]:#x} t={time.time()-t0:.0f}s')
print('\n=== ALL TEXT PRINTED ===')
print(''.join(text))
