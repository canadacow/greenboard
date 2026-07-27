"""Read on-screen text by hooking the ACTUAL glyph blitter inner loop.

fbwatch proved 0050:0A9D is the only routine drawing character cells
(writes es:[di], +0x2000, +0x50, +0x2050 ... = CGA interleaved banks).
At that point SI points at the glyph bitmap inside the font table, so
   char_code = (SI - font_base) / 32
recovers the character. We learn font_base by observing the minimum SI.

Cursor position lives in [0x3b78] (col) / [0x3b7a] (row).
"""
import sys, struct, time, os, shutil, collections
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

shutil.rmtree('scratch/out/c2', ignore_errors=True)
os.makedirs('scratch/out/c2', exist_ok=True)
D={0:bytearray(open('assets/pirates_1.img','rb').read()),
   1:bytearray(open('assets/pirates_2.img','rb').read())}
SEC,SPT,HEADS=512,9,2
PROT=D[0][0x200:0x208]==b'0-PIRATE'
MAX=int(sys.argv[1]) if len(sys.argv)>1 else 200_000_000

mu=Uc(UC_ARCH_X86,UC_MODE_16); mu.mem_map(0,0x110000)
mu.mem_write(0x400,b'\x00'*0x400)
mu.mem_write(0x410,struct.pack('<H',0x0061)); mu.mem_write(0x413,struct.pack('<H',640))
mu.mem_write(0x465,bytes([0x29])); mu.mem_write(0xFFFFE,bytes([0xFE]))
mu.mem_write(0x7C00,bytes(D[0][:512]))
mu.mem_write(0xFE000,b'\xCF'*256)
for v in range(256): mu.mem_write(v*4,struct.pack('<HH',0xE000+v,0xF000))

# Set-1 scancodes, needed both for IRQ1 delivery and for the AH byte that
# INT 16h returns alongside the ASCII code.
SC={'A':0x1E,'B':0x30,'C':0x2E,'D':0x20,'E':0x12,'F':0x21,'G':0x22,'H':0x23,
    'I':0x17,'J':0x24,'K':0x25,'L':0x26,'M':0x32,'N':0x31,'O':0x18,'P':0x19,
    'Q':0x10,'R':0x13,'S':0x1F,'T':0x14,'U':0x16,'V':0x2F,'W':0x11,'X':0x2D,
    'Y':0x15,'Z':0x2C,'1':0x02,'2':0x03,'3':0x04,' ':0x39}
SC_ENTER, SC_SPACE = 0x1C, 0x39
SC_TO_ASCII={v:k for k,v in SC.items()}
SC_TO_ASCII[SC_ENTER]='\r'
SC_TO_ASCII[SC_SPACE]=' '
bios_buf=[]        # BIOS keyboard buffer: (scancode, ascii)
NAME='DRAKE'
# Typed text served over INT 16h (the name prompt reads characters, not the
# IRQ1 key-state bitmap that the sword menus use).
ascii_q=(['\r']*6            # historical period? / nationality menus
         +list(NAME)         # type the family name
         +['\r']*200)        # confirm everything after

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
    os.makedirs('scratch/out/c2',exist_ok=True)
    img.resize((640,400),Image.NEAREST).save(f'scratch/out/c2/{shots[0]:03d}_{tag}.png')
    print(f'  [snap {shots[0]:03d}_{tag}]',flush=True)

def do_int(uc,vec):
    fl=uc.reg_read(UC_X86_REG_EFLAGS)
    cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
    ss=uc.reg_read(UC_X86_REG_SS); sp=(uc.reg_read(UC_X86_REG_SP)-6)&0xFFFF
    uc.mem_write(ss*16+sp,struct.pack('<HHH',ip,cs,fl)); uc.reg_write(UC_X86_REG_SP,sp)
    nip,ncs=struct.unpack('<HH',uc.mem_read(vec*4,4))
    uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x200)
    uc.reg_write(UC_X86_REG_CS,ncs); uc.reg_write(UC_X86_REG_IP,nip)

