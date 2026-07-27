"""Boot emulation, take 2. Proper IVT stubs in unmapped-free area + tracing."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

DISK = {0x00: open('assets/pirates_1.img','rb').read(),
        0x01: open('assets/pirates_2.img','rb').read()}
SEC, SPT, HEADS = 512, 9, 2
md = Cs(CS_ARCH_X86, CS_MODE_16)

MEMSZ = 0x110000
mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, MEMSZ)

mu.mem_write(0x7C00, DISK[0][:512])
mu.mem_write(0x410, struct.pack('<H', 0x0021))
mu.mem_write(0x413, struct.pack('<H', 640))
mu.mem_write(0x465, bytes([0x29]))

# IVT -> F000:1000+v*4, each stub = INT3-like marker we hook, then IRET.
# Use HLT as the trap: hook UC_HOOK_INSN on HLT is unreliable; instead use
# an invalid-but-hookable approach: point all vectors at F000:FF53 which holds IRET.
IRET_AT = 0xFFF53
for v in range(256):
    mu.mem_write(v*4, struct.pack('<HH', 0xFF53, 0xF000))
mu.mem_write(IRET_AT, b'\xCF')

reads = []
def hook_intr(uc, intno, user):
    ax = uc.reg_read(UC_X86_REG_AX); ah = (ax>>8)&0xFF; al = ax&0xFF
    if intno == 0x13:
        cx = uc.reg_read(UC_X86_REG_CX); dx = uc.reg_read(UC_X86_REG_DX)
        bx = uc.reg_read(UC_X86_REG_BX); es = uc.reg_read(UC_X86_REG_ES)
        cyl = ((cx>>8)&0xFF) | ((cx & 0xC0) << 2); sec = cx & 0x3F
        head = (dx>>8)&0xFF; drive = dx & 0xFF
        if ah == 0x02:
            img = DISK.get(drive & 1, DISK[0])
            src = ((cyl*HEADS + head)*SPT + (sec-1))*SEC
            data = img[src: src+al*SEC]
            dest = (es*16 + bx) & 0xFFFFF
            if data: uc.mem_write(dest, data)
            reads.append((cyl,head,sec,al,es,bx,src,dest))
            print(f'  INT13 c={cyl:2d} h={head} s={sec:2d} n={al:2d} -> {es:04X}:{bx:04X} phys {dest:05X}  file 0x{src:06x}')
            uc.reg_write(UC_X86_REG_AX, 0)
        uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS) & ~1)
    elif intno == 0x10:
        if ah == 0x0F: uc.reg_write(UC_X86_REG_AX, 0x0304)
    elif intno == 0x16:
        if ah in (0x00,0x10): uc.reg_write(UC_X86_REG_AX, 0x011B)
        else: uc.reg_write(UC_X86_REG_EFLAGS, uc.reg_read(UC_X86_REG_EFLAGS)|0x40)
    elif intno == 0x1A:
        uc.reg_write(UC_X86_REG_CX,0); uc.reg_write(UC_X86_REG_DX,0)
    # manual IRET
    sp = uc.reg_read(UC_X86_REG_SP); ss = uc.reg_read(UC_X86_REG_SS)
    ip, cs, fl = struct.unpack('<HHH', uc.mem_read(ss*16+sp, 6))
    uc.reg_write(UC_X86_REG_SP,(sp+6)&0xFFFF)
    uc.reg_write(UC_X86_REG_CS, cs); uc.reg_write(UC_X86_REG_IP, ip)

mu.hook_add(UC_HOOK_INTR, hook_intr)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,d: 0, None, 1, 0, UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, lambda u,p,s,v,d: None, None, 1, 0, UC_X86_INS_OUT)

hist = []
def hook_code(uc, addr, size, user):
    hist.append((uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP), addr))
    if len(hist) > 400: hist.pop(0)
mu.hook_add(UC_HOOK_CODE, hook_code)

mu.reg_write(UC_X86_REG_CS,0); mu.reg_write(UC_X86_REG_IP,0x7C00)
mu.reg_write(UC_X86_REG_SS,0); mu.reg_write(UC_X86_REG_SP,0x7000)
mu.reg_write(UC_X86_REG_DX,0x0000)

print('=== booting ===')
try:
    mu.emu_start(0x7C00, 0xFFFFF, 0, 50_000_000)
    print('emu_start returned normally')
except UcError as e:
    cs, ip = mu.reg_read(UC_X86_REG_CS), mu.reg_read(UC_X86_REG_IP)
    print(f'\nSTOP: {e} at {cs:04X}:{ip:04X}  ({len(hist)} recent)')
    print('last 24 executed:')
    for c,i,a in hist[-24:]:
        try:
            code = bytes(mu.mem_read(a, 8))
            ins = next(md.disasm(code, a), None)
            t = f'{ins.mnemonic} {ins.op_str}' if ins else '??'
        except Exception: t='??'
        print(f'   {c:04X}:{i:04X} (lin {a:05X})  {t}')

print(f'\ntotal INT13 reads: {len(reads)}')
open('scratch/out/ram_after_boot.bin','wb').write(bytes(mu.mem_read(0,0x100000)))
