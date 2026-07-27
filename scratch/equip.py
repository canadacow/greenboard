import sys
sys.stdout.reconfigure(encoding='utf-8')
v=0x0021
print(f'equipment word 0x{v:04X} = {v:016b}')
print('  bit0 (floppy present)      =', v&1)
print('  bits7-6 (drives-1)         =', (v>>6)&3, '-> num drives =', ((v>>6)&3)+1)
print()
for nd in (1,2):
    w = (v & ~0x00C0) | ((nd-1)<<6) | 1
    print(f'  {nd} drive(s) -> 0x{w:04X}  bits7-6={(w>>6)&3}')
