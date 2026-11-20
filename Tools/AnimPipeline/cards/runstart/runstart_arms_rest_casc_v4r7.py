# Run start v4r7 (runstart-v7-desktop, 2026-11-20): v4r6 arms MORE AT REST (owner: lower, less high, less forward; feet perfect).
# Identity kept (rule 13a): frantic rev-up pump frequency and back swing, brute torso, same legs/feet/root (untouched).
# 1. Authored part (f0-29): per side and frame, the hand ELEVATION about the shoulder (torso fwd/up plane) is compressed above E0
#    (smooth): e' = e - (1-K)*softplus(e-E0); forward-high hands also pull in (reach * (1 - PULL*s)). The elbow re-solves by 2-bone
#    IK from the native bone lengths with the native elbow as pole hint; hand/forearm helper points follow the forearm frame.
# 2. Hand-off (owner: arms interpolate high->low into the GASP loop): from f24 the arm chain is blended toward the STOCK LOOP arm
#    pose at the same phase (frame f + CYCLE of the stock tail, same rhythm, checked f30<->f48.5, f32<->f50.5), weight 1 on f30-38,
#    back to native by f41 (native f40 already = tail f58). So at the MM exit (f32) the arms already are the loop arms (stock way:
#    the start ends on the loop pose, the blend has nothing to interpolate).
# Run in Cascadeur (run_python, exec) on RunStart_v4r7.casc (= copy of v4r6). Result = short dict.
import math
import numpy as np

SCENE_END = globals().get('SCENE_END', 'runstart/RunStart_v4r7.casc')
E0 = math.radians(float(globals().get('E0_DEG', -40.0)))
K = float(globals().get('K_OVERRIDE', 0.45))
SOFT = math.radians(8.0)
PULL = float(globals().get('PULL_OVERRIDE', 0.12))
CYCLE = float(globals().get('CYCLE_OVERRIDE', 18.5))
W_IN, W_FULL, W_HOLD, W_OUT = globals().get('W_OVERRIDE', (24, 30, 38, 41))
LAST = 58

