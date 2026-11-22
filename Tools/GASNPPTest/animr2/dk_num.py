import json,sys
rows=[json.loads(l) for l in open(sys.argv[1]+'/dropkick_whiff/frames.jsonl') if l.strip()]
m=[(r['f'],r['h'],r['montage'][1]) for r in rows if r.get('montage') and r['montage'][0]!='error']
st=m[0][0]; land=next(x for x in m if x[2]>=58.5)
hs=[r['h'] for r in rows]; fin=hs[-1]
gr=next(r['f'] for r in rows if r['f']>st and abs(r['h']-fin)<0.2 and all(abs(q['h']-fin)<0.2 for q in rows if q['f']>=r['f']))
print('montage start',st,'last montage row',m[-1][0],'land first',land,'first grounded',gr,'final h',fin)
