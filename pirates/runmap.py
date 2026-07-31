"""Render the whole Caribbean by executing Pirates' own tile routine.

No reimplementation and no fitted parameters: this interprets the actual
instructions at 0AC55 and 0AB0D over a copy of the game's memory, for every
map cell, and reads out the bytes the routine writes into the working array.

The routine and its data are taken from a trace snapshot, so the memory image
is exactly what the game had.

    python pirates/runmap.py bench_trace.bcfg
"""

import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "scripts"))
import numpy as np                      # noqa: E402
from bcfg_fast import FastTrace         # noqa: E402

DS = 0x117B
BASE = DS * 16

# Routine entry points, from the trace.
TILE_ROUTINE = 0x0AC55          # builds one map cell into the working array
WORK_ARRAY = 0x1A261            # 44 bytes per row
TILE_GFX = 0x1B830              # 16 bytes per tile
MAP_W, MAP_H = 160, 200


class CPU:
    """Just enough 8086 to run 0AB0D and 0AC55.

    Only the instruction forms those two routines actually use are
    implemented; anything else raises rather than guessing.
    """

    def __init__(self, mem, ds):
        self.m = mem
        self.ds = ds
        self.r = dict(ax=0, bx=0, cx=0, dx=0, si=0, di=0, bp=0, sp=0x400)
        self.zf = False
        self.sf = False
        self.stack = []

    # -- byte/word access, DS-relative --
    def rb(self, off):
        return int(self.m[(self.ds * 16 + (off & 0xFFFF)) & 0xFFFFF])

    def wb(self, off, v):
        self.m[(self.ds * 16 + (off & 0xFFFF)) & 0xFFFFF] = v & 0xFF

    def rw(self, off):
        return self.rb(off) | (self.rb(off + 1) << 8)

    def ww(self, off, v):
        self.wb(off, v & 0xFF)
        self.wb(off + 1, (v >> 8) & 0xFF)

    # -- 8-bit register halves --
    def get8(self, name):
        lo = {"al": "ax", "bl": "bx", "cl": "cx", "dl": "dx"}
        hi = {"ah": "ax", "bh": "bx", "ch": "cx", "dh": "dx"}
        if name in lo:
            return self.r[lo[name]] & 0xFF
        return (self.r[hi[name]] >> 8) & 0xFF

    def set8(self, name, v):
        v &= 0xFF
        lo = {"al": "ax", "bl": "bx", "cl": "cx", "dl": "dx"}
        hi = {"ah": "ax", "bh": "bx", "ch": "cx", "dh": "dx"}
        if name in lo:
            k = lo[name]
            self.r[k] = (self.r[k] & 0xFF00) | v
        else:
            k = hi[name]
            self.r[k] = (self.r[k] & 0x00FF) | (v << 8)

    def flags8(self, v):
        v &= 0xFF
        self.zf = v == 0
        self.sf = (v & 0x80) != 0


