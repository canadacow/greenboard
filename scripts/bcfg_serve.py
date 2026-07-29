"""Local viewer for bench traces: scrub the session, see code and screen.

Serves the whole .bcfg -- not a sample -- so any instruction count can be
inspected. The trace stays resident as numpy arrays; the browser asks for a
point in time and gets back the disassembly around the executing code plus the
framebuffer as it stood at that moment.

    python scripts/bcfg_serve.py bench_trace.bcfg
    -> http://127.0.0.1:8777

Endpoints:
    /api/meta                     session bounds, load events, mode changes
    /api/state?instr=N            code + registers + screen at instruction N
    /api/screen?instr=N           framebuffer PNG at instruction N
    /api/disasm?addr=A&instr=N    disassembly around an address
    /api/blocks                   hottest executed addresses
"""

import io
import json
import os
import struct
import sys
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from bcfg_fast import FastTrace  # noqa: E402

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16
except ImportError:
    print("capstone required:  pip install capstone")
    raise

CGA_BASE, CGA_SIZE = 0xB8000, 0x4000
MDA_BASE, MDA_SIZE = 0xB0000, 0x1000

PALETTE = [
    (0x00, 0x00, 0x00), (0x00, 0x00, 0xAA), (0x00, 0xAA, 0x00), (0x00, 0xAA, 0xAA),
    (0xAA, 0x00, 0x00), (0xAA, 0x00, 0xAA), (0xAA, 0x55, 0x00), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0x55, 0x55, 0xFF), (0x55, 0xFF, 0x55), (0x55, 0xFF, 0xFF),
    (0xFF, 0x55, 0x55), (0xFF, 0x55, 0xFF), (0xFF, 0xFF, 0x55), (0xFF, 0xFF, 0xFF),
]
# 4-colour graphics palettes, matching ISA_CGA::GFX_PALETTES. Six entries:
# normal and intensified variants of green/red/brown, cyan/magenta/white, and
# the mode-BW set. Selection mirrors the shader in cga_display.cpp.
GFX_PALETTES = [
    [0, 2, 4, 6],   [0, 10, 12, 14],
    [0, 3, 5, 7],   [0, 11, 13, 15],
    [0, 3, 4, 7],   [0, 11, 12, 15],
]

MODE_HIRES_TEXT = 0x01
MODE_GRAPHICS = 0x02
MODE_BW = 0x04
MODE_ENABLE = 0x08
MODE_HIRES_GFX = 0x10
MODE_BLINK = 0x20


def gfx_palette(mode, color):
    """Pick the 4-entry palette for 320x200, as the CGA hardware does.

    Intensity comes from colour-select bit 4, palette choice from bit 5, and
    the mode-control BW bit overrides both with the third pair.
    """
    intense = 1 if (color & 0x10) else 0
    if mode & MODE_BW:
        row = 4 + intense
    elif color & 0x20:
        row = 2 + intense
    else:
        row = 0 + intense
    return GFX_PALETTES[row]

TRACE = None
MD = Cs(CS_ARCH_X86, CS_MODE_16)

# 8x8 CGA character ROM: 256 glyphs, one byte per row, MSB leftmost.
# Same file the emulator loads in ISA_CGA::ISA_CGA().
FONT = None


def load_font():
    global FONT
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for p in (os.path.join(here, "assets", "IBM_CGA-8.raw"),
              "assets/IBM_CGA-8.raw"):
        try:
            with open(p, "rb") as f:
                d = f.read()
            if len(d) >= 2048:
                FONT = d
                print("font ROM: %s" % p)
                return
        except OSError:
            continue
    print("font ROM not found -- text mode will render as blocks")


# ---------------------------------------------------------------- rendering

def png_bytes(px):
    h, w = len(px), len(px[0])
    raw = bytearray()
    for row in px:
        raw.append(0)
        for (r, g, b) in row:
            raw += bytes((r, g, b))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
            + chunk(b"IEND", b""))


