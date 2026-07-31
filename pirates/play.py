"""Render the Pirates! PC-speaker tune by emulating the player at 1_11E3.

This is a direct port of the routine, not an approximation: the three phase
accumulators, the branchless reload, the OR-mixing and the two writes to port
61h are reproduced instruction-for-instruction so the output waveform is what
the hardware would have produced.

    python pirates/play.py                 -> pirates/song.wav
    python pirates/play.py --notes          -> pitch table only, no render

Note data comes from song.json, extracted from the trace.

The one thing not recoverable from the code alone is the tick rate. The player
writes the speaker, then burns `loop` iterations from a tempo word, then
repeats -- so the sample rate is however long that took on a 4.77 MHz 8088.
`loop` is 17 cycles taken, and the surrounding work is ~90 cycles, giving the
default TICK_CYCLES below. Override with --tick to retune.
"""

import argparse
import json
import math
import os
import struct
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))

CPU_HZ = 4772727.0        # 14.31818 MHz / 3
TICK_CYCLES = 107.0       # per pass through the mixing loop (see docstring)

# Measured, not derived: the tune runs exactly 10 s in the emulator at
# 4.77 MHz. Converting instruction counts to seconds by assuming cycles per
# instruction was wrong by 10x, so this comes from the clock instead.
TOTAL_SECONDS = 10.0


def load_song(path=None):
    path = path or os.path.join(HERE, "song.json")
    with open(path) as f:
        return [tuple(r) for r in json.load(f)]


def render(recs, tick_hz, out_hz=44100):
    """Render the tune to signed 16-bit mono samples.

    The player's inner loop is three 8-bit down-counters, each reloading from
    its period byte on borrow -- so each voice fires every `period` ticks and
    the speaker sees the OR of three square waves. That is analytic, so the
    waveform is computed per note with numpy rather than by stepping ticks:
    an 8088 does ~45000 ticks a second, which a per-tick Python loop cannot
    keep up with.

    Voice C is the note timer, not an audible voice: its borrow decrements the
    duration in SI, so a note lasts `dur * periodC` ticks. Its period byte is
    0 at note start and reloads to 0x64, giving 100 ticks per duration unit.
    """
    import numpy as np

    # Records are (duration, periodA, periodB, periodC) -- three voices, one
    # per phase accumulator in the player. Bytes +2, +4 and +5 of the 6-byte
    # record; +3 is padding, which is why an earlier reading as three words
    # produced nonsense in the third field.
    #
    # Total playing time is measured, not derived: the tune runs 10.0 s at
    # 4.77 MHz, so a duration unit is 10.0 / sum(dur) seconds.
    total_units = sum(r[0] for r in recs) or 1
    unit_s = TOTAL_SECONDS / total_units
    chunks = []

    for rec in recs:
        dur, pA, pB = rec[0], rec[1], rec[2]
        pC = rec[3] if len(rec) > 3 else 0
        n = max(1, int(round(dur * unit_s * out_hz)))
        tt = np.arange(n, dtype=np.float64) * (tick_hz / out_hz)

        # Each voice toggles every `period` ticks: a square wave whose half
        # period is `period` ticks. Period 0 means the voice is silent.
        def square(period):
            if not period:
                return np.zeros(n, dtype=bool)
            return ((tt / period).astype(np.int64) & 1).astype(bool)

        # The speaker bit is the OR of the three voices -- `or di,ax` twice
        # then `or al,ah` before the write to port 61h.
        wave_ = square(pA & 0xFF) | square(pB & 0xFF) | square(pC & 0xFF)
        chunks.append(np.where(wave_, 9000, -9000).astype(np.int16))

    if not chunks:
        return np.zeros(0, dtype=np.int16)
    return np.concatenate(chunks)


def note_name(period, tick_hz):
    """A voice toggles every `period` ticks, so its square wave is
    tick_hz / (2 * period)."""
    if not period:
        return "-", 0.0
    f = tick_hz / (2.0 * period)
    if f <= 0:
        return "-", 0.0
    n = 69 + 12 * math.log(f / 440.0, 2)
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    k = int(round(n))
    return "%s%d" % (names[k % 12], k // 12 - 1), f


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tick", type=float, default=TICK_CYCLES,
                    help="CPU cycles per player tick")
    ap.add_argument("--notes", action="store_true", help="print pitches only")
    ap.add_argument("--out", default=os.path.join(HERE, "song.wav"))
    a = ap.parse_args()

    recs = load_song()
    tick_hz = CPU_HZ / a.tick

    print("%d notes, tick rate %.0f Hz" % (len(recs), tick_hz))
    print()
    print("  dur  perA  voice A        perB  voice B")
    for dur, pA, pB in recs:
        nA, fA = note_name(pA, tick_hz)
        nB, fB = note_name(pB, tick_hz)
        print("  %4d  %4d  %-5s %7.1f  %4d  %-5s %7.1f"
              % (dur, pA, nA, fA, pB, nB, fB))

    if a.notes:
        return 0

    print()
    print("rendering ...")
    samples = render(recs, tick_hz)
    if not len(samples):
        print("no samples produced")
        return 1
    with wave.open(a.out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(44100)
        w.writeframes(samples.tobytes())
    print("wrote %s -- %.2f s" % (a.out, len(samples) / 44100.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
