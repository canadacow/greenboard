"""Dump the city table around 0x054000 on disk 1: fixed-stride records with
an ASCII name plus binary fields (very likely x/y map position, nationality,
population, defences)."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
d=open('assets/pirates_1.img','rb').read()

LO,HI=0x053f00,0x055400
seg=d[LO:HI]

# Find every ASCII name and its offset; infer the record stride.
import re
names=[(m.start()+LO, m.group().decode()) for m in
       re.finditer(rb'[A-Z][A-Z .\']{3,20}', seg)]
print(f'{len(names)} ASCII names in 0x{LO:06x}-0x{HI:06x}\n')
prev=None
for off,nm in names:
    delta=off-prev if prev is not None else 0
    print(f'  0x{off:06x}  (+{delta:3d})  {nm!r}')
    prev=off

if len(names)>2:
    deltas=[names[i+1][0]-names[i][0] for i in range(len(names)-1)]
    from collections import Counter
    print('\nstride histogram:', Counter(deltas).most_common(6))

# Dump raw bytes of the first few records at the dominant stride.
print('\n=== raw records ===')
for off,nm in names[:8]:
    rec=d[off-8:off+40]
    print(f'  {nm:<14s} @0x{off:06x}: {rec.hex()}')
