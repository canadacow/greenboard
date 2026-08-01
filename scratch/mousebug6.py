# mousebug6.py -- compare CTMOUSE ISR invocation #3 (healthy) vs #4 (poison).
import sys
import numpy as np

sys.stdout.reconfigure(encoding='utf-8')

PATH = 'bench_trace.bcfg'
STAT_DT = np.dtype([('instr', '<u8'), ('addr', '<u4'),
                    ('ax', '<u2'), ('cx', '<u2'), ('dx', '<u2'), ('bx', '<u2'),
                    ('sp', '<u2'), ('bp', '<u2'), ('si', '<u2'), ('di', '<u2'),
                    ('es', '<u2'), ('cs', '<u2'), ('ss', '<u2'), ('ds', '<u2'),
                    ('flags', '<u2'), ('pad', '<u2')])
WRIT_DT = np.dtype([('instr', '<u8'), ('addr', '<u4'), ('cs', '<u2'),
                    ('ip', '<u2'), ('data', 'u1'), ('r', 'u1'), ('pad', '<u2')])

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
o, n = sec['EXEC']
exec_ = np.frombuffer(buf, dtype=np.dtype('<u4'), count=n, offset=o)
o, n = sec['WRIT']
writes = np.frombuffer(buf, dtype=WRIT_DT, count=n, offset=o)

for entry, label in ((4529421, '#3 healthy'), (4532067, '#4 poison')):
    print(f'=== ISR invocation {label} (entry instr {entry}) ===')
    for k in range(entry, entry + 45):
        a = int(exec_[k])
        seg_ip = f'1232:{(a - 0x12320) & 0xFFFF:04X}' if 0x12320 <= a < 0x14320 else f'lin {a:05X}'
        print(f'  instr {k:>9}: {a:05X}  {seg_ip}')
    # writes made during the handler (stack ops)
    m = (writes['instr'] >= entry) & (writes['instr'] <= entry + 45) & \
        (writes['addr'] < 0xA0000)
    print('  writes during handler:')
    for r in writes[m]:
        print(f'    instr {r["instr"]:>9}  [{r["addr"]:05X}] = {r["data"]:02X} '
              f' by {r["cs"]:04X}:{r["ip"]:04X}')
    print()
