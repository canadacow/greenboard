"""Port past the title screen to the credits/splash, reading ported memory only.

From 0050:04E0 (startup):
    call 0x80b                  ; init
    [0x3b98] = 0                ; video mode selector
    ax = 0x20 ; call 0x2820     ; load_resource(0x20)  <- the splash resource
    cs:[0xA] = [0x10d] ; cs:[0xC] = [0x113] ; cs:[0xE] = [0x107]
    call 0x1fe ; call 0x96d
    es = cs:[6] ; clear 0x8000 words          ; wipe the video buffer
    [0x93d1]=4 [0x93d3]=0 [0x93e7]=0x20
    call 0x2c70                 ; blit/present
    call 0x564                  ; splash screens

load_resource (0050:2820) -> res_fetch (0050:2844):
    si = [0x3b84]*2 ; bx = [si + 0x136b]      ; descriptor pointer
    [0x11b8]=cnt  [0x11ae]=cyl+[0x3bc8]  [0x11af]=sec
    [0x11b2]=dest seg  [0x11b4]=dest off
    sec>=10 -> head 1, sec-=9
    read cnt sectors, advancing sector/head/cylinder, dest += 0x200 each
"""
import sys
sys.path.insert(0, 'scratch')
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
from boot_port import CPU, SEC, SPT, HEADS, DISK1
from stage2_port import boot_all, run_main_prologue, dis
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from PIL import Image

md = Cs(CS_ARCH_X86, CS_MODE_16)


def res_fetch(c, index):
    """0050:2844 -- transliterated. Reads one resource by directory index."""
    ds = c.ds
    c.wb(ds, 0x11A3, 0)                       # clear error
    ptr = c.rw(ds, 0x136B + index * 2)        # bx = [si + 0x136b]
    cnt = c.rw(ds, ptr + 0) & 0xFF
    if cnt == 0:
        return False
    cyl = (c.rw(ds, ptr + 4) + c.rw(ds, 0x3BC8)) & 0xFF
    sec = c.rw(ds, ptr + 6) & 0xFF
    dseg = c.rw(ds, ptr + 8)
    doff = c.rw(ds, ptr + 0xA)
    head = 0
    if sec >= 10:
        head, sec = 1, sec - 9
    bx = 0
    for _ in range(cnt):
        src = ((cyl * HEADS + head) * SPT + (sec - 1)) * SEC
        data = DISK1[src:src + SEC]
        c.mem[c.lin(dseg, doff + bx): c.lin(dseg, doff + bx) + len(data)] = data
        sec += 1
        if sec >= 10:
            sec = 1
            head = (head + 1) & 1
            if head == 0:
                cyl += 1
        bx += 0x200
    return True


def load_resource(c, index):
    """0050:2820 -- res_fetch plus the resource-0 pointer relocation."""
    c.ww(c.ds, 0x3B84, index)
    ok = res_fetch(c, index)
    if index == 0:                             # relocate 138 word pointers
        for i in range(0x8A):
            v = c.rw(c.ds, 0x48C8 + i * 2)
            c.ww(c.ds, 0x48C8 + i * 2, (v + 0x48C8) & 0xFFFF)
    return ok


def render_cga(c, seg, path, pal=1):
    """Decode a CGA mode-4 buffer out of ported memory."""
    fb = bytes(c.mem[c.lin(seg, 0): c.lin(seg, 0) + 0x4000])
    P = ([(0,0,0),(85,255,255),(255,85,255),(255,255,255)] if pal == 1
         else [(0,0,0),(85,255,85),(255,85,85),(255,255,85)])
    img = Image.new('RGB', (320, 200)); px = img.load()
    for y in range(200):
        base = (0x2000 if (y & 1) else 0) + (y >> 1) * 80
        for xb in range(80):
            b = fb[base + xb]
            for p in range(4):
                px[xb*4+p, y] = P[(b >> (6 - 2*p)) & 3]
    img.resize((640, 400), Image.NEAREST).save(path)
    return sum(1 for b in fb if b)


if __name__ == '__main__':
    c, seg, off = boot_all()
    run_main_prologue(c)
    print(f'DS={c.ds:04X}  (data segment, derived by execution)')

    nz = render_cga(c, 0xB800, 'scratch/out/port_1_title.png')
    print(f'1. title screen from boot:      {nz} nonzero bytes')

    # The descriptor table lives at DS:136B. Show it before we use it.
    print('\ndescriptor pointers at DS:136B:',
          ' '.join(f'{c.rw(c.ds, 0x136B+i*2):04x}' for i in range(8)))

    ok = load_resource(c, 0x20)
    print(f'\nload_resource(0x20) -> {ok}')
    print('  [0x11A3] error =', c.rb(c.ds, 0x11A3))

    nz = render_cga(c, 0xB800, 'scratch/out/port_2_after_res20.png')
    print(f'2. after loading resource 0x20:  {nz} nonzero bytes')

    dis(c, 0x0050, 0x0564, 0x40, 'splash routine 0x564')