def render_screen(instr):
    t = TRACE
    mode = t.last_port_write(0x3D8, instr)
    color = t.last_port_write(0x3D9, instr) or 0
    fb = t.region_fast(CGA_BASE, CGA_SIZE, instr)

    if mode is None:
        mode = 0x29

    if mode & MODE_GRAPHICS:
        if mode & MODE_HIRES_GFX:
            # 640x200x2. Foreground comes from colour-select bits 0-3 (not a
            # hardcoded white), and the border is hardwired black in this mode.
            fg_idx = color & 0x0F
            if fg_idx == 0:
                fg_idx = 15
            fg = PALETTE[fg_idx]
            px = [[PALETTE[0]] * 640 for _ in range(200)]
            for y in range(200):
                row = (0x2000 if (y & 1) else 0) + (y >> 1) * 80
                for xb in range(80):
                    b = fb[row + xb]
                    if not b:
                        continue
                    for k in range(8):
                        if (b >> (7 - k)) & 1:
                            px[y][xb * 8 + k] = fg
            return px, "640x200x2 mode %02X fg %X" % (mode, fg_idx)

        # 320x200x4. Pixel value 0 takes the background/border colour from
        # colour-select bits 0-3; values 1-3 index the selected palette.
        pal = gfx_palette(mode, color)
        bg = PALETTE[color & 0x0F]
        lut = [bg, PALETTE[pal[1]], PALETTE[pal[2]], PALETTE[pal[3]]]
        px = [[bg] * 320 for _ in range(200)]
        for y in range(200):
            row = (0x2000 if (y & 1) else 0) + (y >> 1) * 80
            prow = px[y]
            for xb in range(80):
                b = fb[row + xb]
                if not b:
                    continue
                base = xb * 4
                prow[base] = lut[(b >> 6) & 3]
                prow[base + 1] = lut[(b >> 4) & 3]
                prow[base + 2] = lut[(b >> 2) & 3]
                prow[base + 3] = lut[b & 3]
        return px, "320x200x4 mode %02X col %02X pal %d" % (
            mode, color, GFX_PALETTES.index(pal))

    # Text, rendered through the real 8x8 character ROM. 40-column cells are
    # 16 dots wide on the CRT (each font column doubled), which is what makes
    # 40-col and 80-col occupy the same screen width.
    cols = 80 if (mode & MODE_HIRES_TEXT) else 40
    cell_w = 8 if (mode & MODE_HIRES_TEXT) else 16
    px = [[PALETTE[0]] * (cols * cell_w) for _ in range(25 * 8)]
    for r in range(25):
        for c in range(cols):
            off = (r * cols + c) * 2
            if off + 1 >= len(fb):
                continue
            ch, attr = fb[off], fb[off + 1]
            fg_i = attr & 0x0F
            bg_i = (attr >> 4) & 0x0F
            if mode & MODE_BLINK:
                # Blink steals the high background bit; treat blinking text as
                # visible rather than guessing a phase.
                bg_i &= 0x07
            fg = PALETTE[fg_i]
            bg = PALETTE[bg_i]
            x0 = c * cell_w
            for dy in range(8):
                glyph = FONT[ch * 8 + dy] if FONT else 0
                rowpx = px[r * 8 + dy]
                for dx in range(cell_w):
                    col = dx * 8 // cell_w
                    rowpx[x0 + dx] = fg if (glyph >> (7 - col)) & 1 else bg
    return px, "text %dx25 mode %02X" % (cols, mode)


def screen_text(instr):
    t = TRACE
    mode = t.last_port_write(0x3D8, instr) or 0x29
    if mode & 0x02:
        return None
    cols = 80 if (mode & 0x01) else 40
    fb = t.region_fast(CGA_BASE, CGA_SIZE, instr)
    out = []
    for r in range(25):
        line = []
        for c in range(cols):
            ch = fb[(r * cols + c) * 2]
            line.append(chr(ch) if 32 <= ch < 127 else
                        (" " if ch in (0, 0xFF) else "."))
        out.append("".join(line).rstrip())
    return "\n".join(out)


# ------------------------------------------------------------------- disasm

