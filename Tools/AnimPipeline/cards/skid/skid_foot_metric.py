"""Skid foot metric (skid-fix-1): per-foot world position/speed log and the back foot's horizontal travel while planted.
    python skid_foot_metric.py <frames.jsonl> <out.csv> [--from=117 --to=167 --plant=8]
Back foot = the foot behind along the old run direction at the plant (window start + 4 frames). Planted = world z <= that foot's
lowest z in the window + plant cm. Travel = sum of horizontal steps between consecutive planted frames. Prints <= 5 lines."""
import json, math, sys
args = [a for a in sys.argv[1:] if not a.startswith('--')]
opt = dict(a[2:].split('=', 1) for a in sys.argv[1:] if a.startswith('--'))
f0, f1, plant = int(opt.get('from', 117)), int(opt.get('to', 167)), float(opt.get('plant', 8))
rows = {}
for line in open(args[0]):
    r = json.loads(line)
    b = r.get('body') or {}
    if b.get('actor') and b.get('foot_l'):
        a = b['actor']
        rows[r['f']] = {'actor': a, 'clip': ((r.get('combat') or {}).get('anim') or ['-'])[0],
                        **{k: [a[i] + b[k][i] for i in range(3)] for k in ('foot_l', 'foot_r')}}
fr = [f for f in sorted(rows) if f0 <= f <= f1]
with open(args[1], 'w') as out:
    out.write('f,clip,lx,ly,lz,l_speed,rx,ry,rz,r_speed\n')
    for f in fr:
        r, p = rows[f], rows.get(f - 1)
        sp = {k: (math.hypot(r[k][0] - p[k][0], r[k][1] - p[k][1]) * 60 if p else 0) for k in ('foot_l', 'foot_r')}
        out.write(f"{f},{r['clip'].split(' ')[0]},{r['foot_l'][0]:.1f},{r['foot_l'][1]:.1f},{r['foot_l'][2]:.1f},{sp['foot_l']:.0f},"
                  f"{r['foot_r'][0]:.1f},{r['foot_r'][1]:.1f},{r['foot_r'][2]:.1f},{sp['foot_r']:.0f}\n")
pre = rows[f0 - 6]['actor'] if f0 - 6 in rows else rows[fr[0]]['actor']
d = [rows[f0]['actor'][0] - pre[0], rows[f0]['actor'][1] - pre[1]]
n = math.hypot(*d) or 1.0
d = [d[0] / n, d[1] / n]
fp = fr[min(4, len(fr) - 1)]
along = {k: rows[fp][k][0] * d[0] + rows[fp][k][1] * d[1] for k in ('foot_l', 'foot_r')}
back = min(along, key=along.get)
res = {}
for k in ('foot_l', 'foot_r'):
    zmin = min(rows[f][k][2] for f in fr)
    planted = [f for f in fr if rows[f][k][2] <= zmin + plant]
    travel = sum(math.hypot(rows[f][k][0] - rows[f - 1][k][0], rows[f][k][1] - rows[f - 1][k][1]) for f in planted if f - 1 in planted)
    res[k] = (travel, len(planted))
print(f"window f{f0}-f{f1}, old dir ({d[0]:.2f},{d[1]:.2f}), back foot {back}")
print(f"BACK FOOT planted travel {res[back][0]:.1f} cm over {res[back][1]} planted frames")
other = 'foot_r' if back == 'foot_l' else 'foot_l'
print(f"other foot {other}: {res[other][0]:.1f} cm over {res[other][1]} planted frames; csv {args[1]}")
