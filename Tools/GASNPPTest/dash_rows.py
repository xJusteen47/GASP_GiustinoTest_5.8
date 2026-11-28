# Dash per stage (2026-11-28): ground speed per frame around each L3 press of a pie_capture_move capture (mv_dash_* sequences).
#   python -I Tools/GASNPPTest/dash_rows.py <capture_dir>
# Per frame: f:stage(0-based)/dash counter/latched dash top speed/presented ground speed, '*' on the frame the boost command is seen.
import json, math, sys, os, re
root = sys.argv[1]
log = open(os.path.join(root, 'capture.log'), encoding='utf-8', errors='replace').read()
for seq in sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d))):
    p = os.path.join(root, seq, 'frames.jsonl')
    if not os.path.exists(p):
        continue
    m = re.search(seq + r': boost \(IA_GASNPP_RunBoost press\) on v(\d+)', log)
    press = int(m.group(1)) if m else None
    rows = [json.loads(l) for l in open(p)]
    out = []
    for r in rows:
        run = ((r.get('combat') or {}).get('run')) or {}
        v = r.get('vel') or [0, 0, 0]
        out.append((r['f'], run.get('stage'), run.get('dash'), round(run.get('dashTop') or 0), run.get('boostNow'), round(math.hypot(v[0], v[1]))))
    print(f'== {seq}: press v{press}, rows {len(rows)}, stage path {[k for i, k in enumerate(o[1] for o in out) if i == 0 or out[i-1][1] != k]}')
    if press is None:
        continue
    pre = [o for o in out if press - 30 <= o[0] < press]
    print(f'  30 f before: stage {sorted({o[1] for o in pre})}, speed {min(o[5] for o in pre)}-{max(o[5] for o in pre)}')
    win = [o for o in out if press - 2 <= o[0] <= press + 40]
    print('  f:stage/dash/top/speed ' + ' '.join(f'{o[0]}:{o[1]}/{o[2]}/{o[3]}/{o[5]}{"*" if o[4] else ""}' for o in win))
    post = [o for o in out if press <= o[0] <= press + 60]
    pk = max(post, key=lambda o: o[5])
    print(f'  peak {pk[5]} at f{pk[0]} (stage {pk[1]}); f+45..60: {[o[5] for o in out if press + 45 <= o[0] <= press + 60]}')
