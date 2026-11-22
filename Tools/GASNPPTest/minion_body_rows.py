"""PUSHBOX-P5: per frame, the fighter's step, its closest minion and the closest minion pair (horizontal, cm) from a capture's frames.jsonl
(the 'minions' puppet positions of pie_capture_move.py's MINION_SEQUENCES).
    python minion_body_rows.py <capture_dir> <sequence> <out.txt>
Writes the table to out.txt and prints at most 5 summary lines (project rule 4)."""
import json
import math
import os
import sys

cap, seq, out = sys.argv[1], sys.argv[2], sys.argv[3]
rows = [json.loads(l) for l in open(os.path.join(cap, seq, 'frames.jsonl')) if l.strip()]
lines, prev, to_fighter, pairs, counts = [], None, [], [], []
for r in rows:
    a = r.get('loc')
    ms = r.get('minions') or []
    step = math.hypot(a[0] - prev[0], a[1] - prev[1]) if a and prev else 0.0
    prev = a
    near = min((math.hypot(m[0] - a[0], m[1] - a[1]) for m in ms), default=None) if a else None
    pair = min((math.hypot(p[0] - q[0], p[1] - q[1]) for i, p in enumerate(ms) for q in ms[i + 1:]), default=None)
    counts.append(len(ms))
    if near is not None:
        to_fighter.append((near, r['f']))
    if pair is not None:
        pairs.append((pair, r['f']))
    lines.append(f"v{r['f']:03d} n={len(ms)} astep={step:.1f} fighter_min={None if near is None else round(near, 1)} pair_min={None if pair is None else round(pair, 1)}")
open(out, 'w').write('\n'.join(lines) + '\n')
print(f'{seq}: {len(rows)} rows, minions seen {min(counts, default=0)}-{max(counts, default=0)}, table {out}')
print(f'closest minion to fighter {min(to_fighter)[0]:.1f} cm at v{min(to_fighter)[1]}' if to_fighter else 'no minion rows')
print(f'closest minion pair {min(pairs)[0]:.1f} cm at v{min(pairs)[1]}' if pairs else 'no minion pairs')
