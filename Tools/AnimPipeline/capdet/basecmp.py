"""Numeric comparison of a variant clip with its BASE clip (rules 10 and 15b: a fix or a variant that takes away more than 15 % of
amplitude, speed or root motion is a FAIL, whatever the other checks say).

Input: two joints json files as written for runstart_pivot_gates.py --json: {"fps": 30, "joints": {name: [[x, y, z], ...]}}, one row
per frame, cm, Y up. Names are the rig's (hand_MainPoint_l ...); only joints present in BOTH files are compared; 'root' is optional.

  python basecmp.py <base.json> <variant.json> [--out report.json]

Measured per joint of the limb ends (hand, foot, ball; l/r), RELATIVE to the pelvis (mean of the two thighs) so the body's travel
does not hide a smaller swing:
  amp    extent = the largest distance between two of its positions over the clip (the swing), like conserve.py's excursion
  speed  the fastest it moves over two frames, on a 3-frame median track (a one-frame flick is not speed), cm/s at the clip's fps
and for the whole body:
  root   length of the root's path on the ground plane (cm), when 'root' exists in both files
Metrics under the FLOOR in the BASE are reported, not judged (15 % of noise is noise). Gains are never a fail. Clips of different
length are fine: the metrics do not depend on duration.

Prints at most 3 lines; the first is "BASE: OK" or "BASE: FAIL <metric> -<loss>%". Exit 0 OK, 1 FAIL, 2 usage / missing file.
Same pure-numpy code runs offline and in the cloud (no editor, no bpy).
"""
import json
import os
import sys

import numpy as np

MAX_LOSS = 0.15
FLOOR = {'amp': 10.0, 'speed': 30.0, 'root': 5.0}   # cm, cm/s, cm (in the BASE)
UP = 1
ENDS = ['hand_MainPoint_', 'foot_MainPoint_', 'ball_MainPoint_']
THIGH = 'thigh_MainPoint_'


def load(path):
    d = json.load(open(path, encoding='utf-8'))
    return float(d.get('fps', 30)), {k: np.array(v, float) for k, v in d['joints'].items()}


def _med3(a):
    if len(a) < 3:
        return a
    b = np.sort(np.stack([a[:-2], a[1:-1], a[2:]]), axis=0)[1]
    return np.concatenate([a[:1], b, a[-1:]])


def _extent(p):
    """largest distance between two positions (subsampled to 200 frames: a held extreme lasts more than two frames)"""
    if len(p) > 200:
        p = p[np.linspace(0, len(p) - 1, 200).astype(int)]
    d = p[:, None, :] - p[None, :, :]
    return float(np.sqrt((d ** 2).sum(-1)).max())


def metrics(fps, J):
    m = {}
    if THIGH + 'l' in J and THIGH + 'r' in J:
        pel = 0.5 * (J[THIGH + 'l'] + J[THIGH + 'r'])
    elif 'root' in J:
        pel = J['root']
    else:
        pel = None
    for b in ENDS:
        for s in 'lr':
            k = b + s
            if k not in J or pel is None:
                continue
            rel = _med3(J[k] - pel)
            m['amp.' + k[:-len('_MainPoint_l')] + '_' + s] = _extent(rel)
            v = np.linalg.norm(rel[2:] - rel[:-2], axis=1) * fps / 2.0 if len(rel) > 2 else np.zeros(1)
            m['speed.' + k[:-len('_MainPoint_l')] + '_' + s] = float(v.max())
    if 'root' in J:
        r = J['root'].copy(); r[:, UP] = 0
        m['root.travel'] = float(np.linalg.norm(r[1:] - r[:-1], axis=1).sum())
    return m


def compare(base_path, var_path, max_loss=MAX_LOSS):
    fb, B = load(base_path)
    fv, V = load(var_path)
    both = {k: B[k] for k in B if k in V}
    mb = metrics(fb, {k: B[k] for k in both})
    mv = metrics(fv, {k: V[k] for k in both})
    rep = {'max_loss': max_loss, 'metrics': {}, 'fails': []}
    for k, b in mb.items():
        v = mv.get(k, 0.0)
        floor = FLOOR[k.split('.')[0]]
        loss = (b - v) / b if b > 1e-9 else 0.0
        rep['metrics'][k] = [round(b, 1), round(v, 1), round(loss * 100, 1)]
        if b >= floor and loss > max_loss + 1e-9:
            rep['fails'].append((k, loss))
    rep['fails'].sort(key=lambda x: -x[1])
    rep['ok'] = bool(mb) and not rep['fails']
    if not mb:
        rep['reason'] = 'no common joints to compare (names differ)'
    return rep


def lines(rep):
    if rep['ok']:
        worst = max(rep['metrics'].items(), key=lambda kv: kv[1][2])
        return [f"BASE: OK (worst loss {worst[1][2]:.0f}% {worst[0]}, limit {rep['max_loss'] * 100:.0f}%)"]
    if rep.get('reason'):
        return [f"BASE: FAIL {rep['reason']}"]
    out = [f"BASE: FAIL {rep['fails'][0][0]} -{rep['fails'][0][1] * 100:.0f}% (limit {rep['max_loss'] * 100:.0f}%)"]
    out += [f"  {k} -{l * 100:.0f}% (base {rep['metrics'][k][0]}, variant {rep['metrics'][k][1]})" for k, l in rep['fails'][1:3]]
    return out


def main(argv):
    argv = list(argv)
    out = None
    if '--out' in argv:
        i = argv.index('--out'); out = argv[i + 1]; del argv[i:i + 2]
    if len(argv) != 2 or not all(os.path.exists(a) for a in argv):
        print('usage: basecmp.py <base.json> <variant.json> [--out report.json]'); return 2
    rep = compare(*argv)
    if out:
        json.dump(rep, open(out, 'w', encoding='utf-8'), indent=1)
    print('\n'.join(lines(rep)))
    return 0 if rep['ok'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
