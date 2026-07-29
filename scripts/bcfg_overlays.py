"""Recover code load events and test code identity in a bench trace.

This is a PC booter: there is no DOS, and essentially the whole program is
written into memory by code that is itself already running (boot sector ->
loader -> game). So the useful frame is not "which addresses got overwritten"
but "which loader wrote which bytes, and when".

For every executed instruction byte, the trace names the write that put it
there and the instruction pointer of the code that performed that write. A
load event is a run of such writes from one loader. Those events are the
natural module boundaries.

The question the IR needs answered: does one physical address ever hold two
DIFFERENT pieces of executed code at different times? If not, address-keyed
identity is safe. If so, identity must be (load event, offset).

Usage:
    python scripts/bcfg_overlays.py trace.bcfg
    python scripts/bcfg_overlays.py trace.bcfg --events     # per-load detail
    python scripts/bcfg_overlays.py trace.bcfg --conflicts  # aliased addresses
"""

import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bcfg import Trace  # noqa: E402

# Writes separated by more than this many instructions start a new load event.
EVENT_GAP = 20000


def code_provenance(t):
    """For each executed address: the write that produced the byte it ran.

    Returns addr -> list of (exec_instr, write_instr, writer_cs, writer_ip,
    value). One entry per distinct generation of code at that address.
    """
    # Earliest execution per address, plus every execution instant, so a
    # second execution after an overwrite is visible as a new generation.
    execs = defaultdict(list)
    for n in t.nodes:
        execs[n["addr"]].append(n["first"])

    prov = {}
    for addr, times in execs.items():
        idxs = t.by_addr.get(addr, [])
        gens = []
        for te in sorted(times):
            # Last write at or before this execution produced the bytes it ran.
            src = None
            for i in idxs:
                w = t.writes[i]
                if w["instr"] > te:
                    break
                src = w
            if src is not None:
                gens.append((te, src["instr"], src["cs"], src["ip"],
                             src["data"]))
        if gens:
            prov[addr] = gens
    return prov


def load_events(prov):
    """Group code-producing writes into load events.

    A load event is a set of writes close together in time from the same
    writing routine. Grouping on time alone is enough here -- loaders run in
    tight bursts -- but the writer's CS is carried so events can be attributed.
    """
    entries = []
    for addr, gens in prov.items():
        for (_te, wi, wcs, wip, _v) in gens:
            entries.append((wi, addr, wcs, wip))
    entries.sort()

    events = []
    cur = None
    for wi, addr, wcs, wip in entries:
        if cur is None or wi - cur["last_instr"] > EVENT_GAP:
            if cur:
                events.append(cur)
            cur = dict(first_instr=wi, last_instr=wi, addrs=[addr],
                       writers=set([(wcs, wip)]))
        else:
            cur["last_instr"] = wi
            cur["addrs"].append(addr)
            cur["writers"].add((wcs, wip))
    if cur:
        events.append(cur)

    for e in events:
        e["lo"] = min(e["addrs"])
        e["hi"] = max(e["addrs"])
        e["count"] = len(e["addrs"])
    return events


def conflicts(t, prov):
    """Addresses that executed more than once with DIFFERENT bytes.

    This is the only condition that breaks address-keyed identity: the same
    location running as code, being replaced, and running again as different
    code.
    """
    out = []
    for addr, gens in prov.items():
        if len(gens) < 2:
            continue
        vals = set(g[4] for g in gens)
        if len(vals) > 1:
            out.append((addr, gens))
    return out


def rewritten_after_exec(t, prov):
    """Executed bytes overwritten with different values after running.

    Distinct from `conflicts`: the address need not execute again. This
    measures how much code is discarded, not how much aliases.
    """
    out = []
    for addr, gens in prov.items():
        last_exec = max(g[0] for g in gens)
        ran_as = None
        for g in gens:
            if g[0] == last_exec:
                ran_as = g[4]
        for i in t.by_addr.get(addr, []):
            w = t.writes[i]
            if w["instr"] > last_exec and w["data"] != ran_as:
                out.append((addr, last_exec, w["instr"], ran_as, w["data"]))
                break
    return out


