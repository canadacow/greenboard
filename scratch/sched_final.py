"""Decode the six schedule blocks (0x400 stride) against the printed manual.

Manual, Treasure Fleet, 1660:
    Caracas Early Sep, Maracaibo Late Sep, Rio de la Hacha Early Oct,
    Santa Marta Late Oct, Puerto Bello Early Nov, Cartagena Early Dec,
    Campeche Early Jan, Vera Cruz Late Jan, Havana Late Feb,
    Florida Channel Late Mar

As month-half codes those are, with Jan=1:
    Sep=17/18, Oct=19/20, Nov=21/22, Dec=23/24,
    Jan=1/2, Feb=3/4, Mar=5/6
    -> 17, 20, 19, 20, 21, 23, 1, 2, 4, 6
The 1660 block body is
    06 04 0a 0e 19 19 1d 05 05 13 13 13 18 18 18 01 04 0e 19 1d 18 18 05 05
Try several origins/offsets and report which reproduces the manual list.
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
d = open('assets/pirates_1.img', 'rb').read()

BLOCKS = [(0x543d8, 1560), (0x547d8, 1600), (0x54bd8, 1620),
          (0x54fd8, 1640), (0x553d8, 1660), (0x557d8, 1680)]
MON = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec']

# Manual answers for the FLEET, by era, as (month index 0-11, half 0=early 1=late)
MANUAL_FLEET = {
 1660: [('Sep',0),('Sep',1),('Oct',0),('Oct',1),('Nov',0),
        ('Dec',0),('Jan',0),('Jan',1),('Feb',1),('Mar',1)],
 1640: [('Oct',0),('Oct',1),('Nov',0),('Nov',1),('Dec',0),
        ('Jan',0),('Feb',0),('Feb',1),('Mar',1),('Apr',1)],
}

def show(v, base, permonth):
    """Interpret v as a date code under a given origin/scale."""
    if permonth == 2:
        idx = v - base
        if idx < 0: return None
        m, h = divmod(idx, 2)
        if m > 11: return None
        return (MON[m], h)
    return None

for off, era in BLOCKS:
    raw = d[off:off+32]
    body = raw[8:]
    print(f'=== {era} @ {off:#07x}  hdr={raw[:8].hex()} ===')
    print('  body:', ' '.join(f'{v:02x}' for v in body))
    for base in (1, 0):
        dec = [show(v, base, 2) for v in body]
        s = ' '.join(f'{m}{"L" if h else "E"}' if t else '--'
                     for t in dec for m, h in [t or ('',0)])
        print(f'   base={base}: {s}')
    if era in MANUAL_FLEET:
        want = MANUAL_FLEET[era]
        print('   manual  :', ' '.join(f'{m}{"L" if h else "E"}' for m, h in want))
        # what codes would the manual require, for base=1?
        codes = [MON.index(m)*2 + h + 1 for m, h in want]
        print('   -> codes:', ' '.join(f'{c:02x}' for c in codes))
    print()
