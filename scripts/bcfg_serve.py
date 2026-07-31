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


def ega_regs(instr):
    """Replay the EGA's indexed register files up to `instr`.

    Every one of these is write-only on the card, so the port log is the only
    record of them. The attribute controller shares one port with an
    index/data flip-flop that a status read resets, so the reads matter too.
    """
    po = TRACE.ports
    if po is None or not len(po):
        return None
    end = int(np.searchsorted(po["instr"], instr, side="right"))
    w = po[:end]
    seq, gc, crtc, attr = {}, {}, {}, {}
    misc = None
    si = gi = ci = ai = 0
    flip = False
    for r in w:
        pt, v, isw = int(r["port"]), int(r["data"]), int(r["is_write"])
        if pt in (0x3BA, 0x3DA):
            if not isw:
                flip = False          # status read resets the flip-flop
            continue
        if not isw:
            continue
        if pt == 0x3C2:   misc = v
        elif pt == 0x3C4: si = v & 7
        elif pt == 0x3C5: seq[si] = v
        elif pt == 0x3CE: gi = v & 15
        elif pt == 0x3CF: gc[gi] = v
        elif pt in (0x3B4, 0x3D4): ci = v & 0x1F
        elif pt in (0x3B5, 0x3D5): crtc[ci] = v
        elif pt == 0x3C0:
            if not flip:
                ai = v & 0x1F
            else:
                attr[ai] = v
            flip = not flip
    if misc is None:
        return None
    return {"misc": misc, "seq": seq, "gc": gc, "crtc": crtc, "attr": attr}


def ega_color(c, rgbi):
    """Palette register to RGB.

    Transcribed from ega_color() in src/display/ega_display.cpp so the viewer
    and the emulator's shader agree. 200-line modes are 15.7 kHz, where the
    monitor decodes RGBI: bit 4 is intensity, secondary red/blue are ignored,
    and colour 6 gets the CGA brown treatment.
    """
    if rgbi:
        idx = (((c >> 4) & 1) << 3) | (c & 7)
        i = (idx >> 3) & 1
        r = ((idx >> 2) & 1) * 0xAA + i * 0x55
        g = ((idx >> 1) & 1) * 0xAA + i * 0x55
        b = ((idx >> 0) & 1) * 0xAA + i * 0x55
        if idx == 6:
            g = 0x55
        return (r, g, b)
    r = (((c >> 2) & 1) * 2 + ((c >> 5) & 1)) * 85
    g = (((c >> 1) & 1) * 2 + ((c >> 4) & 1)) * 85
    b = (((c >> 0) & 1) * 2 + ((c >> 3) & 1)) * 85
    return (r, g, b)


