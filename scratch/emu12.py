"""Pirates! booter emulation with hardware IRQ injection.

Diagnosis: the game reads NO I/O ports and ignores INT 16h -- it installs its
own IRQ1 (INT 09h) keyboard ISR and IRQ0 (INT 08h) timer ISR, then spins on
flags those ISRs set. A pure instruction emulator never delivers interrupts,
so the game waits forever at the config menu.

Fix: periodically simulate a hardware interrupt by hand --
  push FLAGS, CS, IP;  load CS:IP from the IVT slot;  clear IF.
The game's own ISR then runs, reads the scancode (we feed port 0x60), and
sets its internal flag. We drive INT 08h (timer, 18.2Hz) and INT 09h
(keyboard) this way.
"""
import sys, struct, time, os
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

os.makedirs('scratch/out/shots', exist_ok=True)
D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX_INSTR=int(sys.argv[1]) if len(sys.argv)>1 else 120_000_000
MAX_SECS=240

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0x410,struct.pack('<H',0x0021)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))

CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
mode=[4]; shot=[0]
def screenshot(uc,tag):
    shot[0]+=1
    nm=f'scratch/out/shots/x{shot[0]:02d}_{tag}.png'
    fb=bytes(uc.mem_read(0xB8000,0x4000))
    img=Image.new('RGB',(320,200)); px=img.load()
    for y in range(200):
        base=(0x2000 if y&1 else 0)+(y>>1)*80
        for xb in range(80):
            b=fb[base+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    img.resize((640,400),Image.NEAREST).save(nm)
    print(f'  [shot {nm}]',flush=True); return nm

# scancodes: '3' EGA, '2' two drives, '2' keyboard, then Enter/space
SCANS=[0x04,0x03,0x03]+[0x1C,0x39]*40
scan_i=[0]
kbd_data=[0x00]
stats={'13':0,'irq0':0,'irq1':0,'prot':0}
t0=time.time()

def do_int(uc, vec):
    """Simulate hardware interrupt delivery: push flags/cs/ip, jump via IVT."""
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    sp=uc.reg_read(UC_X86_REG_SP); ss=uc.reg_read(UC_X86_REG_SS)
    sp=(sp-6)&0xFFFF
    uc.mem_write(ss*16+sp, struct.pack('<HHH', ip, cs, fl))
    uc.reg_write(UC_X86_REG_SP,sp)
    nip,ncs=struct.unpack('<HH', uc.mem_read(vec*4,4))
    if ncs==0 and nip==0:
        uc.reg_write(UC_X86_REG_SP,(sp+6)&0xFFFF); return False
    uc.reg_write(UC_X86_REG_EFLAGS, fl & ~0x200)
    uc.reg_write(UC_X86_REG_CS,ncs); uc.reg_write(UC_X86_REG_IP,nip)
    return True

def hook_intr(uc,intno,user):
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
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
                print(f'  *** PROTECTION read -> ND + gap fill',flush=True); return
            data=bytes(img[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
            print(f'  INT13 drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d}',flush=True)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00:
            if mode[0]!=al: mode[0]=al; print(f'  INT10 mode {al:#x}',flush=True)
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
        uc.reg_write(UC_X86_REG_AX,0); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

def hin(uc,port,size,user):
    if port==0x60: return kbd_data[0]
    if port==0x3DA: return 0x09
    if port==0x61: return 0x30
    if port==0x64: return 0x01
    return 0
def hout(uc,port,size,value,user): pass

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,hin,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,hout,None,1,0,UC_X86_INS_OUT)

ic=[0]; next_timer=[300_000]; next_key=[1_500_000]
def hc(uc,addr,size,user):
    ic[0]+=1
    n=ic[0]
    if n>=next_timer[0]:
        next_timer[0]=n+300_000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            if do_int(uc,0x08): stats['irq0']+=1
    if n>=next_key[0]:
        next_key[0]=n+1_500_000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200 and scan_i[0]<len(SCANS):
            sc=SCANS[scan_i[0]]; scan_i[0]+=1
            screenshot(uc,f'pre_key{scan_i[0]}_{sc:02x}')
            kbd_data[0]=sc
            if do_int(uc,0x09):
                stats['irq1']+=1
                print(f'  [IRQ1 scancode {sc:#04x} #{scan_i[0]}]',flush=True)
    if (n&0xFFFFF)==0 and time.time()-t0>MAX_SECS:
        print('  [watchdog]',flush=True); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
print(f'=== boot with IRQ injection (protected={PROT}) ===',flush=True)
try:
    mu.emu_start(0x7C00,0xFFFF0,0,MAX_INSTR); print('ended')
except UcError as e:
    print(f'STOP {e} at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
screenshot(mu,'final')
print(f'\ninstrs={ic[0]:,} INT13={stats["13"]} IRQ0={stats["irq0"]} IRQ1={stats["irq1"]} '
      f'prot={stats["prot"]} keys_sent={scan_i[0]} mode={mode[0]:#x} t={time.time()-t0:.0f}s')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
