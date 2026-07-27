"""Decode the 24-byte city records.
Layout guess: 12 bytes name, then 12 bytes of fields. Print them as bytes and
as words so the x/y map coordinates become obvious (they should cluster in a
sensible range and differ per city)."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
d=open('assets/pirates_1.img','rb').read()

BASE=0x05400c      # first record (BORBURATA)
STRIDE=24
N=31

print(f'{"name":<14}{"bytes 12..23":<40}{"words"}')
print('-'*96)
recs=[]
for i in range(N):
    off=BASE+i*STRIDE
    rec=d[off:off+STRIDE]
    name=rec[:12].decode('ascii','replace').strip()
    tail=rec[12:]
    words=struct.unpack('<6H',tail)
    recs.append((name,tail,words))
    print(f'{name:<14}{tail.hex():<40}{" ".join(f"{w:5d}" for w in words)}')

print('\n=== per-field ranges (looking for map coordinates) ===')
for f in range(12):
    vals=[r[1][f] for r in recs]
    print(f'  byte[{f:2d}]  min={min(vals):3d} max={max(vals):3d} distinct={len(set(vals)):2d}')
for f in range(6):
    vals=[r[2][f] for r in recs]
    print(f'  word[{f}]   min={min(vals):5d} max={max(vals):5d} distinct={len(set(vals)):2d}')
