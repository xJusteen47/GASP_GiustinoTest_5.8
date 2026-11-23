# Run start v4r6 (runstart-v6-desktop, 2026-11-20): v4r5 (locked, approved/runstart-v4r5) made MORE UPRIGHT, less sprint pose.
# Identity kept (rule 13a): frantic rev-up from frame 1, brute Overlord arm pump, no planted first step, continuous accel ramp.
# Change: the whole upper body above spine_02 (spine_04, neck, head, both arm chains) rotates BACK about spine_02_MainPoint by U(f)
# about the body's lateral axis; the head then nods forward by HEAD_K * U about neck_01 so the gaze stays ahead (net chin-up ~25 % of
# U = prouder brute). Legs, pelvis, feet, root: untouched (same contacts, same root travel). Arms move rigidly with the chest, so the
# pump arc relative to the torso is unchanged (re-run runstart_arm_clearance.py after).
# v4r5 spine_02->neck lean (v4r4_pts.json shifted 10): f0 33, f3-33 41-52 deg, f42-54 26-31 (= stock loop tail).
# U(f) = UP_MAX * ramp: f0 0.65 -> f3 1.0, hold to OUT0, smooth fade to 0 at OUT1 (tail stays the pure stock loop).
# Run in Cascadeur (run_python) on RunStart_v4r6.casc (= copy of v4r5). Result = short dict.
import math
import numpy as np

UP_MAX = float(globals().get('UP_MAX_OVERRIDE', 15.0))
HEAD_K = float(globals().get('HEAD_K_OVERRIDE', 0.75))
OUT0, OUT1 = globals().get('OUT_OVERRIDE', (29, 45))
SCENE_END = globals().get('SCENE_END', 'runstart/RunStart_v4r6.casc')

ARM = {s: ['clavicle_MainPoint_' + s, 'clavicle_AdditionalPoint_' + s, 'upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s,
           'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s, 'hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s,
           'hand_DirectionPoint_' + s] for s in 'lr'}
SPINE02_SUB = ['spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint', 'neck_01_MainPoint',
               'neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint'] + ARM['l'] + ARM['r']
NECK_SUB = ['neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']


def ss(a, b, x):
    if x <= a: return 0.0
    if x >= b: return 1.0
    t = (x - a) / float(b - a); return t * t * (3 - 2 * t)


def U(f):
    w_in = 0.65 + 0.35 * ss(0, 3, f)
    return UP_MAX * w_in * (1.0 - ss(OUT0, OUT1, f))


def axis_angle(k, d):
    k = np.asarray(k, dtype=np.float64); k = k / np.linalg.norm(k)
    s, c = math.sin(math.radians(d)), math.cos(math.radians(d))
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + s * K + (1 - c) * K @ K


tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
ids = {}
for o in mv.get_objects():
    n = mv.get_object_name(o)
    if n in SPINE02_SUB or n in ('spine_02_MainPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r'):
        ids[n] = bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
missing = [n for n in SPINE02_SUB + ['spine_02_MainPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r'] if n not in ids]
assert not missing, missing
LAST = int(globals().get('LAST_OVERRIDE', 58))
src = {f: {n: np.array([float(c) for c in dv.get_data_value(i, f)], dtype=np.float64) for n, i in ids.items()}
       for f in range(0, LAST + 1)}
lean_before, lean_after, out = {}, {}, {}
for f, P in src.items():
    D = {n: v.copy() for n, v in P.items()}
    u = U(f)
    lat = D['thigh_MainPoint_l'] - D['thigh_MainPoint_r']          # body lateral axis from the hips (follows any yaw)
    lat[1] = 0.0
    piv = D['spine_02_MainPoint']
    # sign: rotate so the neck moves back (toward -forward); pick the sign that reduces the lean, checked numerically
    fwd = np.cross(lat, [0.0, 1.0, 0.0]); fwd /= np.linalg.norm(fwd)
    v0 = D['neck_01_MainPoint'] - piv
    R = axis_angle(lat, u)
    if np.dot(R @ v0, fwd) > np.dot(v0, fwd):
        R = axis_angle(lat, -u)
    for n in SPINE02_SUB:
        D[n] = piv + R @ (D[n] - piv)
    # head nods forward again by HEAD_K * u about neck_01 (opposite sign of R)
    Rh = axis_angle(lat, HEAD_K * u)
    hv = D['head_MainPoint'] - D['neck_01_MainPoint']
    if np.dot(Rh @ hv, fwd) < np.dot(hv, fwd):
        Rh = axis_angle(lat, -HEAD_K * u)
    for n in NECK_SUB:
        D[n] = D['neck_01_MainPoint'] + Rh @ (D[n] - D['neck_01_MainPoint'])
    def lean(Q):
        v = Q['neck_01_MainPoint'] - Q['spine_02_MainPoint']
        return math.degrees(math.atan2(float(np.dot(v, fwd)), float(v[1])))
    lean_before[f], lean_after[f] = round(lean(P), 1), round(lean(D), 1)
    out[f] = {n: D[n] for n in SPINE02_SUB}


def change(me, ue, su):
    de = me.data_editor()
    for f, pose in out.items():
        for n, v in pose.items():
            de.set_data_value(ids[n], f, [float(c) for c in v])
        su.run_update(set(ids.values()), f)


ok = scene.modify_update('runstart v4r6 upright', change)
err = 0.0
for f, pose in out.items():
    for n, v in pose.items():
        err = max(err, float(np.linalg.norm(np.array([float(c) for c in dv.get_data_value(ids[n], f)]) - v)))
result = {'ok': bool(ok), 'frames': len(out), 'max_err_cm': round(err, 3),
          'lean_before': [lean_before[f] for f in range(0, LAST + 1, 6)],
          'lean_after': [lean_after[f] for f in range(0, LAST + 1, 6)]}
