"""Standalone port of the Pirates! image decoder, from the Ghidra output.

Chain (see scratch/out/decoder_fixed.txt):
  read_bytes        1000:0e8f  buffered stream, 512-byte window
  huff_decode_sym   1000:1ccf  Huffman symbol, tree at 0x67b1/0x67b3
  decode_image_rect 1000:1bf9  4bpp Huffman+RLE blit

huff_decode_sym, transcribed:
    cVar2 = [0xc736]      bits left in current byte
    cVar1 = [0xc737]      current byte
    uVar4 = [0xc73d]      cursor into the 512-byte buffer
    iVar3 = 0             tree node
    do {
        if (--bits < 0) { byte = buf[cursor++]; bits = 7 }
        bit  = byte < 0            (MSB)
        byte <<= 1
        node = tree[(node + bit) * 2]
    } while (node >= 0)
    return -1 - node                    (leaf -> symbol)

Tree build (FUN_1000_0ed7):
    for i in range(count*2):  lo[i] = raw[i]                 (one byte each)
    then a second pass over `count` bytes b:
        node[i*4 + 0] = ((b & 0x70) << 4 | lo)  * (b & 0x80 ? -1 : 1)
        node[i*4 + 2] = ((b & 0x07) << 8 | lo') * (b & 0x08 ? -1 : 1)

decode_image_rect(x, y, w, h), transcribed:
    for row in y .. y+h:
      ptr = rowtable[row] + x/2
      acc = 0
      for col in x .. x+w:
        nibble_flag ^= 1
        if nibble_flag == 0:            # low nibble of the pending byte
            v = pending & 0x0f
        else:                           # need a new byte
            if runlen == 0:
                v = huff()
                if v == escape:         # RLE
                    runval = huff()
                    runlen = 4
                    while (d := huff()) == 0xff: runlen += d   # accumulate
                    runlen += d
                    ...
            else:
                runlen -= 1
                v = runval
            pending = v
            v >>= 4                     # high nibble first
        if col even: acc |= (v - bias) << 4
        else:        out[ptr++] ^= (v - bias) & 0x0f | acc; acc = 0
"""
import sys, os, struct
sys.stdout.reconfigure(encoding='utf-8')


class Stream:
    """read_bytes(): 512-byte buffered window over a byte source."""
    def __init__(self, data, pos=0):
        self.data = data
        self.pos = pos          # absolute position of next refill
        self.buf = b''
        self.cur = 0x200        # forces a refill on first access

    def refill(self):
        self.buf = self.data[self.pos:self.pos + 0x200]
        self.pos += 0x200
        self.cur = 0

    def byte(self):
        if self.cur > 0x1ff:
            self.refill()
        b = self.buf[self.cur] if self.cur < len(self.buf) else 0
        self.cur += 1
        return b

    def bytes(self, n):
        return bytes(self.byte() for _ in range(n))


class Huff:
    """huff_decode_sym(): walk the tree MSB-first until a negative node."""
    def __init__(self, stream, nodes):
        self.s = stream
        self.nodes = nodes      # flat list: node[(n+bit)] -> int
        self.bits = 0
        self.cur = 0

    def sym(self):
        node = 0
        while True:
            self.bits -= 1
            if self.bits < 0:
                self.cur = self.s.byte()
                self.bits = 7
            bit = 1 if (self.cur & 0x80) else 0
            self.cur = (self.cur << 1) & 0xff
            idx = node + bit
            node = self.nodes[idx] if idx < len(self.nodes) else -1
            if node < 0:
                return (-1 - node) & 0xff


def build_tree(raw_lo, raw_hi):
    """Reconstruct the node table exactly as FUN_1000_0ed7 does.

    The two child arrays are INTERLEAVED with stride 4:
        left  at 0x67b1 + i*4
        right at 0x67b3 + i*4
    Each is first filled with a byte widened to a word (first pass), then
    OR-ed with high bits taken from a flag byte and negated for leaves:
        left  = ((b & 0x70) << 4 | lo_left ) * (b & 0x80 ? -1 : 1)
        right = ((b & 0x07) << 8 | lo_right) * (b & 0x08 ? -1 : 1)
    huff_decode_sym indexes tree[(node + bit) * 2], i.e. word units, so we
    store left/right adjacent in a flat list.
    """
    n = len(raw_hi)
    nodes = [0] * (n * 2 + 2)
    for i, b in enumerate(raw_hi):
        lo_l = raw_lo[i * 2] if i * 2 < len(raw_lo) else 0
        lo_r = raw_lo[i * 2 + 1] if i * 2 + 1 < len(raw_lo) else 0
        left = ((b & 0x70) << 4) | lo_l
        if b & 0x80:
            left = -left
        right = ((b & 0x07) << 8) | lo_r
        if b & 0x08:
            right = -right
        nodes[i * 2] = left
        nodes[i * 2 + 1] = right
    return nodes


def decode_rect(huff, w, h, escape, bias=0):
    """decode_image_rect(): 4bpp, two pixels per byte, RLE escape."""
    out = bytearray(w * h)
    nib = 0
    runlen = 0
    runval = 0
    pending = 0
    i = 0
    for row in range(h):
        acc = 0
        for col in range(w):
            nib ^= 1
            if nib == 0:
                v = pending & 0x0f
            else:
                if runlen == 0:
                    v = huff.sym()
                    if v == escape:
                        runval = huff.sym()
                        runlen = 4
                        while True:
                            d = huff.sym()
                            runlen += d
                            if d != 0xff:
                                break
                        runlen -= 1
                        v = runval
                else:
                    runlen -= 1
                    v = runval
                pending = v
                v = (v >> 4) & 0x0f
            out[i] = (v - bias) & 0x0f
            i += 1
    return out
