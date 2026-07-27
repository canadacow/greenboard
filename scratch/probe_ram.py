"""Inspect the emulator RAM dump at the stall point: what is in the label
buffer, the resource descriptor table, and the wait-flags?"""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
ram=open('scratch/out/ram.bin','rb').read()
BASE=0x500          # image[0] is at physical 0x500 (seg 0x50)
def at(off,n=32):   # offset within the loaded program image
    p=BASE+off
    return ram[p:p+n]

print('label buffer [0x68b0..0x68d0]:', at(0x68b0,32))
print('  as text:', bytes(b if 32<=b<127 else 46 for b in at(0x68b0,32)))
print()
for name,off in (('[0x3bc6] disk-wanted',0x3bc6),('[0x3bc7] drive',0x3bc7),
                 ('[0x11a3] last-err',0x11a3)):
    print(f'{name} = {at(off,1)[0]:#04x}')
print('[0x3bc8] cyl bias =', struct.unpack('<H',at(0x3bc8,2))[0])
print('[0x3b84] res idx  =', struct.unpack('<H',at(0x3b84,2))[0])
print()
for nm,o in (('3c02',0x3c02),('3c04',0x3c04),('3c06',0x3c06),('3c08',0x3c08),('3c0a',0x3c0a)):
    print(f'timer [{nm}] = {struct.unpack("<H",at(o,2))[0]}')
print()
print('resource ptr table [0x136b..]:',
      [f'{struct.unpack("<H",at(0x136b+i*2,2))[0]:04x}' for i in range(16)])
