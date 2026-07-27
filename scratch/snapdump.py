"""Pirates! harness with a proper SNAPSHOT: screen + registers + stack +
call-stack + disassembly around IP, all at once.

Instead of guessing which routine is waiting, dump_state() shows:
  - regs and flags
  - the instruction stream around CS:IP
  - the raw stack words, each annotated if it looks like a return address
    (i.e. the bytes just before it are a CALL) -- that gives the call chain
  - which INT 16h callers have been polling, and how often
"""
import sys, struct, time, os, shutil, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from PIL import Image

OUT='scratch/out/snap'
shutil.rmtree(OUT, ignore_errors=True); os.makedirs(OUT, exist_ok=True)
D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
md=Cs(CS_ARCH_X86,CS_MODE_16)
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 80_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]
mode=[4]; shots=[0]; i16=collections.Counter(); i16_recent=collections.deque(maxlen=12)

def screenshot(uc,tag):
    fb=bytes(uc.mem_read(0xB8000,0x4000))
    img=Image.new('RGB',(320,200)); px=img.load()
    for y in range(200):
        base=(0x2000 if y&1 else 0)+(y>>1)*80
        for xb in range(80):
            b=fb[base+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    img.resize((640,400),Image.NEAREST).save(f'{OUT}/{shots[0]:03d}_{tag}.png')

def looks_like_retaddr(uc, seg, off):
    """A return address has a CALL instruction ending right before it."""
    lin=(seg*16+off)&0xFFFFF
    if lin<8: return None
    try: pre=bytes(uc.mem_read(lin-8,8))
    except Exception: return None
    for back in (2,3,4,5):          # E8 rel16 = 3 bytes, FF /2 = 2-4, 9A = 5
        b=pre[8-back:]
        try: ins=next(md.disasm(b,(lin-back)&0xFFFFF),None)
        except Exception: continue
        if ins and ins.mnemonic in ('call','lcall') and ins.size==back:
            return f'{ins.mnemonic} {ins.op_str}'
    return None

def dump_state(uc, tag, note=''):
    shots[0]+=1
    screenshot(uc,tag)
    R=lambda r: uc.reg_read(r)
    cs,ip=R(UC_X86_REG_CS),R(UC_X86_REG_IP)
    ss,sp=R(UC_X86_REG_SS),R(UC_X86_REG_SP)
    fl=R(UC_X86_REG_EFLAGS)
    print(f'\n{"="*72}\nSNAPSHOT {shots[0]:03d} [{tag}] {note}')
    print(f'  CS:IP={cs:04X}:{ip:04X}  SS:SP={ss:04X}:{sp:04X}  '
          f'DS={R(UC_X86_REG_DS):04X} ES={R(UC_X86_REG_ES):04X}')
    print(f'  AX={R(UC_X86_REG_AX):04X} BX={R(UC_X86_REG_BX):04X} '
          f'CX={R(UC_X86_REG_CX):04X} DX={R(UC_X86_REG_DX):04X} '
          f'SI={R(UC_X86_REG_SI):04X} DI={R(UC_X86_REG_DI):04X} BP={R(UC_X86_REG_BP):04X}')
    print(f'  FLAGS={fl:04X}  [{"C" if fl&1 else "-"}{"Z" if fl&0x40 else "-"}'
          f'{"I" if fl&0x200 else "-"}{"D" if fl&0x400 else "-"}]')

    print('  --- code at CS:IP ---')
    lin=(cs*16+ip)&0xFFFFF
    try:
        code=bytes(uc.mem_read(max(0,lin-16),64))
        for ins in md.disasm(code,(lin-16)&0xFFFFF):
            mark=' <== IP' if ins.address==lin else ''
            print(f'    {ins.address:05X}: {ins.bytes.hex():<12s} '
                  f'{ins.mnemonic:<7s} {ins.op_str}{mark}')
    except Exception as e:
        print('    (unreadable)',e)

    print('  --- stack (SS:SP upward) ---')
    for i in range(20):
        a=(ss*16+((sp+i*2)&0xFFFF))&0xFFFFF
        try: w=struct.unpack('<H',uc.mem_read(a,2))[0]
        except Exception: break
        why=looks_like_retaddr(uc,cs,w) or ''
        tagc=f'   <- return to {cs:04X}:{w:04X} from "{why}"' if why else ''
        print(f'    SP+{i*2:02X}  {w:04X}{tagc}')

    # Data refs like [0x9aa9] are DS-relative, and DS is the game's data
    # segment (117B), NOT 0050. Reading them at 0050:xxxx returns code bytes.
    ds=R(UC_X86_REG_DS)
    print(f'  --- key gate vars (DS={ds:04X}) ---')
    for nm,off in (('[0x9aa9] mode gate',0x9aa9),('[0x3bf8] space flag',0x3bf8),
                   ('[0x3b96] toggle',0x3b96),('[0x3c0c] kbd mode',0x3c0c),
                   ('[0x3c0e] keystate',0x3c0e)):
        try:
            v=struct.unpack('<H',uc.mem_read((ds*16+off)&0xFFFFF,2))[0]
            print(f'    {nm} = {v:#06x}')
        except Exception as e: print(f'    {nm} unreadable')
    if i16:
        print('  --- INT 16h pollers (cs:ip, AH) ---')
        for (c,p,ah),n in i16.most_common(6):
            print(f'    {c:04X}:{p:04X} AH={ah:#04x}  x{n}')
    if i16_recent:
        print('  --- recent INT 16h results ---')
        print('    '+', '.join(i16_recent))
    print('='*72,flush=True)

def do_int(uc,vec):
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    ss=uc.reg_read(UC_X86_REG_SS); sp=(uc.reg_read(UC_X86_REG_SP)-6)&0xFFFF
    uc.mem_write(ss*16+sp,struct.pack('<HHH',ip,cs,fl)); uc.reg_write(UC_X86_REG_SP,sp)
    nip,ncs=struct.unpack('<HH',uc.mem_read(vec*4,4))
    uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x200)
    uc.reg_write(UC_X86_REG_CS,ncs); uc.reg_write(UC_X86_REG_IP,nip)

