"""PUSHBOX-P2: per-frame attacker / dummy horizontal distance, attacker step and the dummy's state tags from a capture's frames.jsonl.
    python pushbox_rows.py <capture_dir> <sequence> <out.txt>
Writes the table to out.txt and prints at most 5 summary lines (project rule 4)."""
import json
import math
import os
import sys

cap, seq, out = sys.argv[1], sys.argv[2], sys.argv[3]
rows = [json.loads(l) for l in open(os.path.join(cap, seq, 'frames.jsonl')) if l.strip()]
lines, prev, steps, kd, dists = [], None, [], [], []
for r in rows:
    a = r.get('loc')
    d = (r.get('dummy') or {}).get('loc')
    tags = [t.split('.')[-1] for t in ((r.get('dummy') or {}).get('tags') or []) if 'State' in t]
    atags = [t.split('.')[-1] for t in (r.get('tags') or []) if 'State' in t or 'Hitstop' in t]
    dist = math.hypot(a[0] - d[0], a[1] - d[1]) if a and d else None
    step = math.hypot(a[0] - prev[0], a[1] - prev[1]) if a and prev else 0.0
    prev = a
    steps.append((step, r['f']))
    if dist is not None:
        dists.append((dist, r['f']))
    if 'Knockdown' in tags:
        kd.append(r['f'])
    lines.append(f"v{r['f']:03d} dist={dist if dist is None else round(dist, 1)} astep={step:.1f} az={a[2] if a else None} dz={d[2] if d else None} "
                 f"dummy={','.join(tags)} att={','.join(atags)}")
open(out, 'w').write('\n'.join(lines) + '\n')
print(f'{seq}: {len(rows)} rows, table {out}')
print(f'knockdown frames: {kd[0]}-{kd[-1]} ({len(kd)})' if kd else 'knockdown frames: none')
print(f'min horizontal distance {min(dists)[0]:.1f} cm at v{min(dists)[1]}' if dists else 'no dummy rows')
print(f'largest attacker horizontal step {max(steps)[0]:.1f} cm at v{max(steps)[1]}')
