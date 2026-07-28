"""Disassemble the 17 bytecode scripts and infer the grammar.

Interpreter (verified at 0050:3DDF-0x3F05):
    PC = [0x154d]
    b  = *PC++
    [0x9a81] = sign_extend(b)     ; sign is a MODIFIER flag
    [0x9a87] = abs(b)             ; the opcode
    dispatch:
        0x61        -> call 0x3FBD  (protection compare), loop
        0x00 / 0xFF -> ret          (halt)
        0x32..0x37  -> scene = 0..5, draw, loop
        0x29        -> menu 3x9    ; [0x4740]  = pick*2
        0x2A        -> menu 12x12  ; [0x4740] += pick ; call 0x3FA5
        0x06        -> menu 5x1
        default     -> [0x9a83]=op ; call 0x4604 (show text #op)
    if [0x9a81] < 0 before a menu -> call 0x460e   (the sign modifier)

The scripts live behind a 17-entry pointer table at DS:0x154f. Those pointers
read as zeros in the static image at my assumed DS, so locate the table by its
known live contents instead (1444 1450 145e 1470 ...), which also recovers the
true DS base.
"""
import sys, struct, collections
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0,'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')

# Live values observed for the table at DS:0x154f
KNOWN = (0x1444, 0x1450, 0x145e, 0x1470, 0x1481, 0x1491, 0x14a2, 0x14b2,
         0x14c1, 0x14d1, 0x14e0, 0x14f1, 0x1500, 0x1510, 0x151f, 0x152e, 0x153e)

pat = struct.pack('<4H', *KNOWN[:4])
tbl_off = img.find(pat)
if tbl_off < 0:
    print('table not found in static image; scripts are built at load time')
    sys.exit(1)

# tbl_off corresponds to DS:0x154f -> recover the base for DS-relative offsets
BASE = tbl_off - 0x154f
print(f'script table at image 0x{tbl_off:05x}  => DS base = image 0x{BASE:05x}')
print(f'(implies DS = {(BASE//16)+0x50:#06x})\n')

OPNAME = {
    0x00: 'HALT', 0xff: 'HALT',
    0x61: 'CHECK',              # call 0x3FBD -- the protection compare
    0x29: 'MENU_EARLY_LATE',    # 3x9   ; [0x4740]  = pick*2
    0x2a: 'MENU_MONTH',         # 12x12 ; [0x4740] += pick ; then 0x3FA5
    0x06: 'MENU_5x1',
    0x32: 'SCENE 0', 0x33: 'SCENE 1', 0x34: 'SCENE 2',
    0x35: 'SCENE 3+rnd', 0x36: 'SCENE 4', 0x37: 'SCENE 5',
}

def decode(off, limit=64):
    out = []
    p = off
    for _ in range(limit):
        b = img[BASE + p]
        p += 1
        sv = b - 256 if b > 127 else b          # cwde
        op = abs(sv)                            # neg -> opcode
        flag = '-' if sv < 0 else ' '           # the sign modifier
        name = OPNAME.get(op, f'TEXT {op:#04x}')
        out.append((b, flag, op, name))
        if op in (0x00, 0xff):
            break
    return out, p - off

print('=' * 74)
opcount = collections.Counter()
flagged = collections.Counter()
for i, ptr in enumerate(KNOWN):
    ops, n = decode(ptr)
    raw = img[BASE+ptr: BASE+ptr+n]
    print(f'\nscript[{i:2d}] @ DS:{ptr:04x}  ({n} bytes)')
    print(f'   raw: {raw.hex()}')
    line = []
    for b, flag, op, name in ops:
        opcount[op] += 1
        if flag == '-':
            flagged[op] += 1
        line.append(f'{flag}{name}')
    print('   ' + ' ; '.join(line))

print('\n' + '=' * 74)
print('opcode frequency across all 17 scripts:')
for op, n in opcount.most_common():
    f = flagged[op]
    print(f'  {op:#04x} {OPNAME.get(op,"TEXT"):<18s} used {n:3d}x   sign-flagged {f:3d}x')
