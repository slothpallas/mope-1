#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A random MOPE-1 session for the host simulator (host/mope_host.c): every button, held and tapped and
in combinations, every encoder turned far both ways, keys, any USB MIDI (notes, any CC including the
parameter pair 102/103 over every parameter, programs, bends, the deck), the master pot, reboots.
Run it under the sanitizers (tests/fuzz.sh) to look for crashes.

  fuzz_scenario.py SEED N > scenario.mope"""
import random
import sys

BTN = "FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+".split()
ENC = "SELECT ALGO PRESET K1 K2 K3 K4".split()


def main():
    seed, n = int(sys.argv[1]), int(sys.argv[2])
    r = random.Random(seed)
    held_b, held_k = set(), set()
    out = [f"# fuzz seed {seed}"]
    for _ in range(n):
        x = r.random()
        if x < 0.16:
            b = r.choice(BTN)
            if b in held_b:
                out.append(f"release {b}"); held_b.discard(b)
            elif r.random() < 0.3:
                out.append(f"press {b}"); held_b.add(b)
            else:
                out.append(f"tap {b}")
        elif x < 0.40:
            e = r.choice(ENC)
            v = r.choice([1, -1, 3, -3, 10, -10, 40, -40, 200, -200])
            out.append(f"spin {e} {v}" if r.random() < 0.7 else f"turn {e} {max(-20, min(20, v))} {r.randint(5, 60)}")
        elif x < 0.55:
            k = r.randint(0, 26)
            if k in held_k:
                out.append(f"key {k} up"); held_k.discard(k)
            elif r.random() < 0.4:
                out.append(f"key {k} down"); held_k.add(k)
            else:
                out.append(f"tapkey {k}")
        elif x < 0.80:
            t = r.random()
            ch = r.randint(0, 15)
            if t < 0.3:
                out.append(f"midi {0x90 | ch:02x} {r.randint(0, 127):02x} {r.randint(0, 127):02x}")
            elif t < 0.45:
                out.append(f"midi {0x80 | ch:02x} {r.randint(0, 127):02x} 00")
            elif t < 0.7:                                  # the parameter pair: any parameter, any value
                out.append(f"midi b0 66 {r.randint(0, 127):02x}")
                out.append(f"midi b0 67 {r.choice([0, 127, r.randint(0, 127)]):02x}")
            elif t < 0.8:
                out.append(f"midi {0xB0 | ch:02x} {r.randint(0, 127):02x} {r.randint(0, 127):02x}")
            elif t < 0.88:
                out.append(f"midi {0xC0 | ch:02x} {r.randint(0, 127):02x} 00")
            elif t < 0.95:
                out.append(f"midi {0xE0 | ch:02x} {r.randint(0, 127):02x} {r.randint(0, 127):02x}")
            else:
                out.append(f"midi b0 68 {r.choice([0, 127]):02x}")
        elif x < 0.83:
            out.append(f"master {r.randint(0, 4096)}")
        elif x < 0.835:
            out.append("reboot")
            held_b.clear(); held_k.clear()
        else:
            out.append(f"wait {r.choice([1, 5, 20, 50, 120, 400])}")
    out.append("wait 500")
    print("\n".join(out))


if __name__ == "__main__":
    main()
