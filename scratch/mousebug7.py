# mousebug7.py -- disassemble CTMOUSE's packet-complete path and find the
# far pointer it transfers through.
import sys
import numpy as np
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

sys.stdout.reconfigure(encoding='utf-8')

PATH = 'bench_trace.bcfg'
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
o, n = sec['WRIT']
writes = np.frombuffer(buf, dtype=WRIT_DT, count=n, offset=o)

def mem_at(lo, hi, t):
    m = (writes['addr'] >= lo) & (writes['addr'] < hi) & (writes['instr'] <= t)
    w = writes[m]
    img = np.zeros(hi - lo, np.uint8)
    for r in w:
        img[r['addr'] - lo] = r['data']
    return bytes(img)

md = Cs(CS_ARCH_X86, CS_MODE_16)
T = 4532098   # right at the wild transfer

# CTMOUSE ISR: entry, state machine, packet-complete path.
seg = 0x1232
code = mem_at(seg * 16 + 0x0190, seg * 16 + 0x02C8, T)
print('=== CTMOUSE 1232:0190-02C8 at wild-transfer time ===')
for ins in md.disasm(code, 0x0190):
    mark = ''
    if ins.address in (0x029F, 0x02A5, 0x02AE, 0x01C4, 0x01A0):
        mark = '   <====='
    print(f'  1232:{ins.address:04X}  {ins.bytes.hex():<14s} '
          f'{ins.mnemonic} {ins.op_str}{mark}')

# Memory the transfer instruction references (dump generous window around
# CTMOUSE data at 1232:00C0-0110, where 00FE lives).
data = mem_at(seg * 16 + 0x00C0, seg * 16 + 0x0110, T)
print('\n=== CTMOUSE data 1232:00C0-0110 ===')
for i in range(0, len(data), 16):
    hexs = ' '.join(f'{b:02X}' for b in data[i:i+16])
    print(f'  1232:{0xC0 + i:04X}  {hexs}')
