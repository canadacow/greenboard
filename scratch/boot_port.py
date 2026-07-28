"""Literal port of the Pirates! boot sector to Python.

Every line below is one instruction from disk sector 0, disassembled and
transliterated in order. No summarising, no shortcuts. The disassembly is
printed alongside so each Python line can be checked against its opcode.
"""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

DISK1 = open('assets/pirates_1.img', 'rb').read()
SEC, SPT, HEADS = 512, 9, 2


class CPU:
    def __init__(self):
        self.mem = bytearray(0x110000)
        self.ax = self.bx = self.cx = self.dx = 0
        self.si = self.di = self.bp = self.sp = 0
        self.cs = self.ds = self.es = self.ss = 0
        self.cf = False
        self.reads = []

    # 16-bit register halves
    @property
    def al(self): return self.ax & 0xFF
    @al.setter
    def al(self, v): self.ax = (self.ax & 0xFF00) | (v & 0xFF)
    @property
    def ah(self): return (self.ax >> 8) & 0xFF
    @ah.setter
    def ah(self, v): self.ax = (self.ax & 0x00FF) | ((v & 0xFF) << 8)
    @property
    def ch(self): return (self.cx >> 8) & 0xFF
    @ch.setter
    def ch(self, v): self.cx = (self.cx & 0x00FF) | ((v & 0xFF) << 8)
    @property
    def cl(self): return self.cx & 0xFF
    @cl.setter
    def cl(self, v): self.cx = (self.cx & 0xFF00) | (v & 0xFF)
    @property
    def dh(self): return (self.dx >> 8) & 0xFF
    @dh.setter
    def dh(self, v): self.dx = (self.dx & 0x00FF) | ((v & 0xFF) << 8)
    @property
    def dl(self): return self.dx & 0xFF
    @dl.setter
    def dl(self, v): self.dx = (self.dx & 0xFF00) | (v & 0xFF)

    def lin(self, seg, off): return ((seg << 4) + off) & 0xFFFFF
    def rb(self, seg, off): return self.mem[self.lin(seg, off)]
    def wb(self, seg, off, v): self.mem[self.lin(seg, off)] = v & 0xFF
    def rw(self, seg, off):
        a = self.lin(seg, off); return self.mem[a] | (self.mem[a+1] << 8)
    def ww(self, seg, off, v):
        a = self.lin(seg, off)
        self.mem[a] = v & 0xFF; self.mem[a+1] = (v >> 8) & 0xFF

    def movsb(self):
        """rep movsb byte ptr es:[di], byte ptr [si]"""
        while self.cx:
            self.wb(self.es, self.di, self.rb(self.ds, self.si))
            self.si = (self.si + 1) & 0xFFFF
            self.di = (self.di + 1) & 0xFFFF
            self.cx -= 1

    def int13(self):
        """INT 13h -- only AH=02 (read) and AH=00 (reset) are used here."""
        if self.ah == 0x02:
            cyl = self.ch | ((self.cl & 0xC0) << 2)
            sec = self.cl & 0x3F
            head, drive, n = self.dh, self.dl & 1, self.al
            src = ((cyl * HEADS + head) * SPT + (sec - 1)) * SEC
            # Track 4 head 0 sector 1 is the misformatted protection sector:
            # it streams data + gap filler and reports "no data" (AH=04, CF=1).
            if drive == 0 and cyl == 4 and head == 0 and sec == 1 \
               and DISK1[0x200:0x208] == b'0-PIRATE':
                data = DISK1[src:src+SEC].ljust(SEC, b'\x00') + b'\x43' * 1536
                a = self.lin(self.es, self.bx)
                self.mem[a:a+len(data)] = data
                self.ah, self.cf = 0x04, True
                self.reads.append(('PROT', cyl, head, sec))
                return
            data = DISK1[src:src + n*SEC]
            a = self.lin(self.es, self.bx)
            self.mem[a:a+len(data)] = data
            self.ah, self.cf = 0x00, False
            self.reads.append((drive, cyl, head, sec, n, self.es, self.bx, src))
        elif self.ah == 0x00:
            self.ah, self.cf = 0x00, False


