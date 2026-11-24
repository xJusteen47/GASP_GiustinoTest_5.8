"""LANCI-1A2A: launch numbers from the frames.jsonl of lanci_measure_boot.py (plain Python, outside the editor).
python Tools/GASNPPTest/lanci_measure_table.py <capture out dir> [more dirs...]  -> a markdown table on stdout.
Per sequence: hit frame (first frame the dummy's speed jumps by > 150 cm/s or its action state changes), launch velocity (the largest dummy
speed in the 3 frames after the hit, with its vertical part and angle), peak height over the floor (the floor = the host's standing z on frame 0,
the same capsule), flight time (hit -> first frame back on the floor), horizontal distance (hit -> landing), the dummy's action state and percent."""
import json
import math
import os
import sys

FPS = 60.0


def rows(path):
    with open(path, encoding='utf-8') as f:
        return [json.loads(line) for line in f if line.strip()]


def measure(rs):
    d = [(r['f'], r.get('dummy') or {}) for r in rs]
    d = [(f, x) for f, x in d if x.get('loc')]
    if len(d) < 3:
        return None
    floor = rs[0]['loc'][2] if rs[0].get('loc') else d[0][1]['loc'][2]
    speed = lambda x: math.sqrt(sum(c * c for c in (x.get('vel') or (0, 0, 0))))  # noqa: E731
    hit = None
    for (f0, a), (f1, b) in zip(d, d[1:]):
        sa, sb = (a.get('snap') or {}), (b.get('snap') or {})
        if speed(b) - speed(a) > 150.0 or (sa.get('damagePercent') is not None and sb.get('damagePercent', 0) > sa.get('damagePercent', 0) + 0.01):
            hit = (f1, b)
            break
    if hit is None:
        return {'hit': None}
    i0 = [f for f, _ in d].index(hit[0])
    win = d[i0:i0 + 4]
    fv, best = max(win, key=lambda t: speed(t[1]))
    v = best.get('vel') or (0, 0, 0)
    vh = math.hypot(v[0], v[1])
    after = d[i0:]
    peak_f, peak = max(after, key=lambda t: t[1]['loc'][2])
    land = None
    for f, x in after:
        if f > peak_f and x['loc'][2] <= floor + 3.0:
            land = (f, x)
            break
    p0 = hit[1]['loc']
    out = {
        'hit': hit[0], 'v': round(speed(best)), 'vh': round(vh), 'vz': round(v[2]), 'ang': round(math.degrees(math.atan2(v[2], vh)), 1),
        'z_hit': round(p0[2] - floor), 'peak': round(peak['loc'][2] - floor), 'rise': round(peak['loc'][2] - p0[2]),
        'percent': (hit[1].get('snap') or {}).get('damagePercent'),
        'state': [(x.get('snap') or {}).get('actionState') for _, x in after[:40:8]],
        'vz_before': round(((d[i0 - 1][1].get('vel') or (0, 0, 0))[2]) if i0 > 0 else 0),
    }
    if land:
        out['flight_s'] = round((land[0] - hit[0]) / FPS, 2)
        out['dist'] = round(math.hypot(land[1]['loc'][0] - p0[0], land[1]['loc'][1] - p0[1]))
    else:
        last = after[-1][1]['loc']
        out['flight_s'] = f'>{(after[-1][0] - hit[0]) / FPS:.2f}'
        out['dist'] = f'>{round(math.hypot(last[0] - p0[0], last[1] - p0[1]))}'
    attr = next((x.get('attr') for _, x in d if x.get('attr')), None)
    if attr:
        out['attr'] = {k: v for k, v in attr.items() if 'Gravity' in k or 'Fall' in k}
    return out


def main(dirs):
    print('| run | sequence | hit f | % at hit | v cm/s (h, z, angle) | z at hit | vz before | peak m | flight s | distance m | states |')
    print('|---|---|---|---|---|---|---|---|---|---|---|')
    attrs = {}
    for top in dirs:
        for name in sorted(os.listdir(top)):
            p = os.path.join(top, name, 'frames.jsonl')
            if not os.path.isfile(p):
                continue
            m = measure(rows(p))
            if not m or m.get('hit') is None:
                print(f'| {os.path.basename(top)} | {name} | no hit | | | | | | | | |')
                continue
            dist = m['dist'] / 100 if isinstance(m['dist'], (int, float)) else m['dist']
            print(f"| {os.path.basename(top)} | {name} | {m['hit']} | {m['percent']} | {m['v']} ({m['vh']}, {m['vz']}, {m['ang']} deg) | "
                  f"{m['z_hit']} | {m['vz_before']} | {m['peak'] / 100:.2f} | {m['flight_s']} | {dist} | {m['state']} |")
            if m.get('attr'):
                attrs[name] = m['attr']
    for k, v in attrs.items():
        print(f'\nDummy attributes ({k}): {json.dumps(v)}')
        break


if __name__ == '__main__':
    main(sys.argv[1:])
