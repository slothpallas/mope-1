import wave, re, json, numpy as np
SR=44100
def rd(f):
    w=wave.open(f); x=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float64)/32768.0
    return x.reshape(-1,2)
def db(v): return 20*np.log10(max(v,1e-9))
def env(m, ms=10):
    n=int(SR*ms/1000); k=len(m)//n
    return np.sqrt((m[:k*n].reshape(k,n)**2).mean(1))
def spec(m):
    w=np.hanning(len(m)); X=np.abs(np.fft.rfft(m*w)); f=np.fft.rfftfreq(len(m),1/SR); return f,X**2
def centroid(m):
    f,P=spec(m); return float((f*P).sum()/(P.sum()+1e-20))
def bands(m):
    f,P=spec(m); t=P.sum()+1e-20
    return [float(P[(f>=a)&(f<b)].sum()/t) for a,b in ((0,120),(120,500),(500,2000),(2000,6000),(6000,22050))]
src=open('firmware/src/app/ui.c').read()
body=src[src.index('static const mood_t MOODS[] = {'):]
body=body[:body.index('\n};')]
moods=re.findall(r'\{"([^"]+)", (CAT_\w+), (-?\d+), (-?\d+), \{([^}]*)\},\s*\{([^}]*)\}, (\d), \{([^}]*)\}',body)
VN="table pos detune sub cutoff reso envamt moddec attack decay sustain release lforate lfodepth lfoshape lfodest chorus delay time feedback reverb size fry tape".split()
XN="btable btune bmeet blevel ftype drive mattack mwave lfo2rate lfo2depth lfo2shape lfo2dest level".split()
TAB="PLAIN MOAN PULSE CRUSH WARBLE FOLD CHURCH SYNC GLASS BREATH CRY HOLLOW BEND TOY ICE SAG".split()
FT="LOW BAND HIGH NOTCH COMB VOWEL".split(); MEET="MIX FM RING SYNC".split()
LD="POS CUTOFF PITCH VOLUME SHADOW TABLE DETUNE WALLS TEARS COPIES DUST SWIRL BURN MUMBLE".split()
LS="SINE TRI SQUARE SAW RANDOM RISE DRIFT STEPS THROB".split()
ENUM={}
for names in ("LD_POS LD_CUTOFF LD_PITCH LD_VOLUME LD_SHADOW LD_TABLE LD_DETUNE LD_WALLS LD_TEARS LD_COPIES LD_DUST LD_SWIRL LD_BURN LD_MUMBLE",
              "LFO_SINE LFO_TRI LFO_SQUARE LFO_SAW LFO_RANDOM LFO_RISE LFO_DRIFT LFO_STEPS LFO_THROB",
              "FT_LOW FT_BAND FT_HIGH FT_NOTCH FT_COMB FT_VOWEL","MEET_MIX MEET_FM MEET_RING MEET_SYNC","VM_POLY VM_MONO VM_LEGATO VM_UNISON"):
    for k,nm in enumerate(names.split()): ENUM[nm]=k
def num(t):
    t=t.strip(); return ENUM[t] if t in ENUM else int(t)
