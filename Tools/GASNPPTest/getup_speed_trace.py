"""getup-momentum-2: per-frame capsule speed (cm/s, 60 fps) get-up +14..+33; M = montage playing, g = past the get-up segment; driver taps.
    python Tools/GASNPPTest/getup_speed_trace.py <capture_dir> <run> [<run> ...]"""
import json, sys, math, os
root = sys.argv[1]
for run in sys.argv[2:]:
    L = [json.loads(l) for l in open(os.path.join(root, run, 'frames.jsonl'))]
    g0 = next(d['f'] for d in L if d['seg'] == 'getup')
    byf = {d['f']: d for d in L}
    taps = [(d['f'] - g0, d['tap']) for d in L if 'tap' in d]
    out = []
    for k in range(14, 34):
        a, b = byf.get(g0 + k - 1), byf.get(g0 + k)
        if not a or not b: continue
        v = round(math.hypot(b['loc'][0] - a['loc'][0], b['loc'][1] - a['loc'][1]) * 60)
        m = b.get('montage'); out.append(f"{k}:{v}{'' if not m else 'M'}{'' if b['seg']=='getup' else 'g'}")
    print(run, ' '.join(out)); print('  taps', taps)
