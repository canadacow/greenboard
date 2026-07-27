"""Pirates! booter emulation -- realistic keyboard timing.

Previous attempt returned a keystroke on EVERY INT 16h poll (3.3M of them),
which the game rejects/ignores. Real behaviour: the buffer is EMPTY almost
always. AH=01 (peek) must report "no key" (ZF=1) until a key is actually
queued, and AH=00 (blocking get) should only yield after a delay.

Here: a key becomes available every KEY_EVERY polls; between those the
peek reports empty and the blocking read returns the pending key.
"""
import sys, struct, time, os
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

os.makedirs('scratch/out/shots', exist_ok=True)
D = {0: bytearray(open('assets/pirates_1.img','rb').read()),
     1: bytearray(open('assets/pirates_2.img','rb').read())}
SEC, SPT, HEADS = 512, 9, 2
PROT = D[0][0x200:0x208] == b'0-PIRATE'
MAX_INSTR = int(sys.argv[1]) if len(sys.argv)>1 else 80_000_000
MAX_SECS = 180
KEY_EVERY = 12000          # polls between keystrokes

mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, bytes(D[0][:512]))
mu.mem_write(0x410, struct.pack('<H',0x0021))
mu.mem_write(0x413, struct.pack('<H',640))
mu.mem_write(0x465, bytes([0x29]))
mu.mem_write(0xFFFFE, bytes([0xFE]))

CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
shot=[0]
mode=[4]
def screenshot(uc, tag):
    shot[0]+=1
    nm=f'scratch/out/shots/{shot[0]:02d}_{tag}_m{mode[0]:02x}.png'
    fb=bytes(uc.mem_read(0xB8000,0x4000))
    img=Image.new('RGB',(320,200)); px=img.load()
    for y in range(200):
        base=(0x2000 if y&1 else 0)+(y>>1)*80
        for xb in range(80):
            b=fb[base+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    img.resize((640,400),Image.NEAREST).save(nm)
    print(f'  [shot {nm}]',flush=True)
    return nm

KEYS=[(0x04,0x33),(0x03,0x32),(0x03,0x32)]+[(0x1C,0x0D)]*80
kq=list(KEYS)
pending=[None]
polls=[0]
stats={'13':0,'16':0,'prot':0,'keys':0}
t0=time.time()

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==0x13:
        stats['13']+=1
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF; dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            img=D.get(drv&1,D[0])
            if PROT and (drv&1)==0 and cyl==4 and head==0 and sec==1:
                stats['prot']+=1
                nb=max(al,1)*SEC; src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
                buf=bytes(img[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*(nb-SEC)
                if dest+len(buf)<=0x110000: uc.mem_write(dest,buf)
                uc.reg_write(UC_X86_REG_AX,(0x04<<8)|al)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1)
                print(f'  *** PROTECTION read n={al} -> ND/CF=1 + gap fill',flush=True)
                return
            src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            data=bytes(img[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
            print(f'  INT13 rd drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d} f=0x{src:06x}',flush=True)
        uc.reg_write(UC_X86_REG_AX,al)
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)
        return
    if intno==0x10:
        if ah==0x00:
            if mode[0]!=al: mode[0]=al; print(f'  INT10 mode {al:#x}',flush=True)
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)
        return
    if intno==0x16:
        stats['16']+=1; polls[0]+=1
        if pending[0] is None and polls[0]>=KEY_EVERY:
            polls[0]=0
            if kq:
                pending[0]=kq.pop(0); stats['keys']+=1
                print(f"  [key #{stats['keys']} -> {pending[0][1]:#04x}]",flush=True)
                screenshot(uc,f'beforekey{stats["keys"]}')
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        if ah in (0x01,0x11):
            if pending[0] is None:
                uc.reg_write(UC_X86_REG_EFLAGS, fl|0x40)     # ZF=1 empty
            else:
                sc,asc=pending[0]
                uc.reg_write(UC_X86_REG_AX,(sc<<8)|asc)
                uc.reg_write(UC_X86_REG_EFLAGS, fl&~0x40)
            return
        if ah in (0x00,0x10):
            while pending[0] is None:
                if kq: pending[0]=kq.pop(0); stats['keys']+=1
                else: pending[0]=(0x1C,0x0D)
            sc,asc=pending[0]; pending[0]=None; polls[0]=0
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|asc)
            return
        uc.reg_write(UC_X86_REG_AX,0)
        return
    if intno==0x1A:
        t=int((time.time()-t0)*18.2)&0xFFFFFF
        uc.reg_write(UC_X86_REG_CX,(t>>16)&0xFFFF); uc.reg_write(UC_X86_REG_DX,t&0xFFFF)
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:0,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)
ic=[0]
def hc(uc,a,s,u):
    ic[0]+=1
    if (ic[0]&0xFFFFF)==0 and time.time()-t0>MAX_SECS:
        print('  [watchdog]',flush=True); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
print(f'=== boot: EGA/2drv/kbd, protected={PROT} ===',flush=True)
try:
    mu.emu_start(0x7C00,0xFFFF0,0,MAX_INSTR); print('ended')
except UcError as e:
    print(f'STOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
screenshot(mu,'final')
print(f'\ninstrs={ic[0]:,} INT13={stats["13"]} INT16={stats["16"]} keys={stats["keys"]} prot={stats["prot"]} mode={mode[0]:#x} t={time.time()-t0:.0f}s')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
