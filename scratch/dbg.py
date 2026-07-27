"""Pirates! booter debugger.

Strategy (per user): poison ALL memory with INT3 (0xCC) before loading anything.
Any jump into never-loaded memory traps immediately at a known address instead
of silently executing zeros as 'add [bx+si],al'. Every IVT vector points at a
unique stub so we can see exactly which interrupt the game invokes and from
where. Then we trace the real control flow and find what it waits on.

Findings so far, carried forward:
  - Enter at CS=07C0:0000 (so 'mov ax,cs' is right).
  - UC_HOOK_INTR fires BEFORE the CPU pushes; never emulate IRET in the hook.
  - Disk 1 is a protected original ("0-PIRATE GAME DISK" @0x200). A read of
    C=4 H=0 S=1 must deliver 512B data + 0x43 gap filler then fail with ND
    (AH=0x04, CF=1).  [mechanism from src/isa/isa_fdc.cpp]
  - The game uses NO I/O ports and ignores INT 16h -> it drives its own ISRs.
"""
import sys, struct, time, os, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
md=Cs(CS_ARCH_X86,CS_MODE_16)
MAX_INSTR=int(sys.argv[1]) if len(sys.argv)>1 else 20_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16)
mu.mem_map(0,0x110000)

# ---- poison everything with INT3 ----
mu.mem_write(0, b'\xCC'*0x100000)
# real BIOS-ish areas
mu.mem_write(0x400, b'\x00'*0x100)
mu.mem_write(0xB8000, b'\x00'*0x8000)
mu.mem_write(0xA0000, b'\x00'*0x10000)
mu.mem_write(0x410,struct.pack('<H',0x0021))
mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29]))
mu.mem_write(0xFFFFE,bytes([0xFE]))
# stack area must not be INT3 (it's data)
mu.mem_write(0x0500, b'\x00'*0x200)
mu.mem_write(0x7C00, bytes(D[0][:512]))

# ---- IVT: every vector -> unique stub at F000:E000+v, each is just IRET ----
STUB=0xFE000
mu.mem_write(STUB, b'\xCF'*256)
for v in range(256):
    mu.mem_write(v*4, struct.pack('<HH', 0xE000+v, 0xF000))

ivt_hits=collections.Counter()
int3_hits=collections.Counter()
mode=[4]; stats=collections.Counter()
hist=collections.deque(maxlen=60)
t0=time.time()

def where(uc):
    return uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP)

def hook_intr(uc,intno,user):
    cs,ip=where(uc)
    ax=uc.reg_read(UC_X86_REG_AX); ah=(ax>>8)&0xFF; al=ax&0xFF
    if intno==3:
        int3_hits[(cs,ip)]+=1
        if sum(int3_hits.values())<=25:
            print(f'  !!! INT3 TRAP at {cs:04X}:{ip:04X} (executed poisoned memory)',flush=True)
            print('      recent flow:',' '.join(f'{c:04X}:{i:04X}' for c,i in list(hist)[-12:]),flush=True)
        if sum(int3_hits.values())>200: uc.emu_stop()
        return
    ivt_hits[intno]+=1
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
                print(f'  *** PROTECTION READ from {cs:04X}:{ip:04X} -> ND+gapfill',flush=True)
                return
            data=bytes(img[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
            print(f'  INT13 @{cs:04X}:{ip:04X} drv={drv} c={cyl:2d} h={head} s={sec:2d} n={al:2d} -> {dest:05X}',flush=True)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00:
            if mode[0]!=al: mode[0]=al; print(f'  INT10 mode {al:#x} @{cs:04X}:{ip:04X}',flush=True)
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x16:
        stats['16']+=1
        if stats['16'] in (1,2,3):
            print(f'  INT16 ah={ah:#04x} @{cs:04X}:{ip:04X}',flush=True)
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
        uc.reg_write(UC_X86_REG_AX,0); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

port_in=collections.Counter(); port_out=collections.Counter()
def hin(uc,port,size,user):
    port_in[port]+=1
    if port==0x3DA: return 0x09
    if port==0x60: return 0x00
    if port==0x61: return 0x30
    if port==0x64: return 0x01
    return 0
def hout(uc,port,size,value,user): port_out[port]+=1

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,hin,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,hout,None,1,0,UC_X86_INS_OUT)

ic=[0]
loopdet=collections.Counter()
def hc(uc,addr,size,user):
    ic[0]+=1
    cs,ip=where(uc)
    hist.append((cs,ip))
    if ic[0]>MAX_INSTR*0.6:
        loopdet[(cs,ip)]+=1
    if (ic[0]&0x3FFFFF)==0 and time.time()-t0>200:
        uc.emu_stop()
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0)
print(f'=== INT3-poisoned debug boot (protected={PROT}) ===',flush=True)
try:
    mu.emu_start(0x7C00,0xFFFF0,0,MAX_INSTR); print('ended (budget)')
except UcError as e:
    cs,ip=where(mu); print(f'STOP {e} at {cs:04X}:{ip:04X}')

print(f'\ninstrs={ic[0]:,}  t={time.time()-t0:.0f}s  mode={mode[0]:#x}')
print('IVT vectors used:', {f'{k:02X}h':v for k,v in sorted(ivt_hits.items())})
print('IN ports :', {f'{p:#05x}':c for p,c in port_in.most_common(8)})
print('OUT ports:', {f'{p:#05x}':c for p,c in port_out.most_common(8)})
print('INT3 traps:', sum(int3_hits.values()), dict(list(int3_hits.items())[:6]))
print('\nhottest addresses in final phase (the spin loop):')
for (cs,ip),c in loopdet.most_common(14):
    lin=(cs*16+ip)&0xFFFFF
    try:
        ins=next(md.disasm(bytes(mu.mem_read(lin,10)),lin),None)
        t=f'{ins.mnemonic} {ins.op_str}' if ins else '?'
    except Exception: t='?'
    print(f'  {cs:04X}:{ip:04X} x{c:<8d} {t}')
open('scratch/out/ram.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
