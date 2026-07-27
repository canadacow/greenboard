"""Dump Pirates! graphics by running the game and hooking its own decoder.

Rather than guessing where compressed images live on disk, let the game find
them: boot under Unicorn, hook decode_image_rect (1000:1bf9), and after each
call snapshot the decoded rectangle straight out of the game's own bitmap.

The image data is 4bpp -- 16 colours natively -- so we write 16-colour PNGs
using the EGA palette rather than the 4-colour CGA the display path uses.
"""
import sys, struct, time, os, shutil, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

OUT='pirates'
os.makedirs(OUT,exist_ok=True)

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 120_000_000

EGA=[(0x00,0x00,0x00),(0x00,0x00,0xAA),(0x00,0xAA,0x00),(0x00,0xAA,0xAA),
     (0xAA,0x00,0x00),(0xAA,0x00,0xAA),(0xAA,0x55,0x00),(0xAA,0xAA,0xAA),
     (0x55,0x55,0x55),(0x55,0x55,0xFF),(0x55,0xFF,0x55),(0x55,0xFF,0xFF),
     (0xFF,0x55,0x55),(0xFF,0x55,0xFF),(0xFF,0xFF,0x55),(0xFF,0xFF,0xFF)]

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

mode=[4]; pend=[None]; ki=[0]
KEYS=[' ',' ','1','2','2']+['\r']*80

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
                sc=0x1C if pend[0]=='\r' else 0x39
                uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(pend[0]))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            c=pend[0] or '\r'; pend[0]=None
            sc=0x1C if c=='\r' else 0x39
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(c)); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

# decode_image_rect is at 1000:1bf9. Physical = 0x1000*16 + 0x1bf9.
DECODE=0x1000*16+0x1bf9
saved=[0]; seen=set()

def save_rect(uc,tag,x,y,w,h):
    """Pull the decoded rectangle out of the game's 4bpp bitmap.
    Row addresses come from the row table at DS:0x3e31 (2 bytes/row)."""
    ds=uc.reg_read(UC_X86_REG_DS)
    try:
        rows=struct.unpack('<200H',uc.mem_read(ds*16+0x3e31,400))
    except Exception:
        return
    px=bytearray(w*h); i=0; nonzero=0
    for r in range(h):
        yy=y+r
        if yy>=200: break
        base=(ds*16+rows[yy])&0xFFFFF
        for c in range(w):
            xx=x+c
            try:
                b=uc.mem_read(base+(xx>>1),1)[0]
            except Exception:
                b=0
            v=(b>>4) if (xx&1)==0 else (b&0x0f)
            px[i]=v; i+=1
            if v: nonzero+=1
    if nonzero<w*h//20:     # essentially blank
        return
    img=Image.new('P',(w,h)); img.putdata(bytes(px))
    pal=[]
    for c in EGA: pal+=list(c)
    img.putpalette(pal+[0]*(768-len(pal)))
    saved[0]+=1
    nm=f'{OUT}/{saved[0]:03d}_{tag}_{w}x{h}.png'
    img.resize((w*3,h*3),Image.NEAREST).convert('RGB').save(nm)
    print(f'  [{saved[0]:03d}] decode_image_rect x={x} y={y} {w}x{h} -> {nm}',flush=True)

# When decode_image_rect is entered, record its args and the return address.
# When execution reaches that return address, the bitmap is fully written --
# that is the moment to snapshot the rectangle.
pending=[]          # list of (ret_linear, x, y, w, h)
calls=[]
ic=[0]; nt=[25000]; nk=[3_000_000]
def hc(uc,addr,size,user):
    ic[0]+=1
    if addr==DECODE:
        ss=uc.reg_read(UC_X86_REG_SS); sp=uc.reg_read(UC_X86_REG_SP)
        try:
            ret=struct.unpack('<H',uc.mem_read(ss*16+sp,2))[0]
            x,y,w,h=struct.unpack('<4H',uc.mem_read(ss*16+sp+2,8))
        except Exception:
            return
        cs=uc.reg_read(UC_X86_REG_CS)
        calls.append((x,y,w,h))
        if 0<w<=320 and 0<h<=200:
            pending.append(((cs*16+ret)&0xFFFFF,x,y,w,h))
    if pending and addr==pending[-1][0]:
        _,x,y,w,h=pending.pop()
        key=(x,y,w,h)
        if key not in seen:
            seen.add(key)
            save_rect(uc,f'x{x}y{y}',x,y,w,h)
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C); return
    if ic[0]>=nk[0]:
        nk[0]=ic[0]+3_000_000
        if pend[0] is None and ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
print(f'booting (hook decode_image_rect at {DECODE:05X})...',flush=True)
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
print(f'\ninstrs={ic[0]:,} decode calls seen={len(calls)} saved={saved[0]} t={time.time()-t0:.0f}s')
