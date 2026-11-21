# Per-contact foot slide in a capture (runstart-turnaround-desktop; the one-off of w1/w2 made a tool): runs where the foot is below
# Z_CM (13) between frames F0..F1 (80-240), horizontal travel of each run. usage: python runstart_contact_slide.py <metrics_*.json>...
import json, math, sys
Z, F0, F1 = 13.0, 80, 240
for path in sys.argv[1:]:
    M = json.load(open(path)); out = {}
    for s in 'lr':
        runs, cur = [], []
        for d in M:
            if F0 <= d['f'] <= F1 and d['foot_' + s][2] < Z:
                cur.append(d); continue
            if len(cur) > 1:
                runs.append((cur[0]['f'], cur[-1]['f'], round(sum(math.dist(a['foot_' + s][:2], b['foot_' + s][:2]) for a, b in zip(cur, cur[1:])), 1)))
            cur = []
        out[s] = runs
    print(path.split('metrics_')[-1], 'L', [r[2] for r in out['l']], 'R', [r[2] for r in out['r']])
    print('   frames L', [r[:2] for r in out['l']], 'R', [r[:2] for r in out['r']])
