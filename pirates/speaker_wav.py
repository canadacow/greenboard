"""Render the PC speaker straight from the trace's port-61h writes.

No note table, no tempo model, no interpretation of the data format. The trace
records every write to port 61h with the instruction count at which it
happened, and bit 1 of that port IS the speaker cone position -- so the
recorded writes are the waveform. Replaying them is exact by construction.

Earlier attempts rendered from the note table instead, which required guessing
the record layout, which field was duration, and how ticks mapped to seconds.
All three guesses were wrong. This needs none of them.

    python pirates/speaker_wav.py bench_trace_good.bcfg
    python pirates/speaker_wav.py trace.bcfg --from 8200000 --to 9500000
"""

import argparse
import os
import sys
import wave

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "scripts"))
import numpy as np                      # noqa: E402
from bcfg_fast import FastTrace         # noqa: E402

CPU_HZ = 4772727.0        # 14.31818 MHz / 3, the 5150's clock


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--out", default=None)
    ap.add_argument("--from", dest="lo", type=int, default=None)
    ap.add_argument("--to", dest="hi", type=int, default=None)
    ap.add_argument("--rate", type=int, default=44100)
    ap.add_argument("--ipc", type=float, default=None,
                    help="instructions per second; measured from the trace "
                         "if not given")
    a = ap.parse_args()

    t = FastTrace(a.trace)
    p = t.ports
    if p is None or not len(p):
        print("no port records in trace")
        return 1

    m = (p["port"] == 0x61) & (p["is_write"] == 1)
    w = p[m]
    if not len(w):
        print("no writes to port 61h -- nothing drove the speaker")
        return 1

    instr = w["instr"].astype(np.int64)
    data = w["data"].astype(np.uint8)

    lo = a.lo if a.lo is not None else int(instr[0])
    hi = a.hi if a.hi is not None else int(instr[-1])
    sel = (instr >= lo) & (instr <= hi)
    instr, data = instr[sel], data[sel]
    if not len(instr):
        print("no speaker writes in that instruction range")
        return 1

    # Instructions -> seconds. The BIOS tick counter at 0040:006C increments
    # 18.2065 times a second, so the instruction spacing between its writes
    # gives a real clock without assuming cycles per instruction.
    ips = a.ipc
    if ips is None:
        idx = t._slice_for(0x46C)
        if idx is not None and len(idx) > 8:
            ti = t.instr[idx].astype(np.int64)
            step = float(np.median(np.diff(ti)))
            if step > 0:
                ips = step * 18.2065
    if not ips:
        print("could not measure instructions/sec; pass --ipc")
        return 1

    span_s = (int(instr[-1]) - int(instr[0])) / ips
    print("%d speaker writes over %d instructions" % (len(instr),
                                                      int(instr[-1] - instr[0])))
    print("measured %.0f instructions/sec -> %.2f seconds" % (ips, span_s))

    # Bit 1 of port 61h is the speaker data line. Bit 0 gates the PIT tone;
    # this player bit-bangs bit 1 directly, so that is the cone position.
    level = ((data >> 1) & 1).astype(np.int16)

    n = int(span_s * a.rate) + 1
    if n < 2:
        print("range too short to render")
        return 1

    # Each write holds its level until the next one: sample the step function.
    pos = ((instr - instr[0]) / ips * a.rate).astype(np.int64)
    pos = np.clip(pos, 0, n - 1)
    out = np.zeros(n, dtype=np.int16)
    out[pos] = level * 2 - 1          # -1 / +1 at each transition
    # Hold the last written value forward.
    held = np.maximum.accumulate(np.where(out != 0, np.arange(n), 0))
    samples = (out[held] * 9000).astype(np.int16)

    path = a.out or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "speaker.wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(a.rate)
        f.writeframes(samples.tobytes())
    print("wrote %s -- %.2f s" % (path, len(samples) / float(a.rate)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
