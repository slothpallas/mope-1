#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""How hard MOPE-1's audio works on a connected FM-1: plays a preset over USB-MIDI and asks the
firmware for its load (SysEx cmd 0x12, main_fm1.c ota_reply_stats). Needs mido with python-rtmidi.

  fm1_load.py [--preset N ...] [--notes 6] [--secs 3] [--set PARAM=VALUE ...]

PARAM is a synth.h P_* index, VALUE 0..127 over its range (CC 102/103). Prints, per preset, the
average and the worst share of the audio budget a half buffer (5.8 ms) took, and any late halves."""
import argparse
import sys
import time

import mido

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from fm1_install import pack7, unpack7   # noqa: E402


def query():
    body = bytes([0x00, 0x59, 0x12, 0, 0, 0])
    return [0xF0, *pack7(body + bytes([0xFF])), 0xF7]


def ask(i, o):
    while i.poll():
        pass
    o.send(mido.Message.from_bytes(query()))
    end = time.time() + 1.0
    while time.time() < end:
        m = i.poll()
        if m is None:
            time.sleep(0.002)
            continue
        if m.type == "sysex":
            d = unpack7(m.data)
            if len(d) >= 30 and d[1] == 0x59 and d[2] == 0x12:
                w = [int.from_bytes(d[6 + 4 * k:10 + 4 * k], "little") for k in range(6)]
                return {"avg": w[0] * 100 / 256, "worst": 100 * w[1] / w[2], "late": w[3], "voices": w[4], "steals": w[5]}
    return None


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--preset", type=int, nargs="*", default=[0])
    a.add_argument("--notes", type=int, default=6)
    a.add_argument("--secs", type=float, default=3)
    a.add_argument("--set", nargs="*", default=[])
    a.add_argument("--port")
    o_ = a.parse_args()
    name = o_.port or next((n for n in mido.get_output_names() if "FM-1" in n or "Felucca" in n), None)
    if not name:
        sys.exit("fm1_load: no FM-1 MIDI port")
    iname = next((n for n in mido.get_input_names() if "FM-1" in n or "Felucca" in n), name)
    chord = [48, 55, 60, 63, 67, 70, 72, 75][:o_.notes]
    with mido.open_input(iname) as i, mido.open_output(name) as o:
        if ask(i, o) is None:
            sys.exit("fm1_load: no answer (firmware without the load query?)")
        for p in o_.preset:
            o.send(mido.Message("program_change", program=p))
            time.sleep(0.2)
            for kv in o_.set:
                k, v = kv.split("=")
                o.send(mido.Message("control_change", control=102, value=int(k)))
                o.send(mido.Message("control_change", control=103, value=int(v)))
            time.sleep(0.2)
            ask(i, o)
            before = ask(i, o)
            for n in chord:
                o.send(mido.Message("note_on", note=n, velocity=100))
            worst, avgs, end = 0.0, [], time.time() + o_.secs
            while time.time() < end:
                time.sleep(0.25)
                s = ask(i, o)
                if s:
                    worst = max(worst, s["worst"])
                    avgs.append(s["avg"])
            s = ask(i, o) or before
            for n in chord:
                o.send(mido.Message("note_off", note=n))
            time.sleep(1.5)
            print(f"preset {p:2d}: average {max(avgs or [0]):5.1f}%  worst {worst:5.1f}%  late {s['late'] - before['late']}  "
                  f"voices {s['voices']}")


if __name__ == "__main__":
    main()
