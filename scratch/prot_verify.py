import sys
sys.stdout.reconfigure(encoding='utf-8')
for n in (1, 2):
    d = open(f'assets/pirates_{n}.img', 'rb').read()
    print(f'disk {n}: label@0x200 = {d[0x200:0x220]!r}')
    print(f'   MicroProse protected booter: {d[0x200:0x208] == b"0-PIRATE"}')
    # C=4 H=0 S=1 with SPT=9 -> offset
    off = ((4*2 + 0)*9 + 0) * 512
    print(f'   C=4 H=0 S=1 -> file offset 0x{off:06x}')
    print(f'   first 32 bytes: {d[off:off+32].hex()}')
    print(f'   0x43 count in that sector: {d[off:off+512].count(0x43)}/512')
    print()
