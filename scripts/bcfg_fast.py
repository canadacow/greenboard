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
PORT_DT = np.dtype([
    ("instr", "<u8"), ("port", "<u2"), ("cs", "<u2"), ("ip", "<u2"),
    ("data", "u1"), ("is_write", "u1"), ("_pad", "<u2"),
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
            elif tag == b"PORT":
                # PORT is the last section, so its record size must divide the
                # bytes remaining. A mismatch means the file was written by a
                # different build -- say so instead of misparsing it.
                rem = len(buf) - off
                if count and rem != count * PORT_DT.itemsize:
                    raise ValueError(
                        "PORT section is %d bytes for %d records (%.2f each); "
                        "this reader expects %d. Trace and binary are out of "
                        "sync -- rebuild and re-record."
                        % (rem, count, rem / count, PORT_DT.itemsize))
                self.ports = np.frombuffer(buf, PORT_DT, count, off)
                off += count * PORT_DT.itemsize
            else:
                raise ValueError("unknown tag %r" % (tag,))

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
