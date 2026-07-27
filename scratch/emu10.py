"""Pirates! booter emulation: EGA + 2 drives + keyboard, with screenshots.

Config keys sent at the setup menu:
  GRAPHICS  -> '3' (EGA)
  DRIVE     -> '2' (2 floppy drives; disk1 in A:, disk2 in B:)
  CONTROL   -> '2' (keyboard)

Copy protection (per src/isa/isa_fdc.cpp): disk 1 is labelled
"0-PIRATE GAME DISK" at 0x200 and track C=4 H=0 is misformatted. A read of
C=4 H=0 S=1 must stream 512B of sector data + 0x43 gap filler into the
buffer and THEN fail with ND (ST1=0x04 -> INT13 AH=0x04, CF=1).

Screenshots are dumped from the CGA framebuffer (mode 4) or EGA planes
(mode 0Dh/0Eh at A000) whenever the video mode changes and at the end.
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

MAX_INSTR = int(sys.argv[1]) if len(sys.argv) > 1 else 60_000_000
MAX_SECS  = 120

mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, bytes(D[0][:512]))
mu.mem_write(0x410, struct.pack('<H', 0x0021))
mu.mem_write(0x413, struct.pack('<H', 640))
mu.mem_write(0x465, bytes([0x29]))
mu.mem_write(0xFFFFE, bytes([0xFE]))

CGA_PAL = [(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
EGA_PAL = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
           (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
shot_n = [0]

def screenshot(uc, tag):
    m = mode[0]
    shot_n[0] += 1
    name = f'scratch/out/shots/{shot_n[0]:02d}_{tag}_m{m:02x}.png'
    try:
        if m in (0x04, 0x05, 0x06):
            fb = bytes(uc.mem_read(0xB8000, 0x4000))
            img = Image.new('RGB', (320,200)); px = img.load()
            for y in range(200):
                base = (0x2000 if y & 1 else 0) + (y>>1)*80
                for xb in range(80):
                    b = fb[base+xb]
                    for p in range(4):
                        px[xb*4+p, y] = CGA_PAL[(b >> (6-2*p)) & 3]
            img.resize((640,400), Image.NEAREST).save(name)
        elif m in (0x0D, 0x0E, 0x10):
            w, h = (320,200) if m == 0x0D else (640,200) if m == 0x0E else (640,350)
            # Unicorn has flat RAM: planes aren't separable. Read A000 linearly
            # as plane 0 only -- enough to see structure.
            fb = bytes(uc.mem_read(0xA0000, (w//8)*h))
            img = Image.new('RGB', (w,h)); px = img.load()
            bpr = w//8
            for y in range(h):
                for xb in range(bpr):
                    b = fb[y*bpr+xb]
                    for bit in range(8):
                        v = 255 if (b >> (7-bit)) & 1 else 0
                        px[xb*8+bit, y] = (v,v,v)
            img.save(name)
        else:
            fb = bytes(uc.mem_read(0xB8000, 4000))
            img = Image.new('RGB', (80*8, 25*16), (0,0,0))
            img.save(name)
            txt = ''.join(chr(fb[i]) if 32 <= fb[i] < 127 else ' ' for i in range(0, 4000, 2))
            print('  [text screen]')
            for r in range(25):
                line = txt[r*80:(r+1)*80].rstrip()
                if line: print(f'   |{line}')
        print(f'  [screenshot {name}]', flush=True)
    except Exception as e:
        print('  screenshot failed:', e)

# 3=EGA, 2=two drives, 2=keyboard, then Enters
KEYS = [(0x04,0x33), (0x03,0x32), (0x03,0x32)] + [(0x1C,0x0D)]*60
kq = list(KEYS)
mode, stats = [4], {'13':0,'16':0,'prot':0}
t0 = time.time()

def hook_intr(uc, intno, user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    cf=0
    if intno==0x13:
        stats['13']+=1
        cx=uc.reg_read(UC_X86_REG_CX); dx=uc.reg_read(UC_X86_REG_DX)
        bx=uc.reg_read(UC_X86_REG_BX); es=uc.reg_read(UC_X86_REG_ES)
        cyl=((cx>>8)&0xFF)|((cx&0xC0)<<2); sec=cx&0x3F
        head=(dx>>8)&0xFF; drv=dx&0xFF
        dest=(es*16+bx)&0xFFFFF
        if ah==0x02:
            img = D.get(drv & 1, D[0])
            if PROT and (drv&1)==0 and cyl==4 and head==0 and sec==1:
                stats['prot']+=1
                nb = max(al,1)*SEC
                src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
                buf = bytes(img[src:src+SEC]).ljust(SEC,b'\x00') + b'\x43'*(nb-SEC)
                if dest+len(buf)<=0x110000: uc.mem_write(dest, buf)
                uc.reg_write(UC_X86_REG_AX,(0x04<<8)|al)
                uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)|1)
                print(f'  *** PROTECTION read n={al}: 512B + {nb-SEC}B of 0x43, ND/CF=1', flush=True)
                return
            src=((cyl*HEADS+head)*SPT+(sec-1))*SEC
            data=bytes(img[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
            print(f'  INT13 rd drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d} file=0x{src:06x}', flush=True)
            uc.reg_write(UC_X86_REG_AX,al)
        elif ah==0x03:
            uc.reg_write(UC_X86_REG_AX,al)
        else:
            uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00:
            old=mode[0]; mode[0]=al
            print(f'  INT10 video mode {al:#x}', flush=True)
            if old!=al: screenshot(uc, 'modechange')
        elif ah==0x0F:
            uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        stats['16']+=1
        if stats['16'] in (1, 3000, 60000, 200000):
            screenshot(uc, f'wait{stats["16"]}')
        sc,asc = kq.pop(0) if kq else (0x1C,0x0D)
        if ah in (0x00,0x10): uc.reg_write(UC_X86_REG_AX,(sc<<8)|asc)
        elif ah in (0x01,0x11):
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|asc)
            uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)&~0x40); return
    elif intno==0x1A:
        t=int((time.time()-t0)*18.2)&0xFFFFFF
        uc.reg_write(UC_X86_REG_CX,(t>>16)&0xFFFF); uc.reg_write(UC_X86_REG_DX,t&0xFFFF)
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    uc.reg_write(UC_X86_REG_EFLAGS,(fl|1) if cf else (fl&~1))

mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d:0, None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d:None, None,1,0,UC_X86_INS_OUT)
ic=[0]
def hook_code(uc,addr,size,user):
    ic[0]+=1
    if (ic[0]&0xFFFFF)==0 and time.time()-t0>MAX_SECS:
        print('  [watchdog]',flush=True); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook_code)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)

print(f'=== booting: EGA / 2 drives / keyboard  (protected={PROT}) ===', flush=True)
try:
    mu.emu_start(0x7C00,0xFFFF0,0,MAX_INSTR); print('ended')
except UcError as e:
    print(f'STOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
screenshot(mu,'final')
print(f'\ninstrs={ic[0]:,} INT13={stats["13"]} INT16={stats["16"]} prot={stats["prot"]} mode={mode[0]:#x} t={time.time()-t0:.1f}s')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
