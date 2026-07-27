"""The INT 09h ISR does:  bx = (scancode & 0x7f) * 2
                          [0x3c0e] |= [bx + 0xa60]     (make)
                          [0x3c0e] &= ~[bx + 0xa60]    (break)
So the word table at DS:0xa60 maps scancode -> keystate bit.
FUN_0050_24a5 returns (~[0x3c0e]) & 0x1f, so only bits 0..4 are the
directional/action inputs. Dump the table and name the keys."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0,'scratch')
from stage2 import build_image
img=build_image('assets/pirates_1.img')

# DS at runtime is 0x117B; the image is loaded at 0x50:0000, so a DS-relative
# offset X lives at image offset (0x117B-0x50)*16 + X = 0x112B0 + X.
DS_BASE=(0x117B-0x50)*16
TBL=DS_BASE+0xa60

SCNAME={0x01:'ESC',0x0E:'BKSP',0x0F:'TAB',0x1C:'ENTER',0x1D:'CTRL',0x2A:'LSHIFT',
        0x36:'RSHIFT',0x38:'ALT',0x39:'SPACE',0x3B:'F1',0x3C:'F2',0x3D:'F3',
        0x3E:'F4',0x3F:'F5',0x40:'F6',0x41:'F7',0x42:'F8',0x43:'F9',0x44:'F10',
        0x47:'HOME',0x48:'UP',0x49:'PGUP',0x4B:'LEFT',0x4D:'RIGHT',0x4F:'END',
        0x50:'DOWN',0x51:'PGDN',0x52:'INS',0x53:'DEL',
        0x02:'1',0x03:'2',0x04:'3',0x05:'4',0x06:'5',0x07:'6',0x08:'7',
        0x09:'8',0x0A:'9',0x0B:'0',0x2C:'Z',0x2D:'X',0x2E:'C',0x2F:'V',
        0x10:'Q',0x11:'W',0x12:'E',0x13:'R',0x14:'T',0x15:'Y',0x16:'U',
        0x17:'I',0x18:'O',0x19:'P',0x1E:'A',0x1F:'S',0x20:'D',0x21:'F',
        0x22:'G',0x23:'H',0x24:'J',0x25:'K',0x26:'L',0x30:'B',0x31:'N',0x32:'M'}

print(f'scancode -> keystate bit  (table at DS:0xa60, image 0x{TBL:05x})\n')
hits=[]
for sc in range(0x80):
    off=TBL+sc*2
    if off+2>len(img): break
    v=struct.unpack('<H',img[off:off+2])[0]
    if v:
        hits.append((sc,v))
        bits=' '.join(f'b{b}' for b in range(16) if v>>b & 1)
        print(f'  {sc:#04x} {SCNAME.get(sc,"?"):<7s} -> {v:#06x}  {bits}')
print(f'\n{len(hits)} mapped scancodes')
print('\nFUN_0050_24a5 returns ~[0x3c0e] & 0x1f  -> only bits 0-4 matter:')
for sc,v in hits:
    if v & 0x1f:
        print(f'   {SCNAME.get(sc,hex(sc))} sets bit(s) {v & 0x1f:#04x}')
