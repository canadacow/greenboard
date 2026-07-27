import sys, collections, math
sys.stdout.reconfigure(encoding='utf-8')

d = open('assets/pirates_1.img','rb').read()
LO, HI = 0x1c000, 0x32000
region = d[LO:HI]

print(f'region 0x{LO:x}-0x{HI:x} len={len(region)}')
print()

# 1. Byte histogram of the region -- what is actually in here?
c = collections.Counter(region)
print('top 24 byte values in region:')
for b, n in c.most_common(24):
    print(f'  {b:02x}  {n:7d}  {100*n/len(region):5.2f}%')
print(f'distinct byte values: {len(c)}')
print()

# 2. Nibble distribution -- 4bpp CGA/EGA data would show structure
nib = collections.Counter()
for b in region:
    nib[b >> 4] += 1
    nib[b & 15] += 1
print('nibble histogram:', dict(sorted(nib.items())))
print()

# 3. Run-length structure. Packed graphics = many short runs; RLE-compressed = long runs
runs = []
cur = region[0]; n = 1
for b in region[1:]:
    if b == cur: n += 1
    else:
        runs.append((cur, n)); cur = b; n = 1
runs.append((cur, n))
rl = collections.Counter(min(n, 33) for _, n in runs)
print(f'total runs: {len(runs)}, mean run len: {len(region)/len(runs):.2f}')
print('run-length histogram (33=33+):')
for k in sorted(rl):
    print(f'  len {k:3d}: {rl[k]:6d}')
print()

# 4. Longest runs and what byte they are
longest = sorted(runs, key=lambda r: -r[1])[:12]
print('longest runs:', [(f'{b:02x}', n) for b, n in longest])
