"""Every preset with each macro at both ends: is any a dead knob? (host renders; build/host/mope_host)"""
import subprocess, json, wave, sys, numpy as np
SR=44100; OUT='build/review/mac'
names=json.load(open('build/review/names.json'))
def rd(f):
    w=wave.open(f); x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32768; return x.reshape(-1,2).mean(1)
def db(v): return 20*np.log10(max(v,1e-9))
def meas(m):
    T=lambda t:int(t*SR); cs=[]
    for k in range(15):
        s=m[T(0.5+0.1*k):T(0.6+0.1*k)]; P=np.abs(np.fft.rfft(s*np.hanning(len(s))))**2; f=np.fft.rfftfreq(len(s),1/SR); cs.append((f*P).sum()/(P.sum()+1e-20))
    s=m[T(0.5):T(2)]; P=np.abs(np.fft.rfft(s*np.hanning(len(s))))**2; f=np.fft.rfftfreq(len(s),1/SR)
    return dict(cent=float(np.mean(cs)),move=float(np.std(cs)/np.mean(cs)),top=10*np.log10(P[f>2000].sum()/(P.sum()+1e-20)+1e-9),
                rms=db(np.sqrt((s**2).mean())),tail=db(np.sqrt((m[T(2.4):T(3.4)]**2).mean())))
rows=[]
for p,nm in enumerate(names):
    r={}
    for mi,mac in enumerate(("EYES","BITE","NERVES","ROOM")):
        for end in (0,127):
            L=["master 4096","wait 500",f"midi C0 {p:02X} 00","wait 300",f"midi B0 66 {70+mi:02X}",f"midi B0 67 {end:02X}","wait 200","wav m.wav"]
            for n in (48,55,60,63): L.append(f"midi 90 {n:02X} 64")
            L.append("wait 2000")
            for n in (48,55,60,63): L.append(f"midi 80 {n:02X} 00")
            L+=["wait 1500","wavstop"]
            open(f"{OUT}/s.mope","w").write("\n".join(L)+"\n")
            q=subprocess.run(["build/host/mope_host",f"{OUT}/s.mope",OUT],capture_output=True,text=True)
            if q.returncode: print("fail",nm,q.stdout[-200:]); sys.exit(1)
            r[(mac,end)]=meas(rd(f"{OUT}/m.wav"))
    e=np.log2(r[("EYES",127)]["cent"]/r[("EYES",0)]["cent"]); el=r[("EYES",127)]["rms"]-r[("EYES",0)]["rms"]
    b=r[("BITE",127)]["top"]-r[("BITE",0)]["top"]; bc=np.log2(r[("BITE",127)]["cent"]/r[("BITE",0)]["cent"]); bl=r[("BITE",127)]["rms"]-r[("BITE",0)]["rms"]
    n=100*(r[("NERVES",127)]["move"]-r[("NERVES",0)]["move"]); ro=r[("ROOM",127)]["tail"]-r[("ROOM",0)]["tail"]
    rows.append(dict(name=nm,eyes_oct=round(e,1),eyes_db=round(el,1),bite_top_db=round(b,1),bite_oct=round(bc,1),bite_db=round(bl,1),nerves_move=round(n),room_tail_db=round(ro,1)))
json.dump(rows,open('build/review/macros.json','w'),indent=1)
print(f"{'name':20s} EYES oct / dB   BITE top dB / oct / dB   NERVES +move%   ROOM tail dB")
for r in rows:
    flag=[k for k,ok in (("EYES",r['eyes_oct']>=1.5 or abs(r['eyes_db'])>=12),("BITE",r['bite_top_db']>=5 or r['bite_oct']>=0.5),("NERVES",r['nerves_move']>=8),("ROOM",r['room_tail_db']>=15)) if not ok]
    print(f"{r['name']:20s} {r['eyes_oct']:5.1f} {r['eyes_db']:6.1f}   {r['bite_top_db']:6.1f} {r['bite_oct']:5.1f} {r['bite_db']:6.1f}   {r['nerves_move']:5d}   {r['room_tail_db']:6.1f}   {' '.join(flag)}")
