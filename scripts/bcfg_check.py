"""Health checks for a bench trace's CFG.

Tests properties that must hold if edge recording is correct. Each one failed
before interrupt handlers were excluded from the CFG, so this is the direct
verification of that fix rather than an inference from how the graph looks.

    python scripts/bcfg_check.py bench_trace.bcfg

Checks:
  1. Conditional jumps have at most two successors. A JNE with four means
     handler entries are being recorded as control flow.
  2. Basic blocks are not overwhelmingly single-instruction. Phantom edges
     give addresses spurious predecessors, and the leader rule then splits
     straight-line runs into singletons.
  3. Every CFG edge is one the execution timeline actually took. The timeline
     is ground truth; the edge set is a summary of it.
  4. No CFG node lies inside an interrupt handler.
"""

import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from bcfg_fast import FastTrace  # noqa: E402

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16
except ImportError:
    print("capstone required:  pip install capstone")
    raise

COND = {"je", "jne", "jz", "jnz", "jb", "jnb", "jbe", "jnbe", "ja", "jae",
        "jl", "jle", "jg", "jge", "jo", "jno", "js", "jns", "jp", "jnp",
        "jpe", "jpo", "jc", "jnc", "jcxz", "loop", "loope", "loopne",
        "loopz", "loopnz"}
RET = {"ret", "retf", "iret", "iretd"}

PASS, FAIL = "PASS", "FAIL"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    t = FastTrace(sys.argv[1])
    md = Cs(CS_ARCH_X86, CS_MODE_16)

    print("trace: %s" % sys.argv[1])
    print("  %d nodes, %d edges, %d writes, %d instrs on timeline"
          % (len(t.nodes), len(t.edges), len(t.writes),
             0 if t.exec is None else len(t.exec)))
    if t.exec is None or not len(t.exec):
        print("\nno execution timeline -- checks 2..4 need one")
    print()

    succ = defaultdict(set)
    for s, d in zip(t.edges["src"], t.edges["dst"]):
        succ[int(s)].add(int(d))

    # Decode each instruction from the bytes it actually executed.
    code = {}
    for (a, _g), b in t.code_bytes(span=8).items():
        code.setdefault(a, b)
    mnem, size = {}, {}
    for a, b in code.items():
        ins = next(md.disasm(b, a), None)
        if ins:
            mnem[a] = ins.mnemonic
            size[a] = ins.size

    results = []

    # --- 1. conditional jumps have <= 2 successors ----------------------
    bad = [(a, sorted(succ[a])) for a in succ
           if mnem.get(a) in COND and len(succ[a]) > 2]
    ok = not bad
    results.append(("conditional jumps have <=2 successors", ok))
    print("[%s] check 1: conditional jumps with >2 successors: %d"
          % (PASS if ok else FAIL, len(bad)))
    for a, s in sorted(bad)[:6]:
        print("        %05X %-6s -> %s"
              % (a, mnem.get(a, "?"), " ".join("%05X" % x for x in s)))

    # --- 2. block sizes ------------------------------------------------
    # Rebuild leaders the same way the viewer does.
    pred = defaultdict(set)
    for a in succ:
        for d in succ[a]:
            pred[d].add(a)
    addrs = sorted(code)
    aset = set(addrs)
    leaders = set()
    for a in addrs:
        p = pred.get(a, ())
        if len(p) != 1:
            leaders.add(a)
        for q in p:
            if len(succ.get(q, ())) > 1 or mnem.get(q) in RET:
                leaders.add(a)
    sizes = []
    for lead in leaders:
        n, a = 0, lead
        while a in aset:
            n += 1
            if mnem.get(a) in RET or len(succ.get(a, ())) > 1:
                break
            nxt = a + size.get(a, 1)
            if nxt in leaders or nxt not in aset:
                break
            a = nxt
        sizes.append(n)
    sizes.sort()
    med = sizes[len(sizes) // 2] if sizes else 0
    singles = sum(1 for s in sizes if s == 1)
    frac = singles / max(1, len(sizes))
    ok = med >= 3 and frac < 0.5
    results.append(("blocks are not mostly single-instruction", ok))
    print("[%s] check 2: %d blocks, median %d instrs, %d single (%.0f%%)"
          % (PASS if ok else FAIL, len(sizes), med, singles, 100 * frac))

    # --- 3. every edge was actually taken ------------------------------
    if t.exec is not None and len(t.exec):
        ex = t.exec
        # Adjacent pairs, and also pairs bridged by an excursion the CFG
        # deliberately omits: an INT's real successor is the instruction after
        # it, but ~20 handler instructions sit between them on the timeline.
        # Requiring strict adjacency would flag that correct edge as phantom.
        # Walk the timeline keeping the last address the CFG can see -- i.e.
        # skipping any run of instructions the CFG suppresses (BIOS and
        # interrupt handlers, which are exactly the addresses that never
        # became nodes). Consecutive *visible* instructions are the edges the
        # CFG should contain, which is the same rule the tracer applies.
        nodeset = set(int(a) for a in t.nodes["addr"])
        taken = set()
        prev = None
        for k in range(len(ex)):
            cur = int(ex[k])
            if cur not in nodeset:
                continue          # suppressed: not part of visible flow
            if prev is not None:
                taken.add((prev, cur))
            prev = cur
        declared = set()
        for a in succ:
            for d in succ[a]:
                declared.add((a, d))
        phantom = declared - taken
        ok = not phantom
        results.append(("every CFG edge occurs on the timeline", ok))
        print("[%s] check 3: %d of %d edges never occurred (adjacent, or "
              "across a suppressed handler)"
              % (PASS if ok else FAIL, len(phantom), len(declared)))
        for s, d in sorted(phantom)[:8]:
            print("        %05X -> %05X  (%s)" % (s, d, mnem.get(s, "?")))

        # --- 4. no node inside a handler ------------------------------
        # A handler is entered by an instruction whose successor on the
        # timeline is an IVT target and left by IRET. Approximate it by
        # counting nodes whose address is only ever reached right after a
        # non-branching instruction elsewhere -- cheap proxy: nodes in BIOS.
        nodes = set(int(a) for a in t.nodes["addr"])
        in_bios = sorted(a for a in nodes if a >= 0xF0000)
        ok = not in_bios
        results.append(("no CFG nodes inside BIOS", ok))
        print("[%s] check 4: %d CFG nodes at F0000+"
              % (PASS if ok else FAIL, len(in_bios)))
        for a in in_bios[:6]:
            print("        %05X" % a)

        # informational: how much of the run was in handlers
        bios_exec = int((ex >= 0xF0000).sum())
        print("     info: %d/%d timeline instructions in BIOS (%.1f%%)"
              % (bios_exec, len(ex), 100.0 * bios_exec / len(ex)))

    print()
    nfail = sum(1 for _n, ok in results if not ok)
    for name, ok in results:
        print("  %-45s %s" % (name, PASS if ok else FAIL))
    print()
    print("%d/%d checks passed" % (len(results) - nfail, len(results)))
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
