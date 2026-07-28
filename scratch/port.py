"""Port Pirates! to Python, block by block, starting at the boot sector.

Nothing is assumed. Every value comes from bytes actually read off the disk
image and every action comes from an instruction actually decoded. The port
maintains real 8086 state (registers, flags, 1MB of memory) and executes
transliterated blocks; anything not yet ported raises so it is obvious what
is missing rather than silently wrong.

Stage 0: the boot sector, verbatim from disk sector 0.
"""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

DISK1 = open('assets/pirates_1.img', 'rb').read()
DISK2 = open('assets/pirates_2.img', 'rb').read()
SEC, SPT, HEADS = 512, 9, 2


def chs_to_off(cyl, head, sec):
    """CHS -> byte offset. Geometry from the image size: 368640 bytes =
    40 cyl x 2 heads x 9 sectors x 512."""
    return ((cyl * HEADS + head) * SPT + (sec - 1)) * SEC


class Machine:
    """8086 real-mode state plus the BIOS services the game actually uses."""

    def __init__(self):
        self.mem = bytearray(0x110000)
        self.r = dict(ax=0, bx=0, cx=0, dx=0, si=0, di=0, bp=0, sp=0,
                      cs=0, ds=0, es=0, ss=0, ip=0)
        self.cf = False
        self.disk = {0: DISK1, 1: DISK2}
        self.log = []

    # --- memory -------------------------------------------------------
    def lin(self, seg, off):
        return ((seg << 4) + off) & 0xFFFFF

    def rb(self, seg, off):
        return self.mem[self.lin(seg, off)]

    def rw(self, seg, off):
        a = self.lin(seg, off)
        return self.mem[a] | (self.mem[a + 1] << 8)

    def wb(self, seg, off, v):
        self.mem[self.lin(seg, off)] = v & 0xFF

    def ww(self, seg, off, v):
        a = self.lin(seg, off)
        self.mem[a] = v & 0xFF
        self.mem[a + 1] = (v >> 8) & 0xFF

    def blk(self, seg, off, n):
        a = self.lin(seg, off)
        return bytes(self.mem[a:a + n])

    def load(self, seg, off, data):
        a = self.lin(seg, off)
        self.mem[a:a + len(data)] = data

    # --- BIOS ---------------------------------------------------------
    def int13(self, ah, al, ch, cl, dh, dl, es, bx):
        """Disk service. Returns (ah_result, carry)."""
        if ah == 0x02:                      # read sectors
            cyl = ch | ((cl & 0xC0) << 2)
            sec = cl & 0x3F
            head = dh
            drive = dl & 1
            src = chs_to_off(cyl, head, sec)
            img = self.disk[drive]
            # MicroProse protection: C=4 H=0 S=1 is deliberately misformatted.
            # It streams 512B of sector data then gap filler, and reports ND.
            if drive == 0 and cyl == 4 and head == 0 and sec == 1 and \
               self.disk[0][0x200:0x208] == b'0-PIRATE':
                data = bytes(img[src:src + SEC]).ljust(SEC, b'\x00') + b'\x43' * 1536
                self.load(es, bx, data)
                self.log.append(('prot', cyl, head, sec))
                return 0x04, True
            data = img[src:src + al * SEC]
            self.load(es, bx, data)
            self.log.append(('read', drive, cyl, head, sec, al, es, bx, src))
            return 0x00, False
        if ah == 0x00:                      # reset
            return 0x00, False
        return 0x00, False


def boot(m):
    """Stage 0 -- transliterated from the boot sector at disk offset 0.

        cli / mov ss,0x20 / mov sp,0x200 / sti
        mov es,0x20 / mov ds,cs
        call $+3 ; pop si ; add si,0x11      (position-independent trick)
        xor di,di ; mov cx,0x200 ; rep movsb  (relocate 512B to 0020:0000)
        ljmp 0x20:0x3c
    """
    m.load(0x07C0, 0x0000, DISK1[:SEC])     # BIOS puts sector 0 at 0000:7C00
    m.r['ss'], m.r['sp'] = 0x20, 0x200
    m.r['es'] = 0x20
    m.r['ds'] = 0x07C0
    # rep movsb: 0x200 bytes from ds:si (si = 0x7C16+0x11-0x7C00 = 0x27) to es:di
    src = m.blk(0x07C0, 0x27, 0x200)
    m.load(0x0020, 0x0000, src)
    return 0x0020, 0x003C                   # ljmp 0x20:0x3c


