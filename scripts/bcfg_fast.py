"""Memory-efficient reader for bench traces, backed by numpy over the file.

The plain reader in bcfg.py builds a Python dict per record, which costs ~10x
the file size in RAM and makes whole-trace queries slow. This one keeps the
records as numpy structured arrays -- roughly file-sized -- and answers the
queries a scrubber needs:

  * value of any byte at any instruction  (binary search, no scan)
  * a whole memory region reconstructed at a point in time
  * which instructions executed in a window

Writes are appended in execution order, so per-address index lists are already
sorted by instruction count. That is what makes point-in-time lookup a bisect
rather than a replay.
"""

import numpy as np

WRIT_DT = np.dtype([
    ("instr", "<u8"), ("addr", "<u4"), ("cs", "<u2"), ("ip", "<u2"),
    ("data", "u1"), ("_r", "u1"), ("_pad", "<u2"),
])
NODE_DT = np.dtype([
    ("addr", "<u4"), ("cs", "<u2"), ("ip", "<u2"), ("gen", "<u4"),
    ("first", "<u8"), ("hits", "<u8"),
])
EDGE_DT = np.dtype([("src", "<u4"), ("dst", "<u4")])
# Memory reads -- same layout as WRIT. Writes say where data was produced;
# reads say where it was consumed, which is the only way to see a lookup table
# that is never written after load.
READ_DT = np.dtype([
    ("instr", "<u8"), ("addr", "<u4"), ("cs", "<u2"), ("ip", "<u2"),
    ("data", "u1"), ("_r", "u1"), ("_pad", "<u2"),
])
PORT_DT = np.dtype([
    ("instr", "<u8"), ("port", "<u2"), ("cs", "<u2"), ("ip", "<u2"),
    ("data", "u1"), ("is_write", "u1"), ("_pad", "<u2"),
])
# Full register state per traced instruction -- enough to resume the run at
# any point, or to re-run a routine standalone. Field order matches
# IC_8088::Reg16. 40 bytes, matching the static_assert in cfg_tracer.cpp.
STAT_DT = np.dtype([
    ("instr", "<u8"), ("addr", "<u4"),
    ("ax", "<u2"), ("cx", "<u2"), ("dx", "<u2"), ("bx", "<u2"),
    ("sp", "<u2"), ("bp", "<u2"), ("si", "<u2"), ("di", "<u2"),
    ("es", "<u2"), ("cs", "<u2"), ("ss", "<u2"), ("ds", "<u2"),
    ("flags", "<u2"), ("_pad", "<u2"),
])
# EGA plane writes, recorded after the write pipeline has run. On a planar
# card the byte on the bus is not the byte that lands: set/reset, the ALU
# against the latches, the bit mask and the plane mask all intervene, and
# write mode 1 ignores the CPU data entirely. So WRIT cannot reconstruct EGA
# video memory and these can -- `off` is the offset within the plane, which
# is what the beam fetches, not a CPU-visible address.
PLNW_DT = np.dtype([
    ("instr", "<u8"), ("off", "<u4"), ("cs", "<u2"), ("ip", "<u2"),
    ("plane", "u1"), ("data", "u1"), ("_pad", "<u2"),
])


