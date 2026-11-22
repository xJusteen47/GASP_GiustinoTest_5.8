# Restyle v3 of a GASP sprint stop in Cascadeur (run_python, scene = the untouched v0_gaspZ import).
# Why v3: v2 TRANSLATED single MainPoints of chest/head/arms; their Additional/LimbDir points stayed put, the box side vector
# flipped and the rig rolled head/spine 70-180 deg (docs/notes/handovers/loco-runstop-diagnosis.md). v3 moves only WHOLE
# rigid groups: the pelvis box (and everything above it) is translated, and every upper-body change is the rotation of a
# bone = all points of that bone's subtree turned together about the bone's own MainPoint (schema of ganon_ftilt_v3_motion.py
# body(), non-LEGACY). Legs: hips follow the pelvis, ankles/balls are pinned, knees are solved as two-bone IK in the original
# bend plane. Same key timing as v2 (P_DY/C_DZ/H_DZ/A_*), converted to degrees.
# World: Y up, Z forward (travel), X = character left. Globals in: SCALE (amplitude), FRAMES (optional), MIRROR (unused).
import math

SCALE = globals().get('SCALE', 1.0)
F0, F1 = 28, 68

def env(keys, f):
    if f <= keys[0][0]: return keys[0][1]
    if f >= keys[-1][0]: return keys[-1][1]
    for (a, va), (b, vb) in zip(keys, keys[1:]):
        if a <= f <= b:
            t = (f - a) / float(b - a); t = t * t * (3 - 2 * t)
            return va + (vb - va) * t

# pelvis translation (cm): heavy settle into the brake, small forward carry
P_DY = [(28, 0), (34, -2), (41, -7.5), (46, -5.5), (52, 1.0), (58, -0.3), (63, 0)]
P_DZ = [(33, 0), (40, 1.5), (48, -0.5), (56, 0)]
# chest lean (deg, + = forward) = rotation of the spine_02 bone about spine_02 (v2 C_DZ 10 cm on a ~45 cm lever ~ 13 deg)
LEAN = [(31, 0), (39, 13), (45, 6.5), (51, -4), (57, 1), (62, 0)]
# head nod on top of the chest (deg, + = forward) = rotation of neck_01 about neck_01: lags the chest, follow-through
NOD = [(32, 0), (37, -3), (42, 7), (48, 2), (54, -4), (60, 1), (65, 0)]
# shoulder swing (deg): forward swing and abduction of the whole arm about upperarm
ARM_FWD = [(33, 0), (43, 10), (48, 6), (54, -5), (60, 1), (65, 0)]
ARM_OUT = [(34, 0), (42, 7), (52, 4), (62, 0)]


def v_add(a, b): return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
def v_sub(a, b): return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
def v_mul(a, k): return [a[0] * k, a[1] * k, a[2] * k]
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def norm(a): return math.sqrt(dot(a, a))
def unit(a):
    n = norm(a); return v_mul(a, 1.0 / n) if n > 1e-9 else [0.0, 1.0, 0.0]
def mv3(M, v): return [dot(M[0], v), dot(M[1], v), dot(M[2], v)]
def mm(A, B): return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def Rx(d):
    c, s = math.cos(math.radians(d)), math.sin(math.radians(d)); return [[1, 0, 0], [0, c, -s], [0, s, c]]
def Rz(d):
    c, s = math.cos(math.radians(d)), math.sin(math.radians(d)); return [[c, -s, 0], [s, c, 0], [0, 0, 1]]
