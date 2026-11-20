"""Summary of one camera_jitter_run.py output dir in <= 5 lines (full table in <dir>/summary.txt).

usage: python Tools/GASNPPTest/camera_jitter_summary.py <dir>
Per phase (stand / walk / stand2 / jump, from py_rows.csv): fps from dt, duplicate-view frames, and zig-zag kicks (> 2 cm against both
neighbours, as camera_trace_check.py) for each stage: C++ trace root_z, stand_z, damped_z, cam_comp_z, in_z (GASP rig output), out_z (final
view); Python rows root_z, mesh_z, gcc_z, pov_z. The first stage with kicks while standing is the culprit.
"""
import csv, os, sys, statistics

d = sys.argv[1]


def load(p):
    return list(csv.DictReader(open(p))) if os.path.exists(p) else []


def kicks(v, tol=2.0):
    return sum(1 for i in range(1, len(v) - 1) if (v[i] - v[i-1]) * (v[i+1] - v[i]) < 0 and min(abs(v[i] - v[i-1]), abs(v[i+1] - v[i])) > tol)


def amp(v):
    """median |second difference| of the kicking frames: how big a bounce is"""
    s = [abs(v[i+1] - 2 * v[i] + v[i-1]) / 2 for i in range(1, len(v) - 1)
         if (v[i] - v[i-1]) * (v[i+1] - v[i]) < 0 and min(abs(v[i] - v[i-1]), abs(v[i+1] - v[i])) > 2.0]
    return statistics.median(s) if s else 0.0


py = load(os.path.join(d, 'py_rows.csv'))
tr = load(os.path.join(d, 'trace.csv'))
phase_of = {r['frame']: r['phase'] for r in py}
out = []
for ph in ('stand', 'walk', 'stand2', 'jump'):
    p = [r for r in py if r['phase'] == ph]
    t = [r for r in tr if phase_of.get(r['frame']) == ph]
    if not p:
        continue
    dt = [float(r['dt']) for r in p]
    pov = [(float(r['pov_x']), float(r['pov_y']), float(r['pov_z'])) for r in p]
    dup = sum(1 for a, b in zip(pov, pov[1:]) if a == b)
    parts = [f'{ph}: {len(p)} f, fps {1 / statistics.median(dt):.0f} (dt max {max(dt) * 1000:.0f} ms), same-view {dup}']
    for k in ('root_z', 'stand_z', 'damped_z', 'cam_comp_z', 'in_z', 'out_z'):
        if t and k in t[0]:
            v = [float(r[k]) for r in t]
            parts.append(f'tr.{k} {kicks(v)}/{amp(v):.0f}cm')
    for k in ('root_z', 'mesh_z', 'gcc_z', 'pov_z'):
        v = [float(r[k]) for r in p]
        parts.append(f'py.{k} {kicks(v)}/{amp(v):.0f}cm')
    if t:
        parts.append('flips sweep {} anchor {} air {}'.format(*[sum(1 for a, b in zip(t, t[1:]) if a[c] != b[c]) for c in ('sweep_hit', 'anchor_on', 'in_air')]))
    out.append('  '.join(parts))
open(os.path.join(d, 'summary.txt'), 'w').write('\n'.join(out) + '\n')
print(f'{os.path.basename(d)}: py rows {len(py)}, trace rows {len(tr)} (kicks/median size)')
for line in out[:4]:
    print(line[:400])