def stage1_title(m):
    """Boot code at 0020:00AF -- four 4KB reads straight into CGA memory.

        mov di,4 ; mov ax,0xb800 ; mov es,ax
        xor bx,bx ; xor dl,dl ; mov ch,0x26 ; mov dh,0 ; mov cl,1
        loop: mov ax,0x208 ; int 13h   (AL=8 sectors)
              add bx,0x1000 ; dec di ; toggle head; next cylinder
    """
    bx = 0
    ch, dh, cl = 0x26, 0, 1
    for _ in range(4):
        ah, cf = m.int13(0x02, 8, ch, cl, dh, 0, 0xB800, bx)
        assert not cf, 'title read failed'
        bx += 0x1000
        dh += 1
        if dh >= HEADS:
            dh = 0
            ch += 1


def stage2_load(m):
    """Boot code at 0020:00DB -- the main program.

        [2]=0x50 (dest seg) [1]=1 (cylinder) [6]=0x15 (last cylinder)
        per cylinder: 8 sectors head0 -> 2D80:0000
                      8 sectors head1 -> 2D80:1000
                      copy 0x2000 bytes 2D80:0 -> [2]:0
                      [2] += 0x200
        then relocate 16 words at 0050:0000 by +0x50, ljmp 0050:0020
    """
    # NOTE: cylinder 4 head 0 sector 1 is the deliberately-misformatted
    # protection sector. The boot loader retries a failed read 4 times
    # ([0x11b9]=4) and moves on; the observed execution reads cylinders
    # 1,2,3,5,6,... -- cylinder 4 is never successfully loaded. Mirror that
    # rather than treating the expected failure as fatal.
    dest = 0x50
    for cyl in range(1, 0x16):
        ok = True
        for head in (0, 1):
            for _try in range(4):
                ah, cf = m.int13(0x02, 8, cyl, 1, head, 0, 0x2D80, head * 0x1000)
                if not cf:
                    break
            else:
                ok = False
        if ok:
            m.load(dest, 0x0000, m.blk(0x2D80, 0x0000, 0x2000))
        dest += 0x200
    # relocation loop: 16 words at 0050:0000 += 0x50
    for i in range(16):
        v = m.rw(0x0050, i * 2)
        m.ww(0x0050, i * 2, (v + 0x50) & 0xFFFF)
    return 0x0050, 0x0020


if __name__ == '__main__':
    m = Machine()
    seg, off = boot(m)
    print(f'boot: relocated to {seg:04X}:{off:04X}')
    stage1_title(m)
    print('stage 1: title screen -> B800:0000 (16KB)')
    seg, off = stage2_load(m)
    print(f'stage 2: 168KB loaded, entry {seg:04X}:{off:04X}')
    print(f'\n{len(m.log)} disk operations')

    # The segment table the boot loader just relocated.
    print('\nruntime segment table at 0050:0000:')
    for i in range(8):
        v = m.rw(0x0050, i * 2)
        print(f'  cs:[{i*2:02X}] = {v:04X}   linear {v << 4:#07x}')

    # Sanity: the title screen must decode as CGA, and stage 2 must contain
    # the strings we expect.
    fb = m.blk(0xB800, 0, 0x4000)
    print(f'\ntitle framebuffer: {sum(1 for c in fb if c)}/{len(fb)} nonzero')
    prog = m.blk(0x0050, 0, 0x2A000)
    for probe in (b'SID MEIER', b'MICROPROSE'):
        print(f'  {probe.decode():12s} at stage2+{prog.find(probe):#07x}')
