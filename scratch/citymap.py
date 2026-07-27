"""Verify byte[1],byte[2] are map x,y by checking them against real geography
and plotting. Vera Cruz/Campeche should be far WEST, Barbados/Trinidad far
EAST, Florida/Bahamas NORTH, Panama/Cartagena SOUTH."""
import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image, ImageDraw

d=open('assets/pirates_1.img','rb').read()
BASE,STRIDE=0x05400c,24

cities=[]
for i in range(31):
    off=BASE+i*STRIDE
    rec=d[off:off+STRIDE]
    name=rec[:12].decode('ascii','replace').strip()
    x,y=rec[13],rec[14]
    if name and not name.startswith('FLORIDA CHNL'):
        cities.append((name,x,y))

print(f'{"city":<14}{"x":>4}{"y":>4}')
for n,x,y in sorted(cities,key=lambda c:c[1]):
    print(f'{n:<14}{x:4d}{y:4d}')

print('\n=== geography sanity check ===')
by=dict((n,(x,y)) for n,x,y in cities)
def chk(a,b,axis,desc):
    if a in by and b in by:
        i=0 if axis=='x' else 1
        ok='OK ' if by[a][i]<by[b][i] else 'BAD'
        print(f'  {ok} {desc}: {a}{by[a]} vs {b}{by[b]}')
chk('VERA CRUZ','TRINIDAD','x','Vera Cruz west of Trinidad')
chk('CAMPECHE','MARGARITA','x','Campeche west of Margarita')
chk('PANAMA','HAVANA','x','Panama west of Havana')
chk('NASSAU','CARTAGENA','y','Bahamas north of Cartagena')
chk('ST.AUGUSTINE','PANAMA','y','St. Augustine north of Panama')

W=max(x for _,x,_ in cities)+12; H=max(y for _,_,y in cities)+12
S=6
img=Image.new('RGB',(W*S,H*S),(12,26,48))
dr=ImageDraw.Draw(img)
for n,x,y in cities:
    dr.ellipse([x*S-3,y*S-3,x*S+3,y*S+3],fill=(240,200,90))
    dr.text((x*S+5,y*S-4),n,fill=(220,230,240))
img.save('scratch/out/city_positions.png')
print(f'\nplotted {len(cities)} cities -> scratch/out/city_positions.png ({W}x{H} map units)')
