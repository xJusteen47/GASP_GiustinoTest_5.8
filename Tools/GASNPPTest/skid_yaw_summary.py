import json, sys
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
prev = None
for r in rows:
    f = r['f']; b = r.get('body') or {}
    an = (r.get('combat') or {}).get('anim') or []
    key = ' | '.join(an[:2])
    head = key.split(' ')[0]
    fr = b.get('foot_r', [0, 0, 0]); fl = b.get('foot_l', [0, 0, 0])
    if (110 <= f <= 200 and f % 4 == 0) or head != prev or f in (136, 137, 138, 139):
        print(f"f{f} x{r['loc'][0]:.1f} yaw{r.get('yaw')} fR({fr[0]:.0f},{fr[1]:.0f},{fr[2]:.0f}) fL({fl[0]:.0f},{fl[1]:.0f}) {key}")
    prev = head
