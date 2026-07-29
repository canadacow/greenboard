"""Reader for bench execution traces (.bcfg).

A trace is a complete record of a session from RESET: the control flow graph,
every memory write from any source, and every port read. Because tracing
starts at reset there is no initial memory image -- every meaningful byte in
the machine got there via a recorded write, so memory at any point is exactly
the writes up to that point.

Usage:
    python scripts/bcfg.py bench_trace.bcfg              # summary
    python scripts/bcfg.py bench_trace.bcfg --at 7C00    # who wrote this byte
    python scripts/bcfg.py bench_trace.bcfg --snap 7C00 200
    python scripts/bcfg.py bench_trace.bcfg --code       # executed addresses
"""

import struct
import sys
from collections import defaultdict


class Trace:
    def __init__(self, path):
        with open(path, "rb") as f:
            buf = f.read()

        if buf[:4] != b"BCFG":
            raise ValueError("not a bcfg file")
        off = 4
        (self.version,) = struct.unpack_from("<I", buf, off)
        off += 4

        self.nodes = []
        self.edges = []
        self.writes = []
        self.ports = []
        self.exec = []

        while off < len(buf):
            tag = buf[off:off + 4]
            off += 4
            (count,) = struct.unpack_from("<Q", buf, off)
            off += 8

            if tag == b"NODE":
                sz = struct.calcsize("<IHHIQQ")
                for _ in range(count):
                    cs_ip, cs, ip, gen, first, hits = struct.unpack_from(
                        "<IHHIQQ", buf, off)
                    off += sz
                    self.nodes.append(
                        dict(addr=cs_ip, cs=cs, ip=ip, gen=gen,
                             first=first, hits=hits))
            elif tag == b"EDGE":
                sz = struct.calcsize("<II")
                for _ in range(count):
                    a, b = struct.unpack_from("<II", buf, off)
                    off += sz
                    self.edges.append((a, b))
            elif tag == b"WRIT":
                sz = struct.calcsize("<QIHHBBH")
                for _ in range(count):
                    instr, addr, cs, ip, data, _r, _pad = struct.unpack_from(
                        "<QIHHBBH", buf, off)
                    off += sz
                    self.writes.append(
                        dict(instr=instr, addr=addr, cs=cs, ip=ip, data=data))
            elif tag == b"EXEC":
                # Execution timeline: one u32 per instruction, index == the
                # instruction count.
                self.exec = list(struct.unpack_from(
                    "<%dI" % count, buf, off))
                off += count * 4
            elif tag == b"PORT":
                # Two on-disk layouts exist: the original 16-byte record
                # (reads only) and the 18-byte record that added is_write.
                # PORT is the last section, so the true size follows from the
                # bytes remaining.
                rem = len(buf) - off
                sz = rem // count if count else 16
                for _ in range(count):
                    if sz >= 18:
                        instr, port, cs, ip, data, isw, _pad = \
                            struct.unpack_from("<QHHHBBH", buf, off)
                    else:
                        instr, port, cs, ip, data, _pad = \
                            struct.unpack_from("<QHHHBB", buf, off)
                        isw = 0
                    off += sz
                    self.ports.append(
                        dict(instr=instr, port=port, cs=cs, ip=ip,
                             data=data, is_write=bool(isw)))
            else:
                raise ValueError("unknown section tag %r at %d" % (tag, off - 12))

        # Writes are appended in execution order, so per-address lists are
        # already sorted by time. This is the index everything else uses.
        self.by_addr = defaultdict(list)
        for i, w in enumerate(self.writes):
            self.by_addr[w["addr"]].append(i)

    # -- queries ----------------------------------------------------------

    def value_at(self, addr, instr=None):
        """Value of a byte, optionally as of a point in the trace.

        Returns None if the address was never written -- which is a real
        answer: the code read uninitialized memory.
        """
        idxs = self.by_addr.get(addr)
        if not idxs:
            return None
        if instr is None:
            return self.writes[idxs[-1]]["data"]
        last = None
        for i in idxs:
            if self.writes[i]["instr"] > instr:
                break
            last = self.writes[i]["data"]
        return last

    def snapshot(self, base, length, instr=None):
        """Reconstruct a memory region as of a point in the trace."""
        return bytes(self.value_at(base + i, instr) or 0 for i in range(length))

    def writers(self, addr):
        """Distinct CPU positions recorded against writes here."""
        out = []
        seen = set()
        for i in self.by_addr.get(addr, []):
            w = self.writes[i]
            key = "%04X:%04X" % (w["cs"], w["ip"])
            if key not in seen:
                seen.add(key)
                out.append(key)
        return out

    def successors(self):
        g = defaultdict(set)
        for a, b in self.edges:
            g[a].add(b)
        return g

    def call_targets(self, min_preds=2):
        """Addresses reached from several distinct predecessors.

        Every observed predecessor->successor pair is recorded, including
        fall-throughs, so a target with many distinct predecessors is one
        that control converges on -- a subroutine entry or a loop head.
        Separating those two needs instruction lengths from a disassembler;
        this is the cheap approximation.
        """
        targets = defaultdict(int)
        for _a, b in self.edges:
            targets[b] += 1
        return sorted(((a, c) for a, c in targets.items() if c >= min_preds),
                      key=lambda kv: -kv[1])


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    t = Trace(sys.argv[1])
    args = sys.argv[2:]

    if "--at" in args:
        addr = int(args[args.index("--at") + 1], 16)
        print("address %05X" % addr)
        print("  final value: %s" % (
            "%02X" % t.value_at(addr) if t.value_at(addr) is not None
            else "never written"))
        print("  writers: %s" % (", ".join(t.writers(addr)) or "none"))
        for i in t.by_addr.get(addr, [])[:40]:
            w = t.writes[i]
            print("    instr %-10d %04X:%04X -> %02X"
                  % (w["instr"], w["cs"], w["ip"], w["data"]))
        return 0

    if "--snap" in args:
        k = args.index("--snap")
        base = int(args[k + 1], 16)
        length = int(args[k + 2], 16) if len(args) > k + 2 else 0x100
        data = t.snapshot(base, length)
        for off in range(0, length, 16):
            row = data[off:off + 16]
            hexs = " ".join("%02X" % b for b in row)
            txt = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
            print("%05X  %-47s  %s" % (base + off, hexs, txt))
        return 0

    if "--code" in args:
        for n in sorted(t.nodes, key=lambda n: (n["addr"], n["gen"])):
            print("%05X  %04X:%04X  gen=%d  hits=%d  first=%d" % (
                n["addr"], n["cs"], n["ip"], n["gen"], n["hits"], n["first"]))
        return 0

    # Default: summary
    print("bench trace v%d" % t.version)
    print("  CFG nodes    : %d" % len(t.nodes))
    print("  CFG edges    : %d" % len(t.edges))
    print("  memory writes: %d" % len(t.writes))
    print("  port reads   : %d" % len(t.ports))

    regen = [n for n in t.nodes if n["gen"] > 0]
    print("  overwritten code sites: %d node(s) with gen>0" % len(regen))

    if t.nodes:
        lo = min(n["addr"] for n in t.nodes)
        hi = max(n["addr"] for n in t.nodes)
        print("  code span    : %05X - %05X" % (lo, hi))

    if t.writes:
        touched = len(t.by_addr)
        print("  bytes touched: %d" % touched)

    hot = sorted(t.nodes, key=lambda n: -n["hits"])[:10]
    if hot:
        print("\n  hottest instructions:")
        for n in hot:
            print("    %05X (%04X:%04X)  %d hits" % (
                n["addr"], n["cs"], n["ip"], n["hits"]))

    tgts = t.call_targets()[:10]
    if tgts:
        print("\n  most-targeted addresses (subroutine candidates):")
        for addr, cnt in tgts:
            print("    %05X  %d distinct predecessors" % (addr, cnt))

    ports = defaultdict(int)
    for p in t.ports:
        ports[p["port"]] += 1
    if ports:
        print("\n  port reads by port:")
        for port, cnt in sorted(ports.items(), key=lambda kv: -kv[1])[:10]:
            print("    %03X  %d" % (port, cnt))

    return 0


if __name__ == "__main__":
    sys.exit(main())
