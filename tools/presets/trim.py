"""Evens out the presets' loudness on host renders: python3 tools/presets/trim.py NAMES.json [more]"""
import json, sys, numpy as np
R=json.load(open('build/review/presets.json'))
changed=set(json.load(open(sys.argv[1])))
tgt=-16.25                                  # (the loudest 400 ms: where 0.4.1's bank sat, its median)
print("target",tgt)
lv=json.load(open('build/review/levels.json')) if len(sys.argv)>2 else {}
cur={r['name']:r for r in R}
import re
src=open('firmware/src/app/ui.c').read()
for nm in sorted(changed):
    r=cur[nm]; m=re.search(r'\{"%s",[^\n]*\n\s*\{([^}]*)\}'%re.escape(nm),src); level=int(m.group(1).split(',')[12])
    lv[nm]=int(round(level+(tgt-r['loud_db'])/0.241))
    print(f"{nm:20s} loud {r['loud_db']:6.1f} level {level:4d} -> {lv[nm]}")
json.dump(lv,open('build/review/levels.json','w'))
