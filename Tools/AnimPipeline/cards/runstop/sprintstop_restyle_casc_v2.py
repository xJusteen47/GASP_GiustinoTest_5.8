# Restyle v1 del GASP M_Relaxed_Sprint_Stop_F_Lfoot (Cascadeur, run_python). Solo corpo: piedi/palle ri-pinnati all'originale.
# Offsets in cm, mondo Cascadeur (Y su, Z = avanti/viaggio, X = sinistra). Chiavi (frame 30 fps, valore), interpolazione smooth.
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

P_DY = [(28, 0), (34, -2), (41, -7.5), (46, -5.5), (52, 1.0), (58, -0.3), (63, 0)]
P_DZ = [(33, 0), (40, 1.5), (48, -0.5), (56, 0)]
C_DZ = [(31, 0), (39, 10), (45, 5), (51, -3), (57, 0.8), (62, 0)]
C_DY = [(31, 0), (38, -2), (44, -1), (50, 0.5), (56, 0)]
H_DZ = [(32, 0), (41, 13), (47, 5), (53, -3.5), (59, 0.8), (64, 0)]
H_DY = [(32, 0), (40, -3), (46, -1), (52, 0.5), (58, 0)]
A_DZ = [(33, 0), (43, 11), (48, 7), (54, -4), (60, 1), (65, 0)]
A_DY = [(33, 0), (43, 1), (48, 0), (54, -2), (60, 0)]
A_DX = [(34, 0), (42, 6), (52, 4), (62, 0)]

def offsets(f):
    p = (0.0, env(P_DY, f), env(P_DZ, f))
    c = (0.0, env(C_DY, f), env(C_DZ, f))
    h = (0.0, env(H_DY, f), env(H_DZ, f))
    a = (env(A_DX, f), env(A_DY, f), env(A_DZ, f))
    add = lambda *v: [sum(x[i] for x in v) * SCALE for i in range(3)]
    half = lambda v, k: tuple(x * k for x in v)
    o = {}
    o['pelvis_MainPoint'] = add(p)
    o['spine_02_MainPoint'] = add(p, half(c, 0.5))
    for n in ('spine_04_MainPoint', 'clavicle_MainPoint_l', 'clavicle_MainPoint_r', 'upperarm_MainPoint_l', 'upperarm_MainPoint_r'):
        o[n] = add(p, c)
    o['neck_01_MainPoint'] = add(p, half(c, 0.5), half(h, 0.5))
    o['head_MainPoint'] = add(p, h)
    o['head_DirectionPoint'] = add(p, h)
    for side, sx in (('l', 1), ('r', -1)):
        aa = (a[0] * sx, a[1], a[2])
        o['lowerarm_MainPoint_' + side] = add(p, c, half(aa, 0.5))
        o['hand_MainPoint_' + side] = add(p, c, aa)
        o['hand_DirectionPoint_' + side] = add(p, c, aa)
    return o

PIN = ['foot_MainPoint_l', 'foot_MainPoint_r', 'ball_MainPoint_l', 'ball_MainPoint_r',
       'ball_DirectionPoint_l', 'ball_DirectionPoint_r']

mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
def pid(name):
    o = mv.get_objects(name)[0]
    return bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
names = list(offsets(40).keys()) + PIN
ids = {n: pid(n) for n in names}
frames = globals().get('FRAMES', list(range(F0, F1 + 1)))
orig = {f: {n: dv.get_data_value(ids[n], f) for n in names} for f in frames}

def change(me, ue, su):
    de = me.data_editor()
    for f in frames:
        off = offsets(f)
        for n, d in off.items():
            de.set_data_value(ids[n], f, orig[f][n] + d)
        for n in PIN:
            de.set_data_value(ids[n], f, orig[f][n])
        su.run_update(set(ids.values()), f)

ok = scene.modify_update('restyle_v2', change)
drift = 0.0
for f in frames:
    for n in PIN:
        v = dv.get_data_value(ids[n], f) - orig[f][n]
        drift = max(drift, math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2))
err = 0.0
for f in frames:
    for n, d in offsets(f).items():
        v = dv.get_data_value(ids[n], f) - (orig[f][n] + d)
        err = max(err, math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2))
result = {'ok': bool(ok), 'frames': len(frames), 'max_pin_drift_cm': round(drift, 3), 'max_target_err_cm': round(err, 3)}