def run(cpu, ip, trace=False):
    """Execute from `ip` until the matching RET. Hand-decoded to the exact
    instruction forms present in 0AB0D / 0AC55."""
    # Read through int() everywhere: cpu.m is uint8, so raw numpy scalars
    # overflow as soon as they enter address arithmetic.
    raw = cpu.m

    class _M:
        def __getitem__(self, i):
            return int(raw[i])

        def __setitem__(self, i, v):
            raw[i] = v & 0xFF

    m = _M()
    depth = 0
    steps = 0
    while True:
        steps += 1
        if steps > 100000:
            raise RuntimeError("runaway at %05X" % ip)
        if trace is not False and trace is not None:
            trace.append(ip)
        op = m[ip]
        nxt = ip

        # --- the specific encodings used by these two routines ---
        if op == 0xA0:                      # mov al, [imm16]
            off = m[ip + 1] | (m[ip + 2] << 8)
            cpu.set8("al", cpu.rb(off)); nxt = ip + 3
        elif op == 0xA2:                    # mov [imm16], al
            off = m[ip + 1] | (m[ip + 2] << 8)
            cpu.wb(off, cpu.get8("al")); nxt = ip + 3
        elif op == 0xA1:                    # mov ax, [imm16]
            off = m[ip + 1] | (m[ip + 2] << 8)
            cpu.r["ax"] = cpu.rw(off); nxt = ip + 3
        elif op == 0xA3:                    # mov [imm16], ax
            off = m[ip + 1] | (m[ip + 2] << 8)
            cpu.ww(off, cpu.r["ax"]); nxt = ip + 3
        elif op == 0x80 and m[ip + 1] == 0x3E:   # cmp byte [imm16], imm8
            off = m[ip + 2] | (m[ip + 3] << 8)
            v = cpu.rb(off)
            cpu.zf = v == m[ip + 4]
            cpu.sf = ((v - m[ip + 4]) & 0x80) != 0
            nxt = ip + 5
        elif op == 0xC6 and m[ip + 1] == 0x06:   # mov byte [imm16], imm8
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.wb(off, m[ip + 4]); nxt = ip + 5
        elif op == 0xC7 and m[ip + 1] == 0x06:   # mov word [imm16], imm16
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.ww(off, m[ip + 4] | (m[ip + 5] << 8)); nxt = ip + 6
        elif op == 0xFE and m[ip + 1] == 0x06:   # inc byte [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            v = (cpu.rb(off) + 1) & 0xFF
            cpu.wb(off, v); cpu.zf = v == 0; cpu.sf = (v & 0x80) != 0
            nxt = ip + 4
        elif op == 0xFE and m[ip + 1] == 0x0E:   # dec byte [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            v = (cpu.rb(off) - 1) & 0xFF
            cpu.wb(off, v); cpu.zf = v == 0; cpu.sf = (v & 0x80) != 0
            nxt = ip + 4
        elif op == 0x88 and m[ip + 1] == 0x84:   # mov [si+disp16], al
            d = m[ip + 2] | (m[ip + 3] << 8)
            cpu.wb(cpu.r["si"] + d, cpu.get8("al")); nxt = ip + 4
        elif op == 0x58:                    # pop ax
            cpu.r["ax"] = cpu.stack.pop() if cpu.stack else 0; nxt = ip + 1
        elif op == 0x50:                    # push ax
            cpu.stack.append(cpu.r["ax"]); nxt = ip + 1
        elif op == 0xE9:                    # jmp rel16
            d = m[ip + 1] | (m[ip + 2] << 8)
            d = d - 65536 if d > 32767 else d
            nxt = (ip + 3 + d) & 0xFFFFF
        elif op == 0xEB:                    # jmp rel8
            d = m[ip + 1]
            d = d - 256 if d > 127 else d
            nxt = ip + 2 + d
        elif op == 0x05:                    # add ax, imm16
            imm = m[ip + 1] | (m[ip + 2] << 8)
            cpu.r["ax"] = (cpu.r["ax"] + imm) & 0xFFFF; nxt = ip + 3
        elif op == 0x25:                    # and ax, imm16
            imm = m[ip + 1] | (m[ip + 2] << 8)
            cpu.r["ax"] &= imm; cpu.flags8(cpu.r["ax"]); nxt = ip + 3
        elif op == 0x24:                    # and al, imm8
            cpu.set8("al", cpu.get8("al") & m[ip + 1])
            cpu.flags8(cpu.get8("al")); nxt = ip + 2
        elif op == 0x0C:                    # or al, imm8
            cpu.set8("al", cpu.get8("al") | m[ip + 1]); nxt = ip + 2
        elif op == 0x34:                    # xor al, imm8
            cpu.set8("al", cpu.get8("al") ^ m[ip + 1]); nxt = ip + 2
        elif op == 0x32 and m[ip + 1] == 0xE4:   # xor ah, ah
            cpu.set8("ah", 0); nxt = ip + 2
        elif op == 0x8B and m[ip + 1] == 0xD8:   # mov bx, ax
            cpu.r["bx"] = cpu.r["ax"]; nxt = ip + 2
        elif op == 0x8B and m[ip + 1] == 0xF0:   # mov si, ax
            cpu.r["si"] = cpu.r["ax"]; nxt = ip + 2
        elif op == 0x8B and m[ip + 1] == 0x1E:   # mov bx, [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.r["bx"] = cpu.rw(off); nxt = ip + 4
        elif op == 0xD1 and m[ip + 1] == 0xE0:   # shl ax, 1
            cpu.r["ax"] = (cpu.r["ax"] << 1) & 0xFFFF; nxt = ip + 2
        elif op == 0xD1 and m[ip + 1] == 0xE8:   # shr ax, 1
            cpu.r["ax"] >>= 1; nxt = ip + 2
        elif op == 0xD1 and m[ip + 1] == 0xE3:   # shl bx, 1
            cpu.r["bx"] = (cpu.r["bx"] << 1) & 0xFFFF; nxt = ip + 2
        elif op == 0x03 and m[ip + 1] == 0xD8:   # add bx, ax
            cpu.r["bx"] = (cpu.r["bx"] + cpu.r["ax"]) & 0xFFFF; nxt = ip + 2
        elif op == 0xB1:                    # mov cl, imm8
            cpu.set8("cl", m[ip + 1]); nxt = ip + 2
        elif op == 0xB8:                    # mov ax, imm16
            cpu.r["ax"] = m[ip + 1] | (m[ip + 2] << 8); nxt = ip + 3
        elif op == 0xBB:                    # mov bx, imm16
            cpu.r["bx"] = m[ip + 1] | (m[ip + 2] << 8); nxt = ip + 3
        elif op == 0xBF:                    # mov di, imm16
            cpu.r["di"] = m[ip + 1] | (m[ip + 2] << 8); nxt = ip + 3
        elif op == 0x88 and m[ip + 1] == 0x01:   # mov [bx+di], al
            cpu.wb(cpu.r["bx"] + cpu.r["di"], cpu.get8("al")); nxt = ip + 2
        elif op == 0x4B:                    # dec bx
            cpu.r["bx"] = (cpu.r["bx"] - 1) & 0xFFFF
            cpu.sf = (cpu.r["bx"] & 0x8000) != 0; nxt = ip + 1
        elif op == 0x4A:                    # dec dx
            cpu.r["dx"] = (cpu.r["dx"] - 1) & 0xFFFF
            cpu.sf = (cpu.r["dx"] & 0x8000) != 0; nxt = ip + 1
        elif op == 0x83 and m[ip + 1] == 0xC6:   # add si, imm8
            cpu.r["si"] = (cpu.r["si"] + m[ip + 2]) & 0xFFFF; nxt = ip + 3
        elif op == 0x83 and m[ip + 1] == 0xC7:   # add di, imm8
            cpu.r["di"] = (cpu.r["di"] + m[ip + 2]) & 0xFFFF; nxt = ip + 3
        elif op == 0xBA:                    # mov dx, imm16
            cpu.r["dx"] = m[ip + 1] | (m[ip + 2] << 8); nxt = ip + 3
        elif op == 0xBE:                    # mov si, imm16
            cpu.r["si"] = m[ip + 1] | (m[ip + 2] << 8); nxt = ip + 3
        elif op == 0xD3 and m[ip + 1] == 0xE0:   # shl ax, cl
            cpu.r["ax"] = (cpu.r["ax"] << cpu.get8("cl")) & 0xFFFF
            nxt = ip + 2
        elif op == 0xD2 and m[ip + 1] == 0xE8:   # shr al, cl
            cpu.set8("al", cpu.get8("al") >> cpu.get8("cl")); nxt = ip + 2
        elif op == 0xD0 and m[ip + 1] == 0xE1:   # shl cl, 1
            v = cpu.get8("cl") << 1
            cpu.set8("cl", v); cpu.zf = (v & 0xFF) == 0; nxt = ip + 2
        elif op == 0x80 and m[ip + 1] == 0xE1:   # and cl, imm8
            cpu.set8("cl", cpu.get8("cl") & m[ip + 2]); nxt = ip + 3
        elif op == 0x80 and m[ip + 1] == 0xF1:   # xor cl, imm8
            cpu.set8("cl", cpu.get8("cl") ^ m[ip + 2]); nxt = ip + 3
        elif op == 0x81 and m[ip + 1] == 0xE3:   # and bx, imm16
            imm = m[ip + 2] | (m[ip + 3] << 8)
            cpu.r["bx"] &= imm; nxt = ip + 4
        elif op == 0x8A and m[ip + 1] == 0x00:   # mov al, [bx+si]
            cpu.set8("al", cpu.rb(cpu.r["bx"] + cpu.r["si"])); nxt = ip + 2
        elif op == 0x88 and m[ip + 1] == 0x00:   # mov [bx+si], al
            cpu.wb(cpu.r["bx"] + cpu.r["si"], cpu.get8("al")); nxt = ip + 2
        elif op == 0x8A and m[ip + 1] == 0x0E:   # mov cl, [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.set8("cl", cpu.rb(off)); nxt = ip + 4
        elif op == 0x8A and m[ip + 1] == 0x1E:   # mov bl, [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.set8("bl", cpu.rb(off)); nxt = ip + 4
        elif op == 0x8A and m[ip + 1] == 0x87:   # mov al, [bx+disp16]
            d = m[ip + 2] | (m[ip + 3] << 8)
            cpu.set8("al", cpu.rb(cpu.r["bx"] + d)); nxt = ip + 4
        elif op == 0x88 and m[ip + 1] == 0x87:   # mov [bx+disp16], al
            d = m[ip + 2] | (m[ip + 3] << 8)
            cpu.wb(cpu.r["bx"] + d, cpu.get8("al")); nxt = ip + 4
        elif op == 0x8A and m[ip + 1] == 0x80:   # mov al, [bx+si+disp16]
            d = m[ip + 2] | (m[ip + 3] << 8)
            cpu.set8("al", cpu.rb(cpu.r["bx"] + cpu.r["si"] + d)); nxt = ip + 4
        elif op == 0x0A and m[ip + 1] == 0x06:   # or al, [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.set8("al", cpu.get8("al") | cpu.rb(off)); nxt = ip + 4
        elif op == 0x3A and m[ip + 1] == 0x06:   # cmp al, [imm16]
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.flags8((cpu.get8("al") - cpu.rb(off)) & 0xFF)
            cpu.zf = cpu.get8("al") == cpu.rb(off); nxt = ip + 4
        elif op == 0x83 and m[ip + 1] == 0x06:   # add word [imm16], imm8
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.ww(off, (cpu.rw(off) + m[ip + 4]) & 0xFFFF); nxt = ip + 5
        elif op == 0x01 and m[ip + 1] == 0x06:   # add word [imm16], ax
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.ww(off, (cpu.rw(off) + cpu.r["ax"]) & 0xFFFF); nxt = ip + 4
        elif op == 0x29 and m[ip + 1] == 0x06:   # sub word [imm16], ax
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.ww(off, (cpu.rw(off) - cpu.r["ax"]) & 0xFFFF); nxt = ip + 4
        elif op == 0x80 and m[ip + 1] == 0x36:   # xor byte [imm16], imm8
            off = m[ip + 2] | (m[ip + 3] << 8)
            cpu.wb(off, cpu.rb(off) ^ m[ip + 4]); nxt = ip + 5
        elif op == 0x4E:                    # dec si
            cpu.r["si"] = (cpu.r["si"] - 1) & 0xFFFF
            cpu.sf = (cpu.r["si"] & 0x8000) != 0; nxt = ip + 1
        elif op == 0x46:                    # inc si
            cpu.r["si"] = (cpu.r["si"] + 1) & 0xFFFF; nxt = ip + 1
        elif op == 0x42:                    # inc dx
            cpu.r["dx"] = (cpu.r["dx"] + 1) & 0xFFFF
            cpu.zf = cpu.r["dx"] == 0; nxt = ip + 1
        elif op == 0x83 and m[ip + 1] == 0xFA:   # cmp dx, imm8
            cpu.zf = cpu.r["dx"] == m[ip + 2]; nxt = ip + 3
        elif op == 0xB0:                    # mov al, imm8
            cpu.set8("al", m[ip + 1]); nxt = ip + 2
        elif op == 0x75:                    # jne rel8
            d = m[ip + 1]
            d = d - 256 if d > 127 else d
            nxt = ip + 2 + (d if not cpu.zf else 0)
        elif op == 0x74:                    # je rel8
            d = m[ip + 1]
            d = d - 256 if d > 127 else d
            nxt = ip + 2 + (d if cpu.zf else 0)
        elif op == 0x79:                    # jns rel8
            d = m[ip + 1]
            d = d - 256 if d > 127 else d
            nxt = ip + 2 + (d if not cpu.sf else 0)
        elif op == 0xE8:                    # call rel16
            d = m[ip + 1] | (m[ip + 2] << 8)
            d = d - 65536 if d > 32767 else d
            cpu.stack.append(ip + 3)
            depth += 1
            nxt = (ip + 3 + d) & 0xFFFFF
        elif op == 0xC3:                    # ret
            if not cpu.stack:
                return
            nxt = cpu.stack.pop()
            if nxt is None:                 # sentinel: outermost return
                return
            depth -= 1
        else:
            raise RuntimeError("unhandled opcode %02X at %05X" % (op, ip))
        ip = nxt & 0xFFFFF


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "bench_trace.bcfg"
    t = FastTrace(path)
    base_mem = np.array(t.snapshot(20_600_000), dtype=np.uint8)

    # One CPU over a working copy; the routine mutates state between cells,
    # exactly as the game does.
    out = np.zeros((MAP_H * 2, MAP_W * 4), dtype=np.uint8)

    for Y in range(MAP_H):
        mem = base_mem.copy()
        cpu = CPU(mem, DS)
        # Working-array cursor to a known row, so writes land predictably.
        cpu.ww(0x9a57, WORK_ARRAY - BASE)
        cpu.wb(0x647a, Y)
        for X in range(MAP_W):
            cpu.wb(0x6479, X)
            cpu.ww(0x9a57, (WORK_ARRAY - BASE))
            try:
                run(cpu, TILE_ROUTINE)
            except RuntimeError as e:
                print("stopped at X=%d Y=%d: %s" % (X, Y, e))
                return 1
            # The routine wrote 4 tiles x 2 rows starting at the cursor.
            for dr in range(2):
                for si in range(4):
                    out[Y * 2 + dr, X * 4 + si] = mem[
                        (WORK_ARRAY + dr * 44 + si) & 0xFFFFF]
        if Y % 40 == 0:
            print("  row %d/%d" % (Y, MAP_H))

    np.save(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "map_tiles.npy"), out)
    print("tile bytes: %s" % np.unique(out)[:24])

    # Render through the tile graphics.
    PAL = [(0, 42, 160), (64, 176, 176), (200, 164, 88), (255, 255, 255)]
    TH, TW = out.shape
    W, H = TW * 8, TH * 8
    img = np.zeros((H, W, 3), dtype=np.uint8)
    for ty in range(TH):
        for tx in range(TW):
            g = TILE_GFX + int(out[ty, tx]) * 16
            for row in range(8):
                w0 = int(base_mem[g + row * 2])
                w1 = int(base_mem[g + row * 2 + 1])
                r = img[ty * 8 + row]
                for q in range(4):
                    r[tx * 8 + q] = PAL[(w0 >> (6 - 2 * q)) & 3]
                    r[tx * 8 + 4 + q] = PAL[(w1 >> (6 - 2 * q)) & 3]

    F = 8
    sm = img.reshape(H // F, F, W // F, F, 3).mean(axis=(1, 3)).astype(np.uint8)
    h2, w2 = sm.shape[:2]
    raw = bytearray()
    for y in range(h2):
        raw.append(0)
        raw += sm[y].tobytes()

    def ch(tag, d):
        return (struct.pack(">I", len(d)) + tag + d
                + struct.pack(">I", zlib.crc32(tag + d) & 0xFFFFFFFF))

    png = os.path.join(os.path.dirname(os.path.abspath(__file__)), "map.png")
    with open(png, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n"
                + ch(b"IHDR", struct.pack(">IIBBBBB", w2, h2, 8, 2, 0, 0, 0))
                + ch(b"IDAT", zlib.compress(bytes(raw), 6))
                + ch(b"IEND", b""))
    print("wrote %s (%dx%d from %dx%d)" % (png, w2, h2, W, H))
    return 0


if __name__ == "__main__":
    sys.exit(main())
