"""Search BOTH disk images for Caribbean city names. If the map's city table
is on disk as text, that pins the map data structure immediately."""
import sys, re
sys.stdout.reconfigure(encoding='utf-8')

CITIES=[b'HAVANA',b'PORT ROYALE',b'PORT ROYAL',b'TORTUGA',b'PANAMA',b'CARACAS',
        b'MARACAIBO',b'CAMPECHE',b'VERA CRUZ',b'CARTAGENA',b'SANTIAGO',
        b'TRINIDAD',b'BARBADOS',b'CURACAO',b'ST. KITTS',b'NOMBRE DE DIOS',
        b'PUERTO RICO',b'SANTO DOMINGO',b'GIBRALTAR',b'MARGARITA',b'FLORIDA',
        b'JAMAICA',b'HISPANIOLA',b'CUBA',b'BERMUDA',b'ANTIGUA',b'ELEUTHERA']

for n in (1,2):
    d=open(f'assets/pirates_{n}.img','rb').read()
    print(f'=== pirates_{n}.img ===')
    found=[]
    for c in CITIES:
        i=d.find(c)
        if i>=0: found.append((i,c.decode()))
    for i,c in sorted(found):
        print(f'  0x{i:06x}  {c}')
    if not found: print('  (none as plain ASCII)')
    print()

# Also: any long run of uppercase-ish text anywhere?
print('=== longest uppercase strings on disk 1 (>=6 chars) ===')
d=open('assets/pirates_1.img','rb').read()
hits=sorted(set(m.group() for m in re.finditer(rb'[A-Z][A-Z .\']{5,}', d)),
            key=len, reverse=True)
for h in hits[:40]:
    print('  ',h.decode('ascii','replace'))
