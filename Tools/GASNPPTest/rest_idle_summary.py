import json, sys
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
prev = None
for r in rows:
    f = r['f']; b = r.get('body') or {}
    an = (r.get('combat') or {}).get('anim') or []
    key = ' | '.join(a.rsplit(' ', 1)[0] for a in an[:2])
    fl, fr = b.get('foot_l', [0, 0]), b.get('foot_r', [0, 0])
    line = f"f{f} x{r['loc'][0]:.1f} y{r['loc'][1]:.1f} footL({fl[0]:.1f},{fl[1]:.1f}) footR({fr[0]:.1f},{fr[1]:.1f}) {key}"
    if f in (20, 175, 180, 200, 240, 250, 255, 260, 270, 280, 300, 330, 360, 400, 439) or (prev is not None and key.split(' ')[0] != prev.split(' ')[0]):
        print(line)
    prev = key
