import json, sys, math
# per segment: capsule travel, max foot world travel (while the segment lasts), pelvis z range relative to capsule z
rows = [json.loads(l) for l in open(sys.argv[1])]
segs = {}
for r in rows:
    if 'foot_l' not in r: continue
    segs.setdefault((r['seg'], r['f'] - r['segf']), []).append(r)
out = []
for (seg, f0), rs in segs.items():
    a = rs[0]
    d = lambda p, q: math.hypot(p[0] - q[0], p[1] - q[1])
    cap = max(d(r['loc'], a['loc']) for r in rs)
    fl = max(d(r['foot_l'], a['foot_l']) for r in rs); fr = max(d(r['foot_r'], a['foot_r']) for r in rs)
    rootoff = max(d(r['root'], r['loc']) for r in rs)
    pz = [r['pelvis'][2] - r['loc'][2] for r in rs]
    out.append(f"{seg:<22} f{f0}-{rs[-1]['f']} n{len(rs)} capsule {cap:.1f} footL {fl:.1f} footR {fr:.1f} root-capsule max {rootoff:.1f} pelvis-capsule z {min(pz):.1f}..{max(pz):.1f}")
print('\n'.join(out))
