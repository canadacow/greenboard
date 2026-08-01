# mousebug5.py -- what did mouse IRQ #4 interrupt, and where did its frame go?
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
o, n = sec['WRIT']
writes = np.frombuffer(buf, dtype=WRIT_DT, count=n, offset=o)
o, n = sec['STAT']
stats = np.frombuffer(buf, dtype=STAT_DT, count=n, offset=o)
o, n = sec['EXEC']
exec_ = np.frombuffer(buf, dtype=np.dtype('<u4'), count=n, offset=o)

for entry in (4133593, 4526759, 4529421, 4532067, 4535040):
    print(f'=== IRQ4 handler entry at instr {entry} ===')
    print('exec before/after:')
    for k in range(entry - 6, entry + 3):
        print(f'  instr {k:>9}: {exec_[k]:05X}')
    # STAT rows around it (may be gappy inside handlers)
    m = (stats['instr'] >= entry - 8) & (stats['instr'] <= entry + 4)
    for s in stats[m]:
        ip = (int(s['addr']) - int(s['cs']) * 16) & 0xFFFF
        print(f'  stat instr {s["instr"]:>9} {s["cs"]:04X}:{ip:04X} '
              f'SS:SP={s["ss"]:04X}:{s["sp"]:04X} FL={s["flags"]:04X}')
    # The interrupt frame pushes are stamped with the dispatch instr count.
    m = (writes['instr'] >= entry - 1) & (writes['instr'] <= entry) & \
        (writes['addr'] < 0xA0000)
    for r in writes[m]:
        print(f'  write instr {r["instr"]:>9}  [{r["addr"]:05X}] = {r["data"]:02X} '
              f' by {r["cs"]:04X}:{r["ip"]:04X}')
    print()
