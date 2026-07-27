import sys
sys.stdout.reconfigure(encoding='utf-8')

d = open('assets/pirates_1.img', 'rb').read()
print('image size', len(d), '=', len(d)/512, 'sectors')
# 368640 bytes = 720 sectors. Standard 360K: 40 cyl x 2 heads x 9 spt = 720. SPT is 9, NOT 8!
for spt in (8, 9):
    cyl = len(d) // (512 * 2 * spt)
    print(f'  if SPT={spt}: {cyl} cylinders ({len(d)/(512*2*spt):.2f})')
print()
print('Boot reads AL=8 sectors starting at CL=1 -- reads 8 of the 9 sectors per track.')
print('So sector stride is still 9 per track; only 8 are consumed.\n')

SEC = 512
def off9(cyl, head, sec=1):
    return ((cyl * 2 + head) * 9 + (sec - 1)) * SEC

print('=== corrected: SPT=9 geometry ===')
print('Stage 1 title chunks (cyl 0x26=38):')
for i, (c, h) in enumerate([(38,0),(38,1),(39,0),(39,1)]):
    print(f'   chunk {i}: c={c} h={h} -> file 0x{off9(c,h):06x} -> B800:{i*0x1000:04X}')
print()
print('Stage 2 program (cyl 1..21):')
first, last = off9(1,0), off9(0x15,1) + 8*SEC
print(f'   spans file 0x{first:06x} .. 0x{last:06x}')
dest=0x50
for cylv in (1,2,3,20,21):
    print(f'   cyl {cylv:2d}: 0x{off9(cylv,0):06x} h0 / 0x{off9(cylv,1):06x} h1 -> {0x50+0x200*(cylv-1):04X}:0000')
