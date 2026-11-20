# Run start v4 (pose round 1), stage 1: same as runstart_compose_casc_v3.py with a 0.75 s stock-loop tail (T_END 68). Was: Run start v3, stage 1 (Cascadeur run_python): build the BASE timeline of the new run start from two real clips, no typed body poses.
#   f0-8   AS_GSTN_RunStart_Frantic (ClipCheck export, 30 fps) with ONE stomp: left foot pinned (its mini-stamp f3-5 removed), right
#          foot = the stomp leg (knee lift + forward commit step, keys RF below), pelvis group carried forward/up (PRE_DZ/PRE_DY).
#   f8-45  stock GASP run loop (M_Run_Loop export GASP_RunLoop.fbx, 76 f cycle, 500 cm/s) time-warped: squash hold (rate 0.6),
#          frantic cadence (rate 1.45) with stride shortened (K 0.72, feet only, no slide: travel = K * rate * stock travel),
#          then rate/K back to 1.0 by f42 so the clip ends ON the stock loop phase (src ~56.7/75, left foot contact) for the hand-off.
#   f8-13  cross-fade Frantic hold -> run (body pelvis-relative, feet in world, knees two-bone IK).
# Stage 2 (runstart_restyle_casc_v3.py) adds the rigid-group style on top (squash, chest/head/arms). Scene: current tab must be
# Saved/AnimPipeline/runstart/RunStart_v4base.casc (copy of RunStart_v0_gaspZ = Frantic imported up Z); tab RunLoop_v0_gaspZ.casc open.
# World: Y up, Z forward, X = character left. Prints nothing; result = short dict.
import math
import numpy as np

T_END = int(globals().get("T_END_OVERRIDE", 68))
STOMP = 8
SRC0 = 10.0          # run loop frame of the right-foot contact (fr y 10.5, 20 cm ahead of the pelvis)
CYC, CYC_DZ = 75, 1250.0

def env(keys, f):
    if f <= keys[0][0]: return keys[0][1]
    if f >= keys[-1][0]: return keys[-1][1]
    for (a, va), (b, vb) in zip(keys, keys[1:]):
        if a <= f <= b:
            t = (f - a) / float(b - a); t = t * t * (3 - 2 * t)
            return va + (vb - va) * t

RATE = [(8, 0.6), (11, 0.6), (14, 1.45), (34, 1.45), (42, 1.0), (45, 1.0)]
K = [(8, 0.72), (34, 0.72), (42, 1.0), (45, 1.0)]
WIDE = 4.0           # cm each foot outward in the run (heavier, wider Ganon track)
PRE_DZ = [(0, 0), (1, 0.5), (2, 1.5), (3, 3.5), (4, 6), (5, 9), (6, 12), (7, 15), (8, 17)]
PRE_DY_TARGET = [88.3, 86.5, 84.5, 84.5, 85.5, 86.0, 85.0, 82.0, 78.5]   # pelvis height: small dip, rise on the lift, fall into the plant
# right foot (ankle) world path x, y, z and pitch (deg, + = toe down) for the stomp
RF = {0: (-14.3, 9.7, -20.4, 0), 1: (-14.3, 9.7, -20.4, 0), 2: (-14.0, 13.7, -19.0, 15), 3: (-12.5, 22.0, -12.0, 25),
      4: (-11.0, 36.0, -3.0, 20), 5: (-10.0, 46.0, 7.0, 10), 6: (-9.0, 47.0, 15.0, 0), 7: (-8.5, 30.0, 24.0, -5),
      8: (-8.0, 9.7, 28.0, 0)}

def v_add(a, b): return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
def v_sub(a, b): return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
def v_mul(a, k): return [a[0] * k, a[1] * k, a[2] * k]
def lerp(a, b, w): return [a[i] + (b[i] - a[i]) * w for i in range(3)]
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def norm(a): return math.sqrt(dot(a, a))
def unit(a):
    n = norm(a); return v_mul(a, 1.0 / n) if n > 1e-9 else [0.0, 1.0, 0.0]
def mv3(M, v): return [dot(M[0], v), dot(M[1], v), dot(M[2], v)]
def mm(A, B): return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def Rx(d):
    c, s = math.cos(math.radians(d)), math.sin(math.radians(d)); return [[1, 0, 0], [0, c, -s], [0, s, c]]
