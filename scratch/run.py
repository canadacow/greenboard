"""Pirates! booter -- working driver.

THE BLOCKER, found by tracing (not guessing):
  At 0050:128D the game installs its own tick handler at IVT vector 0x1C
  (di=0x70 = 0x1C*4). That ISR (0050:12A0) decrements five countdown timers:
  [0x3c02] [0x3c04] [0x3c06] [0x3c08] [0x3c0a].
  The menu loop at 0050:07DA waits for [0x3c08] to reach 0 before it will even
  read a key, and 0050:357E waits on [0x3c02].
  A pure CPU emulator never fires IRQ0/INT 1Ch, so those counters never
  decrement and the game waits forever at the drive-config screen.

FIX: deliver INT 1Ch periodically as a real hardware interrupt (push
FLAGS/CS/IP, vector through the IVT, clear IF) at ~18.2 Hz of emulated time.
"""
import sys, struct, time, os, hashlib, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

import shutil
shutil.rmtree('scratch/out/run', ignore_errors=True)
os.makedirs('scratch/out/run',exist_ok=True)
D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 200_000_000
MAXSEC=int(sys.argv[2]) if len(sys.argv)>2 else 300
TICK=25_000          # instructions per timer tick

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0,b'\xCC'*0x100000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0xB8000,b'\x00'*0x8000); mu.mem_write(0xA0000,b'\x00'*0x10000)
# Equipment word: bit0=floppy present, bits7-6 = (num drives - 1).
# 0x0061 => TWO floppy drives. Was 0x0021 (one drive), which contradicted the
# "2 FLOPPY DRIVES" menu choice and left disk 2 unreachable in B:.
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
mode=[4]; shot=[0]; last=['']
def grab(uc):
    fb=bytes(uc.mem_read(0xB8000,0x4000))
    img=Image.new('RGB',(320,200)); px=img.load()
    for y in range(200):
        base=(0x2000 if y&1 else 0)+(y>>1)*80
        for xb in range(80):
            b=fb[base+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    return img,hashlib.md5(fb).hexdigest()
def snap(uc,tag):
    img,h=grab(uc); shot[0]+=1
    nm=f'scratch/out/run/{shot[0]:03d}_{tag}.png'
    img.resize((640,400),Image.NEAREST).save(nm)
    print(f'  [snap {nm}]',flush=True); last[0]=h; return nm

# --- screen text capture ---------------------------------------------------
# Mode 4 has no BIOS text, so the game blits its own glyphs. The character
# renderer entry is 0050:0A00 (AL = char code, font at si=0xA080), which for
# CGA dispatches to the blitter at 0050:12E8. Cursor col/row live in
# [0x3b78]/[0x3b7a]. Hooking the blitter gives us the exact text on screen.
GLYPH_BLIT = 0x50*16 + 0x12E8
screen_chars=[]          # (row, col, char)
def on_glyph(uc):
    al=uc.reg_read(UC_X86_REG_AX)&0xFF
    try:
        col=struct.unpack('<H',uc.mem_read(0x50*16+0x3b78,2))[0]
        row=struct.unpack('<H',uc.mem_read(0x50*16+0x3b7a,2))[0]
    except Exception:
        return
    if 32<=al<127:
        screen_chars.append((row,col,chr(al)))
        if len(screen_chars)>4000: del screen_chars[:2000]

def screen_text():
    if not screen_chars: return ''
    return ''.join(c for _,_,c in screen_chars[-800:])

def menu_on_screen(uc):
    t=screen_text()
    return 'CONFIGURATION' in t

# Keys are NOT sent on a fixed schedule -- there are several splash screens,
# so a timed '1' lands on the wrong one. Instead we send SPACE to advance
# splashes until the CONFIG MENU is actually detected on screen, then send
# the config keys, then Enter forever.
CONFIG_KEYS=['1','2','2']
cfg_i=[0]; menu_seen=[False]
si=[0]; pend=[None]
stats=collections.Counter(); t0=time.time()

def do_int(uc,vec):
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    ss=uc.reg_read(UC_X86_REG_SS); sp=(uc.reg_read(UC_X86_REG_SP)-6)&0xFFFF
    uc.mem_write(ss*16+sp,struct.pack('<HHH',ip,cs,fl))
    uc.reg_write(UC_X86_REG_SP,sp)
    nip,ncs=struct.unpack('<HH',uc.mem_read(vec*4,4))
    uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x200)
    uc.reg_write(UC_X86_REG_CS,ncs); uc.reg_write(UC_X86_REG_IP,nip)

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==3:
        stats['int3']+=1; return
    if intno==0x13:
        stats['13']+=1
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF; dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            # A: (drv 0) = pirates_1.img, B: (drv 1) = pirates_2.img
            unit = drv & 1
            im=D[unit]; src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            stats[f'rd_drv{unit}']+=1
            if PROT and unit==0 and cyl==4 and head==0 and sec==1:
                stats['prot']+=1
                nb=max(al,1)*SEC
                buf=bytes(im[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*(nb-SEC)
                if dest+len(buf)<=0x110000: uc.mem_write(dest,buf)
                uc.reg_write(UC_X86_REG_AX,(0x04<<8)|al)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1)
                print('  *** PROTECTION READ -> ND + 0x43 gap fill',flush=True); return
            data=bytes(im[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
            print(f'  INT13 {"AB"[unit]}: c={cyl:2d} h={head} s={sec:2d} n={al:2d} -> {dest:05X}',flush=True)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00:
            if mode[0]!=al:
                mode[0]=al; print(f'  INT10 mode {al:#x}',flush=True); snap(uc,f'mode{al:02x}')
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        if ah in (0x01,0x11):
            stats['peek']+=1
            if pend[0] is None: uc.reg_write(UC_X86_REG_EFLAGS,fl|0x40)
            else:
                uc.reg_write(UC_X86_REG_AX,(0x1C<<8)|ord(pend[0]))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            stats['get']+=1
            c=pend[0] or '\r'; pend[0]=None
            cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
            print(f"  [GET #{stats['get']} -> '{'ENTER' if c==chr(13) else c}' "
                  f"from {cs:04X}:{ip:04X} at {ic[0]//1000000}M]",flush=True)
            snap(uc,f"get{stats['get']}_{'ENTER' if c==chr(13) else c}")
            uc.reg_write(UC_X86_REG_AX,(0x1C<<8)|ord(c))
            return
        uc.reg_write(UC_X86_REG_AX,0); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

hot=collections.Counter()
ic=[0]; nt=[TICK]; nk=[6_000_000]
def hc(uc,addr,size,user):
    ic[0]+=1; n=ic[0]
    if addr==GLYPH_BLIT:
        on_glyph(uc)
    if n>MAX-2_000_000:
        hot[(uc.reg_read(UC_X86_REG_CS),uc.reg_read(UC_X86_REG_IP))]+=1
    if n>=nt[0]:
        nt[0]=n+TICK
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            # Deliver IRQ0 as INT 08h -- the game hooks the REAL timer IRQ
            # (1038:0B80) which increments the frame flag [1038:0AEA] that the
            # main loop spins on, then EOIs the PIC and chains to INT 1Ch.
            # Delivering only INT 1Ch left that flag permanently 0.
            stats['tick']+=1; do_int(uc,0x08); return
    if n>=nk[0]:
        nk[0]=n+6_000_000
        img,h=grab(uc)
        if h!=last[0]: snap(uc,f'{n//1000000}M')
        t=screen_text()
        if t.strip(): print(f'  [screen] {t[-160:]!r}',flush=True)
        if pend[0] is None:
            if not menu_seen[0]:
                if menu_on_screen(uc):
                    menu_seen[0]=True
                    snap(uc,'MENU_DETECTED')
                    print('  [config menu detected -- sending config keys]',flush=True)
                    pend[0]=CONFIG_KEYS[0]; cfg_i[0]=1
                else:
                    pend[0]=' '        # advance splash screens
            elif cfg_i[0]<len(CONFIG_KEYS):
                pend[0]=CONFIG_KEYS[cfg_i[0]]; cfg_i[0]+=1
            else:
                pend[0]='\r'
            k=pend[0]
            print(f"  [key '{'ENTER' if k==chr(13) else ('SPACE' if k==' ' else k)}']",flush=True)
    if (n&0x3FFFFF)==0 and time.time()-t0>MAXSEC:
        print('  [watchdog]',flush=True); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
print(f'=== run: EGA/2drv/kbd, protected={PROT}, INT1Ch every {TICK} instrs ===',flush=True)
try:
    mu.emu_start(0x7C00,0xFFFF0,0,MAX); print('ended (budget)')
except UcError as e:
    print(f'STOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
snap(mu,'final')
print(f"\ninstrs={ic[0]:,} INT13={stats['13']} ticks={stats['tick']} peek={stats['peek']} "
      f"get={stats['get']} prot={stats['prot']} int3={stats['int3']} mode={mode[0]:#x} t={time.time()-t0:.0f}s")
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
_md=Cs(CS_ARCH_X86,CS_MODE_16)
print('\n=== hottest addresses at end (the stall) ===')
for (cs,ip),c in hot.most_common(20):
    lin=(cs*16+ip)&0xFFFFF
    try:
        i=next(_md.disasm(bytes(mu.mem_read(lin,10)),lin),None)
        t=f'{i.mnemonic} {i.op_str}' if i else '?'
    except Exception: t='?'
    print(f'  {cs:04X}:{ip:04X} x{c:<7d} {t}')