def rot_between(a, b):
    a, b = unit(a), unit(b); v = cross(a, b); c = dot(a, b); s = norm(v)
    if s < 1e-9: return [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    k = v_mul(v, 1.0 / s); K = [[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]]
    KK = mm(K, K)
    return [[(1 if i == j else 0) + s * K[i][j] + (1 - c) * KK[i][j] for j in range(3)] for i in range(3)]

PELVIS_UP = ['pelvis_MainPoint', 'pelvis_AdditionalPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r',
             'spine_02_MainPoint', 'spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint',
             'neck_01_MainPoint', 'neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']
ARM = {s: ['clavicle_MainPoint_' + s, 'clavicle_AdditionalPoint_' + s, 'upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s,
           'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s, 'hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s,
           'hand_DirectionPoint_' + s] for s in 'lr'}
SPINE02_SUB = ['spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint', 'neck_01_MainPoint',
               'neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint'] + ARM['l'] + ARM['r']
NECK_SUB = ['neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']
UPPERARM_SUB = {s: ARM[s][3:] for s in 'lr'}
LEG = {s: ['calf_MainPoint_' + s, 'calf_AdditionalPoint_' + s, 'calf_LimbDir_' + s] for s in 'lr'}
PIN = ['foot_MainPoint_l', 'foot_MainPoint_r', 'foot_Self0Point_l', 'foot_Self0Point_r', 'ball_MainPoint_l', 'ball_MainPoint_r',
       'ball_DirectionPoint_l', 'ball_DirectionPoint_r', 'ball_AdditionalPoint_l', 'ball_AdditionalPoint_r']
ALL = PELVIS_UP + ARM['l'] + ARM['r'] + LEG['l'] + LEG['r'] + PIN


def rotate(D, names, piv, R):
    for n in names:
        D[n] = v_add(piv, mv3(R, v_sub(D[n], piv)))


def pose(O, f):
    """New positions of every point at frame f from the original O (dict name -> [x,y,z])."""
    D = {n: list(v) for n, v in O.items()}
    p = [0.0, env(P_DY, f) * SCALE, env(P_DZ, f) * SCALE]
    for n in PELVIS_UP + ARM['l'] + ARM['r']:
        D[n] = v_add(D[n], p)
    # chest: rotation of the spine_02 bone (its subtree) about spine_02
    rotate(D, SPINE02_SUB, list(D['spine_02_MainPoint']), Rx(env(LEAN, f) * SCALE))
    # head: rotation of the neck_01 bone about neck_01
    rotate(D, NECK_SUB, list(D['neck_01_MainPoint']), Rx(env(NOD, f) * SCALE))
    # arms: rotation of the upperarm bone about the shoulder (forward swing = hand toward +Z, out = away from the body)
    for s, sx in (('l', 1.0), ('r', -1.0)):
        R = mm(Rz(sx * env(ARM_OUT, f) * SCALE), Rx(-env(ARM_FWD, f) * SCALE))
        rotate(D, UPPERARM_SUB[s], list(D['upperarm_MainPoint_' + s]), R)
    # legs: hip moved, ankle pinned -> knee by two-bone IK in the original bend plane; the thigh/calf side points ride along
    for s in 'lr':
        H0, K0, A = O['thigh_MainPoint_' + s], O['calf_MainPoint_' + s], O['foot_MainPoint_' + s]
        H = D['thigh_MainPoint_' + s]
        a, b = norm(v_sub(K0, H0)), norm(v_sub(A, K0))
        d = min(norm(v_sub(A, H)), a + b - 1e-3)
        u = unit(v_sub(A, H))
        bend = v_sub(v_sub(K0, H0), v_mul(unit(v_sub(A, H0)), dot(v_sub(K0, H0), unit(v_sub(A, H0)))))
        bend = unit(v_sub(bend, v_mul(u, dot(bend, u))))
        x = (a * a - b * b + d * d) / (2 * d); h = math.sqrt(max(0.0, a * a - x * x))
        K = v_add(v_add(H, v_mul(u, x)), v_mul(bend, h))
        Rt = rot_between(v_sub(K0, H0), v_sub(K, H))
        D['calf_AdditionalPoint_' + s] = v_add(H, mv3(Rt, v_sub(O['calf_AdditionalPoint_' + s], H0)))
        D['calf_LimbDir_' + s] = v_add(K, mv3(Rt, v_sub(O['calf_LimbDir_' + s], K0)))
        D['calf_MainPoint_' + s] = K
    return D


mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
def pid(name):
    o = mv.get_objects(name)[0]
    return bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
ids = {n: pid(n) for n in ALL}
frames = globals().get('FRAMES', list(range(F0, F1 + 1)))
orig = {f: {n: [float(c) for c in dv.get_data_value(ids[n], f)] for n in ALL} for f in frames}
target = {f: pose(orig[f], f) for f in frames}

def change(me, ue, su):
    de = me.data_editor()
    for f in frames:
        for n in ALL:
            de.set_data_value(ids[n], f, csc.math.Vec3(*target[f][n]) if hasattr(csc, 'math') and hasattr(csc.math, 'Vec3') else target[f][n])
        su.run_update(set(ids.values()), f)

GATE_JOINTS = ['pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01', 'head', 'clavicle_l', 'clavicle_r',
               'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r', 'hand_l', 'hand_r', 'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
               'foot_l', 'foot_r']
rid = {}
for j in GATE_JOINTS:
    ob = mv.get_objects(j)
    if ob: rid[j] = bv.get_behaviour_data(bv.get_behaviour_by_name(ob[0], 'Transform'), 'global_rotation')
def quat(rot):
    q = rot.to_quaternion()
    g = lambda a: float(getattr(q, a)() if callable(getattr(q, a)) else getattr(q, a))
    return [g('x'), g('y'), g('z'), g('w')] if hasattr(q, 'x') else [float(c) for c in q]
def qang(a, b):
    return math.degrees(2 * math.acos(min(1.0, abs(sum(x * y for x, y in zip(a, b))))))
all_frames = list(range(max(0, F0 - 2), F1 + 3))
rot0 = {f: {j: quat(dv.get_data_value(i, f)) for j, i in rid.items()} for f in all_frames}

def change(me, ue, su):
    de = me.data_editor()
    for f in frames:
        for n in ALL:
            de.set_data_value(ids[n], f, np.array(target[f][n], dtype=np.float32))
        su.run_update(set(ids.values()), f)

import numpy as np
ok = scene.modify_update('restyle_v3', change)
err = 0.0; drift = 0.0
for f in frames:
    for n in ALL:
        v = [float(c) for c in dv.get_data_value(ids[n], f)]
        e = norm(v_sub(v, target[f][n])); err = max(err, e)
        if n in PIN: drift = max(drift, norm(v_sub(v, orig[f][n])))
# pre-UE gate: no joint > ~25 deg off the base, no per-frame jump > 90 deg (rule from loco-runstop-diagnosis)
rot1 = {f: {j: quat(dv.get_data_value(i, f)) for j, i in rid.items()} for f in all_frames}
top = {}
jump = 0.0
for f in all_frames:
    for j in rid:
        top[j] = max(top.get(j, (0, 0)), (round(qang(rot0[f][j], rot1[f][j]), 1), f))
        if f > all_frames[0]:
            jump = max(jump, qang(rot1[f - 1][j], rot1[f][j]))
worst = sorted(((v[0], j, v[1]) for j, v in top.items()), reverse=True)[:6]
result = {'ok': bool(ok), 'frames': len(frames), 'max_pin_drift_cm': round(drift, 3), 'max_target_err_cm': round(err, 3),
          'top_delta_deg': worst, 'max_frame_jump_deg': round(jump, 1),
          'GATE': 'OK' if worst[0][0] <= 25.0 and jump <= 90.0 else 'FAIL'}
