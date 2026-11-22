# Run start v4r3 = pose round 3 (runstart-v3-pose-2, owner feedback on v4r2): copy of runstart_restyle_casc_v4r1.py with
#   - arms clear of the body: the fist passes the hip OUTSIDE the thigh (+14 cm out at the bottom of the arc, inward only in the top
#     half, -9 cm at the chin instead of -12), elbows wider (pole out 1.6 + 0.6 at the chin); arc, radius and timing unchanged (13e);
#   - spring stiffness follows the compose cadence (RATE_OVERRIDE) so the faster first steps keep the arm amplitude.
#   Check: runstart_arm_clearance.py (v4r2: -10.1 cm fist_r-thigh_r f20, 18 frames < 0).
# Was: Run start v4, pose round 1 (Cascadeur run_python) on RunStart_v4r1.casc (= v4base: runstart_compose_casc_v4.py, 69 f at 30 fps,
# f46-68 = 0.75 s stock run loop tail for the stock "almost complete" exit). Fixes of review_fable_r1.md (posa 1, 2):
#   - stomp f0-11 untouched (owner/Fable keep f87-124); every new layer ramps in f11-16 and out f42-48 (tail = pure stock loop);
#   - arms: brute pump, fist from behind the hip (out 26 cm) to the chin, elbows OUT (pole lateral-back), hand target through an
#     underdamped spring (1 f explicit lag + spring lag, overshoot ~15 %) so the fists arrive late and overshoot at each swap;
#   - legs: knee drive, swing thigh rotated forward about the hip (FK) up to 88 deg from vertical near the base swing peak, shin
#     gives back 60 % at the knee, toe down, then a fast drop to the base contact (stomping feet); stance feet untouched;
#   - torso: +18 deg lean (spine_02 subtree) over the base lean, head counter -14 deg (gaze ahead, head low), shoulders hunched
#     (arm chain +3 up, +2 fwd, +2 out), shoulder counter-rotation +-15 deg about the torso axis, in phase with the thighs;
#   - pelvis: extra -4 cm dip on each contact (sigma 1 f, centred contact + 0.5).
# Gate (declared): body joints <= 30 deg vs base except legs (knee drive, <= 75) and arms (pump vs the mocap swing, <= 135); frame jump <= 90 deg.
import math
import numpy as np

F0, F1 = 0, 68
R_IN = (11, 16)
# r1: (42, 48) = release in 0.2 s, Fable r2 FAIL (snap f166-182). r2: (39, 59) = peak ~1.1 deg per 60 fps frame for a 30 deg release, f0-39 unchanged
R_OUT = tuple(globals().get('R_OUT_OVERRIDE', (39, 59)))
SCENE_END = globals().get('SCENE_END', 'runstart/RunStart_v4r3.casc')


def ss(a, b, x):
    if x <= a: return 0.0
    if x >= b: return 1.0
    t = (x - a) / float(b - a); return t * t * (3 - 2 * t)


def ramp(f):
    return ss(R_IN[0], R_IN[1], f) * (1.0 - ss(R_OUT[0], R_OUT[1], f))


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
def axis_angle(k, d):
    k = unit(k); s, c = math.sin(math.radians(d)), math.cos(math.radians(d))
    Km = [[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]]; KK = mm(Km, Km)
    return [[(1 if i == j else 0) + s * Km[i][j] + (1 - c) * KK[i][j] for j in range(3)] for i in range(3)]