pend=[None]
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
                # MicroProse protection. Per bench's FDC log the game issues
                # the read with sector-size code N=4 (2048 bytes), not N=2:
                #   READ DATA: C=4 H=0 R=1 N=4 ... size=2048
                # The real uPD765 streams the 512 bytes of sector data then
                # 1536 bytes of format gap filler (0x43) into the DMA buffer
                # and only THEN reports ND. AL is the sector COUNT (1), so
                # sizing the transfer as al*512 delivered no filler at all --
                # that was the bug. Emit the full 2048-byte oversized transfer.
                nb=2048
                buf=bytes(im[src:src+SEC]).ljust(SEC,b'\x00')+b'\x43'*(nb-SEC)
                if dest+len(buf)<=0x110000: uc.mem_write(dest,buf)
                uc.reg_write(UC_X86_REG_AX,(0x04<<8)|0x00)
                uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)|1)
                cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
                print(f'  *** PROTECTION READ from {cs:04X}:{ip:04X} -> '
                      f'512B data + 1536B 0x43, AH=04 CF=1',flush=True)
                return
            data=bytes(im[src:src+al*SEC])
            if data and dest+len(data)<=0x110000: uc.mem_write(dest,data)
        uc.reg_write(UC_X86_REG_AX,al)
    elif intno==0x10:
        if ah==0x00: mode[0]=al
        elif ah==0x0F: uc.reg_write(UC_X86_REG_AX,(0x28<<8)|mode[0])
    elif intno==0x09:
        # BIOS keyboard ISR stand-in. The game's own INT 09h handler
        # (0050:26E0) only processes the key when [0x3c0c]==1; otherwise it
        # CHAINS to the previous handler via 'ljmp cs:[0x26dc]'. That previous
        # handler is us. If we do nothing, the keystroke is lost and INT 16h
        # has nothing to return -- which is exactly why typing did nothing.
        sc=kbd_port[0]
        if not (sc & 0x80):                       # make code only
            ch=SC_TO_ASCII.get(sc)
            if ch: bios_buf.append((sc,ch))
        uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)
        return
    elif intno==0x16:
        fl=uc.reg_read(UC_X86_REG_EFLAGS)
        # pend[0] holds a config key; ascii_q holds typed text (family name).
        # Typed text is only offered once the config menu (1/2/2) is finished,
        # otherwise the name queue would answer the config prompts.
        cfg_done = menu[0] and ki[0]>=len(KEYS)
        if pend[0] is not None:      nxt=pend[0]
        elif cfg_done and ascii_q:   nxt=ascii_q[0]
        else:                        nxt=None
        if ah in (0x01,0x11):
            if nxt is None: uc.reg_write(UC_X86_REG_EFLAGS,fl|0x40)
            else:
                sc=SC.get(nxt.upper(), SC_ENTER if nxt=='\r' else 0x39)
                uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(nxt))
                uc.reg_write(UC_X86_REG_EFLAGS,fl&~0x40)
            return
        if ah in (0x00,0x10):
            if pend[0] is not None:
                c=pend[0]; pend[0]=None
            elif cfg_done and ascii_q:
                c=ascii_q.pop(0)
            else:
                c='\r'
            gets[0]+=1
            sc=SC.get(c.upper(), SC_ENTER if c=='\r' else 0x39)
            cs=uc.reg_read(UC_X86_REG_CS); ip=uc.reg_read(UC_X86_REG_IP)
            print(f"  [INT16 GET #{gets[0]} -> {'ENTER' if c==chr(13) else repr(c)} "
                  f"from {cs:04X}:{ip:04X}]",flush=True)
            uc.reg_write(UC_X86_REG_AX,(sc<<8)|ord(c)); return
    uc.reg_write(UC_X86_REG_EFLAGS,uc.reg_read(UC_X86_REG_EFLAGS)&~1)

kbd_port=[0x00]        # what the ISR reads from port 0x60
def port_in(uc,port,size,user):
    if port==0x60: return kbd_port[0]
    if port==0x3DA: return 0x09
    if port==0x61: return 0x30
    if port==0x64: return 0x01
    return 0