band=rd('build/review/wav/band.wav'); band_rms=np.sqrt((band**2).mean())
rows=[]
for i,(name,cat,octv,glide,v,x,hn,n) in enumerate(moods):
    v=[num(t) for t in v.split(',')]; x=[num(t) for t in x.split(',')]
    a=rd(f'build/review/wav/a{i:02d}.wav'); m=a.mean(1); s=(a[:,0]-a[:,1])/2
    T=lambda t:int(t*SR)
    ch=m[T(0):T(3)]; e=env(ch); pk=e.max()
    att=float(np.argmax(e>=0.9*pk)*10)                      # ms to 90% of the chord's peak level
    sus=m[T(1.5):T(3)]
    tail=env(m[T(3):T(5.5)]); below=np.where(tail<pk*0.01)[0]
    rel=float(below[0]*10) if len(below) else 2500.0        # ms to -40 dB after the keys are up
    # movement: how the brightness and the level change while held (1 .. 3 s, 100 ms frames)
    cs=[centroid(m[T(1+0.1*k):T(1.1+0.1*k)]) for k in range(20)]
    es=env(m[T(1):T(3)],100)
    width=float(np.sqrt((s[T(1.5):T(3)]**2).mean())/(np.sqrt((sus**2).mean())+1e-9))
    f,P=spec(sus); flat=float(np.exp(np.log(P[(f>100)&(f<8000)]+1e-20).mean())/(P[(f>100)&(f<8000)].mean()+1e-20))
    riff=m[T(5.5):T(7.9)]; re_=env(riff,5)
    # how separate the riff's notes are: the level's dip between notes, against its peaks
    seg=re_[:len(re_)//40*40].reshape(-1,40)                 # 200 ms a note
    dip=float(np.mean(seg.min(1)/(seg.max(1)+1e-9)))
    ratt=float(np.mean([np.argmax(q>=0.8*q.max())*5 for q in seg]))   # ms to 80% in each riff note
    bass=m[T(8.5):T(10.5)]; bb=bands(bass)
    b=rd(f'build/review/wav/b{i:02d}.wav'); L=min(len(b),len(band))
    synth_only=b[:L]-band[:L]                                 # (the band renders the same each time: what the preset adds)
    rows.append(dict(i=i,name=name,cat=cat[4:],oct=int(octv),glide=int(glide),
        table=TAB[v[0]],pos=v[1],detune=v[2],sub=v[3],cutoff=v[4],reso=v[5],envamt=v[6],
        A=v[8],D=v[9],S=v[10],R=v[11],lfo=f"{LS[v[14]]}>{LD[v[15]]} r{v[12]} d{v[13]}",
        lfo2=f"{LS[x[10]]}>{LD[x[11]]} r{x[8]} d{x[9]}",chorus=v[16],delay=v[17],fb=v[19],reverb=v[20],size=v[21],fry=v[22],tape=v[23],
        shadow=(f"{TAB[x[0]]} {MEET[x[2]]} t{x[1]} l{x[3]}" if x[3]>0 else "-"),filter=FT[x[4]],drive=x[5],sigh=f"a{x[6]} w{x[7]}",
        chord_rms_db=round(db(np.sqrt((sus**2).mean())),1),chord_peak_db=round(db(np.abs(a[T(0):T(3)]).max()),1),
        attack_ms=att,release_ms=rel,centroid_hz=round(float(np.mean(cs))),centroid_move_pct=round(100*float(np.std(cs)/(np.mean(cs)+1e-9))),
        level_move_pct=round(100*float(np.std(es)/(np.mean(es)+1e-9))),width=round(width,2),noisiness=round(flat,3),
        bands_chord=[round(q,2) for q in bands(sus)],riff_attack_ms=round(ratt),riff_dip=round(dip,2),
        riff_centroid_hz=round(centroid(riff)),bass_rms_db=round(db(np.sqrt((bass**2).mean())),1),bass_sub_share=round(bb[0],2),bass_low_share=round(bb[0]+bb[1],2),
        loud_db=round(db(env(m,400).max()),1),top_db=round(10*np.log10(sum(bands(riff)[3:])+1e-9),1),
        with_band_synth_db=round(db(np.sqrt((synth_only**2).mean())),1),band_db=round(db(band_rms),1),mix_peak_db=round(db(np.abs(b).max()),1)))
json.dump(rows,open('build/review/presets.json','w'),indent=1)
H=["i","name","cat","table","pos","detune","sub","filter","cutoff","reso","envamt","drive","A","D","S","R","lfo","lfo2","sigh","shadow","chorus","delay","fb","reverb","size","fry","tape","oct","glide"]
M=["i","name","cat","loud_db","top_db","chord_rms_db","chord_peak_db","attack_ms","release_ms","centroid_hz","centroid_move_pct","level_move_pct","width","noisiness","bands_chord","riff_attack_ms","riff_dip","riff_centroid_hz","bass_rms_db","bass_sub_share","bass_low_share","with_band_synth_db","mix_peak_db"]
def tab(cols):
    o="| "+" | ".join(cols)+" |\n|"+"---|"*len(cols)+"\n"
    for r in rows: o+="| "+" | ".join(str(r[c]) for c in cols)+" |\n"
    return o
open('build/review/presets.md','w').write("# MOPE-1: the factory presets\n\n## Settings (0..100 unless said)\n\n"+tab(H)+
 "\n## Measured from host renders (the same engine as the device)\n\nchord = C2 G2 C3 Eb3 held 3 s; riff = twelve 120 ms notes, 200 ms apart, C3..C5; bass = four 400 ms notes at C1..G1; band = the default beat, drums and bass (band alone: "+str(round(db(band_rms),1))+" dB rms).\n"
 "attack_ms: time to 90% of the chord's peak. release_ms: time to -40 dB after the keys are up (2500 = longer). centroid: the spectrum's centre (brightness), its move = std/mean over 2 s held. level_move: the same for the level. width: side/mid. noisiness: spectral flatness 100 Hz..8 kHz. bands_chord: share of energy <120, 120-500, 500-2k, 2k-6k, >6k Hz. top_db: the riff's energy above 2 kHz against all of it (an open saw at these notes: about -11 dB; -25: dull). riff_dip: the level between riff notes against their peaks (1 = no gap, smeared; 0 = clean gaps). bass_sub_share <120 Hz, bass_low_share <500 Hz.\n\n"+tab(M))
import collections
print(len(rows)); c=collections.Counter(r['cat'] for r in rows); print(c)
print("attack>200ms:",sum(r['attack_ms']>200 for r in rows),"release>1500:",sum(r['release_ms']>1500 for r in rows),"riff_dip>0.5:",sum(r['riff_dip']>0.5 for r in rows))
print("centroid_move<5%:",sum(r['centroid_move_pct']<5 for r in rows),"median centroid:",np.median([r['centroid_hz'] for r in rows]))
print("drive>0:",sum(r['drive']>0 for r in rows),"reverb>=40:",sum(r['reverb']>=40 for r in rows))
