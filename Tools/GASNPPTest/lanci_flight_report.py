"""LANCI-1A2A (desktop 2026-11-24): flight numbers from the frames.jsonl + lanci_cam.jsonl of lanci_measure_boot.py (plain Python).
python Tools/GASNPPTest/lanci_flight_report.py <seq dir> [more seq dirs...]  -> one markdown row per sequence.
hit = first frame the dummy's action state leaves 0 or its percent rises. From the hit: rise (peak z - z at the hit), horizontal distance at the
first floor contact after the peak (and the farthest point), flight time to that contact, the largest downward speed just before it (the ground
impact), the frames the dummy carries an action state != 0 (hitstun + tumble, the balloon check) and the share of frames it is inside the host's
viewport (lanci_cam.jsonl), with the host camera's largest distance from the host (the launch zoom)."""
import json
import math
import os
import sys

FPS = 60.0


def load(p):
    with open(p, encoding='utf-8') as f:
        return [json.loads(x) for x in f if x.strip()]


def report(d):
    rows = load(os.path.join(d, 'frames.jsonl'))
    cam = {r['f']: r for r in load(os.path.join(d, 'lanci_cam.jsonl'))} if os.path.exists(os.path.join(d, 'lanci_cam.jsonl')) else {}
    dm = [(r['f'], r.get('dummy') or {}) for r in rows if (r.get('dummy') or {}).get('loc')]
    floor = min(x['loc'][2] for _, x in dm)
    p0 = None
    for i, (f, x) in enumerate(dm):
        s = x.get('snap') or {}
        prev = (dm[i - 1][1].get('snap') or {}) if i else {}
        if i and (s.get('actionState', 0) != 0 or (s.get('damagePercent') or 0) > (prev.get('damagePercent') or 0) + 0.01):
            p0 = i
            break
    if p0 is None:
        return {'seq': d, 'hit': None}
    fh, xh = dm[p0]
    after = dm[p0:]
    peak_i = max(range(len(after)), key=lambda k: after[k][1]['loc'][2])
    land = next((k for k in range(peak_i, len(after)) if after[k][1]['loc'][2] <= floor + 3.0 and k > 0), None)
    h = lambda a, b: math.hypot(a[0] - b[0], a[1] - b[1])  # noqa: E731
    far = max(h(x['loc'], xh['loc']) for _, x in after)
    vz_imp = None
    if land is not None:
        vz_imp = min(((x.get('vel') or (0, 0, 0))[2] for _, x in after[max(0, land - 4):land + 1]), default=None)
    states = [(f, (x.get('snap') or {}).get('actionState', 0)) for f, x in after]
    busy = sum(1 for _, s in states if s != 0)
    seen = [cam[f]['on_screen'] for f, _ in after if f in cam]
    camd = [cam[f]['cam_pawn_cm'] for f, _ in dm if f in cam]
    hv = max((math.hypot(*(x.get('vel') or (0, 0, 0))[:2]) for _, x in after[:20]), default=0)
    vzmax = max(((x.get('vel') or (0, 0, 0))[2] for _, x in after[:20]), default=0)
    bounces = sum(1 for k in range(1, len(after)) if (after[k - 1][1].get('vel') or (0, 0, 0))[2] < -200 and (after[k][1].get('vel') or (0, 0, 0))[2] > 50)
    return {
        'seq': os.path.basename(os.path.dirname(d)) + '/' + os.path.basename(d), 'hit': fh,
        'pct': round((xh.get('snap') or {}).get('damagePercent') or 0, 1), 'z_hit': round(xh['loc'][2] - floor),
        'vz_up': round(vzmax), 'vh': round(hv), 'rise_m': round((after[peak_i][1]['loc'][2] - xh['loc'][2]) / 100, 2),
        'dist_land_m': round(h(after[land][1]['loc'], xh['loc']) / 100, 2) if land is not None else None, 'far_m': round(far / 100, 2),
        'flight_s': round((after[land][0] - fh) / FPS, 2) if land is not None else None, 'vz_impact': vz_imp, 'bounces': bounces,
        'state_frames': busy, 'states_seen': sorted({s for _, s in states}),
        'on_screen': f'{sum(seen)}/{len(seen)}' if seen else None, 'cam_dist_cm': [min(camd), max(camd)] if camd else None,
    }


def main(dirs):
    keys = ['seq', 'hit', 'pct', 'z_hit', 'vz_up', 'vh', 'rise_m', 'dist_land_m', 'far_m', 'flight_s', 'vz_impact', 'bounces', 'state_frames',
            'states_seen', 'on_screen', 'cam_dist_cm']
    print('| ' + ' | '.join(keys) + ' |')
    print('|' + '---|' * len(keys))
    for d in dirs:
        r = report(d)
        print('| ' + ' | '.join(str(r.get(k, '')) for k in keys) + ' |')


if __name__ == '__main__':
    main(sys.argv[1:])