def render_ega(instr):
    """Render from the PLNW section, or None if this is not an EGA trace.

    The planes are recorded post-pipeline, so nothing here has to model
    set/reset, the ALU, the latches or the bit mask -- the bytes are already
    what the beam would fetch.
    """
    pl = TRACE.plane_snapshot(instr)
    if pl is None:
        return None
    reg = ega_regs(instr)
    if reg is None:
        return None

    crtc, seq, attr = reg["crtc"], reg["seq"], reg["attr"]
    # Geometry straight off the CRTC rather than assumed.
    w_chars = crtc.get(1, 39) + 1
    stride = crtc.get(19, 40) * 2 or w_chars
    vde = crtc.get(18, 0xC7) | ((crtc.get(7, 0) & 0x02) << 7) \
                             | ((crtc.get(7, 0) & 0x40) << 3)
    height = vde + 1
    if not (64 <= height <= 480):
        height = 200
    # Sequencer Clocking bit 3 halves the dot clock: 8 px/char at 320 wide.
    width = w_chars * 8
    if width < 8 or width > 800:
        width = 320
    height = min(height, 480)

    vt = crtc.get(6, 0) | ((crtc.get(7, 0) & 1) << 8)
    rgbi = 1 if (vt + 2) < 300 else 0

    lut = [ega_color(attr.get(i, 0), rgbi) for i in range(16)]
    # Index 0 is a palette entry like any other -- it is NOT black. Filling
    # with lut[0] and skipping all-zero bytes would be equivalent, but doing
    # it explicitly keeps the two paths from drifting apart.
    px = [[lut[0]] * width for _ in range(height)]
    nbytes = min(stride, (width + 7) // 8)
    for y in range(height):
        base = y * stride
        if base + stride > 0x10000:
            break
        row = px[y]
        p0, p1, p2, p3 = (pl[0][base:base + stride], pl[1][base:base + stride],
                          pl[2][base:base + stride], pl[3][base:base + stride])
        for xb in range(nbytes):
            b0, b1, b2, b3 = int(p0[xb]), int(p1[xb]), int(p2[xb]), int(p3[xb])
            if not (b0 | b1 | b2 | b3):
                continue          # already lut[0] from the fill
            for k in range(8):
                x = xb * 8 + k
                if x >= width:
                    break
                sh = 7 - k
                row[x] = lut[((b0 >> sh) & 1)
                             | (((b1 >> sh) & 1) << 1)
                             | (((b2 >> sh) & 1) << 2)
                             | (((b3 >> sh) & 1) << 3)]
    return px, "EGA %dx%d %s stride %d" % (
        width, height, "RGBI" if rgbi else "rgbRGB", stride)


def render_screen(instr):
    t = TRACE
    # EGA first: if the trace has plane records, the CGA aperture is not
    # where the picture is.
    e = render_ega(instr)
    if e is not None:
        return e

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



# ------------------------------------------------------------------- disasm


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
        sz, txt, mn, _tgt = decode_at(a, code.get(a, b"\x90"))
        decoded[a] = (sz, txt, mn)
    global CODE
    CODE = code

    RET = ("ret", "retf", "iret", "iretd")
    CALL = ("call", "lcall")

    # Step to the address the CPU actually went to, not addr+size.
    #
    # The 8088 executes segment-override and REP prefixes as separate
    # instructions (cases 27 and 23 in eu_run), so the trace holds a node at
    # the prefix AND at the instruction it modifies -- 253 of them here.
    # Capstone decodes the prefix node as one merged instruction, so its size
    # spans both and addr+size steps straight over the body node. That skipped
    # 841 executed addresses, whole runs of straight-line code.
    #
    # The recorded single successor is authoritative and needs no such
    # reasoning.
    def next_addr(a):
        s = succ.get(a, ())
        if len(s) == 1:
            return next(iter(s))
        return a + decoded[a][0]

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
            nxt = next_addr(a)
            if nxt in aset:
                leaders.add(nxt)
    if addrs:
        leaders.add(addrs[0])

    # Partition the executed addresses into blocks.
    #
    # Walk in address order, following observed flow, and stop when the next
    # address is another leader. Every executed address ends up in exactly one
    # body: no overlap (a leader's walk cannot enter another leader) and no
    # gaps (an address that is not a leader is reached from its predecessor,
    # and one that is starts its own block).
    #
    # An earlier version walked leaders in dict order and let one block's walk
    # swallow addresses that were themselves leaders, which orphaned 900
    # instructions -- whole runs of straight-line code with a single
    # contiguous predecessor each.
    blocks = {}
    claimed = set()
    for lead in sorted(leaders):
        if lead in claimed or lead not in aset:
            continue
        body = []
        a = lead
        while True:
            if a not in aset or a in claimed:
                break
            body.append(a)
            claimed.add(a)
            mn = decoded[a][2]
            if mn in RET:
                break
            if len(succ.get(a, ())) > 1 or mn in CALL:
                break
            nxt = next_addr(a)
            if nxt in leaders or nxt not in aset or nxt in claimed:
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
        if not outs and mn not in RET:
            # Straight-line end with no recorded successor: the next executed
            # address continues the flow.
            nxt = next_addr(last)
            if nxt in aset:
                outs.append(nxt)
        blocks[lead] = dict(
            addr=lead, end=last, n=len(body),
            cs=info[lead]["cs"], ip=info[lead]["ip"],
            first=info[lead]["first"], hits=info[lead]["hits"],
            term=mn, outs=outs, body=body,
        )

    # Map every executed address to its owning block.
    #
    # Walk the recorded body rather than re-deriving it from decoded lengths:
    # a block's instructions come from following observed edges, so stepping
    # by length can diverge and leave the tail unowned. Bodies are rebuilt
    # here from the same rule that produced them.
    # Use the addresses the block walk actually visited. Re-deriving them by
    # stepping decoded instruction lengths diverges wherever a recorded
    # successor is not addr+size, which orphaned 42% of executed addresses.
    owner = {}
    for lead, b in blocks.items():
        for a in b["body"]:
            owner[a] = lead

    # Anything executed but still unowned is attributed to the nearest
    # preceding block start in the same region, so time->block lookup always
    # resolves.
    #
    # How much this has to do is a health check on the CFG rather than a
    # feature: before interrupt handlers were excluded from edge recording,
    # phantom edges fragmented straight-line code and left ~24% of the
    # execution timeline unowned. A large count here means edges are still
    # being recorded that did not happen.
    unowned_before = sum(1 for a in addrs if a not in owner)
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
    if unowned_before:
        print("  %d/%d executed addresses needed fallback ownership (%.1f%%)"
              % (unowned_before, len(addrs),
                 100.0 * unowned_before / max(1, len(addrs))))

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


def build_functions(blocks, callers=None):
    """Group basic blocks into functions.

    A function's entry is a block that something CALLs, plus the program's
    first block. Its body is everything reachable from that entry by ordinary
    flow -- branches and fall-through -- without passing through another
    entry and without following call edges, which belong to the callee.

    Blocks reachable from no entry at all (dead ends, or code the trace
    entered by a jump from somewhere unrecorded) become their own single-block
    functions so nothing is dropped from the view.
    """
    # Entries: the target of a CALL, or of a far jump.
    #
    # A far jump is a transfer to another region entirely -- the boot sector
    # ends `ljmp 0x20:0x3c` into the loaded program -- so it starts a new
    # routine just as a call does. Near jumps are ordinary intra-function
    # control flow and are not entries.
    entries = set()
    for b in blocks.values():
        if b["term"] in ("call", "lcall", "ljmp") and b["outs"]:
            entries.add(b["outs"][0])
    if blocks:
        entries.add(min(blocks, key=lambda a: blocks[a]["first"]))

    # Flow edges only -- a call's target is the callee's problem, but its
    # fall-through continues this function. A far jump leaves entirely, so it
    # contributes no flow: without this the walk runs straight through the
    # ljmp and swallows the whole next region.
    flow = {}
    for a, b in blocks.items():
        if b["term"] in ("call", "lcall"):
            # outs[0] is the callee; anything after is the return path.
            flow[a] = b["outs"][1:]
        elif b["term"] == "ljmp":
            flow[a] = []
        else:
            flow[a] = list(b["outs"])

    # Claim blocks first-come, so each belongs to exactly one function.
    # Without this a block reachable from two entries is counted in both and
    # the instruction totals exceed what actually executed.
    #
    # Order matters: entries are visited by first execution, so the routine
    # that ran earliest claims shared tails. That is arbitrary where two
    # functions genuinely share code, which does happen in hand-written asm --
    # the alternative is duplicating the tail, which double-counts instead.
    funcs = {}
    assigned = {}
    order = sorted(e for e in entries if e in blocks)
    order.sort(key=lambda a: blocks[a]["first"])
    for e in order:
        if e in assigned:
            continue
        body, stack = [e], [e]
        assigned[e] = e
        while stack:
            cur = stack.pop()
            for nxt in flow.get(cur, ()):
                if nxt in assigned or nxt in entries or nxt not in blocks:
                    continue
                assigned[nxt] = e
                body.append(nxt)
                stack.append(nxt)
        funcs[e] = sorted(body)

    # Anything unclaimed becomes its own function so the view is complete.
    for a in sorted(blocks):
        if a not in assigned:
            funcs[a] = [a]
            assigned[a] = a

    out = {}
    for e, body in funcs.items():
        ins = sum(blocks[a]["n"] for a in body if a in blocks)
        hits = blocks[e]["hits"] if e in blocks else 0
        lo = min(body)
        hi = max(blocks[a]["end"] for a in body if a in blocks)
        # Which functions this one calls, and who calls it.
        outc = set()
        for a in body:
            b = blocks.get(a)
            if b and b["term"] in ("call", "lcall") and b["outs"]:
                t = assigned.get(b["outs"][0], b["outs"][0])
                if t != e:
                    outc.add(t)
        out[e] = dict(
            addr=e, blocks=body, nblocks=len(body), ins=ins,
            hits=hits, lo=lo, hi=hi,
            cs=blocks[e]["cs"] if e in blocks else 0,
            ip=blocks[e]["ip"] if e in blocks else 0,
            first=blocks[e]["first"] if e in blocks else 0,
            ncall=blocks[e]["ncall"] if e in blocks else 0,
            calls=sorted(outc),
        )
    # Callers, derived from the calls sets.
    rev = {}
    for e, f in out.items():
        for t in f["calls"]:
            rev.setdefault(t, set()).add(e)
    for e, f in out.items():
        f["callers"] = sorted(rev.get(e, ()))
    return out, assigned


CFG = None
DECODED = None
OWNER = None
CODE = None
FUNCS = None
FUNC_OF = None
MAP = None


def flow_at(instr, before=40, after=120):
    """The blocks executed around instruction `instr`, in execution order.

    This is the chart: the actual path the program took, read top to bottom,
    with each entry stamped by the instruction count where it began. Because
    the rows ARE the timeline, scrolling the chart and dragging the scrubber
    are the same motion.

    A graph-distance layout was the wrong model -- it showed an abstract rank
    from the entry point, which is not the order anything happens in.
    """
    ex = TRACE.exec
    if ex is None or not len(ex):
        return dict(rows=[], instr=instr)

    n = max(0, min(len(ex) - 1, int(instr)))

    # The window is counted in BLOCK ENTRIES, not instructions. A row is
    # emitted when the executing block changes, so in a tight loop a window
    # measured in instructions would yield almost nothing -- 60 instructions
    # of a 3-instruction loop is one row.
    # BIOS and interrupt handlers are skipped entirely. They are known code,
    # they are not what is being reverse-engineered, and timer ticks land at
    # arbitrary points -- a row saying "a handler ran here" is noise that
    # breaks up the flow being read.
    def blk_at(i):
        return OWNER.get(int(ex[i]))

    # Backwards: find where the previous `before` block entries began.
    back = []
    cur = blk_at(n)
    i = n
    while i > 0 and len(back) < before:
        b = blk_at(i - 1)
        if b is not None and b != cur:
            back.append(i)
            cur = b
        i -= 1
    lo = max(0, (back[-1] - 1) if back else 0)

    # Forwards: walk until we have collected `after` block entries.
    rows = []
    cur_blk = None
    entries = 0
    for i in range(lo, len(ex)):
        blk = blk_at(i)
        if blk is None or blk == cur_blk:
            continue
        cur_blk = blk
        entries += 1
        if entries > before + after:
            break
        b = CFG.get(blk, {})
        a = int(ex[i])
        # Show the instruction AT this address -- the one about to execute --
        # not the block's terminator. Those are different instructions and
        # pairing one address with the other's mnemonic is simply wrong.
        _sz, _txt, _mn, _tgt = decode_at(a, CODE.get(a, b"\x90"))
        rows.append(dict(
            instr=i, addr=a, blk=blk,
            fn=FUNC_OF.get(blk, blk) if FUNC_OF else blk,
            n=b.get("n", 0),
            t=_txt,
            term=b.get("term", ""),
            hits=b.get("hits", 0),
            kind="call" if b.get("term") in ("call", "lcall") else "blk",
        ))
    hi = rows[-1]["instr"] if rows else lo

    # Call depth, so nesting is visible as indentation: a CALL pushes, and a
    # return shows up as the next row landing back at a shallower address.
    depth = 0
    stack = []
    for r in rows:
        # Returning: this row is back in the function that made the call.
        if stack and stack[-1] == r["fn"]:
            stack.pop()
            depth = max(0, depth - 1)
        r["depth"] = depth
        if r["kind"] == "call":
            stack.append(r["fn"])
            depth += 1
    return dict(rows=rows, instr=n, lo=lo, hi=hi, total=len(ex))


def block_listing(lead, instr=None):
    """Disassemble one basic block.

    Each instruction is decoded from the bytes it actually executed. Passing
    `instr` overrides that with the bytes live at that moment, which is how
    you inspect a later generation of self-modified code.
    """
    b = CFG.get(lead)
    if not b:
        return []
    live = None
    if instr is not None:
        live = TRACE.snapshot(instr)
    out = []
    # Walk the addresses the block actually visited. Stepping by decoded
    # instruction length instead would list whatever bytes happen to follow,
    # which is not where control went: at 002C1 `jae 0x2c9` the branch was
    # taken every time, so 002C9 ran and 002C3 never did -- yet a
    # length-stepping walk shows 002C3 and omits 002C9.
    for a in b["body"]:
        buf = (live[a:a + 8].tobytes() if live is not None
               else CODE.get(a, b"\x90"))
        sz, txt, mn, tgt = decode_at(a, buf)
        if not sz:
            continue
        out.append(dict(addr=a, bytes=buf[:sz].hex(), text=txt,
                        target=tgt))
    return out




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
    if t.ports is not None:
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
            elif u.path == "/api/seek":
                # Navigate the execution timeline by structure rather than by
                # a fixed instruction count. Nothing is simulated -- these are
                # searches over what was recorded.
                #
                #   into  next instruction in a DIFFERENT function (a call, or
                #         any transfer into other code)
                #   out   where this routine returned: the first instruction
                #         whose SP is above the current SP, which is what a
                #         RET leaves behind
                #   over  the instruction after the call at the cursor, found
                #         by the same SP rule
                #   fn    next entry to a different function, forwards or back
                what = q.get("what", ["into"])[0]
                instr = qi("instr", 0)
                d = qi("dir", 1)
                ex, st = TRACE.exec, TRACE.states
                res = instr
                if ex is not None and len(ex):
                    n = max(0, min(len(ex) - 1, instr))
                    limit = 4_000_000

                    def fn_of(i):
                        b = OWNER.get(int(ex[i]))
                        return FUNC_OF.get(b, b) if (FUNC_OF and b is not None) else b

                    if what in ("fn", "into"):
                        cur = fn_of(n)
                        i = n
                        while 0 <= i < len(ex) and limit:
                            i += d
                            limit -= 1
                            if not (0 <= i < len(ex)):
                                break
                            f = fn_of(i)
                            if f is not None and f != cur:
                                res = i
                                break
                    elif what in ("out", "over"):
                        # SP at the cursor, from the recorded register state.
                        k = int(np.searchsorted(st["instr"], n, "right")) - 1 \
                            if st is not None and len(st) else -1
                        if k >= 0:
                            sp0 = int(st["sp"][k])
                            ss0 = int(st["ss"][k])
                            # For "over", the call has not pushed yet, so the
                            # target SP is the current one; for "out" we want
                            # SP strictly above, past this frame's return.
                            want = sp0 if what == "over" else sp0 + 1
                            j = k
                            while j + 1 < len(st) and limit:
                                j += 1
                                limit -= 1
                                if int(st["ss"][j]) != ss0:
                                    continue
                                if int(st["sp"][j]) >= want:
                                    res = int(st["instr"][j])
                                    break
                self._send(json.dumps(dict(instr=res)))
            elif u.path == "/api/step":
                # One row per INSTRUCTION around a point in time, with the
                # register state at each. This is the view that answers "what
                # was BX here" without inferring it from anything.
                instr = qi("instr", 0)
                before = qi("before", 24)
                after = qi("after", 80)
                ex = TRACE.exec
                st = TRACE.states
                rows = []
                if ex is not None and len(ex):
                    lo = max(0, instr - before)
                    hi = min(len(ex), instr + after)
                    for i in range(lo, hi):
                        a = int(ex[i])
                        r = dict(instr=i, addr=a)
                        # Decode from the bytes this instruction executed, at
                        # its segment offset so relative targets resolve.
                        buf = CODE.get(a, b"\x90")
                        sz, txt, mn, tgt = decode_at(a, buf)
                        r["text"] = txt
                        r["bytes"] = buf[:sz].hex()
                        r["target"] = tgt
                        r["blk"] = OWNER.get(a)
                        rows.append(r)
                    # Attach register state. Records exist only for traced
                    # (non-BIOS, non-handler) instructions, so match on the
                    # instruction count rather than assuming alignment.
                    if st is not None and len(st):
                        k0 = int(np.searchsorted(st["instr"], lo, "left"))
                        k1 = int(np.searchsorted(st["instr"], hi, "left"))
                        by = {int(st["instr"][k]): k for k in range(k0, k1)}
                        for r in rows:
                            k = by.get(r["instr"])
                            if k is None:
                                continue
                            s = st[k]
                            r["regs"] = {n: int(s[n]) for n in (
                                "ax", "cx", "dx", "bx", "sp", "bp", "si",
                                "di", "es", "cs", "ss", "ds", "flags")}
                self._send(json.dumps(dict(instr=instr, rows=rows)))
            elif u.path == "/api/flow":
                # The chart: blocks in the order they actually executed around
                # a point in time. Rows are timeline positions, so scrolling
                # the chart and moving the scrubber are the same motion.
                instr = qi("instr", 0)
                self._send(json.dumps(flow_at(
                    instr, qi("before", 40), qi("after", 120))))
            elif u.path == "/api/asm":
                # Full disassembly of the function containing an address,
                # decoded from the bytes each instruction actually executed.
                a = qi("addr", 0)
                fe = FUNC_OF.get(OWNER.get(a, a), None) if FUNC_OF else None
                f = FUNCS.get(fe) if fe is not None else None
                if not f:
                    blk = OWNER.get(a)
                    f = dict(addr=blk if blk is not None else a,
                             blocks=[blk] if blk is not None else [])
                # One listing per address, in address order.
                #
                # Blocks can share instructions -- a block's tail is often
                # another block's head -- so listing block-by-block prints
                # those runs twice. Collect by address instead.
                seen = {}
                for blk in f["blocks"]:
                    if blk not in CFG:
                        continue
                    for ins in block_listing(blk, None):
                        seen.setdefault(ins["addr"], ins)

                # Loops, as ranges. A back-edge from `src` to `head` where
                # head <= src means everything between them is the loop body.
                #
                # "Target is at a lower address" is not sufficient on its own:
                # a far jump forward to a lower PHYSICAL address (ljmp
                # 0x20:0x3c -> 0023C) looks identical. Require the whole span
                # to be contiguous executed code, which a real loop body is
                # and a segment change is not.
                addr_set = set(seen)

                def contiguous(head, end):
                    a = head
                    while a < end:
                        ins = seen.get(a)
                        if ins is None:
                            return False
                        a += max(1, len(ins["bytes"]) // 2)
                    return a == end

                loops = []
                for blk in f["blocks"]:
                    b = CFG.get(blk)
                    if not b:
                        continue
                    for tgt in b["outs"]:
                        if (tgt in addr_set and tgt <= b["end"]
                                and tgt in CFG and contiguous(tgt, b["end"])):
                            loops.append((tgt, b["end"]))
                # Innermost first, so nesting depth comes out right.
                loops.sort(key=lambda lh: (lh[0], -lh[1]))

                addrs = sorted(seen)
                out = []
                prev_end = None
                for addr in addrs:
                    ins = seen[addr]
                    if prev_end is not None and addr != prev_end:
                        out.append(dict(addr=None, sep=1, gap=addr - prev_end))
                    # Depth = how many loop bodies contain this address.
                    depth = sum(1 for (h, e) in loops if h <= addr <= e)
                    ins = dict(ins)
                    ins["depth"] = depth
                    ins["head"] = 1 if any(h == addr for (h, _e) in loops) else 0
                    ins["tail"] = 1 if any(e == addr for (_h, e) in loops) else 0
                    out.append(ins)
                    prev_end = addr + max(1, len(ins["bytes"]) // 2)
                self._send(json.dumps(dict(
                    fn=f["addr"], ins=out,
                    nblocks=len(f["blocks"]),
                    loops=[dict(head=h, end=e) for (h, e) in loops])))
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
            else:
                self.send_error(404)
        except Exception as e:  # keep the server alive on a bad query
            import traceback
            traceback.print_exc()
            self.send_error(500, str(e))


def decode_at(addr, buf, cs=None):
    """Decode one instruction, resolving branch targets correctly.

    Capstone must be given the instruction's OFFSET WITHIN ITS SEGMENT, not its
    physical address. A near call or jump encodes a 16-bit displacement from
    IP, and the target wraps at 16 bits. Decoding with a 20-bit physical base
    instead computes `phys + size + disp` with no wrap, which produces targets
    outside the address space -- `call 0x1262b` for an instruction whose real
    target is 0262B, because the true sum wrapped past 0xFFFF.

    `cs` comes from the recorded register state. Without it the physical base
    is used and relative targets are only right when no wrap occurs, which is
    why local jumps looked fine and calls did not.

    Returns (size, text, mnemonic, target_phys or None).
    """
    if cs is None:
        cs = CS_AT.get(addr)
    if cs is None:
        # No recorded segment: decode at the physical address. Non-relative
        # operands are still correct; relative ones may not be.
        ins = next(MD.disasm(buf, addr), None)
        if ins is None:
            return 1, "db ?", "", None
        txt = "%s %s" % (ins.mnemonic, ins.op_str) if ins.op_str else ins.mnemonic
        return ins.size, txt, ins.mnemonic, None

    base = (cs << 4) & 0xFFFFF
    ip = (addr - base) & 0xFFFF
    ins = next(MD.disasm(buf, ip), None)
    if ins is None:
        return 1, "db ?", "", None

    mn, ops = ins.mnemonic, ins.op_str
    target = None
    # Relative branches: rewrite the operand as a physical address so it can be
    # followed. Capstone reports the target already wrapped to 16 bits because
    # it decoded at the segment offset.
    if ops.startswith("0x") and mn in _REL:
        try:
            off = int(ops, 16) & 0xFFFF
            target = (base + off) & 0xFFFFF
            ops = "%05X" % target
        except ValueError:
            pass
    txt = "%s %s" % (mn, ops) if ops else mn
    return ins.size, txt, mn, target


_REL = {"call", "jmp", "je", "jne", "jz", "jnz", "jb", "jnb", "jbe", "jnbe",
        "ja", "jae", "jl", "jle", "jg", "jge", "jo", "jno", "js", "jns",
        "jp", "jnp", "jpe", "jpo", "jc", "jnc", "jcxz", "loop", "loope",
        "loopne", "loopz", "loopnz"}

# Physical address -> the CS it executed under, from the register trace.
CS_AT = {}


def build_cs_map():
    """CS per executed address, so relative operands decode correctly."""
    out = {}
    st = TRACE.states
    if st is None or not len(st):
        return out
    # One pass; first CS seen for an address wins. Code executing under two
    # different CS values would need per-generation handling, but nothing in
    # these traces does that.
    for a, c in zip(st["addr"], st["cs"]):
        a = int(a)
        if a not in out:
            out[a] = int(c)
    return out


def _load_page():
    """UI markup lives next to this file so it can be edited without a
    restart of anything but the browser."""
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "bcfg_page.html")
    with open(p, encoding="utf-8") as f:
        return f.read()


def main():
    global TRACE, META, CFG, DECODED, OWNER, FUNCS, FUNC_OF
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
    # CS per address first: build_cfg decodes, and relative branch targets are
    # only correct when decoded at the segment offset.
    print("mapping segments ...")
    CS_AT.update(build_cs_map())
    print("  %d addresses with a recorded CS" % len(CS_AT))
    print("building CFG ...")
    CFG, DECODED, OWNER, _s, _p = build_cfg()
    META["blocks"] = len(CFG)
    FUNCS, FUNC_OF = build_functions(CFG)
    META["funcs"] = len(FUNCS)
    print("  %d basic blocks in %d functions" % (len(CFG), len(FUNCS)))
    if TRACE.exec is None or not len(TRACE.exec):
        print("  ERROR: no execution timeline in this trace. The chart is the"
              " executed path, so it needs one -- rebuild and re-record.")
        return 1
    META["max_instr"] = int(len(TRACE.exec)) - 1
    print("  execution timeline: %d instructions" % len(TRACE.exec))

    srv = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print("\n  http://127.0.0.1:%d\n" % port)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