def runs(addrs, gap=64):
    """Collapse a sorted address list into contiguous regions."""
    if not addrs:
        return []
    addrs = sorted(addrs)
    out = []
    start = prev = addrs[0]
    for a in addrs[1:]:
        if a - prev > gap:
            out.append((start, prev))
            start = a
        prev = a
    out.append((start, prev))
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    t = Trace(sys.argv[1])
    args = sys.argv[2:]

    prov = code_provenance(t)
    total = len(prov)
    missing = len(set(n["addr"] for n in t.nodes)) - total

    print("executed instruction addresses : %d" % (total + missing))
    print("  with a recorded producing write: %d" % total)
    print("  never written (ROM / pre-trace): %d" % missing)

    # --- Load events -------------------------------------------------------
    events = load_events(prov)
    print()
    print("--- code load events (gap > %d instrs starts a new event) ---"
          % EVENT_GAP)
    print("  events: %d" % len(events))
    for k, e in enumerate(events):
        writers = sorted(e["writers"])
        wtxt = ", ".join("%04X:%04X" % w for w in writers[:3])
        if len(writers) > 3:
            wtxt += " +%d more" % (len(writers) - 3)
        print("  [%2d] instr %-9d..%-9d  %5d bytes  %05X-%05X"
              % (k, e["first_instr"], e["last_instr"], e["count"],
                 e["lo"], e["hi"]))
        print("       written by: %s" % wtxt)

    if "--events" in args:
        print()
        for k, e in enumerate(events):
            print("  event %d regions:" % k)
            for lo, hi in runs(e["addrs"]):
                print("    %05X-%05X  (%d bytes)" % (lo, hi, hi - lo + 1))

    # --- Identity conflicts ------------------------------------------------
    conf = conflicts(t, prov)
    print()
    print("--- address identity ---")
    print("  addresses executing >1 distinct code generation: %d" % len(conf))
    if conf:
        for lo, hi in runs([c[0] for c in conf])[:20]:
            print("    %05X-%05X" % (lo, hi))
        if "--conflicts" in args:
            print()
            for addr, gens in conf[:30]:
                print("    %05X:" % addr)
                for (te, wi, wcs, wip, v) in gens:
                    print("      ran@%-10d bytes from write@%-10d by %04X:%04X = %02X"
                          % (te, wi, wcs, wip, v))

    # --- Discarded code ----------------------------------------------------
    rew = rewritten_after_exec(t, prov)
    print()
    print("--- code overwritten after its last execution ---")
    print("  addresses: %d" % len(rew))
    for lo, hi in runs([r[0] for r in rew])[:20]:
        print("    %05X-%05X  (%d bytes)" % (lo, hi, hi - lo + 1))

    # --- Segments ----------------------------------------------------------
    base_for_cs = defaultdict(set)
    cs_for_addr = defaultdict(set)
    for n in t.nodes:
        base_for_cs[n["cs"]].add((n["addr"] - n["ip"]) & 0xFFFFF)
        cs_for_addr[n["addr"]].add(n["cs"])
    multi_base = {c: s for c, s in base_for_cs.items() if len(s) > 1}
    multi_cs = {a: s for a, s in cs_for_addr.items() if len(s) > 1}

    print()
    print("--- segments ---")
    print("  distinct CS values executing code: %d" % len(base_for_cs))
    for cs in sorted(base_for_cs):
        bases = sorted(base_for_cs[cs])
        print("    CS=%04X -> base %s" % (
            cs, ", ".join("%05X" % b for b in bases)))
    print("  CS values mapping to >1 base : %d" % len(multi_base))
    print("  addresses reached under >1 CS: %d" % len(multi_cs))

    # --- Verdict -----------------------------------------------------------
    print()
    print("=== assessment ===")
    if conf:
        print("  %d addresses ran as two or more DIFFERENT pieces of code."
              % len(conf))
        print("  -> address-keyed identity ALIASES unrelated routines.")
        print("  -> IR must key on (load event, offset).")
    else:
        print("  No address ever ran as two different pieces of code.")
        print("  -> address-keyed identity is SAFE for this run.")
        if rew:
            print("  (%d code bytes were overwritten after their last"
                  % len(rew))
            print("   execution -- discarded, not aliased.)")
    if multi_base or multi_cs:
        print("  Relocation detected -- the same code runs at >1 address.")
    else:
        print("  No relocation: each code segment sits at one fixed base.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
