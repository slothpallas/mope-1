"""Rewrites MOODS in ui.c from 0.4.1's bank (ui-0.4.1.c) and sounds.py: reworks by name, replacements by name, new
ones appended, then EXTRA (movement added). Levels from build/review/levels.json (trim.py). Run from the repository root:
  python3 tools/presets/presets.py"""
import re, json, os, sys
UI='firmware/src/app/ui.c'
V="table pos detune sub cutoff reso envamt moddec attack decay sustain release lforate lfodepth lfoshape lfodest chorus delay time feedback reverb size fry tape".split()
X="btable btune bmeet blevel ftype drive mattack mwave lfo2rate lfo2depth lfo2shape lfo2dest level".split()
N="rtone ddark chrate chdepth chwidth crackle dust vmode".split()
F="swirl swrate swdepth swfb burn bntone bncrush bnrate mumble mbway mbrange mbreso".split()
ND=dict(rtone=50,ddark=50,chrate=30,chdepth=50,chwidth=100,crackle=0,dust=0,vmode=0)
FD=dict(swirl=0,swrate=30,swdepth=60,swfb=30,burn=0,bntone=60,bncrush=0,bnrate=0,mumble=0,mbway=0,mbrange=60,mbreso=50)
TAB="PLAIN MOAN PULSE CRUSH WARBLE FOLD CHURCH SYNC GLASS BREATH CRY HOLLOW BEND TOY ICE SAG".split()
E={}
for names in ("LD_POS LD_CUTOFF LD_PITCH LD_VOLUME LD_SHADOW LD_TABLE LD_DETUNE LD_WALLS LD_TEARS LD_COPIES LD_DUST LD_SWIRL LD_BURN LD_MUMBLE LD_BPITCH",
              "LFO_SINE LFO_TRI LFO_SQUARE LFO_SAW LFO_RANDOM LFO_RISE LFO_DRIFT LFO_STEPS LFO_THROB",
              "FT_LOW FT_BAND FT_HIGH FT_NOTCH FT_COMB FT_VOWEL","MEET_MIX MEET_FM MEET_RING MEET_SYNC","VM_POLY VM_MONO VM_LEGATO VM_UNISON","MB_BEAT MB_LFO MB_PLAY"):
    for k,nm in enumerate(names.split()): E[nm]=k
for k,nm in enumerate(TAB): E[nm]=k
def num(t):
    if isinstance(t,int): return t
    t=t.strip(); return E[t] if t in E else int(t)
def parse(src):
    body=src[src.index('static const mood_t MOODS[] = {'):]; body=body[:body.index('\n};')]
    out=[]
    for name,cat,o,g,v,x,hn,n in re.findall(r'\{"([^"]+)", (CAT_\w+), (-?\d+), (-?\d+), \{([^}]*)\},\s*\{([^}]*)\}, (\d), \{([^}]*)\}',body):
        d=dict(name=name,cat=cat,oct=int(o),glide=int(g))
        d.update(zip(V,[num(t) for t in v.split(',')])); d.update(zip(X,[num(t) for t in x.split(',')]))
        nn=[num(t) for t in n.split(',')]
        if int(hn):
            nn+= [0]*(8-len(nn)); d.update(zip(N,nn))
        out.append(d)
    return out
def base(**k):
    d=dict(oct=0,glide=0,table=0,pos=0,detune=10,sub=0,cutoff=70,reso=15,envamt=30,moddec=40,attack=0,decay=50,sustain=70,release=25,
           lforate=30,lfodepth=0,lfoshape=0,lfodest=0,chorus=0,delay=0,time=50,feedback=35,reverb=12,size=40,fry=0,tape=0,
           btable=0,btune=-12,bmeet=0,blevel=0,ftype=0,drive=0,mattack=0,mwave=0,lfo2rate=55,lfo2depth=0,lfo2shape=1,lfo2dest=2,level=0)
    d.update(k); return d
def emit(d):
    q=lambda keys,dd: ", ".join(str(num(dd[k])) for k in keys)
    hn=any(k in d for k in N); hf=any(k in d for k in F)
    nd=dict(ND); nd.update({k:d[k] for k in N if k in d}); fd=dict(FD); fd.update({k:d[k] for k in F if k in d})
    return ('    {"%s", %s, %d, %d, {%s},\n     {%s}, %d, {%s}, %d, {%s}},\n' %
            (d['name'],d['cat'],d['oct'],d['glide'],q(V,d),q(X,d),int(hn),q(N,nd) if hn else "0",int(hf),q(F,fd) if hf else "0"))
exec(open(os.path.join(os.path.dirname(__file__),'sounds.py')).read())
src=open(os.path.join(os.path.dirname(__file__),'ui-0.4.1.c')).read()   # (0.4.1's bank: what is reworked, replaced, added)
old=parse(src); assert len(old)==54,len(old)
lv={}
if os.path.exists('build/review/levels.json'): lv=json.load(open('build/review/levels.json'))
res=[]
for d in old:
    if d['name'] in REPLACE: d=dict(REPLACE[d['name']])
    elif d['name'] in REWORK: d.update(REWORK[d['name']])
    res.append(d)
res+= [dict(x) for x in NEW]
for d in res:
    if d['name'] in EXTRA: d.update(EXTRA[d['name']])
for d in res:
    if d['name'] in lv: d['level']=max(-80,min(80,lv[d['name']]))
text=''.join(emit(d) for d in res[:54])
text+='    /* 0.5: bold ones for the macros, the swirl, the burn and the mumble: basses, leads, keys that move */\n'+''.join(emit(d) for d in res[54:])
cur=open(UI).read()
i=cur.index('static const mood_t MOODS[] = {')+len('static const mood_t MOODS[] = {\n'); j=cur.index('\n};',i)+1
open(UI,'w').write(cur[:i]+text+cur[j:])
json.dump([d['name'] for d in res],open('build/review/names.json','w'))
print(len(res),"presets")
