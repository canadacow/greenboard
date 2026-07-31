"""Render the Pirates! tune as two independent voices.

The hardware ORs both voices onto one speaker bit, so what the PC actually
produced is the interference of two square waves -- the characteristic buzz.
This renders the same note data as two separate tones summed in a mixer, which
is what the music would sound like if the machine could do it.

Note data is read from the address the player actually fetched from, recovered
from the register trace: DS=1038, BX walking 0583 in 6-byte steps, so physical
0x10903, 34 records. Each record is three words; the periods are the LOW bytes
of words 1 and 2 (mov dh,dl / mov bh,cl in the player), and word 0 is the
duration. A zero duration terminates.

Tick rate is derived, not guessed: total ticks = sum(duration) * 100, where 100
is voice C's period constant from 1127B, over the measured 10 second runtime.

    python pirates/harmony.py bench_trace.bcfg
    python pirates/harmony.py bench_trace.bcfg --square   # square, not sine
"""

import argparse
import math
import os
import sys
import wave

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "scripts"))
import numpy as np                      # noqa: E402
from bcfg_fast import FastTrace         # noqa: E402

TABLE = 0x10903        # physical address of the tune, from the register trace
RECORDS = 34
RUNTIME_S = 10.0       # measured

# Tick rate, measured rather than derived.
#
# Deriving it as sum(duration) * voice-C's reload constant / runtime gave
# 25130 Hz, which is wrong by about 3.3x. The measurement uses the seven
# records where both periods are equal: both voices then toggle together, so
# the speaker transition count maps to a single period with no interference to
# misread. Those seven notes independently give 78288..85148 Hz.
#
# The spread across them is ~8%, so this is good to roughly a semitone. If the
# rendered pitch still sounds off, the fix is to measure more precisely, not to
# scale by an assumed interval.
TICK_HZ = 82000.0

RATE = 44100

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def read_records(trace):
    t = FastTrace(trace)
    snap = t.snapshot(None)

    def word(a):
        return int(snap[a]) | (int(snap[a + 1]) << 8)

    out = []
    for k in range(RECORDS):
        a = TABLE + k * 6
        dur = word(a)
        if dur == 0:
            break
        out.append((dur, int(snap[a + 2]), int(snap[a + 4])))
    return out


def note_name(freq):
    if freq <= 0:
        return "-"
    n = int(round(69 + 12 * math.log(freq / 440.0, 2)))
    return "%s%d" % (NAMES[n % 12], n // 12 - 1)


def voice(freq, n, square, phase):
    """One voice's samples, continuing from `phase` so held notes do not click."""
    if freq <= 0:
        return np.zeros(n, dtype=np.float64), phase
    t = (np.arange(n, dtype=np.float64) / RATE) * freq + phase
    if square:
        w = np.where((t % 1.0) < 0.5, 1.0, -1.0)
    else:
        # A few odd harmonics: rounder than a square, still reedy enough to
        # sound like the original rather than a synth pad.
        w = (np.sin(2 * np.pi * t)
             + 0.30 * np.sin(6 * np.pi * t)
             + 0.12 * np.sin(10 * np.pi * t))
        w /= 1.42
    return w, (phase + n * freq / RATE) % 1.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--out", default=None)
    ap.add_argument("--square", action="store_true",
                    help="plain square waves instead of a rounder timbre")
    ap.add_argument("--tick", type=float, default=TICK_HZ,
                    help="player tick rate in Hz (measured: ~82000)")
    a = ap.parse_args()

    recs = read_records(a.trace)
    units = sum(r[0] for r in recs)
    tick_hz = a.tick
    unit_s = RUNTIME_S / units

    print("%d notes, %d duration units, tick rate %.0f Hz"
          % (len(recs), units, tick_hz))
    print()
    print("  secs   melody      bass")

    mel = []
    bas = []
    phA = phB = 0.0
    for dur, pA, pB in recs:
        n = max(1, int(round(dur * unit_s * RATE)))
        # A voice toggles every `period` ticks, so its square wave frequency is
        # tick_hz / (2 * period).
        fA = tick_hz / (2.0 * pA) if pA else 0.0
        fB = tick_hz / (2.0 * pB) if pB else 0.0
        print("  %5.2f   %-5s %6.1f  %-5s %6.1f"
              % (dur * unit_s, note_name(fA), fA, note_name(fB), fB))

        wA, phA = voice(fA, n, a.square, phA)
        wB, phB = voice(fB, n, a.square, phB)

        # Short fades so note boundaries do not click.
        env = np.ones(n)
        k = min(n // 8, int(0.004 * RATE))
        if k > 1:
            env[:k] = np.linspace(0, 1, k)
            env[-k:] = np.linspace(1, 0, k)
        mel.append(wA * env)
        bas.append(wB * env)

    melody = np.concatenate(mel)
    bass = np.concatenate(bas)

    # Two voices, summed with the bass slightly back and panned apart so they
    # are separable by ear -- the point of the exercise.
    left = melody * 0.42 + bass * 0.26
    right = melody * 0.26 + bass * 0.42
    stereo = np.empty(len(left) * 2, dtype=np.float64)
    stereo[0::2] = left
    stereo[1::2] = right
    peak = np.max(np.abs(stereo)) or 1.0
    pcm = (stereo / peak * 0.85 * 32767).astype(np.int16)

    path = a.out or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "harmony.wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
    print()
    print("wrote %s -- %.2f s, 2 voices" % (path, len(left) / float(RATE)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