def disasm_at(addr, instr, before=6, count=24):
    """Disassemble around `addr` using the bytes live at `instr`.

    Starts a little before the anchor and resyncs: x86 is not self-
    synchronising backwards, so the leading instructions may be wrong until
    the stream aligns. The anchor line itself is always correct because it is
    decoded from a known instruction boundary.
    """
    t = TRACE
    start = max(0, addr - before)
    length = 128
    buf = t.snapshot(instr)[start:start + length].tobytes()

    known = set(int(a) for a in t.nodes["addr"])
    out = []
    for ins in MD.disasm(buf, start):
        out.append(dict(
            addr=ins.address,
            bytes=ins.bytes.hex(),
            text="%s %s" % (ins.mnemonic, ins.op_str) if ins.op_str
                 else ins.mnemonic,
            executed=ins.address in known,
            anchor=ins.address == addr,
        ))
        if len(out) >= count:
            break
    return out


def build_cfg():
    """Group executed instructions into basic blocks and link them.

    A block starts at a leader: an address with no predecessor, more than one
    predecessor, or one whose sole predecessor branches. It runs until control
    leaves -- a branch, a return, or the next leader.

    Returns are treated as terminators rather than as edges to their many
    observed return sites. The trace records every caller's resume point as a
    successor of the RET, which is true but useless for structure: one RET in
    this trace has 245 successors and would swamp any layout.
    """
    t = TRACE
    succ = {}
    pred = {}
    for s, d in zip(t.edges["src"], t.edges["dst"]):
        s, d = int(s), int(d)
        succ.setdefault(s, set()).add(d)
        pred.setdefault(d, set()).add(s)

    info = {}
    for n in t.nodes:
        a = int(n["addr"])
        if a not in info or int(n["first"]) < info[a]["first"]:
            info[a] = dict(cs=int(n["cs"]), ip=int(n["ip"]),
                           first=int(n["first"]), hits=int(n["hits"]))
    addrs = sorted(info)
    aset = set(addrs)

    # Decode each instruction from the bytes IT executed, not from the end of
    # the trace: 252 addresses in this trace were overwritten after running,
    # including one that ran as RET and was later patched to NOP.
    raw = t.code_bytes(span=8)
    code = {}
    for (a, g), b in raw.items():
        # Prefer the earliest generation as the block's canonical decode;
        # later generations are reachable through the per-node listing.
        if a not in code or g == 0:
            code[a] = b

    decoded = {}
    for a in addrs:
        ins = next(MD.disasm(code.get(a, b"\x90"), a), None)
        if ins is None:
            decoded[a] = (1, "db ?", "")
        else:
            decoded[a] = (ins.size,
                          "%s %s" % (ins.mnemonic, ins.op_str) if ins.op_str
                          else ins.mnemonic,
                          ins.mnemonic)
    global CODE
    CODE = code

    RET = ("ret", "retf", "iret", "iretd")
    CALL = ("call", "lcall")

    leaders = set()
    for a in addrs:
        p = pred.get(a, ())
        if len(p) != 1:
            leaders.add(a)
        for q in p:
            if len(succ.get(q, ())) > 1 or decoded.get(q, (0, "", ""))[2] in RET:
                leaders.add(a)
    # A call's fall-through starts a new block.
    for a in addrs:
        if decoded[a][2] in CALL:
            nxt = a + decoded[a][0]
            if nxt in aset:
                leaders.add(nxt)
    if addrs:
        leaders.add(addrs[0])

    blocks = {}
    for lead in sorted(leaders):
        body = []
        a = lead
        while True:
            if a not in aset:
                break
            body.append(a)
            mn = decoded[a][2]
            if mn in RET:
                break
            s = succ.get(a, set())
            nxt = a + decoded[a][0]
            if len(s) > 1 or mn in CALL:
                break
            if nxt in leaders or nxt not in aset:
                break
            if s and nxt not in s:
                break
            a = nxt
        if not body:
            continue
        last = body[-1]
        mn = decoded[last][2]
        outs = []
        if mn not in RET:
            for d in sorted(succ.get(last, ())):
                outs.append(d)
            if mn in CALL:
                fall = last + decoded[last][0]
                if fall in aset and fall not in outs:
                    outs.append(fall)
        blocks[lead] = dict(
            addr=lead, end=last, n=len(body),
            cs=info[lead]["cs"], ip=info[lead]["ip"],
            first=info[lead]["first"], hits=info[lead]["hits"],
            term=mn, outs=outs,
        )

    # Map every executed address to its owning block.
    #
    # Walk the recorded body rather than re-deriving it from decoded lengths:
    # a block's instructions come from following observed edges, so stepping
    # by length can diverge and leave the tail unowned. Bodies are rebuilt
    # here from the same rule that produced them.
    owner = {}
    for lead, b in blocks.items():
        a = lead
        for _ in range(b["n"]):
            owner[a] = lead
            if a not in decoded:
                break
            a += decoded[a][0]

    # Anything executed but still unowned -- addresses interior to an
    # instruction, or reached by a path the block walk did not cover -- is
    # attributed to the nearest preceding block start in the same region.
    # Without this, roughly a quarter of the execution timeline lands in a
    # hole and time->block lookup reports nothing.
    leads_sorted = sorted(blocks)
    if leads_sorted:
        import bisect as _bi
        for a in addrs:
            if a in owner:
                continue
            k = _bi.bisect_right(leads_sorted, a) - 1
            if k >= 0:
                lead = leads_sorted[k]
                # Only claim it if it plausibly belongs to that block's span.
                if a - lead <= 64:
                    owner[a] = lead

    for b in blocks.values():
        seen, tgt = set(), []
        for d in b["outs"]:
            o = owner.get(d, d)
            if o in blocks and o not in seen:
                seen.add(o)
                tgt.append(o)
        b["outs"] = tgt

    # Callers per block, for the function view.
    callers = {}
    for b in blocks.values():
        if b["term"] in CALL and b["outs"]:
            callers.setdefault(b["outs"][0], set()).add(b["addr"])

    for b in blocks.values():
        b["ncall"] = len(callers.get(b["addr"], ()))

    return blocks, decoded, owner, succ, pred


