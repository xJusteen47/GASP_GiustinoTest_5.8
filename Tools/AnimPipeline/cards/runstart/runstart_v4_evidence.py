"""Run start v4r1 evidence (runstart-v3-pose-1; copy of runstart_v3_evidence.py): V3 (base 1, mv_run_start_v3s, top) vs V4R1 (base 1,
mv_run_start_v4s, bottom) on the same capture frames. Was: Run start v3 evidence (runstart-v3-1): one grid per view (az90, az0, game) with STOCK (top) and V3 (bottom) on the SAME capture frames,
the v3 mp4, and metrics (per frame: top clips, actor x, root and world feet; summary: v3 weight in the gate window, SS_180 max, feet slide
while planted).
    python runstart_v3_evidence.py <stock_cap> <v3_cap> <out_dir> [--from=80 --to=176 --step=6]
Stock = mv_run_start (base 1), v3 = mv_run_start_v3. Uses Tools/GASNPPTest/pie_capture_grid.py. Prints <= 5 lines."""
import json
import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
GRID = os.path.join(HERE, '..', '..', '..', 'GASNPPTest', 'pie_capture_grid.py')
stock, v3, out = sys.argv[1:4]
opt = dict(a[2:].split('=', 1) for a in sys.argv[4:] if a.startswith('--'))
f0, f1, step = int(opt.get('from', 80)), int(opt.get('to', 176)), int(opt.get('step', 6))
TAGS = os.environ.get('EV_CAPS', 'v3:mv_run_start_v3s,v4r1:mv_run_start_v4s').split(',')   # top,bottom = tag:sequence
CAPS = tuple((t.split(':')[0], c, t.split(':')[1]) for t, c in zip(TAGS, (stock, v3)))
os.makedirs(out, exist_ok=True)
tmp = os.path.join(out, '_tmp')
os.makedirs(tmp, exist_ok=True)
cols = (f1 - f0) // step + 1
for view in ('az90', 'az0', 'game'):
    imgs = []
    for tag, cap, seq in CAPS:
        p = os.path.join(tmp, f'{tag}_{view}.png')
        subprocess.run([sys.executable, GRID, 'sheet', cap, seq, view, p, f'--from={f0}', f'--to={f1}', f'--step={step}',
                        f'--cols={cols}', '--cell=200'], check=True, capture_output=True)
        imgs.append((tag, Image.open(p).convert('RGB')))
    w = max(i.width for _, i in imgs)
    sheet = Image.new('RGB', (w + 70, sum(i.height for _, i in imgs)), 'white')
    y = 0
    for tag, i in imgs:
        sheet.paste(i, (70, y))
        ImageDraw.Draw(sheet).text((8, y + i.height // 2), tag.upper(), fill='black')
        y += i.height
    sheet.save(os.path.join(out, f'grid_{view}.png'))
for f in os.listdir(tmp):
    os.remove(os.path.join(tmp, f))
os.rmdir(tmp)
subprocess.run([sys.executable, GRID, 'video', v3, CAPS[1][2], os.path.join(out, 'runstart_%s.mp4' % CAPS[1][0]), '--from=60', '--to=240'],
               check=True, capture_output=True)


def metrics(cap, seq):
    rows = [json.loads(l) for l in open(os.path.join(cap, seq, 'frames.jsonl')) if l.strip()]
    per = []
    for r in rows:
        b, a = r.get('body') or {}, (r.get('combat') or {}).get('anim') or []
        act = b.get('actor') or r.get('loc') or [0, 0, 0]
        w = lambda k: [round(act[i] + b[k][i], 1) for i in range(3)] if k in b else None
        per.append({'f': r['f'], 'anim': a[:3], 'actor_x': round(act[0], 1), 'root': w('root'), 'foot_l': w('foot_l'), 'foot_r': w('foot_r')})
    return per


def weight(row, name):
    return sum(float(s.split()[1]) for s in row['anim'] if s.split()[0].lower() == name.lower())


summ = {}
for tag, cap, seq in CAPS:
    per = metrics(cap, seq)
    json.dump(per, open(os.path.join(out, f'metrics_{tag}.json'), 'w'))
    win = [r for r in per if 80 <= r['f'] <= 240]
    slide = {}
    for s in ('l', 'r'):   # horizontal travel of a foot on frames where it is within 3 cm of its lowest point (planted), f80-240
        k = 'foot_' + s
        lo = min(r[k][2] for r in win if r[k])
        d = 0.0
        for a, b in zip(win, win[1:]):
            if a[k] and b[k] and a[k][2] - lo < 3 and b[k][2] - lo < 3:
                d += math.hypot(b[k][0] - a[k][0], b[k][1] - a[k][1])
        slide[s] = round(d, 1)
    summ[tag] = {'ss180_max_w': round(max(sum(float(x.split()[1]) for x in r['anim'] if x.startswith('SS_180')) for r in win), 2),
                 'actor_x_f200': next((r['actor_x'] for r in per if r['f'] == 200), None),
                 'planted_feet_travel_cm_f80_240': slide}
    clip = 'AS_GSTN_RunStart_' + tag
    if not any(weight(r, clip) for r in per):   # generic/stock tags (g1_h1, generic_h1): the run start clip actually seen
        seen = [s.split()[0] for r in per for s in r['anim'] if 'RunStart' in s.split()[0] or 'Run_Start' in s.split()[0]]
        clip = max(set(seen), key=seen.count) if seen else clip
    summ[tag]['clip'] = clip
    summ[tag]['clip_weight_f80_210'] = [(r['f'], round(weight(r, clip), 2)) for r in per if 80 <= r['f'] <= 210 and r['f'] % 5 == 0]
json.dump(summ, open(os.path.join(out, 'metrics_summary.json'), 'w'), indent=1)
print('evidence: ' + ', '.join(sorted(x for x in os.listdir(out) if x.endswith(('.png', '.mp4', '.json')))))
print('; '.join('%s feet %s x200 %s' % (t, summ[t]['planted_feet_travel_cm_f80_240'], summ[t]['actor_x_f200']) for t, _, _ in CAPS))
