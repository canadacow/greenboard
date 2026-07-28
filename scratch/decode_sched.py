"""Decode the fleet / silver-train schedule blocks.

Three regions share the header 11 87 11 00 31 00 31 48 and are followed by
~28 bytes of values <= 0x30 -- month/half codes. Decode them and check the
result against the printed manual, which for the Treasure Fleet gives:

  1640: Caracas Early Oct, Maracaibo Late Oct, Rio de la Hacha Early Nov,
        Santa Marta Late Nov, Puerto Bello Early Dec, Cartagena Early Jan,
        Campeche Early Feb, Vera Cruz Late Feb, Havana Late Mar,
        Florida Channel Late Apr
  1660: Caracas Early Sep, Maracaibo Late Sep, Rio de la Hacha Early Oct,
        Santa Marta Late Oct, Puerto Bello Early Nov, Cartagena Early Dec,
        Campeche Early Jan, Vera Cruz Late Jan, Havana Late Feb,
        Florida Channel Late Mar
  1680: Caracas Early Oct, Rio de la Hacha Late Oct, Santa Marta Early Nov,
        Puerto Bello Late Nov, Cartagena Late Dec, Campeche Late Jan,
        Vera Cruz Early Feb, Havana Early Mar, Florida Channel Late Apr

If a simple encoding reproduces those, it is the right table.
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
d = open('assets/pirates_1.img', 'rb').read()

BLOCKS = [(0x54fcc + 12, 1640), (0x553cc + 12, 1660), (0x557cc + 12, 1680)]
MONTH = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec']

def as_halfmonth(v):
    """Candidate encoding: value = month*2 + half, 1-based-ish."""
    if not (1 <= v <= 24):
        return None
    m, h = divmod(v - 1, 2)
    return f'{"Early" if h == 0 else "Late "} {MONTH[m]}'

for off, era in BLOCKS:
    raw = d[off:off + 32]
    print(f'=== {era} @ {off:#07x} ===')
    print('  header:', raw[:8].hex())
    body = raw[8:]
    print('  body  :', body.hex())
    print('  values:', ' '.join(f'{v:2d}' for v in body))
    print('  decoded (v = month*2 + half):')
    for i, v in enumerate(body):
        s = as_halfmonth(v)
        if s:
            print(f'    [{i:2d}] {v:#04x} {v:3d} -> {s}')
    print()