def rot_between(a, b):
    a, b = unit(a), unit(b); v = cross(a, b); s = norm(v)
    if s < 1e-9: return [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    return axis_angle(v, math.degrees(math.atan2(s, dot(a, b))))
def Rx(d): return axis_angle([1, 0, 0], d)

PELVIS_UP = ['pelvis_MainPoint', 'pelvis_AdditionalPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r',
             'spine_02_MainPoint', 'spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint',
             'neck_01_MainPoint', 'neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']
ARM = {s: ['clavicle_MainPoint_' + s, 'clavicle_AdditionalPoint_' + s, 'upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s,
           'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s, 'hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s,
           'hand_DirectionPoint_' + s] for s in 'lr'}
SPINE02_SUB = ['spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint', 'neck_01_MainPoint',
               'neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint'] + ARM['l'] + ARM['r']
SPINE04_SUB = SPINE02_SUB[2:]
NECK_SUB = ['neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']
UPPERARM_SUB = {s: ARM[s][3:] for s in 'lr'}
FORE_SUB = {s: ['hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s, 'hand_DirectionPoint_' + s] for s in 'lr'}
CALF = {s: ['calf_MainPoint_' + s, 'calf_AdditionalPoint_' + s, 'calf_LimbDir_' + s] for s in 'lr'}
FOOT = {s: ['foot_MainPoint_' + s, 'foot_Self0Point_' + s, 'ball_MainPoint_' + s, 'ball_DirectionPoint_' + s,
            'ball_AdditionalPoint_' + s] for s in 'lr'}
ALL = PELVIS_UP + ARM['l'] + ARM['r'] + CALF['l'] + CALF['r'] + FOOT['l'] + FOOT['r']

LEAN = 18.0; NOD = -14.0; YAW = 15.0; DIP = 4.0
HUNCH = (2.0, 3.0, 2.0)          # out, up, fwd (cm)
TOE = 25.0                      # toe-down pitch at the drive (deg)
SPR_W, SPR_Z, LAG = 0.9, 0.55, 1.0
RATE_ENV = globals().get('RATE_OVERRIDE', [(8, 0.6), (11, 0.6), (14, 1.45), (34, 1.45), (42, 1.0), (45, 1.0)])
LAT_OUT, LAT_IN = 14.0, -9.0      # fist lateral offset from the shoulder line at the bottom / at the chin (v4r1: 6 / -12)
POLE_OUT = (1.6, 0.6)            # elbow pole outward: base + extra at the chin (v4r1: 1.0, 0.8)
THIGH_GOAL = 88.0              # thigh from vertical at the swing peak (horizontal)
SHIN_K = 0.6                    # share of the thigh rotation the shin gives back at the knee


def env(keys, f):
    if f <= keys[0][0]: return keys[0][1]
    if f >= keys[-1][0]: return keys[-1][1]
    for (a, va), (b, vb) in zip(keys, keys[1:]):
        if a <= f <= b:
            t = (f - a) / float(b - a); t = t * t * (3 - 2 * t)
            return va + (vb - va) * t


def rotate(D, names, piv, R):
    for n in names:
        D[n] = v_add(piv, mv3(R, v_sub(D[n], piv)))


def thigh_ang(P, s):
    h, k = P['thigh_MainPoint_' + s], P['calf_MainPoint_' + s]
    return math.degrees(math.atan2(k[2] - h[2], h[1] - k[1]))


def two_bone(S, T, a, b, pole):
    d = min(norm(v_sub(T, S)), a + b - 0.5); u = unit(v_sub(T, S))
    bend = unit(v_sub(pole, v_mul(u, dot(pole, u))))
    x = (a * a - b * b + d * d) / (2 * d); h = math.sqrt(max(0.0, a * a - x * x))
    return v_add(v_add(S, v_mul(u, x)), v_mul(bend, h)), v_add(S, v_mul(u, d))


def frame_cols(d, b):
    x = unit(d); y = unit(v_sub(b, v_mul(x, dot(b, x)))); return x, y, cross(x, y)


def frame_rot_n(ds_, ns_, dt_, nt_):
    def fr(d, n):
        x = unit(d); z = unit(v_sub(n, v_mul(x, dot(n, x)))); return x, cross(z, x), z
    S_, T_ = fr(ds_, ns_), fr(dt_, nt_)
    return [[sum(T_[k][i] * S_[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def frame_rot(ds_, bs_, dt_, bt_):
    S_, T_ = frame_cols(ds_, bs_), frame_cols(dt_, bt_)
    return [[sum(T_[k][i] * S_[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def bend_of(S, E, H):
    u = unit(v_sub(H, S)); v = v_sub(E, S); return unit(v_sub(v, v_mul(u, dot(v, u))))


tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
def pid(name):
    o = mv.get_objects(name)[0]
    return bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
ids = {n: pid(n) for n in ALL}
frames = list(range(F0, F1 + 1))
orig = {f: {n: [float(c) for c in dv.get_data_value(ids[n], f)] for n in ALL} for f in frames}
O0 = orig[20]
LEG = {s: (norm(v_sub(O0['calf_MainPoint_' + s], O0['thigh_MainPoint_' + s])),
           norm(v_sub(O0['foot_MainPoint_' + s], O0['calf_MainPoint_' + s]))) for s in 'lr'}
ARML = {s: (norm(v_sub(O0['lowerarm_MainPoint_' + s], O0['upperarm_MainPoint_' + s])),
            norm(v_sub(O0['hand_MainPoint_' + s], O0['lowerarm_MainPoint_' + s]))) for s in 'lr'}
FLOOR = min(orig[f]['foot_MainPoint_' + s][1] for f in frames for s in 'lr')

# --- contacts and swing windows (base feet) -----------------------------------------------------------------------------
planted = {s: [orig[f]['foot_MainPoint_' + s][1] < FLOOR + 3.0 for f in frames] for s in 'lr'}
swings = {s: [] for s in 'lr'}; contacts = []
for s in 'lr':
    f = 0
    while f < len(frames):
        if not planted[s][f]:
            a = f
            while f < len(frames) and not planted[s][f]: f += 1
            swings[s].append((a - 1, f))          # last planted frame .. touchdown frame
            if f < len(frames): contacts.append(f)
        else:
            f += 1
contacts = sorted(c for c in contacts if c >= 12)


def swing_of(s, f):
    for a, b in swings[s]:
        if a < f < b and b < len(frames):
            return a, b, max(thigh_ang(orig[k], s) for k in range(a, b + 1))
    return None


# --- arm phase: spring on the opposite-thigh phase ------------------------------------------------------------------------
PH_AMP = max(abs(thigh_ang(orig[f], 'r') - thigh_ang(orig[f], 'l')) for f in range(14, 46))


def arm_target(s, f):
    ph = (thigh_ang(orig[f], 'r') - thigh_ang(orig[f], 'l')) / PH_AMP    # + = right thigh forward (smooth, no clamp: no square wave)
    return 0.5 * (1.0 + (ph if s == 'l' else -ph))                        # 1 = this fist at the chin


def lagged(s, t):
    t = max(F0, min(F1, t)); i = int(math.floor(t)); w = t - i
    return arm_target(s, i) * (1 - w) + arm_target(s, min(F1, i + 1)) * w


ARMW = {}
for s in 'lr':
    x, v = lagged(s, F0 - LAG), 0.0; n = 10; dt = 1.0 / n
    for f in frames:
        for k in range(n):
            tgt = lagged(s, f - LAG + k * dt)
            wn = SPR_W * max(1.0, env(RATE_ENV, f) / 1.45)                # stiffer spring when the steps are faster than v4r2's
            acc = wn * wn * (tgt - x) - 2 * SPR_Z * wn * v
            v += acc * dt; x += v * dt
        ARMW[(s, f)] = max(-0.15, min(1.15, x))
    raw = [ARMW[(s, f)] for f in frames]                                  # 1-2-1 smoothing: keeps the overshoot, trims the snap peak
    for i, f in enumerate(frames):
        ARMW[(s, f)] = 0.25 * raw[max(0, i - 1)] + 0.5 * raw[i] + 0.25 * raw[min(len(raw) - 1, i + 1)]


def pose(O, f):
    g = ramp(f)
    D = {n: list(v) for n, v in O.items()}
    if g <= 0.0:
        return D
    # pelvis dip on contact
    dy = -DIP * sum(math.exp(-((f - (c + 0.5)) / 1.0) ** 2) for c in contacts) * g
    for n in PELVIS_UP + ARM['l'] + ARM['r']:
        D[n] = v_add(D[n], [0.0, dy, 0.0])
    # lean, head counter, hunch, shoulder counter-rotation
    rotate(D, SPINE02_SUB, list(D['spine_02_MainPoint']), Rx(LEAN * g))
    rotate(D, NECK_SUB, list(D['neck_01_MainPoint']), Rx(NOD * g))
    up = unit(v_sub(D['neck_01_MainPoint'], D['spine_02_MainPoint']))
    ph = max(-1.0, min(1.0, (thigh_ang(O, 'l') - thigh_ang(O, 'r')) / 40.0))   # + = left thigh fwd -> right shoulder fwd
    rotate(D, SPINE04_SUB, list(D['spine_02_MainPoint']), axis_angle(up, YAW * ph * g))
    xh = unit(v_sub([1.0, 0.0, 0.0], v_mul(up, up[0]))); fw = cross(xh, up)
    for s, sx in (('l', 1.0), ('r', -1.0)):
        off = v_add(v_add(v_mul(xh, sx * HUNCH[0]), v_mul(up, HUNCH[1])), v_mul(fw, HUNCH[2]))
        for n in ARM[s][1:]:
            D[n] = v_add(D[n], v_mul(off, g))
    # arms: hand target hip <-> chin through the spring, elbows out
    for s, sx in (('l', 1.0), ('r', -1.0)):
        w = ARMW[(s, f)]
        # hand on an ARC about the shoulder in the torso's sagittal plane (a straight hip->chin line passes through the
        # shoulder in this lean and breaks the IK): back -22 deg from hanging (= behind the hip in this lean, 44 cm), chin +140 deg (24 cm, folded)
        we = max(-0.05, min(1.1, (w - 0.1) / 0.8))                                           # phase peaks reach ~0.9: stretch so the extremes are hit
        th = math.radians(-22.0 + 162.0 * we)                           # angles from the torso's hanging axis (lean included)
        rr = 44.0 + (24.0 - 44.0) * max(0.0, min(1.0, we))
        dirn = v_add(v_mul(up, -math.cos(th)), v_mul(fw, math.sin(th)))
        T = v_add(v_add(D['upperarm_MainPoint_' + s], v_mul(dirn, rr)), v_mul(xh, sx * (LAT_OUT + (LAT_IN - LAT_OUT) * ss(0.45, 1.0, we))))
        S, E0, H0 = D['upperarm_MainPoint_' + s], D['lowerarm_MainPoint_' + s], D['hand_MainPoint_' + s]
        T = lerp(H0, T, g)
        wc = max(0.0, min(1.0, w))
        pole_new = unit(v_add(v_add(v_mul(xh, sx * (POLE_OUT[0] + POLE_OUT[1] * wc)), v_mul(fw, -1.2 * (1.0 - wc) - 0.2)), v_mul(up, -0.6 * wc)))   # elbow out; back on the back swing, down at the chin (no chicken wing)
        pole = unit(lerp(bend_of(S, E0, H0), pole_new, g))
        E, H = two_bone(S, T, ARML[s][0], ARML[s][1], pole)
        # rotations from frames built on the ARM PLANE NORMAL (shoulder->hand x elbow bend), never a shortest-arc rot_between
        # or a projected pole: the normal is defined for straight and folded arms alike, so the roll cannot flip
        ut = unit(v_sub(H, S)); bt = unit(v_sub(pole, v_mul(ut, dot(pole, ut)))); nt = cross(ut, bt)
        uh = unit(v_sub(H0, S)); bs = v_sub(v_sub(E0, S), v_mul(uh, dot(v_sub(E0, S), uh)))
        bs = unit(bs) if norm(bs) > 3.0 else unit(v_sub(v_mul(xh, sx), v_mul(uh, dot(v_mul(xh, sx), uh))))
        ns_ = cross(uh, bs)
        rotate(D, UPPERARM_SUB[s], S, frame_rot_n(v_sub(E0, S), ns_, v_sub(E, S), nt))
        E1, H1 = D['lowerarm_MainPoint_' + s], D['hand_MainPoint_' + s]
        rotate(D, FORE_SUB[s], E1, frame_rot_n(v_sub(H1, E1), nt, v_sub(H, E1), nt))
        # elbow-plane point: re-placed along the new bend direction (it only rode with the upper arm), so the rig's forearm roll
        # follows the elbow and the wrist keeps its base bend relative to the forearm
        Ld = norm(v_sub(O['lowerarm_LimbDir_' + s], O['lowerarm_MainPoint_' + s]))
        D['lowerarm_LimbDir_' + s] = v_add(E1, v_mul(bt, Ld))
    # legs: knee drive on the swing leg as FK about the hip (no IK flip when the ankle passes the hip), stance by IK
    for s in 'lr':
        Hp = D['thigh_MainPoint_' + s]; H0p = O['thigh_MainPoint_' + s]
        sw = swing_of(s, f)
        if sw is not None:
            a, b, peak = sw; u = (f - a) / float(b - a)
            wd = ss(0.3, 0.7, u) * (1.0 - ss(0.75, 0.98, u)) * g
            dth = max(0.0, THIGH_GOAL - peak) * wd
            hd = v_sub(Hp, H0p)
            for n in CALF[s] + FOOT[s]:
                D[n] = v_add(O[n], hd)
            rotate(D, CALF[s] + FOOT[s], Hp, Rx(-dth))                     # thigh forward (Rx(-) lifts a down vector forward)
            Kn = list(D['calf_MainPoint_' + s])
            rotate(D, ['calf_LimbDir_' + s] + FOOT[s], Kn, Rx(SHIN_K * dth))  # shin hangs back under the knee
            An = D['foot_MainPoint_' + s]
            rotate(D, FOOT[s][1:], An, Rx(TOE * wd))                       # toe down
            continue
        A0 = O['foot_MainPoint_' + s]; A = list(A0); wd = 0.0
        K0 = O['calf_MainPoint_' + s]
        bend0 = bend_of(H0p, K0, A0)
        pole = unit(lerp(bend0, [0.0, 0.25, 1.0], wd))
        K, _ = two_bone(Hp, A, LEG[s][0], LEG[s][1], pole)
        Rt = rot_between(v_sub(K0, H0p), v_sub(K, Hp))
        D['calf_AdditionalPoint_' + s] = v_add(Hp, mv3(Rt, v_sub(O['calf_AdditionalPoint_' + s], H0p)))
        Rk = rot_between(v_sub(A0, K0), v_sub(A, K))
        D['calf_LimbDir_' + s] = v_add(K, mv3(Rk, v_sub(O['calf_LimbDir_' + s], K0)))
        D['calf_MainPoint_' + s] = K
    return D


target = {f: pose(orig[f], f) for f in frames}

GATE_JOINTS = ['pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01', 'head', 'clavicle_l', 'clavicle_r',
               'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r', 'hand_l', 'hand_r', 'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
               'foot_l', 'foot_r']
ARM_J = {'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r', 'hand_l', 'hand_r'}
LEG_J = {'thigh_l', 'thigh_r', 'calf_l', 'calf_r', 'foot_l', 'foot_r'}
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
rot0 = {f: {j: quat(dv.get_data_value(i, f)) for j, i in rid.items()} for f in frames}

def change(me, ue, su):
    de = me.data_editor()
    for f in frames:
        for n in ALL:
            de.set_data_value(ids[n], f, np.array(target[f][n], dtype=np.float32))
        su.run_update(set(ids.values()), f)

ok = scene.modify_update('runstart v4r3 pose round 3', change)
err = 0.0; drift = 0.0; err_top = []
for f in frames:
    for n in ALL:
        v = [float(c) for c in dv.get_data_value(ids[n], f)]
        e = norm(v_sub(v, target[f][n])); err = max(err, e)
        if e > 5.0: err_top.append((round(e, 1), n, f))
    if f <= 11:
        for n in ALL:
            drift = max(drift, norm(v_sub([float(c) for c in dv.get_data_value(ids[n], f)], orig[f][n])))
rot1 = {f: {j: quat(dv.get_data_value(i, f)) for j, i in rid.items()} for f in frames}
top = {}; jump = (0.0, '', 0)
for f in frames:
    for j in rid:
        top[j] = max(top.get(j, (0, 0)), (round(qang(rot0[f][j], rot1[f][j]), 1), f))
        if f > frames[0]:
            jump = max(jump, (round(qang(rot1[f - 1][j], rot1[f][j]), 1), j, f))
worst = sorted(((v[0], j, v[1]) for j, v in top.items()), reverse=True)
body = [w for w in worst if w[1] not in ARM_J | LEG_J][:3]
legs = [w for w in worst if w[1] in LEG_J][:3]
arms = [w for w in worst if w[1] in ARM_J][:3]
gate_ok = body[0][0] <= 30.0 and legs[0][0] <= 75.0 and arms[0][0] <= 135.0 and jump[0] <= 90.0
result = {'ok': bool(ok), 'frames': len(frames), 'contacts': contacts, 'stomp_drift_f0_11_cm': round(drift, 3),
          'max_target_err_cm': round(err, 2), 'body': body, 'legs': legs, 'arms': arms, 'jump': jump,
          'GATE': 'OK' if gate_ok else 'FAIL', 'err_top': sorted(err_top, reverse=True)[:8]}
