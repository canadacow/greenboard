import sys
sys.stdout.reconfigure(encoding='utf-8')

SEC = 512
def off9(cyl, head, sec=1):
    return ((cyl * 2 + head) * 9 + (sec - 1)) * SEC

def build_image(path):
    """Reconstruct the stage-2 program image exactly as the loader lays it in RAM.
    Loader: for cyl 1..0x15, read 8 sectors head0 -> stage, 8 sectors head1 -> stage+0x1000,
    then block-copy 0x2000 bytes to dest segment, dest += 0x200 (i.e. +0x2000 bytes)."""
    d = open(path, 'rb').read()
    img = bytearray()
    for cyl in range(1, 0x16):
        img += d[off9(cyl, 0): off9(cyl, 0) + 8*SEC]
        img += d[off9(cyl, 1): off9(cyl, 1) + 8*SEC]
    return bytes(img)

# Base of the loaded image is physical 0x500 (segment 0x50, offset 0).
BASE_PHYS = 0x500
BASE_SEG  = 0x50

if __name__ == '__main__':
    img = build_image('assets/pirates_1.img')
    open('scratch/out/stage2.bin', 'wb').write(img)
    print(f'stage2 image: {len(img)} bytes ({len(img)/1024:.0f} KB)')
    print(f'loads at phys 0x{BASE_PHYS:05X} (seg {BASE_SEG:04X}:0000)')
    print(f'covers phys 0x{BASE_PHYS:05X} .. 0x{BASE_PHYS+len(img):05X}')
    print(f'entry: {BASE_SEG:04X}:0020  -> image offset 0x20')
    print()
    print('first 64 bytes (relocation table area):', img[:64].hex())
