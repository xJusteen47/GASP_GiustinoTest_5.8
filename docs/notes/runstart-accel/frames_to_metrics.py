"""frames.jsonl (pie_capture_move.py) -> metrics json [{f, actor_x, anim}] for speed_curve.py. Prints 1 line.
Usage: python frames_to_metrics.py <capture>/<sequence>/frames.jsonl out.json"""
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
out = []
for r in rows:
    b = r.get('body') or {}
    act = b.get('actor') or r.get('loc') or [0, 0, 0]
    out.append({'f': r['f'], 'actor_x': round(act[0], 1), 'anim': ((r.get('combat') or {}).get('anim') or [])[:3]})
json.dump(out, open(sys.argv[2], 'w'), indent=0)
print('frames', len(out), '->', sys.argv[2])