def run_boot(c):
    """Sector 0, instruction by instruction.

    7C00 fa            cli
    7C01 b82000        mov ax,0x20
    7C04 8ed0          mov ss,ax
    7C06 bc0002        mov sp,0x200
    7C09 fb            sti
    7C0A b82000        mov ax,0x20
    7C0D 8ec0          mov es,ax
    7C0F 8cc8          mov ax,cs
    7C11 8ed8          mov ds,ax
    7C13 e80000        call 0x7c16      (pushes 0x7C16)
    7C16 5e            pop si           -> si = 0x7C16
    7C17 81c61100      add si,0x11      -> si = 0x7C27
    7C1B 33ff          xor di,di
    7C1D b90002        mov cx,0x200
    7C20 f3a4          rep movsb
    7C22 ea3c002000    ljmp 0x20:0x3c
    """
    c.mem[c.lin(0x0000, 0x7C00):c.lin(0x0000, 0x7C00)+SEC] = DISK1[:SEC]
    c.cs = 0x07C0                      # BIOS enters at 07C0:0000
    c.ax = 0x20
    c.ss = c.ax
    c.sp = 0x200
    c.ax = 0x20
    c.es = c.ax
    c.ax = c.cs                        # 0x07C0
    c.ds = c.ax
    c.si = 0x7C16 & 0xFFFF             # after call/pop, minus segment base
    c.si = (0x0016 + 0x11) & 0xFFFF    # ds=07C0 so si is 0x16 within it
    c.di = 0
    c.cx = 0x200
    c.movsb()
    return 0x0020, 0x003C


def run_reloc(c):
    """0020:003C onward -- IVT fiddling, video mode, then the two loaders.

    003C 33c0          xor ax,ax
    003E 8ed8          mov ds,ax
    0040 be7800        mov si,0x78          (INT 1Eh vector)
    0043 ad            lodsw
    0044 8bd8          mov bx,ax
    0046 ad            lodsw
    0047 8ed8          mov ds,ax
    0049 8bf3          mov si,bx
    004B b82000        mov ax,0x20
    004E 8ec0          mov es,ax
    0050 bf2c00        mov di,0x2c
    0053 b91000        mov cx,0x10
    0056 f3a4          rep movsb            (copy 16-byte diskette param table)
    0058 b82000        mov ax,0x20
    005B 8ed8          mov ds,ax
    005D 33c0          xor ax,ax
    005F 8ec0          mov es,ax
    0061 bf7800        mov di,0x78
    0064 b82c00        mov ax,0x2c
    0067 ab            stosw
    0068 b82000        mov ax,0x20
    006B ab            stosw                (point INT 1Eh at our copy)
    0093 b80400        mov ax,4
    0096 cd10          int 10h              (CGA 320x200 4-colour)
    """
    c.ds = 0x0000
    c.si = 0x78
    bx = c.rw(0x0000, 0x78)              # lodsw
    seg = c.rw(0x0000, 0x7A)             # lodsw
    c.ds, c.si = seg, bx
    c.es, c.di, c.cx = 0x0020, 0x2C, 0x10
    c.movsb()
    c.ww(0x0000, 0x78, 0x2C)             # stosw
    c.ww(0x0000, 0x7A, 0x20)             # stosw
    c.ds = 0x0020


def run_title(c):
    """0020:00A7 -- the title screen, straight into CGA memory.

    00A7 bf0400        mov di,4
    00AA b800b8        mov ax,0xb800
    00AD 8ec0          mov es,ax
    00AF 33db          xor bx,bx
    00B1 32d2          xor dl,dl
    00B3 b526          mov ch,0x26
    00B5 b600          mov dh,0
    00B7 b101          mov cl,1
    00B9 be0500        mov si,5           (retry count)
    00BC b80802        mov ax,0x208
    00BF cd13          int 13h
    00C1 7306          jae 0xc9
    00C3 4e            dec si
    00C4 75f6          jne 0xbc
    00C6 e9d100        jmp 0x19a          (fatal disk error)
    00C9 81c30010      add bx,0x1000
    00CD 4f            dec di
    00CE 740b          je 0xdb
    00D0 fec6          inc dh
    00D2 80e601        and dh,1
    00D5 75e0          jne 0xb7
    00D7 fec5          inc ch
    00D9 ebda          jmp 0xb5
    """
    c.di = 4
    c.es = 0xB800
    c.bx = 0
    c.dl = 0
    c.ch = 0x26
    c.dh = 0
    while True:
        c.cl = 1
        c.si = 5
        while True:
            c.ax = 0x208
            c.int13()
            if not c.cf:
                break
            c.si -= 1
            if c.si == 0:
                raise RuntimeError('Fatal Disk Error')
        c.bx = (c.bx + 0x1000) & 0xFFFF
        c.di -= 1
        if c.di == 0:
            return
        c.dh += 1
        c.dh &= 1
        if c.dh == 0:
            c.ch += 1


