# mousebug2.py -- find where the trap flag turned on and who supplied it.
import sys
import numpy as np

sys.stdout.reconfigure(encoding='utf-8')

PATH = 'bench_trace.bcfg'
WRIT_DT = np.dtype([('instr', '<u8'), ('addr', '<u4'), ('cs', '<u2'),
                    ('ip', '<u2'), ('data', 'u1'), ('r', 'u1'), ('pad', '<u2')])
STAT_DT = np.dtype([('instr', '<u8'), ('addr', '<u4'),
                    ('ax', '<u2'), ('cx', '<u2'), ('dx', '<u2'), ('bx', '<u2'),
                    ('sp', '<u2'), ('bp', '<u2'), ('si', '<u2'), ('di', '<u2'),
                    ('es', '<u2'), ('cs', '<u2'), ('ss', '<u2'), ('ds', '<u2'),
                    ('flags', '<u2'), ('pad', '<u2')])

def load(path):
    buf = np.memmap(path, dtype=np.uint8, mode='r')
    assert bytes(buf[0:4]) == b'BCFG'
    off = 8
    sizes = {'NODE': 28, 'EDGE': 8, 'WRIT': 20, 'READ': 20, 'PLNW': 20,
             'EXEC': 4, 'STAT': 40, 'PORT': 18}
    sec = {}
    while off < len(buf):
        tag = bytes(buf[off:off + 4]).decode()
        n = int(np.frombuffer(buf[off + 4:off + 12], '<u8')[0])
        off += 12
        sec[tag] = (off, n)
        off += n * sizes[tag]
    return buf, sec

buf, sec = load(PATH)

def arr(tag, dt):
    o, n = sec[tag]
    return np.frombuffer(buf, dtype=dt, count=n, offset=o)

exec_ = arr('EXEC', np.dtype('<u4'))
writes = arr('WRIT', WRIT_DT)
stats = arr('STAT', STAT_DT)
print(f'{len(stats)} state records, {len(exec_)} exec, {len(writes)} writes')

tf = (stats['flags'] & 0x100) != 0
idx = np.flatnonzero(tf)
print(f'records with TF set: {len(idx)}')
if len(idx):
    first = int(idx[0])
    print(f'first TF-set state: record {first}, instr {stats[first]["instr"]}')
    lo = max(0, first - 12)
    for i in range(lo, min(first + 12, len(stats))):
        s = stats[i]
        mark = ' <-- TF ON' if tf[i] else ''
        ip = (s['addr'] - s['cs'] * 16) & 0xFFFF
        print(f'  instr {s["instr"]:>9} {s["cs"]:04X}:{ip:04X} '
              f'lin={s["addr"]:05X} AX={s["ax"]:04X} SP={s["ss"]:04X}:{s["sp"]:04X} '
              f'FL={s["flags"]:04X}{mark}')

    i0 = int(stats[first - 1]['instr']) if first else 0
    i1 = int(stats[first]['instr'])
    print(f'\nexec timeline instr {i0}..{i1} '
          f'({i1 - i0} instrs, showing <= 60):')
    for k in range(i0, min(i0 + 60, i1 + 1)):
        print(f'  instr {k:>9}: {exec_[k]:05X}')
    if i1 - i0 > 60:
        print('  ...')
        for k in range(max(i1 - 10, i0 + 60), i1 + 1):
            print(f'  instr {k:>9}: {exec_[k]:05X}')

    s = stats[first]
    sp_lin = int(s['ss']) * 16 + ((int(s['sp']) - 8) & 0xFFFF)
    lo_a, hi_a = sp_lin, sp_lin + 16
    m = (writes['addr'] >= lo_a) & (writes['addr'] < hi_a) & \
        (writes['instr'] <= int(s['instr']) + 1)
    w = writes[m]
    print(f'\nwrites to stack {lo_a:05X}-{hi_a:05X} before instr {s["instr"]} '
          f'(last 24 of {len(w)}):')
    for r in w[-24:]:
        print(f'  instr {r["instr"]:>9}  [{r["addr"]:05X}] = {r["data"]:02X}  '
              f'by {r["cs"]:04X}:{r["ip"]:04X}')
