"""Restore the Treasure Fleet / Silver Train schedule from disk.

Located by tracing the loader: resources 4,6,7,8,9,10 each load 1024 bytes
to DS:4240 -- one per era. Each block is 24-byte city records starting at
+0x0C, terminated by two FLORIDA CHNL entries, followed by 28 bytes of
schedule data.

Values are month-half codes. Checking against the printed manual for 1660
Treasure Fleet (Caracas Early Sep ... Florida Channel Late Mar) fixes the
encoding.
"""
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
d = open('assets/pirates_1.img','rb').read()
ERAS = [(1560,0x054000),(1580,0x054400),(1600,0x054800),
        (1620,0x054c00),(1640,0x055000),(1660,0x055400)]
MON = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec']

def tail(off):
    blk = d[off:off+1024]
    i = blk.find(b'FLORIDA CHNL')
    j = blk.find(b'FLORIDA CHNL', i+1)
    return blk[j+0x18:]

print('=== raw schedule tails ===')
tails = {}
for era, off in ERAS:
    t = tail(off)
    tails[era] = t
    print(f'  {era}: ' + ' '.join(f'{v:02x}' for v in t))

def half(v):
    """v -> 'Early/Late Month'. Try 1-based: v=1 -> Early Jan."""
    if not (1 <= v <= 24):
        return None
    m, h = divmod(v-1, 2)
    return f'{"Early" if h==0 else "Late "} {MON[m]}'

print('\n=== decoded (v = (month-1)*2 + half + 1) ===')
for era, off in ERAS:
    t = tails[era]
    out = []
    for v in t:
        s = half(v)
        out.append(s if s else f'[{v:02x}]')
    print(f'\n{era}:')
    for k in range(0, len(out), 7):
        print('   ' + ' | '.join(f'{x:<10s}' for x in out[k:k+7]))

print('\n=== manual check, 1660 Treasure Fleet ===')
manual = ['Early Sep','Late  Sep','Early Oct','Late  Oct','Early Nov',
          'Early Dec','Early Jan','Late  Jan','Late  Feb','Late  Mar']
print('  manual :', ' '.join(manual))
codes = []
for s in manual:
    e = s.startswith('Early')
    m = MON.index(s[-3:])
    codes.append(m*2 + (0 if e else 1) + 1)
print('  needs  :', ' '.join(f'{c:02x}' for c in codes))
print('  1660   :', ' '.join(f'{v:02x}' for v in tails[1660]))