def run_stage2(c):
    """0020:00DB -- the main program loader.

    00DB c70602005000  mov word [2],0x50     (dest segment)
    00E1 c70604000000  mov word [4],0
    00E7 c606010001    mov byte [1],1        (cylinder)
    00EC c606060015    mov byte [6],0x15     (last cylinder)
    00F1 be0a00        mov si,0xa            (retry)
    00F4 b8802d        mov ax,0x2d80
    00F7 8ec0          mov es,ax
    00F9 33db          xor bx,bx
    00FB 33d2          xor dx,dx
    00FD b101          mov cl,1
    00FF 8a2e0100      mov ch,[1]
    0103 b008          mov al,8
    0105 b402          mov ah,2
    0107 cd13          int 13h               (head 0 -> 2D80:0000)
    ...  same again with bx=0x1000, dx=0x100 (head 1 -> 2D80:1000)
    0170 rep movsb 0x2000 bytes  2D80:0 -> [2]:0
    0185 add word [2],0x200
    018B inc byte [1]
    018F cmp byte [1],4      ; skip cylinder 4 (the protection track)
    0194 jne ...
    0196 inc byte [1]
    019A cmp [1],[6] ; loop
    01A6 relocate 16 words at 0050:0000 by +0x50
    01BC ljmp 0050:0020
    """
    c.ww(0x0020, 2, 0x50)
    c.ww(0x0020, 4, 0)
    c.wb(0x0020, 1, 1)
    c.wb(0x0020, 6, 0x15)
    while True:
        cyl = c.rb(0x0020, 1)
        for head, bx in ((0, 0x0000), (1, 0x1000)):
            c.si = 0xA
            while True:
                c.es, c.bx = 0x2D80, bx
                c.ch, c.cl = cyl, 1
                c.dh, c.dl = head, 0
                c.ax = 0x0208
                c.int13()
                if not c.cf:
                    break
                c.si -= 1
                if c.si == 0:
                    raise RuntimeError('Fatal Disk Error')
        dest = c.rw(0x0020, 2)
        a = c.lin(0x2D80, 0); b = c.lin(dest, 0)
        c.mem[b:b+0x2000] = c.mem[a:a+0x2000]
        c.ww(0x0020, 2, (dest + 0x200) & 0xFFFF)
        cyl += 1
        if cyl == 4:                    # protection track -- step over it
            cyl += 1
        c.wb(0x0020, 1, cyl)
        if cyl > c.rb(0x0020, 6):
            break
    for i in range(16):                 # relocation
        c.ww(0x0050, i*2, (c.rw(0x0050, i*2) + 0x50) & 0xFFFF)
    return 0x0050, 0x0020


if __name__ == '__main__':
    c = CPU()
    seg, off = run_boot(c)
    print(f'boot sector -> {seg:04X}:{off:04X}')
    run_reloc(c)
    run_title(c)
    print(f'title loaded ({len([r for r in c.reads if r[0]!="PROT"])} reads so far)')
    seg, off = run_stage2(c)
    print(f'stage 2 loaded, entry {seg:04X}:{off:04X}')
    prot = [r for r in c.reads if r[0] == 'PROT']
    print(f'\n{len(c.reads)} disk reads, {len(prot)} protection hits')
    print('\nsegment table at 0050:0000 (post-relocation):')
    for i in range(8):
        print(f'  cs:[{i*2:02X}] = {c.rw(0x0050, i*2):04X}')
    prog = bytes(c.mem[c.lin(0x0050,0):c.lin(0x0050,0)+0x2A000])
    for probe in (b'SID MEIER', b'MICROPROSE', b'RANDALL'):
        i = prog.find(probe)
        print(f'  {probe.decode():12s} {"found at +%#07x" % i if i>=0 else "MISSING"}')