CFG = None
DECODED = None
OWNER = None
CODE = None


def block_listing(lead, instr=None):
    """Disassemble one basic block.

    Each instruction is decoded from the bytes it actually executed. Passing
    `instr` overrides that with the bytes live at that moment, which is how
    you inspect a later generation of self-modified code.
    """
    b = CFG.get(lead)
    if not b:
        return []
    out, a = [], b["addr"]
    live = None
    if instr is not None:
        live = TRACE.snapshot(instr)
    for _ in range(b["n"]):
        buf = (live[a:a + 8].tobytes() if live is not None
               else CODE.get(a, b"\x90"))
        ins = next(MD.disasm(buf, a), None)
        if ins is None:
            break
        out.append(dict(addr=a, bytes=ins.bytes.hex(),
                        text="%s %s" % (ins.mnemonic, ins.op_str)
                             if ins.op_str else ins.mnemonic))
        a += ins.size
    return out


def cs_ip_at(instr):
    """The address executing at instruction count `instr`.

    Read straight from the execution timeline. Older traces without one fall
    back to the nearest first-execution, which is only meaningful during the
    discovery phase -- past that it is arbitrary, so it is reported as
    inexact rather than presented as the live position.
    """
    t = TRACE
    pc = t.pc_at(instr)
    if pc is not None:
        n = NODE_BY_ADDR.get(pc)
        if n:
            return pc, n["cs"], n["ip"], True
        # Executing inside an excluded region (BIOS): no node, but the
        # address is still exact.
        return pc, pc >> 4, pc & 0xF, True

    f = t.nodes["first"]
    order = np.argsort(f)
    k = np.searchsorted(f[order], instr, side="right")
    if k == 0:
        k = 1
    n = t.nodes[order[k - 1]]
    return int(n["addr"]), int(n["cs"]), int(n["ip"]), False


NODE_BY_ADDR = {}


# ---------------------------------------------------------------- meta/index

