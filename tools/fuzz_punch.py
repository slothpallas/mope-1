#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A random session heavy on punches: SEL held or let go, black keys and the whites beside them, the knobs spun,
parameters thrown about over MIDI (CC 102/103), chords, punches by CC 105, SEQ steps, saves, restarts. It found
HALF/DOUBLE heads reading past the write head when MORE or knob 1 changed mid-splice.

  fuzz_punch.py SEED COMMANDS > session.mope"""
import random, sys
seed,n=int(sys.argv[1]),int(sys.argv[2]); r=random.Random(seed*7919+3)
o=[f"# fuzz2 seed {seed}","wait 200"]; A=o.append
def par(p,v): A(f"midi b0 66 {p:02x}"); A(f"midi b0 67 {v:02x}")
par(42,127); A("tap PLAY")
BL=[f"b{i}" for i in range(11)]
fx=False; held=set()
for _ in range(n):
    x=r.random()
    if x<0.12:
        if not fx: A("press SEL"); fx=True
        k=r.choice(BL)
        if k in held: A(f"key {k} up"); held.discard(k)
        else: A(f"key {k} down"); held.add(k)
    elif x<0.18:
        w=f"w{r.randint(0,15)}"
        A(f"key {w} down" if w not in held else f"key {w} up"); held ^= {w}
    elif x<0.22:
        if fx: A("release SEL"); fx=False
        else: A("tap SEL")
    elif x<0.40:
        A(f"spin {r.choice(['K1','K2','K3','K4','SELECT','ALGO','PRESET'])} {r.choice([1,-1,4,-4,30,-30])}")
    elif x<0.52:
        par(r.randint(0,68), r.choice([0,127,r.randint(0,127)]))
    elif x<0.64:
        ch=r.choice([[48,52,55],[50,53,57,60],[36,43,48,55,60,64]])
        for nn in ch: A(f"midi 90 {nn:02x} {r.randint(30,127):02x}")
        A(f"wait {r.choice([5,40,200])}")
        for nn in ch: A(f"midi 80 {nn:02x} 00")
    elif x<0.70:
        A(f"midi b0 69 {r.choice([64,0])+r.randint(0,11):02x}")
    elif x<0.74:
        A(f"tap {r.choice(['SEQ','HOME','ARP','REC','PLAY','OCT+','OCT-','ENV','LFO','EDIT','GLO','FX'])}")
    elif x<0.78:
        A(f"tapkey w{r.randint(0,15)}")
    elif x<0.79:
        A("tap SAVE"); A("wait 30"); A("tap SAVE")
    elif x<0.795:
        A("reboot"); held.clear(); fx=False
    else:
        A(f"wait {r.choice([1,2,5,16,50,150])}")
A("wait 500")
print("\n".join(o))
