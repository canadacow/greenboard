"""The emulator has run the game far enough that it has demand-loaded assets.
Rather than fighting the menu, scan the WHOLE disk for the Caribbean map.

Pirates! draws the sailing map as a 2bpp CGA bitmap. Rather than guess, render
every plausible region at CGA widths and score each candidate for
"map-likeness": a map has large connected regions (sea) with irregular
coastline edges -- high horizontal run-lengths, moderate vertical correlation
between adjacent scanlines, and only a few distinct colours.
"""
import sys, os
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image

os.makedirs('scratch/out/cand', exist_ok=True)
d1=open('assets/pirates_1.img','rb').read()
d2=open('assets/pirates_2.img','rb').read()
CGA=[(0,0,0),(85,255,255),(255,85,255),(255,255,255)]

def render(data, w_px, path):
    bpr=w_px//4
    rows=len(data)//bpr
    if rows<20: return None
    img=Image.new('RGB',(w_px,rows)); px=img.load()
    for y in range(rows):
        for xb in range(bpr):
            b=data[y*bpr+xb]
            for p in range(4): px[xb*4+p,y]=CGA[(b>>(6-2*p))&3]
    img.save(path); return img

def score(data, w_px):
    """vertical similarity between adjacent scanlines -- pictures are smooth,
    code/text is not."""
    bpr=w_px//4
    rows=min(len(data)//bpr, 200)
    if rows<20: return 0
    same=0; tot=0
    for y in range(rows-1):
        a=data[y*bpr:(y+1)*bpr]; b=data[(y+1)*bpr:(y+2)*bpr]
        for i in range(0,bpr,2):
            tot+=1
            if a[i]==b[i]: same+=1
    return same/max(tot,1)

print('scanning both disks for picture-like regions at CGA widths...')
best=[]
for name,d in (('d1',d1),('d2',d2)):
    for w in (320,):
        step=0x800
        for off in range(0, len(d)-16384, step):
            seg=d[off:off+16384]
            s=score(seg,w)
            best.append((s,name,off,w))
best.sort(reverse=True)
print('\ntop 25 picture-like regions (vertical scanline similarity):')
for s,name,off,w in best[:25]:
    print(f'  {name} 0x{off:06x} w={w}  score={s:.3f}')

print('\nrendering top 12...')
for i,(s,name,off,w) in enumerate(best[:12]):
    d = d1 if name=='d1' else d2
    p=f'scratch/out/cand/{i:02d}_{name}_{off:06x}_w{w}.png'
    render(d[off:off+32000], w, p)
    print('  ',p)