ARM = {s: ['upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s, 'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s,
           'hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s, 'hand_DirectionPoint_' + s] for s in 'lr'}
LOW_AUX = ['lowerarm_AdditionalPoint_', 'lowerarm_LimbDir_']
HAND_AUX = ['hand_AdditionalPoint_', 'hand_DirectionPoint_']
BODY = ['spine_02_MainPoint', 'spine_04_MainPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r']


def ss(a, b, x):
    if x <= a: return 0.0
    if x >= b: return 1.0
    t = (x - a) / float(b - a); return t * t * (3 - 2 * t)


def W(f):
    return ss(W_IN, W_FULL, f) * (1.0 - ss(W_HOLD, W_OUT, f))


def softplus(x):
    return SOFT * math.log1p(math.exp(x / SOFT)) if x < 20 * SOFT else x


def nrm(v):
    return v / np.linalg.norm(v)


tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
ids = {}
for n in BODY + ARM['l'] + ARM['r']:
    o = mv.get_objects(n)[0]
    ids[n] = bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
src = {f: {n: np.array([float(c) for c in dv.get_data_value(i, f)], dtype=np.float64) for n, i in ids.items()}
       for f in range(0, LAST + 1)}


def basis(P):
    lat = P['thigh_MainPoint_l'] - P['thigh_MainPoint_r']; lat[1] = 0.0; lat = nrm(lat)
    up = P['spine_04_MainPoint'] - P['spine_02_MainPoint']; up = nrm(up - np.dot(up, lat) * lat)
    fwd = np.cross(lat, up)
    if fwd[0] * 0 + np.dot(fwd, P['spine_04_MainPoint'] - P['spine_02_MainPoint']) < -1e9: pass
    return np.vstack([fwd, up, lat])          # rows: local = B @ world


def local_arm(P, s):
    B = basis(P); S = P['upperarm_MainPoint_' + s]
    return {n: B @ (P[n] - S) for n in ARM[s]}


def ff(Lc, s):
    """forearm frame (columns) from local points"""
    E, H, S = Lc['lowerarm_MainPoint_' + s], Lc['hand_MainPoint_' + s], np.zeros(3)
    u = nrm(H - E); n = np.cross(E - S, H - E)
    n = nrm(n) if np.linalg.norm(n) > 1e-4 else nrm(np.cross(u, [0.0, 0.0, 1.0]))
    return np.column_stack([u, n, np.cross(u, n)])


def solve(Lc, s, Ht, hint, la, lb):
    d = float(np.linalg.norm(Ht)); d = min(max(d, abs(la - lb) + 0.5), la + lb - 0.3)
    Ht = nrm(Ht) * d; ax = nrm(Ht)
    x = (la * la - lb * lb + d * d) / (2 * d); h = math.sqrt(max(la * la - x * x, 0.0))
    p = hint - np.dot(hint, ax) * ax; p = nrm(p)
    return Ht, ax * x + p * h


def place(Lnat, s, Ht, Et, aux_local):
    """new local arm with helper points carried by the forearm frame; aux_local = helper offsets in forearm frame"""
    out = {'upperarm_MainPoint_' + s: np.zeros(3), 'lowerarm_MainPoint_' + s: Et, 'hand_MainPoint_' + s: Ht}
    F = ff(out, s)
    for p in LOW_AUX:
        out[p + s] = Et + F @ aux_local[p + s]
    for p in HAND_AUX:
        out[p + s] = Ht + F @ aux_local[p + s]
    return out


def aux_of(Lc, s):
    F = ff(Lc, s)
    a = {p + s: F.T @ (Lc[p + s] - Lc['lowerarm_MainPoint_' + s]) for p in LOW_AUX}
    a.update({p + s: F.T @ (Lc[p + s] - Lc['hand_MainPoint_' + s]) for p in HAND_AUX})
    return a


def loop_local(f, s):
    t = f + CYCLE
    if t > LAST: return None
    a, b = int(math.floor(t)), min(int(math.floor(t)) + 1, LAST); w = t - a
    La, Lb = local_arm(src[a], s), local_arm(src[b], s)
    return {n: (1 - w) * La[n] + w * Lb[n] for n in La}


out, rep = {}, {'peak_hup_before': {}, 'peak_hup_after': {}, 'max_bone_err_cm': 0.0}
for f in range(0, LAST + 1):
    w = W(f)
    comp = 1.0 - ss(W_FULL, W_FULL + 1, f)          # compression only lives in the authored part (gone where w = 1)
    if w <= 1e-6 and comp <= 1e-6: continue
    P = src[f]; Bm = basis(P)
    out[f] = {}
    for s in 'lr':
        Ln = local_arm(P, s)
        E0p, H0 = Ln['lowerarm_MainPoint_' + s], Ln['hand_MainPoint_' + s]
        la, lb = float(np.linalg.norm(E0p)), float(np.linalg.norm(H0 - E0p))
        aux_n = aux_of(Ln, s)
        # 1. compression (rotation of hand and elbow about the lateral axis through the shoulder + reach pull)
        e = math.atan2(H0[1], H0[0]); dist = float(np.linalg.norm(H0[:2]))
        de = -(1 - K) * softplus(e - E0) * comp
        sfront = ss(math.degrees(E0) + 10, math.degrees(E0) + 50, math.degrees(e)) if H0[0] > 0 else 0.0
        c, sn = math.cos(de), math.sin(de)
        R = np.array([[c, -sn, 0], [sn, c, 0], [0, 0, 1.0]])
        Hc = R @ H0; Hc[:2] *= (1 - PULL * sfront * comp)
        hint = R @ E0p
        Hc, Ec = solve(Ln, s, Hc, hint, la, lb)
        Lc = place(Ln, s, Hc, Ec, aux_n)
        # 2. blend toward the stock loop arm at the same phase
        Lt = loop_local(f, s) if w > 1e-6 else None
        if Lt is not None:
            aux_t = aux_of(Lt, s)
            Hb = (1 - w) * Lc['hand_MainPoint_' + s] + w * Lt['hand_MainPoint_' + s]
            hb = (1 - w) * Lc['lowerarm_MainPoint_' + s] + w * Lt['lowerarm_MainPoint_' + s]
            Hb, Eb = solve(Ln, s, Hb, hb, la, lb)
            aux_b = {}
            for k in aux_n:
                v = (1 - w) * aux_n[k] + w * aux_t[k]
                aux_b[k] = nrm(v) * float(np.linalg.norm(aux_n[k]))
            Lc = place(Ln, s, Hb, Eb, aux_b)
        rep['peak_hup_before'][s] = max(rep['peak_hup_before'].get(s, -99), round(float(H0[1]), 1)) if f < 39 else rep['peak_hup_before'].get(s, -99)
        rep['peak_hup_after'][s] = max(rep['peak_hup_after'].get(s, -99), round(float(Lc['hand_MainPoint_' + s][1]), 1)) if f < 39 else rep['peak_hup_after'].get(s, -99)
        S = P['upperarm_MainPoint_' + s]
        for n, v in Lc.items():
            if n == 'upperarm_MainPoint_' + s: continue
            out[f][n] = S + Bm.T @ v
        rep['max_bone_err_cm'] = max(rep['max_bone_err_cm'],
                                     abs(float(np.linalg.norm(Lc['lowerarm_MainPoint_' + s])) - la),
                                     abs(float(np.linalg.norm(Lc['hand_MainPoint_' + s] - Lc['lowerarm_MainPoint_' + s])) - lb))


def change(me, ue, su):
    de_ = me.data_editor()
    for f, pose in out.items():
        for n, v in pose.items():
            de_.set_data_value(ids[n], f, [float(c) for c in v])
        su.run_update(set(ids.values()), f)


ok = scene.modify_update('runstart v4r7 arms at rest + loop hand-off', change)
err = 0.0
for f, pose in out.items():
    for n, v in pose.items():
        err = max(err, float(np.linalg.norm(np.array([float(c) for c in dv.get_data_value(ids[n], f)]) - v)))
rep['max_bone_err_cm'] = round(rep['max_bone_err_cm'], 3)
result = {'ok': bool(ok), 'frames': len(out), 'max_set_err_cm': round(err, 3), **rep}
