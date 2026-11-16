"""python count_repeats.py  : per capture, video frames where the montage position did not advance (outside hitstop/hold) / frames with a montage."""
import json,glob
for f in sorted(glob.glob('*/*.jsonl')):
    rows=[json.loads(l) for l in open(f)]; rep=tot=0; first=[]
    for p,r in zip(rows,rows[1:]):
        if not r['montage'] or not p['montage'] or r['montage'][0]!=p['montage'][0]: continue
        if r['combat']['timelinePaused']!=p['combat']['timelinePaused']: continue
        tot+=1
        if abs(r['montage'][1]-p['montage'][1])<0.01: rep+=1; first.append(r['f'])
    print(f, 'repeated %d/%d'%(rep,tot), 'v', first[:12])