# Enter IS accepted by the "Your name?" prompt (an empty name yields
# "MR. INCOGNITO" and the game moves on to Difficulty Level), so the reader
# there is NOT the sword-selector poll at 0050:2C40. Send Enter and let the
# per-call-site snapshots show which routine actually reads it.
KEYS=[' ',' ','1','2','2']+['\r']*60
ki=[0]; pend=[None]
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
        cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
        i16[(cs,ip,ah)]+=1
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        if ah in (0x01,0x11):
            peeks[(cs,ip,pend[0] is None)]+=1
            if pend[0] is None: uc.reg_write(UC_X86_REG_EFLAGS,fl|0x40)
            else:
                sc=0x1C if pend[0]=='\r' else 0x39
                uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(pend[0]))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            lin=(cs*16+ip)&0xFFFFF
            gets_by_site[lin]+=1
            if lin not in GET_SITES:
                GET_SITES.add(lin)
                dump_state(uc,f'NEWREADER_{lin:05X}',f'first AH=0 here, keys_sent={ki[0]}')
            c=pend[0] or '\r'; pend[0]=None
            sc=0x1C if c=='\r' else 0x39
            i16_recent.append(f'{cs:04X}:{ip:04X}->{"CR" if c==chr(13) else repr(c)}')
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(c)); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,d:(0x09 if p==0x3DA else 0),None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

ic=[0]; nt=[25000]; nk=[3_000_000]; nsnap=[10_000_000]
# INT 16h AH=0 (get-key) call sites, discovered live in hook_intr.
GET_SITES=set(); seen_sites=set(); gets_by_site=collections.Counter()
peeks=collections.Counter()
def hc(uc,addr,size,user):
    ic[0]+=1
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C); return
    # IMPORTANT: the selector poll at 0050:2C40 only RETURNS when INT 16h
    # AH=1 reports an EMPTY buffer (je 0x2C6D). Any key it doesn't recognise
    # loops back and polls again. So the keyboard must go quiet between
    # keystrokes -- if we keep a key permanently queued it spins forever.
    # Feed one key, then leave the buffer empty for a good while.
    if ic[0]>=nk[0]:
        nk[0]=ic[0]+3_000_000
        if pend[0] is None and ki[0]<len(KEYS):
            pend[0]=KEYS[ki[0]]; ki[0]+=1
    # Snapshot at EVERY distinct INT 16h AH=0 (get) call site, once each, so
    # we see every reader -- the config menu, the sword selector, and whatever
    # reads the "Your name?" prompt -- with its own call stack.
    if ic[0]>=nsnap[0]:
        nsnap[0]=ic[0]+20_000_000
        dump_state(uc,f'{ic[0]//1000000}M',f'keys_sent={ki[0]}')
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
dump_state(mu,'final',f'mode={mode[0]:#x} t={time.time()-t0:.0f}s')
print('\n=== INT 16h AH=1 (peek) results: (site, buffer_empty) -> count ===')
for (c,p,empty),n in peeks.most_common(8):
    print(f'  {c:04X}:{p:04X} empty={empty}  x{n}')
print('\n=== INT 16h AH=0 (get) counts by call site ===')
for lin,n in gets_by_site.most_common():
    print(f'  live {lin:05X}  (image 0x{lin-0x500:05X})  x{n}')