def rot_between(a, b):
    a, b = unit(a), unit(b); v = cross(a, b); c = dot(a, b); s = norm(v)
    if s < 1e-9: return [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    k = v_mul(v, 1.0 / s); Km = [[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]]
    KK = mm(Km, Km)
    return [[(1 if i == j else 0) + s * Km[i][j] + (1 - c) * KK[i][j] for j in range(3)] for i in range(3)]

BODY = ['pelvis_MainPoint', 'pelvis_AdditionalPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r',
        'spine_02_MainPoint', 'spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint',
        'neck_01_MainPoint', 'neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']
ARM = {s: ['clavicle_MainPoint_' + s, 'clavicle_AdditionalPoint_' + s, 'upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s,
           'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s, 'hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s,
           'hand_DirectionPoint_' + s] for s in 'lr'}
UPPER = BODY + ARM['l'] + ARM['r']
CALF = {s: ['calf_MainPoint_' + s, 'calf_AdditionalPoint_' + s, 'calf_LimbDir_' + s] for s in 'lr'}
FOOT = {s: ['foot_MainPoint_' + s, 'foot_Self0Point_' + s, 'ball_MainPoint_' + s, 'ball_DirectionPoint_' + s,
            'ball_AdditionalPoint_' + s] for s in 'lr'}
ALL = UPPER + CALF['l'] + CALF['r'] + FOOT['l'] + FOOT['r']


def leg_ik(D, O, s, pole=None):
    """Knee by two-bone IK from D's hip and ankle; bend plane from O (or an explicit pole); calf side points ride along."""
    H0, K0, A0 = O['thigh_MainPoint_' + s], O['calf_MainPoint_' + s], O['foot_MainPoint_' + s]
    H, A = D['thigh_MainPoint_' + s], D['foot_MainPoint_' + s]
    a, b = LEN[s]
    d = min(norm(v_sub(A, H)), a + b - 0.05)
    u = unit(v_sub(A, H))
    bend = pole if pole else v_sub(v_sub(K0, H0), v_mul(unit(v_sub(A0, H0)), dot(v_sub(K0, H0), unit(v_sub(A0, H0)))))
    bend = unit(v_sub(bend, v_mul(u, dot(bend, u))))
    x = (a * a - b * b + d * d) / (2 * d); h = math.sqrt(max(0.0, a * a - x * x))
    Kn = v_add(v_add(H, v_mul(u, x)), v_mul(bend, h))
    Rt = rot_between(v_sub(K0, H0), v_sub(Kn, H))
    D['calf_AdditionalPoint_' + s] = v_add(H, mv3(Rt, v_sub(O['calf_AdditionalPoint_' + s], H0)))
    D['calf_LimbDir_' + s] = v_add(Kn, mv3(Rt, v_sub(O['calf_LimbDir_' + s], K0)))
    D['calf_MainPoint_' + s] = Kn
    return D


sm = app.get_scene_manager()
def tab_of(end):
    return [t for t in sm.scenes() if str(t.get_path_name()).replace("\\", "/").endswith(end)][0]
dst = sm.current_scene()
assert str(dst.get_path_name()).replace("\\", "/").endswith("runstart/RunStart_v4base.casc"), dst.get_path_name()
src_run = tab_of("runstart/RunLoop_v0_gaspZ.casc")

def reader(tab):
    ds = tab.domain_scene(); mv = ds.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
    ids = {n: bv.get_behaviour_data(bv.get_behaviour_by_name(mv.get_objects(n)[0], 'Transform'), 'global_position') for n in ALL}
    return ds, mv, bv, dv, ids

ds, mv, bv, dv, ids = reader(dst)
_, _, _, rdv, rids = reader(src_run)
FR = {f: {n: [float(c) for c in dv.get_data_value(ids[n], f)] for n in ALL} for f in range(0, 27)}
RLraw = {f: {n: [float(c) for c in rdv.get_data_value(rids[n], f)] for n in ALL} for f in range(0, CYC + 1)}
LEN = {s: (norm(v_sub(FR[0]['calf_MainPoint_' + s], FR[0]['thigh_MainPoint_' + s])),
           norm(v_sub(FR[0]['foot_MainPoint_' + s], FR[0]['calf_MainPoint_' + s]))) for s in 'lr'}

def RL(s):
    n_c = math.floor(s / CYC); s0 = s - n_c * CYC
    i = int(math.floor(s0)); w = s0 - i
    A, B = RLraw[i], RLraw[min(i + 1, CYC)]
    return {n: v_add(lerp(A[n], B[n], w), [0.0, 0.0, CYC_DZ * n_c]) for n in ALL}

# --- stomp segment f0-8 --------------------------------------------------------------------------------------------
def stomp_pose(t):
    O = FR[t]; D = {n: list(v) for n, v in O.items()}
    dpos = [0.0, PRE_DY_TARGET[t] - O['pelvis_MainPoint'][1], env(PRE_DZ, t)]
    for n in UPPER: D[n] = v_add(D[n], dpos)
    for n in FOOT['l']: D[n] = list(FR[0][n])                         # left foot planted (support), no mini-stamp
    x, y, z, p = RF[t]; A0 = FR[0]['foot_MainPoint_r']; R = Rx(p)
    for n in FOOT['r']:
        D[n] = v_add([x, y, z], mv3(R, v_sub(FR[0][n], A0)))
    leg_ik(D, O, 'l')
    leg_ik(D, O, 'r', pole=None if t < 2 else [0.0, 0.35, 1.0])      # knee forward and up during the lift
    return D

STOMPP = {t: stomp_pose(t) for t in range(0, STOMP + 1)}

# --- run segment f8-45 -----------------------------------------------------------------------------------------------
S = {STOMP: SRC0}
for t in range(STOMP, T_END):
    S[t + 1] = S[t] + 0.5 * (env(RATE, t) + env(RATE, t + 1))
plant_r = STOMPP[STOMP]['foot_MainPoint_r']

def run_raw(t):
    O = RL(S[t]); k = env(K, t); pel = O['pelvis_MainPoint']
    D = {n: list(v) for n, v in O.items()}
    for s, sx in (('l', 1.0), ('r', -1.0)):
        dz = (k - 1.0) * (O['foot_MainPoint_' + s][2] - pel[2])
        for n in FOOT[s]: D[n] = v_add(D[n], [sx * WIDE, 0.0, dz])
    return O, D

# world travel: pelvis z advances by K * stock pelvis travel; aligned so the right foot sits on the stomp plant at f8
Zt = {}
O8, D8 = run_raw(STOMP)
Zt[STOMP] = 0.0
for t in range(STOMP, T_END):
    Zt[t + 1] = Zt[t] + 0.5 * (env(K, t) + env(K, t + 1)) * (RL(S[t + 1])['pelvis_MainPoint'][2] - RL(S[t])['pelvis_MainPoint'][2])
off0 = v_sub(plant_r, D8['foot_MainPoint_r'])                       # x/z offset of the run at f8 (y kept: same floor)
off0[1] = 0.0

def run_pose(t):
    O, D = run_raw(t)
    shift = [off0[0], 0.0, off0[2] + Zt[t] - (O['pelvis_MainPoint'][2] - O8['pelvis_MainPoint'][2])]
    D = {n: v_add(v, shift) for n, v in D.items()}
    Os = {n: v_add(v, shift) for n, v in O.items()}
    leg_ik(D, Os, 'l'); leg_ik(D, Os, 'r')
    return D, Os

target = {t: STOMPP[t] for t in range(0, STOMP)}
XF0, XF1 = STOMP, 13
hold = STOMPP[STOMP]
for t in range(STOMP, T_END + 1):
    Rn, Ro = run_pose(t)
    w = 0.0 if t <= XF0 else (1.0 if t >= XF1 else env([(XF0, 0.0), (XF1, 1.0)], t))
    if w >= 1.0:
        target[t] = Rn; continue
    pr, ph = Rn['pelvis_MainPoint'], hold['pelvis_MainPoint']
    pel = lerp(ph, pr, w)
    D = {}
    for n in UPPER:
        D[n] = v_add(pel, lerp(v_sub(hold[n], ph), v_sub(Rn[n], pr), w))
    for s in 'lr':
        ws = w if s == 'l' else min(1.0, w * 1.0)
        for n in FOOT[s]: D[n] = lerp(hold[n], Rn[n], ws)
    for s in 'lr':
        for n in CALF[s]: D[n] = lerp(hold[n], Rn[n], w)
    Ob = {n: list(v) for n, v in D.items()}
    leg_ik(D, Ob, 'l'); leg_ik(D, Ob, 'r')
    target[t] = D

FINGERS = [mv.get_object_name(o) for o in mv.get_objects() if mv.get_object_type_name(o) == 'Joint' and
           any(k in mv.get_object_name(o) for k in ('thumb', 'index', 'middle', 'ring', 'pinky'))]
fid = {n: bv.get_behaviour_data(bv.get_behaviour_by_name(mv.get_objects(n)[0], 'Transform'), 'local_rotation') for n in FINGERS}
fing26 = {n: dv.get_data_value(fid[n], 26) for n in FINGERS}
lv = ds.layers_viewer()

def trim(me, ue, su):
    le = me.layers_editor()
    for lid in lv.all_layer_ids():
        for f in sorted(lv.layer(lid).key_frame_indices()):
            if f > T_END:
                le.unset_section(f, lid)

def change(me, ue, su):
    de = me.data_editor()
    for f in range(0, T_END + 1):
        for n in ALL:
            de.set_data_value(ids[n], f, np.array(target[f][n], dtype=np.float32))
        if f > 26:
            for n in FINGERS:
                de.set_data_value(fid[n], f, fing26[n])
        su.run_update(set(ids.values()), f)

ok1 = ds.modify_update('runstart v3 trim', trim)
ok2 = ds.modify_update('runstart v3 compose', change)
err = (0.0, '', 0)
for f in range(0, T_END + 1):
    for n in ALL:
        err = max(err, (norm(v_sub([float(c) for c in dv.get_data_value(ids[n], f)], target[f][n])), n, f))
err = (round(err[0], 2), err[1], err[2])
result = {'ok': [bool(ok1), bool(ok2)], 'size': dv.get_animation_size(), 'src_end': round(S[T_END], 2),
          'travel_cm': round(target[T_END]['pelvis_MainPoint'][2] - target[0]['pelvis_MainPoint'][2], 1),
          'max_target_err': err, 'fingers': len(FINGERS)}
