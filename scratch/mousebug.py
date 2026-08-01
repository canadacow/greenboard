# mousebug.py -- analyze bench_trace.bcfg for the CTMOUSE wild-execution bug.
#
# Questions:
#  Q1: For each IRQ4 handler invocation (CTMOUSE at 1232:019A), where does
#      execution go when the handler exits? (IRET target sanity)
#  Q2: Who wrote the interrupt frame words on the SS=0100 stack (linear
#      ~0x1A40-0x1AA0), and what were they?
#  Q3: Where is the first wild control transfer and what precedes it?

import sys, struct
import numpy as np

sys.stdout.reconfigure(encoding='utf-8')

PATH = 'bench_trace.bcfg'

WRIT_DT = np.dtype([('instr', '<u8'), ('addr', '<u4'), ('cs', '<u2'),
                    ('ip', '<u2'), ('data', 'u1'), ('r', 'u1'), ('pad', '<u2')])
PORT_DT = np.dtype([('instr', '<u8'), ('port', '<u2'), ('cs', '<u2'),
                    ('ip', '<u2'), ('data', 'u1'), ('w', 'u1'), ('pad', '<u2')])
assert PORT_DT.itemsize == 18
STAT_DT = np.dtype([('instr', '<u8'), ('addr', '<u4'),
                    ('ax', '<u2'), ('cx', '<u2'), ('dx', '<u2'), ('bx', '<u2'),
                    ('sp', '<u2'), ('bp', '<u2'), ('si', '<u2'), ('di', '<u2'),
                    ('es', '<u2'), ('cs', '<u2'), ('ss', '<u2'), ('ds', '<u2'),
                    ('flags', '<u2'), ('pad', '<u2')])
PLNW_DT = np.dtype([('instr', '<u8'), ('off', '<u4'), ('cs', '<u2'),
                    ('ip', '<u2'), ('plane', 'u1'), ('data', 'u1'), ('pad', '<u2')])

def load(path):
    buf = np.memmap(path, dtype=np.uint8, mode='r')
    off = 0
    assert bytes(buf[0:4]) == b'BCFG'
    off = 8
    sec = {}
    while off < len(buf):
        tag = bytes(buf[off:off + 4]).decode()
        n = int(np.frombuffer(buf[off + 4:off + 12], '<u8')[0])
        off += 12
        if tag == 'NODE':
            sz = 28
        elif tag == 'EDGE':
            sz = 8
        elif tag in ('WRIT', 'READ', 'PLNW'):
            sz = 20
        elif tag == 'EXEC':
            sz = 4
        elif tag == 'STAT':
            sz = 40
        elif tag == 'PORT':
            sz = 18
        else:
            raise RuntimeError(f'unknown tag {tag!r} at {off}')
        sec[tag] = (off, n, sz)
        off += n * sz
    return buf, sec

buf, sec = load(PATH)
for t, (o, n, sz) in sec.items():
    print(f'{t}: {n} records')

def arr(tag, dt):
    o, n, sz = sec[tag]
    return np.frombuffer(buf, dtype=dt, count=n, offset=o)

exec_ = arr('EXEC', np.dtype('<u4'))
writes = arr('WRIT', WRIT_DT)
ports = arr('PORT', PORT_DT)

HANDLER = 0x1232 * 16 + 0x019A          # CTMOUSE IRQ4 entry (linear)
CT_LO, CT_HI = 0x1232 * 16, 0x1232 * 16 + 0x2000   # CTMOUSE resident range

print()
print('=== Q1: handler invocations and exit targets ===')
entries = np.flatnonzero(exec_ == HANDLER)
print(f'handler entries: {len(entries)}')
for k in entries:
    # walk forward until execution leaves CTMOUSE's resident range
    j = k
    n = len(exec_)
    while j < n and CT_LO <= exec_[j] < CT_HI:
        j += 1
    if j < n:
        tgt = exec_[j]
        print(f'  entry at instr {k}: {j - k} instrs in handler, '
              f'exit -> {tgt:05X} (lin)')
    else:
        print(f'  entry at instr {k}: runs to end of trace inside handler')

print()
print('=== Q2: writes to the SS=0100 stack area (lin 0x1A30-0x1AB0) ===')
m = (writes['addr'] >= 0x1A30) & (writes['addr'] < 0x1AB0)
w = writes[m]
print(f'{len(w)} writes; last 60:')
for r in w[-60:]:
    print(f'  instr {r["instr"]:>10}  [{r["addr"]:05X}] = {r["data"]:02X}  '
          f'by {r["cs"]:04X}:{r["ip"]:04X}')

print()
print('=== Q3: UART port ops (3F8-3FF) -- last 40 ===')
pm = (ports['port'] >= 0x3F8) & (ports['port'] <= 0x3FF)
p = ports[pm]
print(f'{len(p)} UART port ops')
for r in p[-40:]:
    d = 'OUT' if r['w'] else 'IN '
    print(f'  instr {r["instr"]:>10}  {d} {r["port"]:04X} = {r["data"]:02X}  '
          f'at {r["cs"]:04X}:{r["ip"]:04X}')

print()
print('=== tail of execution timeline (last 80 instrs) ===')
for i in range(max(0, len(exec_) - 80), len(exec_)):
    print(f'  instr {i:>10}: {exec_[i]:05X}')
