import subprocess, os, sys
OUT = "build/review/wav"
def script(p, band):
    L = ["master 4096", "wait 600", f"midi C0 {p:02X} 00", "wait 400"]
    if band:
        L += ["tap PLAY", "wait 300", f"wav b{p:02d}.wav"]
        t = [(60,180),(63,180),(67,180),(72,180),(70,180),(67,180),(63,180),(60,180)] * 3
        for n, ms in t:
            L += [f"midi 90 {n:02X} 64", f"wait {ms-40}", f"midi 80 {n:02X} 00", "wait 40"]
        L += ["wait 600", "wavstop"]
    else:
        L += [f"wav a{p:02d}.wav"]
        for n in (48, 55, 60, 63):                      # chord: 3 s held, 2.5 s tail (0 .. 5.5 s)
            L += [f"midi 90 {n:02X} 64"]
        L += ["wait 3000"]
        for n in (48, 55, 60, 63):
            L += [f"midi 80 {n:02X} 00"]
        L += ["wait 2500"]
        for n in (60, 63, 67, 72, 70, 67, 63, 60, 72, 75, 79, 84):   # riff: 12 x 200 ms, 120 on (5.5 .. 7.9 s)
            L += [f"midi 90 {n:02X} 64", "wait 120", f"midi 80 {n:02X} 00", "wait 80"]
        L += ["wait 600"]                               # (.. 8.5 s)
        for n in (36, 36, 43, 39):                      # bass: 4 x 500 ms, 400 on (8.5 .. 10.5 s)
            L += [f"midi 90 {n:02X} 64", "wait 400", f"midi 80 {n:02X} 00", "wait 100"]
        L += ["wait 800", "wavstop"]
    return "\n".join(L) + "\n"
n = int(sys.argv[1])
for p in range(-1, n):
    for band in (0, 1):
        if p < 0:
            if not band: continue
            s = "master 4096\nwait 600\ntap PLAY\nwait 300\nwav band.wav\nwait 4920\nwavstop\n"
        else:
            s = script(p, band)
        f = f"{OUT}/s.mope"; open(f, "w").write(s)
        r = subprocess.run(["build/host/mope_host", f, OUT], capture_output=True, text=True)
        if r.returncode: print("fail", p, band, r.stdout[-200:], r.stderr[-200:]); sys.exit(1)
print("ok")
