# mousebug4.py -- walk SP through the INT 16h handler body to find where
# the stack gains 4 bytes.
import sys
import numpy as np

sys.stdout.reconfigure(encoding='utf-8')

PATH = 'bench_trace.bcfg'
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
o, n = sec['STAT']
stats = np.frombuffer(buf, dtype=STAT_DT, count=n, offset=o)
o, n = sec['EXEC']
exec_ = np.frombuffer(buf, dtype=np.dtype('<u4'), count=n, offset=o)

# Window: from the HostFS int 16h (instr ~4533100) to the poisoned IRET.
m = (stats['instr'] >= 4533096) & (stats['instr'] <= 4533250)
w = stats[m]
print(f'{len(w)} state records in window')
prev = None
for s in w:
    ip = (int(s['addr']) - int(s['cs']) * 16) & 0xFFFF
    gap = ''
    if prev is not None:
        di = int(s['instr']) - prev
        if di != 1:
            gap = f'  <-- GAP of {di - 1} untraced instr(s)'
    prev = int(s['instr'])
    print(f'  instr {s["instr"]:>9} {s["cs"]:04X}:{ip:04X} SP={s["sp"]:04X} '
          f'FL={s["flags"]:04X}{gap}')

# For gaps, show the exec timeline (addresses only) so we can see what ran.
print('\nexec timeline for the window:')
for k in range(4533096, 4533252):
    print(f'  instr {k:>9}: {exec_[k]:05X}')
