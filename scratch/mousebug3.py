# mousebug3.py -- reconstruct and disassemble the 0238 interrupt stub and
# its callers from the trace write log.
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

T = 4533250   # just after the poisoned IRET

def mem_at(lo, hi, t=T):
    m = (writes['addr'] >= lo) & (writes['addr'] < hi) & (writes['instr'] <= t)
    w = writes[m]
    img = np.zeros(hi - lo, np.uint8)
    for r in w:   # log is chronological; later writes overwrite
        img[r['addr'] - lo] = r['data']
    return bytes(img)

md = Cs(CS_ARCH_X86, CS_MODE_16)

def dis(seg, ip_lo, ip_hi, label):
    lo = seg * 16 + ip_lo
    code = mem_at(lo, seg * 16 + ip_hi)
    print(f'=== {label}: {seg:04X}:{ip_lo:04X}-{ip_hi:04X} ===')
    for ins in md.disasm(code, ip_lo):
        print(f'  {seg:04X}:{ins.address:04X}  {ins.bytes.hex():<14s} '
              f'{ins.mnemonic} {ins.op_str}')
    print()

# The stub that built the frame (writes came from 0238:0047-01AC).
dis(0x0238, 0x0030, 0x00C0, 'stub entry (register saves, frame build)')
dis(0x0238, 0x0190, 0x01C0, 'stub tail (wrote IP slot at 01AC)')
# The code that did the poisoned IRET (0001:4222-422A) and its lead-in.
dis(0x0001, 0x40E0, 0x4130, 'code around 0001:40EB/410E (frame pushes)')
dis(0x0001, 0x4200, 0x4240, 'code ending in the IRET at 0001:422A')
# Where the IRET landed with TF garbage.
dis(0x00AC, 0xF240, 0xF280, 'IRET target 00AC:F246')
