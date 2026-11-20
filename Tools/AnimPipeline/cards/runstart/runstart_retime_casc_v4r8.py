# Run start v4r8 (runstart-v8, 2026-11-20): owner "measure Luigi's run start and shorten ours to match, let's see".
# Reference IMG_9588.mov (Mario 3D World, Luigi, phone 30 fps): rev-up (first move -> first normal stride) = 6 f = 0.20 s
# (straight start, t 10.13-10.33) and 8 f = 0.27 s (start with a ~120 deg turn, t 1.33-1.60). Target 0.23 s (14 sim frames).
# v4r7 (approved, LOCKED, not touched): frantic cadence R3 L6.5 R11.5 L16.5 R21.5 L28.5 (step every ~5 f), normal cadence from
# L28.5 (R37.5 L46.5 R56, every 9 f) = rev-up 0.95 s; in game the clip hands off at t 1.28-1.42.
# What this does (identity kept: frantic pumps from frame 1, same poses, same arms, same loop-matched tail f>=REV_END bit-identical):
#  1. CUT one whole frantic stride (CUT0 -> CUT1, same foot contact, L6 -> L16 by default), short crossfade in root-relative space.
#     Fewer pumps (R L R L instead of R L R L R L) so the faster rev-up does not alias at 30 fps.
#  2. WARP the remaining rev-up (REV_END - (CUT1-CUT0) source frames) into N_REV = round(TARGET_S*30) frames. RATE_SHAPE 'decay':
#     play rate starts high and falls linearly to 1.0 at REV_END (engine revs highest at frame 1, no cadence jump into the run);
#     'const': uniform rate (cadence jump at the hand-off).
#  3. ROOT re-profiled: forward speed = linear ramp F0*V_END -> V_END over the rev-up (same shape as the DA Run Stages ramp:
#     RampStartFraction F0, AccelRampFrames = N_REV at 62.5 Hz), V_END = source root speed at REV_END. Without this the warped root
#     would run 2-4x faster than the loop.
#  4. STRIDE scaled (offline stride warping): leg points' horizontal offset from their hip scaled by s = v_new / v_warped
#     (clamped S_MIN..1, smoothed), so planted feet stay planted while the root covers less ground: short scrambling steps.
# Run in Cascadeur (run_python, exec) on RunStart_v4r8.casc (= copy of RunStart_v4r7.casc). Globals override any constant.
# After it: delete keys from N_NEW to the old end (58), scene size N_NEW (as v4r5: keys 59-68 deleted), export_joints up Z.
# Result = short dict (N_NEW, EXIT_AT for runstart_v4r8_ue.py, speeds, s range).
import math
import csc
import numpy as np

G = globals()
SCENE_END = G.get('SCENE_END', 'runstart/RunStart_v4r8.casc')
TARGET_S = float(G.get('TARGET_S', 0.23))          # Luigi: 0.20 straight / 0.27 with turn. v4r8b: 0.46 (2x Luigi)
FPS = 30.0
LAST = int(G.get('LAST', 58))                     # v4r7 last frame (59 f)
REV_END = int(G.get('REV_END', 28))               # first normal-cadence contact (L28.5): tail from here is copied 1:1
CUT0, CUT1 = G.get('CUT', (6, 16))                # same-foot contacts (L6.5 / L16.5); (0, 0) = no cut
XF = float(G.get('XF', 1.5))                      # crossfade half-width at the cut, source frames
RATE_SHAPE = G.get('RATE_SHAPE', 'decay')
F0 = float(G.get('F0', 0.15))                     # DA Stages[0] RampStartFraction (setup_run_start_ramp.py value)
S_MIN = float(G.get('S_MIN', 0.3))
UP = int(G.get('UP_AXIS', 1))                     # Cascadeur Y-up; asserted below on the root
EXIT_SRC = float(G.get('EXIT_SRC', 32.0))         # v4r7 exit_at 1.0667 s = f32
LEG_PREFIX = ('calf_', 'foot_', 'ball_', 'ik_foot_l', 'ik_foot_r')

tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()

pts, boxes = {}, {}
for o in mv.get_objects():
    n = mv.get_object_name(o)
    if not n or n.startswith('EdgeView') or 'constraint' in n or n == 'Points':
        continue
    try:
        tr = bv.get_behaviour_by_name(o, 'Transform')
    except Exception:
        continue
    try:
        if 'Point' in n or 'LimbDir' in n or n in ('root', 'ik_foot_root', 'ik_hand_root'):
            pts[n] = bv.get_behaviour_data(tr, 'global_position')
        elif '_Box' in n or 'offset box' in n:
            boxes[n] = (bv.get_behaviour_data(tr, 'local_rotation'), bv.get_behaviour_data(tr, 'local_position'))
    except Exception:
        pass
assert 'root' in pts, 'root point not found'

def vec(i, f):                                       # Rotation -> [w, x, y, z]; vectors as is
    v = dv.get_data_value(i, f)
    if hasattr(v, 'to_quaternion'):
        q = v.to_quaternion(); return np.array([q.w(), q.x(), q.y(), q.z()], dtype=np.float64)
    return np.array([float(c) for c in v], dtype=np.float64)
SP = {n: np.stack([vec(i, f) for f in range(LAST + 1)]) for n, i in pts.items()}
SB = {n: (np.stack([vec(r, f) for f in range(LAST + 1)]), np.stack([vec(p, f) for f in range(LAST + 1)]))
      for n, (r, p) in boxes.items()}
root = SP['root']
travel = root[LAST] - root[0]
assert abs(travel[UP]) < 5.0, ('up axis?', travel.tolist())
H = [a for a in range(3) if a != UP]
fwd = np.zeros(3); fwd[H] = travel[H]; fwd /= np.linalg.norm(fwd)
hz = lambda v: v - v[..., UP:UP + 1] * np.eye(3)[UP]          # horizontal part


def samp(arr, t):                                    # linear sample at fractional source frame
    t = min(max(t, 0.0), float(LAST)); a = int(math.floor(t)); b = min(a + 1, LAST); w = t - a
    return arr[a] * (1 - w) + arr[b] * w


def qsamp(arr, t):                                   # quaternion-safe sample (nlerp) when 4 components
    if arr.shape[1] != 4:
        return samp(arr, t)
    t = min(max(t, 0.0), float(LAST)); a = int(math.floor(t)); b = min(a + 1, LAST); w = t - a
    qa, qb = arr[a], arr[b] * (1 if np.dot(arr[a], arr[b]) >= 0 else -1)
    q = qa * (1 - w) + qb * w
    return q / np.linalg.norm(q)


def mix(a, b, w, quat=False):
    if quat and a.shape[0] == 4:
        b = b if np.dot(a, b) >= 0 else -b
        q = a * (1 - w) + b * w; return q / np.linalg.norm(q)
    return a * (1 - w) + b * w


def ss(x):
    x = min(max(x, 0.0), 1.0); return x * x * (3 - 2 * x)


GAP = float(CUT1 - CUT0)
L = REV_END - GAP                                    # compressed source length of the rev-up
N_REV = max(2, int(round(TARGET_S * FPS)))
r0 = (2.0 * L / N_REV - 1.0) if RATE_SHAPE == 'decay' else L / N_REV
r0 = max(r0, 1.0)


def c_of_u(u):                                       # compressed source time at output frame u (u <= N_REV)
    if RATE_SHAPE == 'decay':
        return r0 * u + (1.0 - r0) * u * u / (2.0 * N_REV)
    return L * u / N_REV


def rate(u):
    return (r0 + (1.0 - r0) * u / N_REV) if RATE_SHAPE == 'decay' else L / N_REV


