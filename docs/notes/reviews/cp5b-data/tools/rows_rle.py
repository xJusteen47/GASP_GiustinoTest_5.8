import json,sys
rows=[json.loads(l) for l in open(sys.argv[1])]
prev=None; start=None; pos=[]
def flush(end):
    if prev is None: return
    print("v%03d-%03d %s | a: %s"%(start,end,prev,' '.join(pos)))
for r in rows:
    c=r['combat']; m=r['montage']
    key="tl=%s%s mon=%s tags=%s jump=%d cmd=%s"%(c['timeline'],'*' if c['inTimeline'] else '',m[0].replace('AM_','') if m else '-',','.join(t.split('.')[-1] for t in r['tags']),c['jump']['phase'],json.dumps(c['timelineStarted']).replace(' ',''))
    if key!=prev:
        flush(r['f']-1); prev=key; start=r['f']; pos=[]
    s=("%g"%round(m[1],1)) if m else ''
    if c['timelinePaused']: s+='H%d'%c['timelinePaused']
    if r['tap']: s+='T'
    z=round(r['loc'][2]); 
    pos.append(s+("z%d"%z if '-z' in sys.argv else ''))
flush(rows[-1]['f'])
