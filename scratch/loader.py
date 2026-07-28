"""Reimplement Pirates' resource loader in Python and run it on the disk image.

Straight transliteration of the decompiled code -- no searching, no emulator.

FUN_0050_2844 (called by load_resource, AX = resource index):
    piVar4 = *(int **)(&DAT_0050_0e6b + [0x3b84]*2)   ; descriptor pointer
    if (*piVar4 == 0) return
    [0x11b8] = (byte)piVar4[0]              ; sector count
    [0x11ae] = (byte)piVar4[2] + [0x3bc8]   ; cylinder (+ disk bias)
    [0x11af] = (byte)piVar4[3]              ; sector
    [0x11b2] = piVar4[4]                    ; dest segment
    [0x11b4] = piVar4[5]                    ; dest offset
    [0x11ad] = 0                            ; head
    if ([0x11af] >= 10) { [0x11ad] = 1; [0x11af] -= 9 }
    loop: INT 13h AH=2 AL=1  CH=[0x11ae] CL=[0x11af] DH=[0x11ad]
          ES:BX = [0x11b2]:[0x11b0]
          [0x11af]++ ; if >=10 { [0x11af]=1; [0x11ad]++; and 1;
                                 if now 0 -> [0x11ae]++ }
          [0x11b0] += 0x200
          while (--[0x11b8])

The boot sector loads stage 2 to 0050:0000, so image offset == 0050 offset.
The descriptor table is DS-relative; DS for this code is word[0] of the
segment table +0x50. Rather than assume, derive DS from the image itself and
try each candidate, accepting the one whose descriptors are all VALID for a
40-cylinder / 2-head / 9-sector disk.
"""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, 'scratch')
from stage2 import build_image

img = build_image('assets/pirates_1.img')
disk = open('assets/pirates_1.img', 'rb').read()
SEC, SPT, HEADS = 512, 9, 2

def chs(cyl, head, sec):
    return ((cyl * HEADS + head) * SPT + (sec - 1)) * SEC

def read_descriptor(base, idx):
    """base = image offset of DS:0000. Returns the 6 words of descriptor idx."""
    # The real instruction is  mov bx,[si+0x136b]  (0x2855-0x285B).
    # Ghidra's DAT_0050_0e6b label was wrong.
    p = base + 0x136B + idx * 2
    if p + 2 > len(img):
        return None
    ptr = struct.unpack_from('<H', img, p)[0]
    q = base + ptr
    if ptr == 0 or q + 12 > len(img):
        return None
    # Real byte offsets from the disassembly at 0x2866-0x2882:
    #   [bx+0]=count  [bx+4]=cylinder  [bx+6]=sector
    #   [bx+8]=dest segment  [bx+0xA]=dest offset
    g = lambda o: struct.unpack_from('<H', img, q + o)[0]
    return (g(0), g(4), g(6), g(8), g(0xA))

def valid(d):
    if d is None: return False
    cnt, cyl, sec, seg, off = d
    return (0 < (cnt & 0xFF) <= 200 and cyl <= 45 and 1 <= sec <= 18)

# The segment table sits at the start of stage 2; word[0]+0x50 is the data
# segment. Read it rather than assume.
seg_words = struct.unpack_from('<8H', img, 0)
cands = []
for w in seg_words:
    ds = (w + 0x50) & 0xFFFF
    cands.append(ds)
cands += [0x117B, 0x1038]          # observed live values
print('candidate DS values from the segment table:',
      ' '.join(f'{c:04X}' for c in dict.fromkeys(cands)))
print()

best = None
for ds in dict.fromkeys(cands):
    base = ds * 16 - 0x500          # image offset of DS:0000
    if base < 0 or base >= len(img):
        continue
    good = sum(1 for i in range(64) if valid(read_descriptor(base, i)))
    print(f'  DS={ds:04X}  base=image {base:#07x}  valid descriptors: {good}/64')
    if best is None or good > best[0]:
        best = (good, ds, base)

good, ds, base = best
print(f'\nbest: DS={ds:04X} with {good} valid descriptors\n')
if good == 0:
    print('No candidate works -- the descriptor table is not resident in the')
    print('static stage-2 image; it is itself loaded at runtime.')
    sys.exit(1)

print(f'{"idx":>4} {"cnt":>4} {"cyl":>4} {"sec":>4}  {"dest":>10}  file offset  bytes')
print('-' * 66)
for i in range(64):
    d = read_descriptor(base, i)
    if not valid(d): continue
    cnt, cyl, sec, seg, off = d
    cnt &= 0xFF
    head = 0
    s = sec
    if s >= 10:
        head, s = 1, s - 9
    fo = chs(cyl, head, s)
    print(f'{i:>4} {cnt:>4} {cyl:>4} {sec:>4}  {seg:04X}:{off:04X}  {fo:#08x}  {cnt*SEC}')