def pose_at_c(c):
    """Root-relative points + boxes at compressed source time c, crossfaded across the cut."""
    tA = c; tB = c + GAP
    if GAP <= 0 or c <= CUT0 - XF:
        ws = [(tA, 1.0)]
    elif c >= CUT0 + XF:
        ws = [(tB, 1.0)]
    else:
        w = ss((c - (CUT0 - XF)) / (2 * XF)); ws = [(tA, 1 - w), (tB, w)]
    P, B = {}, {}
    for t, w in ws:
        rt = samp(root, t)
        for n, a in SP.items():
            v = samp(a, t) - hz(rt)
            P[n] = v if n not in P else mix(P[n], v, w)
        for n, (ra, pa) in SB.items():
            rv, pv = qsamp(ra, t), samp(pa, t)
            B[n] = (rv, pv) if n not in B else (mix(B[n][0], rv, w, True), mix(B[n][1], pv, w))
    return P, B


def src_t(c):
    return c if c <= CUT0 else c + GAP


def v_src(t):                                        # source forward root speed, cm/s
    return float(np.dot(samp(root, min(t + 1, LAST)) - samp(root, max(t - 1, 0)), fwd)) * FPS / 2.0


V_END = v_src(REV_END)
N_NEW = N_REV + (LAST - REV_END) + 1

# root distance along the rev-up (linear speed ramp F0*V_END -> V_END), then the source tail 1:1
dist = [0.0]
for u in range(1, N_REV + 1):
    va = V_END * (F0 + (1 - F0) * (u - 1) / N_REV); vb = V_END * (F0 + (1 - F0) * u / N_REV)
    dist.append(dist[-1] + (va + vb) / 2.0 / FPS)
s_raw = []
for u in range(N_REV + 1):
    vw = rate(u) * max(v_src(src_t(c_of_u(u))), 1.0)
    s_raw.append(min(1.0, max(S_MIN, V_END * (F0 + (1 - F0) * u / N_REV) / vw)))
s_raw[-1] = 1.0
s_sm = [s_raw[0]] + [(s_raw[i - 1] + s_raw[i] + s_raw[i + 1]) / 3.0 for i in range(1, N_REV)] + [1.0]

out = {}
for u in range(N_NEW):
    if u <= N_REV:
        P, B = pose_at_c(c_of_u(u))
        rnew = hz(root[0]) + fwd * dist[u]
        s = s_sm[u]
    else:
        t = REV_END + (u - N_REV)
        rt = root[t]
        P = {n: a[t] - hz(rt) for n, a in SP.items()}
        B = {n: (ra[t], pa[t]) for n, (ra, pa) in SB.items()}
        rnew = hz(root[0]) + fwd * dist[N_REV] + hz(rt - root[REV_END])
        s = 1.0
    W = {}
    for n, v in P.items():
        if s < 1.0 and n.startswith(LEG_PREFIX):
            side = n[-1]
            hip = P.get('thigh_MainPoint_' + side)
            if hip is not None:
                v = hip + hz(v - hip) * s + (v - hip - hz(v - hip))
        W[n] = v + rnew
    out[u] = (W, B)


def change(me, ue, su):
    de = me.data_editor()
    ids = set(pts.values()) | {x for r_p in boxes.values() for x in r_p}
    for u, (W, B) in out.items():
        for n, v in W.items():
            de.set_data_value(pts[n], u, v.tolist())
        for n, (rv, pv) in B.items():
            de.set_data_value(boxes[n][0], u, csc.math.Rotation.from_quaternion(*[float(c) for c in rv]) if rv.shape[0] == 4 else rv.tolist())
            de.set_data_value(boxes[n][1], u, pv.tolist())
        su.run_update(ids, u)


ok = scene.modify_update('runstart v4r8 retime (Luigi %.2f s)' % TARGET_S, change)
exit_at = (N_REV + (EXIT_SRC - REV_END)) / FPS
result = {'ok': bool(ok), 'points': len(pts), 'boxes': len(boxes), 'N_REV': N_REV, 'N_NEW': N_NEW,
          'EXIT_AT': round(exit_at, 4), 'rate0': round(r0, 2), 'V_END_cms': round(V_END, 1),
          'rev_dist_cm': round(dist[N_REV], 1), 's_min_used': round(min(s_sm), 2),
          'delete_keys': [N_NEW, LAST]}