mu.hook_add(UC_HOOK_INTR,hook_intr)
mu.hook_add(UC_HOOK_INSN,port_in,None,1,0,UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN,lambda u,p,s,v,d:None,None,1,0,UC_X86_INS_OUT)

CELL=0x50*16+0x0A9D
FONT_BASE, FONT_BIAS = 0x300, 7      # char = (SI-base)/16 + bias  (solved)
def si_to_char(si):
    i=((si-FONT_BASE)//16)+FONT_BIAS
    return chr(32+i) if 0<=i<95 else '?'
cells=[]          # (row, col, si)
screen=[]         # decoded characters, in draw order
hot=collections.Counter()
# The config menu reads keys via INT 16h (0050:08B0) -- 1/2/2 works there.
# The in-game sword menus do NOT: the game installs its own INT 09h keyboard
# ISR (0050:26E0, body at 0050:270E) which reads port 0x60 directly, keeps a
# key-state bitmap in [0x3c0e], and EOIs the PIC. Enter is detected from that
# bitmap, so faking INT 16h returns can never select anything -- we must
# deliver real IRQ1 with scancodes.
KEYS=['1','2','2']                     # INT16 path (config menu only)
scan_q=[]                              # pending scancode make/break pairs
irqlog=[]                              # (scancode, keystate-before) samples
KEYSTROKES=([SC_ENTER]*8              # welcome / period / nationality menus
            +[SC[c] for c in NAME]    # type the family name
            +[SC_ENTER]*200)          # confirm through the rest of creation
ki=[0]; menu=[False]; sent=[0]; gets=[0]; ks=[0]
ic=[0]; nt=[25000]; nk=[3_000_000]
def hc(uc,addr,size,user):
    ic[0]+=1
    if ic[0]>MAX-2_000_000:
        hot[(uc.reg_read(UC_X86_REG_CS),uc.reg_read(UC_X86_REG_IP))]+=1
    if addr==CELL:
        si=uc.reg_read(UC_X86_REG_SI)
        cells.append((0,0,si))
        screen.append(si_to_char(si))
        if not menu[0] and 'CONFIGURATION' in ''.join(screen[-400:]):
            menu[0]=True
            print(f"  [MENU DETECTED] ...{''.join(screen[-60:])}",flush=True)
    if ic[0]>=nt[0]:
        nt[0]=ic[0]+25000
        if uc.reg_read(UC_X86_REG_EFLAGS)&0x200:
            # Real IRQ0 goes to INT 08h, whose BIOS handler chains to INT 1Ch.
            # The game hooks INT 1Ch (0050:12A0) to decrement its countdown
            # timers ([0x3c02].. [0x3c0a]); the spin at 0050:2BAC waits on
            # [0x3c04]. If INT 08h is still our default IRET stub the chain
            # never happens, so deliver 1Ch directly when 08h is unhooked.
            v8=struct.unpack('<HH',uc.mem_read(0x08*4,4))
            do_int(uc, 0x08 if v8[1]!=0xF000 else 0x1C)
            return
    # Two independent input paths, and BOTH are required:
    #  * INT 16h  -- the config reader (0050:08B0) and typed text.
    #  * IRQ1     -- the sword-menu poll wrapper (0050:2740) LOOPS on any key
    #                it doesn't recognise (only 0xE0/0x16/'V'/SPACE), so Enter
    #                can never arrive that way; menu selection comes from the
    #                INT 09h ISR updating the key-state bitmap at [0x3c0e].
    if scan_q and (ic[0]&0x1FFF)==0 and (uc.reg_read(UC_X86_REG_EFLAGS)&0x200):
        sc=scan_q.pop(0)
        kbd_port[0]=sc
        f=struct.unpack('<H',uc.mem_read(0x50*16+0x3c0c,2))[0]
        b=struct.unpack('<H',uc.mem_read(0x50*16+0x3c0e,2))[0]
        irqlog.append((sc,f,b))
        do_int(uc,0x09)
        return
    if ic[0]>=nk[0]:
        nk[0]=ic[0]+1_500_000
        snap(uc,f'{ic[0]//1000000}M')
        if menu[0] and ki[0]>=len(KEYS) and not scan_q and ks[0]<len(KEYSTROKES):
            # The game's INT 09h ISR only handles the key when [0x3c0c]==1;
            # otherwise it chains to the previous handler. Make sure the flag
            # is set so our injected scancodes reach the key-state bitmap.
            uc.mem_write(0x50*16+0x3c0c, struct.pack('<H',1))
            sc=KEYSTROKES[ks[0]]; ks[0]+=1
            scan_q.extend([sc, sc|0x80])       # make + break
            print(f'  [IRQ1 sc={sc:#04x} #{ks[0]}]',flush=True)
        if pend[0] is None:
            if not menu[0]: pend[0]=' '
            elif ki[0]<len(KEYS):
                pend[0]=KEYS[ki[0]]; ki[0]+=1; sent[0]+=1
                k=pend[0]
                print(f"  [key '{'ENTER' if k==chr(13) else k}'] screen tail: "
                      f"{''.join(screen[-70:])}",flush=True)
                snap(uc,f'key{ki[0]}_{k if k!=chr(13) else "ENTER"}')
mu.hook_add(UC_HOOK_CODE,hc)

mu.reg_write(UC_X86_REG_CS,0x07C0); mu.reg_write(UC_X86_REG_IP,0)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
t0=time.time()
try: mu.emu_start(0x7C00,0xFFFF0,0,MAX)
except UcError as e: print('STOP',e)
snap(mu,'final')
print(f'\ninstrs={ic[0]:,} cells={len(cells)} mode={mode[0]:#x} t={time.time()-t0:.0f}s')

from capstone import Cs, CS_ARCH_X86, CS_MODE_16
_md=Cs(CS_ARCH_X86,CS_MODE_16)
ks_now=struct.unpack('<H',mu.mem_read(0x50*16+0x3c0e,2))[0]
print(f'\n=== IRQ1: {len(irqlog)} scancodes, keystate [0x3c0e] now {ks_now:#06x} ===')
print('  (sc, [0x3c0c] mode flag, [0x3c0e] keystate BEFORE the ISR ran)')
for sc,f,b in irqlog[:20]:
    print(f'  sc={sc:#04x}  mode={f}  keystate={b:#06x}')

print('\n=== WHERE IT IS STUCK (hottest addresses at end) ===')
for (cs,ip),c in hot.most_common(18):
    lin=(cs*16+ip)&0xFFFFF
    try:
        i=next(_md.disasm(bytes(mu.mem_read(lin,10)),lin),None)
        t=f'{i.mnemonic} {i.op_str}' if i else '?'
    except Exception: t='?'
    print(f'  {cs:04X}:{ip:04X} x{c:<7d} {t}')

if cells:
    sis=[s for _,_,s in cells]
    print(f'SI range {min(sis):#06x}..{max(sis):#06x}  step=0x10 (16-byte glyphs)')
    # The credits screen is known: "WRITTEN BY / SID MEIER / IBM VERSION / ..."
    # Solve for the font base by trying every base and scoring how many SI
    # values map into printable ASCII forming known words.
    best=None
    for base in range(0x300,0x900,0x10):
        for bias in range(0,96):
            idx=[((x-base)//16)+bias for x in sis]
            if any(i<0 or i>94 for i in idx): continue
            s=''.join(chr(32+i) for i in idx)
            score=sum(s.count(w) for w in
                      ('WRITTEN','MEIER','COPYRIGHT','MICROPROSE','CONFIGURATION',
                       'TANDY','KEYBOARD','JOYSTICK','FLOPPY'))
            if best is None or score>best[0]: best=(score,base,bias,s)
    score,base,bias,s=best
    print(f'font base={base:#06x} bias={bias} score={score}')
    print('\n=== DECODED SCREEN TEXT ===')
    print(s)