class FastTrace:
    def __init__(self, path):
        self.path = path
        buf = np.memmap(path, dtype="u1", mode="r")
        self.raw = buf
        if bytes(buf[:4]) != b"BCFG":
            raise ValueError("not a bcfg file")

        off = 8
        self.nodes = self.edges = self.writes = self.ports = None
        self.exec = None
        self.states = None
        self.reads = None
        self.planes = None

        while off < len(buf):
            tag = bytes(buf[off:off + 4])
            off += 4
            count = int(np.frombuffer(buf[off:off + 8].tobytes(), "<u8")[0])
            off += 8
            if tag == b"NODE":
                self.nodes = np.frombuffer(
                    buf[off:off + count * NODE_DT.itemsize].tobytes(), NODE_DT)
                off += count * NODE_DT.itemsize
            elif tag == b"EDGE":
                self.edges = np.frombuffer(
                    buf[off:off + count * EDGE_DT.itemsize].tobytes(), EDGE_DT)
                off += count * EDGE_DT.itemsize
            elif tag == b"WRIT":
                n = count * WRIT_DT.itemsize
                # View directly over the mapping -- no copy, no per-record
                # Python object. This is the whole point.
                self.writes = np.frombuffer(buf, WRIT_DT, count, off)
                off += n
            elif tag == b"EXEC":
                # Execution timeline: one u32 address per instruction, index
                # == instruction count. This is what makes point-in-time code
                # position exact rather than inferred from first-execution.
                self.exec = np.frombuffer(buf, "<u4", count, off)
                off += count * 4
            elif tag == b"READ":
                self.reads = np.frombuffer(buf, READ_DT, count, off)
                off += count * READ_DT.itemsize
            elif tag == b"STAT":
                self.states = np.frombuffer(buf, STAT_DT, count, off)
                off += count * STAT_DT.itemsize
            elif tag == b"PORT":
                self.ports = np.frombuffer(buf, PORT_DT, count, off)
                off += count * PORT_DT.itemsize
            elif tag == b"PLNW":
                self.planes = np.frombuffer(buf, PLNW_DT, count, off)
                off += count * PLNW_DT.itemsize
            else:
                raise ValueError("unknown tag %r" % (tag,))

            # Every section must land exactly on the next tag. Overshooting the
            # file means a record size here disagrees with the writer, which
            # otherwise shows up as a bogus tag or silently misparsed data.
            if off > len(buf):
                raise ValueError(
                    "section %r for %d records overruns the file by %d bytes. "
                    "Trace and reader are out of sync -- rebuild and re-record."
                    % (tag, count, off - len(buf)))

        self._build_index()

    def _build_index(self):
        """Group write indices by address, keeping execution order.

        A counting sort on address gives, for each address, the slice of
        write-indices touching it -- in ascending instruction order, because
        the log itself is ordered.
        """
        addrs = self.writes["addr"]
        self.order = np.argsort(addrs, kind="stable")
        sorted_addr = addrs[self.order]
        # Start offset of each distinct address within `order`.
        self.uniq_addr, self.addr_start = np.unique(sorted_addr,
                                                    return_index=True)
        self.addr_end = np.append(self.addr_start[1:], len(self.order))
        self.instr = self.writes["instr"]
        self.max_instr = int(self.instr[-1]) if len(self.instr) else 0

    # -- queries ----------------------------------------------------------

    def _slice_for(self, addr):
        i = np.searchsorted(self.uniq_addr, addr)
        if i >= len(self.uniq_addr) or self.uniq_addr[i] != addr:
            return None
        return self.order[self.addr_start[i]:self.addr_end[i]]

    def value_at(self, addr, instr=None):
        """Byte value at `addr` as of `instr` (None => end of trace)."""
        idx = self._slice_for(addr)
        if idx is None or not len(idx):
            return None
        if instr is None:
            return int(self.writes["data"][idx[-1]])
        times = self.instr[idx]
        k = np.searchsorted(times, instr, side="right")
        if k == 0:
            return None
        return int(self.writes["data"][idx[k - 1]])

    def region(self, base, length, instr=None):
        """Reconstruct [base, base+length) at a point in time."""
        out = bytearray(length)
        for i in range(length):
            v = self.value_at(base + i, instr)
            if v is not None:
                out[i] = v
        return bytes(out)

    def region_fast(self, base, length, instr=None):
        """Vectorised region reconstruction.

        Selects every write into the range at or before `instr`, then keeps
        the last per address. Much faster than per-byte lookup for large
        regions such as a framebuffer.
        """
        a = self.writes["addr"]
        sel = (a >= base) & (a < base + length)
        if instr is not None:
            sel &= self.instr <= instr
        idx = np.flatnonzero(sel)
        out = np.zeros(length, dtype="u1")
        if len(idx):
            # Later writes overwrite earlier ones; the log is time-ordered,
            # so a plain scatter in index order leaves the final value.
            out[a[idx] - base] = self.writes["data"][idx]
        return out.tobytes()

    def snapshot(self, instr=None):
        """Whole 1MB address space as of `instr`.

        A single scatter over the writes at or before that point. The log is
        time-ordered, so later writes land on top of earlier ones and the
        result is the memory image -- no per-address search. Far faster than
        repeated point queries when more than a few bytes are wanted.
        """
        if instr is None:
            end = len(self.writes)
        else:
            end = int(np.searchsorted(self.instr, instr, side="right"))
        mem = np.zeros(1 << 20, dtype="u1")
        if end:
            mem[self.writes["addr"][:end]] = self.writes["data"][:end]
        return mem

    def plane_snapshot(self, instr=None):
        """EGA video memory as 4 planes of 64KB, as of `instr`.

        Same scatter as snapshot(), one array per plane. These records are
        post-pipeline, so this is literally what the beam would fetch -- no
        set/reset, ALU, latch or bit-mask emulation is involved.

        Returns a (4, 65536) array, or None if the trace has no PLNW section
        (recorded before EGA plane tracing existed, or a non-EGA run).
        """
        if self.planes is None or not len(self.planes):
            return None
        if instr is None:
            end = len(self.planes)
        else:
            end = int(np.searchsorted(self.planes["instr"], instr, side="right"))
        out = np.zeros((4, 0x10000), dtype="u1")
        if end:
            p = self.planes["plane"][:end].astype(np.intp)
            o = self.planes["off"][:end].astype(np.intp)
            # Flatten to one index so a single scatter keeps write order.
            out.reshape(-1)[p * 0x10000 + o] = self.planes["data"][:end]
        return out

    def code_bytes(self, span=8):
        """Bytes each instruction actually executed, per generation.

        A node's bytes must be read as of ITS first execution, not from the
        end of the trace: code that is later overwritten would otherwise
        decode from bytes the CPU never ran, and because x86 is variable
        length one wrong opcode desynchronises everything after it.

        Sorting nodes by first-execution and advancing a running memory image
        across the (time-ordered) write log does this in a single pass rather
        than one reconstruction per node.

        Returns addr -> bytes, keyed per (addr, gen) via the node list, so an
        address that ran as two different instructions yields both.
        """
        order = np.argsort(self.nodes["first"], kind="stable")
        mem = np.zeros(1 << 20, dtype="u1")
        addrs = self.writes["addr"]
        datas = self.writes["data"]
        times = self.instr
        out = {}
        w = 0
        nw = len(self.writes)
        for i in order:
            n = self.nodes[i]
            fe = int(n["first"])
            while w < nw and times[w] <= fe:
                mem[addrs[w]] = datas[w]
                w += 1
            a = int(n["addr"])
            out[(a, int(n["gen"]))] = mem[a:a + span].tobytes()
        return out

    def written_mask(self, instr=None):
        """Which addresses have ever been written at or before `instr`."""
        if instr is None:
            end = len(self.writes)
        else:
            end = int(np.searchsorted(self.instr, instr, side="right"))
        m = np.zeros(1 << 20, dtype=bool)
        if end:
            m[self.writes["addr"][:end]] = True
        return m

    REGS = ("ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
            "es", "cs", "ss", "ds", "flags")

    def reads_by(self, addr, limit=200000):
        """Every memory read performed by the instruction at `addr`.

        This is how a lookup table is found: the table is written once at load
        time and never again, so it is invisible in the write log, but the
        code that indexes into it reads it constantly.
        """
        if self.reads is None or not len(self.reads):
            return []
        r = self.reads
        m = np.flatnonzero(((r["cs"].astype(np.uint32) << 4)
                            + r["ip"]) == int(addr))[:limit]
        return [dict(instr=int(r["instr"][i]), addr=int(r["addr"][i]),
                     data=int(r["data"][i])) for i in m]

    def read_span(self, lo, hi, instr_lo=None, instr_hi=None):
        """Which instructions read from an address range, and how often.

        Returns {reader_phys_addr: (count, min_addr, max_addr)} -- enough to
        tell a table scan from an incidental access.
        """
        if self.reads is None or not len(self.reads):
            return {}
        r = self.reads
        m = (r["addr"] >= lo) & (r["addr"] < hi)
        if instr_lo is not None:
            m &= r["instr"] >= instr_lo
        if instr_hi is not None:
            m &= r["instr"] <= instr_hi
        idx = np.flatnonzero(m)
        out = {}
        for i in idx:
            k = int((int(r["cs"][i]) << 4) + int(r["ip"][i])) & 0xFFFFF
            a = int(r["addr"][i])
            if k in out:
                n, mn, mx = out[k]
                out[k] = (n + 1, min(mn, a), max(mx, a))
            else:
                out[k] = (1, a, a)
        return out

    def state_at(self, instr):
        """Register state at instruction count `instr`.

        Records are in execution order, so this is a binary search on the
        `instr` column. Returns the state at or just before the requested
        point, or None if nothing was recorded (BIOS and handler instructions
        are not).
        """
        if self.states is None or not len(self.states):
            return None
        k = int(np.searchsorted(self.states["instr"], int(instr),
                                side="right"))
        if k == 0:
            return None
        s = self.states[k - 1]
        out = {"instr": int(s["instr"]), "addr": int(s["addr"])}
        out.update({r: int(s[r]) for r in self.REGS})
        return out

    def states_at_addr(self, addr, limit=64):
        """Every recorded state for an address, in execution order.

        A routine called many times yields one record per call, so this shows
        how its inputs varied -- which is what tells you a register is an
        argument rather than a constant.
        """
        if self.states is None or not len(self.states):
            return []
        m = np.flatnonzero(self.states["addr"] == int(addr))[:limit]
        out = []
        for i in m:
            s = self.states[int(i)]
            d = {"instr": int(s["instr"]), "addr": int(s["addr"])}
            d.update({r: int(s[r]) for r in self.REGS})
            out.append(d)
        return out

    def pc_at(self, instr):
        """Address of the instruction executing at instruction count `instr`.

        Exact, not inferred: the execution timeline stores one address per
        instruction in order, so this is a direct index. Returns None for
        traces recorded before the timeline existed.
        """
        if self.exec is None or not len(self.exec):
            return None
        i = max(0, min(len(self.exec) - 1, int(instr)))
        return int(self.exec[i])

    def pc_window(self, instr, before=8, after=24):
        """Addresses executed around `instr`, in execution order.

        This is the real control-flow path -- the actual sequence taken on
        this pass through a loop, not the CFG's set of possible successors.
        """
        if self.exec is None or not len(self.exec):
            return 0, []
        lo = max(0, int(instr) - before)
        hi = min(len(self.exec), int(instr) + after)
        return lo, [int(x) for x in self.exec[lo:hi]]

    def nodes_in_window(self, lo, hi):
        """Nodes whose first execution falls in [lo, hi]."""
        f = self.nodes["first"]
        m = (f >= lo) & (f <= hi)
        return self.nodes[m]

    def port_writes(self, port, instr=None):
        """All writes to a port at or before `instr`."""
        if self.ports is None:
            return []
        p = self.ports
        m = (p["port"] == port) & (p["is_write"] == 1)
        if instr is not None:
            m &= p["instr"] <= instr
        return p[m]

    def last_port_write(self, port, instr=None):
        w = self.port_writes(port, instr)
        return int(w["data"][-1]) if len(w) else None
