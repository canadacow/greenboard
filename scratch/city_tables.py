"""Dump all six era city tables, restored from disk via the resource
directory (see get_dir.py). Records: 12-byte padded name + 12 data bytes,
24 bytes total, starting at +0x0C in each 1024-byte block."""
import sys, re
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
d = open('assets/pirates_1.img','rb').read()

ERAS = [(1560,0x054000),(1580,0x054400),(1600,0x054800),
        (1620,0x054c00),(1640,0x055000),(1660,0x055400)]
REC = 0x18

def records(off):
    blk = d[off:off+1024]
    out = []
    p = 0x0C
    while p + REC <= len(blk):
        rec = blk[p:p+REC]
        name = ''.join(chr(c) if 32<=c<127 else '' for c in rec[:12]).strip()
        if name and name[0].isalpha():
            out.append((name, rec[12:]))
        p += REC
    return out

allrecs = {}
for era, off in ERAS:
    r = records(off)
    allrecs[era] = r
    print(f'=== {era}  ({len(r)} cities, file {off:#08x}) ===')
    for name, t in r:
        print(f'  {name:<14s} ' + ' '.join(f'{v:02x}' for v in t))
    print()

# Which byte positions vary per era for a city present in all six?
common = set(n for n,_ in allrecs[1560])
for era in allrecs:
    common &= set(n for n,_ in allrecs[era])
print(f'cities present in all six eras: {len(common)}')
if common:
    city = sorted(common)[0]
    print(f'\nfield variation for {city}:')
    print(f'  {"era":>6} ' + ' '.join(f'b{i:<2d}' for i in range(12)))
    cols = []
    for era,_ in ERAS:
        t = dict(allrecs[era])[city]
        cols.append(t)
        print(f'  {era:>6} ' + ' '.join(f'{v:3d}' for v in t))
    print('\n  varying byte positions:',
          [i for i in range(12) if len(set(c[i] for c in cols)) > 1])
