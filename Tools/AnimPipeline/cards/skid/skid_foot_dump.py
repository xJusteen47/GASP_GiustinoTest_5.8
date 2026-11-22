"""Per-frame skid dump from a capture's frames.jsonl: f, ABP state, top clip, actor speed (cm/s at 60 fps), yaw, feet world xy/z.
    python skid_foot_dump.py <frames.jsonl> [from] [to]"""
import json, math, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
f0 = int(sys.argv[2]) if len(sys.argv) > 2 else 90
f1 = int(sys.argv[3]) if len(sys.argv) > 3 else 200
prev = None
for r in rows:
    b = r.get('body') or {}
    a = b.get('actor')
    if not a:
        continue
    sp = 0.0 if prev is None else math.hypot(a[0] - prev[0], a[1] - prev[1]) * 60
    prev = a
    if not f0 <= r['f'] <= f1:
        continue
    fw = {k: [a[0] + b[k][0], a[1] + b[k][1], a[2] + b[k][2]] for k in ('foot_l', 'foot_r')}
    anim = ((r.get('combat') or {}).get('anim') or ['-'])[0]
    st = (r.get('abp') or {}).get('State Controller', ['-', 0])
    tags = ','.join(t.split('.')[-1] for t in (r.get('tags') or []) if 'Skid' in t or 'Run' in t)
    print(r['f'], st[0][:14], st[1], anim[:40], round(sp), r.get('yaw', [0])[0], 'L', [round(v) for v in fw['foot_l']], 'R', [round(v) for v in fw['foot_r']], tags)