def build_meta():
    t = TRACE
    # Load events: group code-producing writes by time gap.
    prov = {}
    for n in t.nodes:
        a = int(n["addr"])
        if a not in prov or int(n["first"]) < prov[a]:
            prov[a] = int(n["first"])

    entries = []
    for addr, fe in prov.items():
        idx = t._slice_for(addr)
        if idx is None or not len(idx):
            continue
        times = t.instr[idx]
        k = np.searchsorted(times, fe, side="right")
        if k:
            j = idx[k - 1]
            entries.append((int(t.instr[j]), addr,
                            int(t.writes["cs"][j]), int(t.writes["ip"][j])))
    entries.sort()

    events = []
    cur = None
    for wi, addr, wcs, wip in entries:
        if cur is None or wi - cur["last"] > 20000:
            if cur:
                events.append(cur)
            cur = dict(first=wi, last=wi, addrs=[addr], writers=set())
        else:
            cur["last"] = wi
            cur["addrs"].append(addr)
        cur["writers"].add("%04X:%04X" % (wcs, wip))
    if cur:
        events.append(cur)

    ev = [dict(first=e["first"], last=e["last"], count=len(e["addrs"]),
               lo=min(e["addrs"]), hi=max(e["addrs"]),
               writers=sorted(e["writers"])[:4]) for e in events]

    modes = []
    if t.ports is not None and t.port_has_write:
        p = t.ports
        m = (p["port"] == 0x3D8) & (p["is_write"] == 1)
        seen = None
        for rec in p[m]:
            if int(rec["data"]) != seen:
                seen = int(rec["data"])
                modes.append(dict(instr=int(rec["instr"]), mode=seen,
                                  by="%04X:%04X" % (int(rec["cs"]),
                                                    int(rec["ip"]))))

    first_exec = int(t.nodes["first"].min()) if len(t.nodes) else 0
    return dict(
        max_instr=t.max_instr,
        first_exec=first_exec,
        nodes=len(t.nodes), edges=len(t.edges),
        writes=int(len(t.writes)), ports=int(len(t.ports)),
        events=ev, modes=modes,
    )


META = None


