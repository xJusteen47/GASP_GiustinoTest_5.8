# Bug pass C1 check in Play: a slow launch into a wall (the bump), then, once the victim is free (no Tumble / Hitstun / Knockdown / Bump tag for
# FREE_FRAMES frames), the host walks with the stick held. Before the C1 fix the bump's 0.25 drag lived ~240 move frames, so the walk was slow
# for a few seconds; after it the walk speed is normal at once. Reuses pie_wall_bounce.py (weak run only, reaction set NOT edited: DA_ReactionAnims
# is the owner's), no images. Output: Saved/C1Bump/<date_time>/weak/frames.jsonl (+ 'walk' and 'hspeed' fields), c1_summary.json.
# Boot: vd2_mover first (owner rule), then this file; Run-Timed.ps1 -Preset Capture.
import json
import math
import os
import runpy
import time

_here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(_here, 'vd2_mover.py'), run_name='vd2_mover')
except Exception as e:  # noqa: BLE001
    print('vd2_mover failed:', e)

import unreal  # noqa: E402

os.environ['WALL_SKIP_EDIT'] = '1'
os.environ['WALL_ONLY_WEAK'] = '1'
_saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.environ['PIE_CAPTURE_OUT'] = os.path.join(_saved, 'C1Bump', time.strftime('%Y%m%d_%H%M%S'))
ns = {'__file__': os.path.join(_here, 'pie_wall_bounce.py'), '__name__': 'pie_wall_bounce'}
exec(open(ns['__file__']).read(), ns)

FREE_FRAMES = 6
WALK_FRAMES = 150
ns['POST'] = 520
ns['_shoot'] = lambda run, frame: None
_orig_row = ns['_row']
_orig_summary = ns['_summary']
c1 = {'free': 0, 'walk_from': None, 'hit_seen': False, 'stick': None}
LOCK = ('Tumble', 'Hitstun', 'Knockdown', 'Bump', 'Splat', 'Hitstop')


def _row(run, frame, tap):
    _orig_row(run, frame, tap)
    r = run['rows'][-1]
    v = r['vel']
    r['hspeed'] = round(math.hypot(v[0], v[1]))
    locked = any(t in r['tags'] for t in LOCK)
    if locked:
        c1['hit_seen'] = True
    if c1['hit_seen'] and c1['walk_from'] is None:
        c1['free'] = 0 if locked else c1['free'] + 1
        if c1['free'] >= FREE_FRAMES:
            # Away from the wall: the stick is camera relative, so try the world direction and log what it gives
            fx, fy = ns['st']['wall']['fwd']
            c1['stick'] = (-fx, -fy, 0.0)
            c1['walk_from'] = frame
            ns['g'].hold(0, 'move', c1['stick'])
    if c1['walk_from'] is not None:
        r['walk'] = frame - c1['walk_from']
        if r['walk'] >= WALK_FRAMES:
            ns['g'].release(0, 'move')
            ns['st']['frame'] = ns['PRE'] + ns['POST']   # end the run on the next tick


def _summary(run):
    s = _orig_summary(run)
    walk = [r for r in run['rows'] if 'walk' in r]
    sp = [r['hspeed'] for r in walk]
    out = {'walk_from_frame': c1['walk_from'], 'stick': c1['stick'], 'walk_rows': len(walk),
           'speed_at': {k: sp[k] for k in (5, 15, 30, 60, 90, 120, 145) if k < len(sp)},
           'steady_max': max(sp) if sp else None, 'frames_to_90pct': next((i for i, x in enumerate(sp) if sp and x >= 0.9 * max(sp)), None),
           'tags_during_walk': sorted({t for r in walk for t in r['tags']}), 'bump': s.get('bump_frames')}
    with open(os.path.join(ns['OUT'], 'c1_summary.json'), 'w') as f:
        json.dump(out, f, indent=1)
    ns['_log'](f'C1 {json.dumps(out)}')
    return s


ns['_row'] = _row
ns['_summary'] = _summary
