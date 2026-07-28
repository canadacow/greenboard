"""Extract the fleet / silver-train schedule from the binary.

Structure found on disk 1: for each historical era there is a block of
24-byte city records (12-byte padded name + 12 bytes of stats), and the block
is FOLLOWED by ~40 bytes of small values with no name field -- the schedule.

Era blocks are identified by their trailing 'FLORIDA CHNL' record, which is
the fleet's last stop and appears twice per era.
"""
import sys, re, struct
sys.stdout.reconfigure(encoding='utf-8')
d = open('assets/pirates_1.img', 'rb').read()

REC = 0x18
ends = [m.start() for m in re.finditer(rb'FLORIDA CHNL', d)]
print(f'FLORIDA CHNL occurrences: {[hex(x) for x in ends]}\n')

# Each era block ends with two FLORIDA CHNL records; the second is followed
# by the schedule bytes.
blocks = []
for i in range(0, len(ends) - 1, 2):
    a, b = ends[i], ends[i+1]
    if b - a == REC:
        blocks.append(b)
print(f'{len(blocks)} era blocks (schedule begins after each second FLORIDA CHNL)\n')

MONTH = ['January','February','March','April','May','June',
         'July','August','September','October','November','December']

def decode_half(v):
    """Small values look like month-half codes: 0..23 -> (month, early/late)."""
    if v == 0 or v > 0x30:
        return None
    m, h = divmod(v - 1, 2) if v <= 24 else (None, None)
    if m is None or m > 11:
        return None
    return f'{"Early" if h == 0 else "Late"} {MONTH[m]}'

ERAS = [1560, 1600, 1620, 1640, 1660, 1680]
for i, b in enumerate(blocks):
    start = b + REC          # just past the second FLORIDA CHNL record
    raw = d[start:start + 48]
    era = ERAS[i] if i < len(ERAS) else f'block{i}'
    print(f'=== era {era}  schedule bytes at {start:#07x} ===')
    print('  hex:', raw.hex())
    print('  dec:', ' '.join(f'{v:3d}' for v in raw))
    dec = [decode_half(v) for v in raw]
    named = [(j, v, s) for j, (v, s) in enumerate(zip(raw, dec)) if s]
    if named:
        print('  as month-halves:')
        for j, v, s in named[:24]:
            print(f'    [{j:2d}] {v:#04x} = {s}')
    print()