# -------------------------------------------------------------------- server

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, body, ctype="application/json"):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)

        def qi(name, default=None):
            if name in q:
                return int(q[name][0], 0)
            return default

        try:
            if u.path in ("/", "/index.html"):
                self._send(_load_page(), "text/html; charset=utf-8")
            elif u.path == "/api/meta":
                self._send(json.dumps(META))
            elif u.path == "/api/state":
                instr = qi("instr", META["max_instr"])
                addr, cs, ip, exact = cs_ip_at(instr)
                self._send(json.dumps(dict(
                    instr=instr, addr=addr, cs=cs, ip=ip, exact=exact,
                    block=OWNER.get(addr),
                    disasm=disasm_at(addr, instr),
                    text=screen_text(instr),
                )))
            elif u.path == "/api/nextblock":
                # Next/previous BLOCK transition along the real executed
                # path. Walks the execution timeline until the owning block
                # changes, so it follows the route actually taken rather than
                # a CFG successor that may not have been the one used here.
                instr = qi("instr", 0)
                d = qi("dir", 1)
                ex = TRACE.exec
                if ex is None or not len(ex):
                    self._send(json.dumps(dict(instr=instr)))
                else:
                    n = max(0, min(len(ex) - 1, instr))
                    cur = OWNER.get(int(ex[n]))
                    limit = 2_000_000
                    i = n
                    while 0 <= i < len(ex) and limit:
                        i += d
                        limit -= 1
                        if not (0 <= i < len(ex)):
                            break
                        if OWNER.get(int(ex[i])) != cur:
                            break
                    i = max(0, min(len(ex) - 1, i))
                    self._send(json.dumps(dict(
                        instr=i, addr=int(ex[i]),
                        block=OWNER.get(int(ex[i])))))
            elif u.path == "/api/pc":
                # Exact execution position, plus the real path taken around
                # it -- the actual sequence on this pass, not CFG successors.
                instr = qi("instr", META["max_instr"])
                addr, cs, ip, exact = cs_ip_at(instr)
                lo, win = TRACE.pc_window(instr, qi("before", 6),
                                          qi("after", 18))
                self._send(json.dumps(dict(
                    instr=instr, addr=addr, cs=cs, ip=ip, exact=exact,
                    block=OWNER.get(addr),
                    lo=lo, path=win,
                    blocks=[OWNER.get(a) for a in win],
                )))
            elif u.path == "/api/screen":
                instr = qi("instr", META["max_instr"])
                px, desc = render_screen(instr)
                body = png_bytes(px)
                self.send_response(200)
                self.send_header("Content-Type", "image/png")
                self.send_header("X-Screen-Desc", desc)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            elif u.path == "/api/disasm":
                addr = qi("addr", 0)
                instr = qi("instr", META["max_instr"])
                self._send(json.dumps(disasm_at(
                    addr, instr, before=0, count=qi("count", 40))))
            elif u.path == "/api/cfg":
                # Whole graph, laid out client-side.
                self._send(json.dumps(dict(
                    blocks=[dict(a=b["addr"], e=b["end"], n=b["n"],
                                 cs=b["cs"], ip=b["ip"], h=b["hits"],
                                 f=b["first"], t=b["term"],
                                 o=b["outs"], c=b["ncall"])
                            for b in CFG.values()])))
            elif u.path == "/api/block":
                lead = qi("addr", 0)
                instr = qi("instr", None)
                b = CFG.get(lead)
                if not b:
                    lead = OWNER.get(lead)
                    b = CFG.get(lead)
                if not b:
                    self._send(json.dumps(dict(error="no block")))
                else:
                    ins = block_listing(lead, instr)
                    inbound = [x["addr"] for x in CFG.values()
                               if lead in x["outs"]]
                    self._send(json.dumps(dict(
                        addr=b["addr"], end=b["end"], term=b["term"],
                        hits=b["hits"], first=b["first"], cs=b["cs"],
                        outs=b["outs"], ins=ins, inbound=inbound[:40],
                        ninbound=len(inbound))))
            elif u.path == "/api/mem":
                base = qi("base", 0) & 0xFFFFF
                length = min(qi("len", 256), 4096)
                instr = qi("instr", None)
                data = TRACE.region_fast(base, length, instr)
                # Mark which bytes were ever written at all, so unwritten
                # memory is visibly distinct from a byte that holds zero.
                a = TRACE.writes["addr"]
                sel = (a >= base) & (a < base + length)
                if instr is not None:
                    sel &= TRACE.instr <= instr
                touched = sorted(set(int(x) - base
                                     for x in a[np.flatnonzero(sel)]))
                self._send(json.dumps(dict(
                    base=base, data=data.hex(), touched=touched)))
            elif u.path == "/api/blocks":
                t = TRACE
                order = np.argsort(-t.nodes["hits"])[:qi("count", 200)]
                self._send(json.dumps([
                    dict(addr=int(t.nodes["addr"][i]),
                         cs=int(t.nodes["cs"][i]), ip=int(t.nodes["ip"][i]),
                         hits=int(t.nodes["hits"][i]),
                         first=int(t.nodes["first"][i]))
                    for i in order]))
            else:
                self.send_error(404)
        except Exception as e:  # keep the server alive on a bad query
            import traceback
            traceback.print_exc()
            self.send_error(500, str(e))


def _load_page():
    """UI markup lives next to this file so it can be edited without a
    restart of anything but the browser."""
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "bcfg_page.html")
    with open(p, encoding="utf-8") as f:
        return f.read()


def main():
    global TRACE, META, CFG, DECODED, OWNER
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    path = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8777

    load_font()
    print("loading %s ..." % path)
    TRACE = FastTrace(path)
    print("indexing ...")
    META = build_meta()
    print("building CFG ...")
    CFG, DECODED, OWNER, _s, _p = build_cfg()
    META["blocks"] = len(CFG)
    for n in TRACE.nodes:
        a = int(n["addr"])
        if a not in NODE_BY_ADDR:
            NODE_BY_ADDR[a] = dict(cs=int(n["cs"]), ip=int(n["ip"]))
    META["has_exec"] = TRACE.exec is not None and len(TRACE.exec) > 0
    META["exec_len"] = int(len(TRACE.exec)) if META["has_exec"] else 0
    if META["has_exec"]:
        print("  execution timeline: %d instructions" % META["exec_len"])
    else:
        print("  no execution timeline in this trace -- rebuild to enable "
              "exact time->code position")
    print("  %d nodes, %d writes, %d basic blocks, %d mode changes"
          % (META["nodes"], META["writes"], len(CFG), len(META["modes"])))

    srv = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print("\n  http://127.0.0.1:%d\n" % port)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
