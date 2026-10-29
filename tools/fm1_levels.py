#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""How loud each MOPE-1 preset is on a connected FM-1: plays a chord on each over USB-MIDI, records
the FM-1's USB audio and prints its loudness (K-weighted like BS.1770: the deep bass counts less, the
top a little more; the loudest 400 ms). Rain, drums and bass off while it measures.
Needs mido with python-rtmidi, sounddevice and numpy.

  fm1_levels.py [--presets 0-37] [--octaves FILE]   (FILE: one octave per preset, -1 0 1 ...)"""
import argparse
import re
import sys
import time

import mido
import numpy as np
import sounddevice as sd

FS = 44100


def biquad(x, b, a):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i, v in enumerate(x):            # (short takes: plain Python is fine)
        o = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def rbj(kind, f0, q=0.707, gain_db=0.0):
    w = 2 * np.pi * f0 / FS
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    if kind == "hp":
        b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
        a = [1 + al, -2 * c, 1 - al]
    else:                                  # high shelf
        A = 10 ** (gain_db / 40)
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s)]
        a = [(A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s]
    return [v / a[0] for v in b], [1.0] + [v / a[0] for v in a[1:]]


def loudness(lr):
    hp = rbj("hp", 60.0, 0.5)
    sh = rbj("shelf", 1500.0, 0.707, 4.0)
    e = 0.0
    m = lr.mean(axis=1)                   # (mono: fast enough, and the presets differ by more than pan)
    k = biquad(biquad(m.astype(np.float64)[::2], *hp), *sh)   # (decimated by 2: the meter, not the sound)
    win = int(0.4 * FS / 2)
    best = 1e-12
    for s in range(0, len(k) - win, win // 4):
        best = max(best, float(np.mean(k[s:s + win] ** 2)))
    return 10 * np.log10(best)


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--presets", default="0-37")
    a.add_argument("--octaves", help="a file: the presets' octaves, one per line")
    o_ = a.parse_args()
    lo, hi = (int(v) for v in o_.presets.split("-"))
    octs = [int(v) for v in open(o_.octaves).read().split()] if o_.octaves else [0] * 64
    out = next((n for n in mido.get_output_names() if "FM-1" in n or "Felucca" in n), None)
    dev = next((i for i, d in enumerate(sd.query_devices()) if "FM-1" in d["name"] and d["max_input_channels"] >= 2), None)
    if out is None or dev is None:
        sys.exit("fm1_levels: no FM-1 (MIDI and USB audio)")
    with mido.open_output(out) as o:
        def setp(p, v):
            o.send(mido.Message("control_change", control=102, value=p))
            o.send(mido.Message("control_change", control=103, value=v))
        o.send(mido.Message("control_change", control=104, value=0))
        for p in range(lo, hi + 1):
            o.send(mido.Message("program_change", program=p))
            time.sleep(0.3)
            setp(35, 0)                    # P_GUNK: no rain
            setp(27, 0)                    # P_BASS: no auto-bass
            time.sleep(0.7)
            base = 60 + 12 * octs[p]
            chord = [base, base + 3, base + 7]
            rec = sd.rec(int(3.0 * FS), samplerate=FS, channels=2, device=dev, dtype="float32")
            time.sleep(0.05)
            for n in chord:
                o.send(mido.Message("note_on", note=n, velocity=100))
            time.sleep(2.4)
            for n in chord:
                o.send(mido.Message("note_off", note=n))
            sd.wait()
            print(f"{p:2d} {loudness(rec):7.2f}", flush=True)
            time.sleep(1.5)


if __name__ == "__main__":
    main()
