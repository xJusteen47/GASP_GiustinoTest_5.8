# Run start w2v (runstart-v23-desktop, 2026-11-21) = Variant 23: w2 (approved, locked) + "vita" for the BASE agile character
# (owner-pasted Gemini numbers, docs/notes/reviews/runstart-w2s/gemini-agile.md). Run in Cascadeur (run_python, exec) on
# RunStart_Generic_w2v.casc (= copy of w2). Points only (global positions), the rig solves the boxes, like runstart_upright_casc_v4r6.
# Identity kept (rule 13a): w2 frantic rev-up, stock GASP gait, ends exactly on the stock loop (all offsets are 0 from OUT1 on).
# Edits, all scaled by envelopes that are 0 at f0 (idle hand-over) and from OUT1 (loop hand-off):
#   SWAY   pelvis + upper body shift toward the stance (lower) foot, knees half; legs below the knee untouched (contacts kept).
#   PITCH  upper body above spine_02 leans forward extra, peak on the first push (PEAK); head lags DRAG frames (head drag).
#   YAW    shoulder counter-rotation vs the hips amplified in its own direction (snappy, alternates with the steps); head keeps gaze.
#   ROLL   small: the shoulder of the forward arm drops.
#   ELBOW  the rear arm swings further back about the shoulder in the sagittal plane (no abduction: elbows stay tight).
# Tunables via globals: SWAY, PITCH, YAW, ROLL, ELBOW, DRAG, PEAK, OUT, LAST. Result = short dict.
import math
import numpy as np

G = globals()
SWAY = float(G.get('SWAY', 2.0))       # cm
PITCH = float(G.get('PITCH', 5.0))     # deg
YAW = float(G.get('YAW', 8.0))         # deg
ROLL = float(G.get('ROLL', 2.5))       # deg
ELBOW = float(G.get('ELBOW', 12.0))    # deg
DRAG = float(G.get('DRAG', 1.5))       # frames
PEAK = float(G.get('PEAK', 12.0))      # first push frame (capture v92)
OUT0, OUT1 = G.get('OUT', (20, 29))    # fade out between 2nd and 3rd step
SCENE_END = G.get('SCENE_END', 'runstart/RunStart_Generic_w2v.casc')

ARM = {s: ['upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s, 'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s,
           'hand_MainPoint_' + s, 'hand_AdditionalPoint_' + s, 'hand_DirectionPoint_' + s] for s in 'lr'}
CLAV = {s: ['clavicle_MainPoint_' + s, 'clavicle_AdditionalPoint_' + s] for s in 'lr'}
NECK_SUB = ['neck_01_AdditionalPoint', 'head_MainPoint', 'head_DirectionPoint', 'head_AdditionalPoint']
SPINE02_SUB = ['spine_02_AdditionalPoint', 'spine_04_MainPoint', 'spine_04_AdditionalPoint', 'neck_01_MainPoint'] + NECK_SUB \
    + CLAV['l'] + CLAV['r'] + ARM['l'] + ARM['r']
UPPER = ['pelvis_MainPoint', 'pelvis_AdditionalPoint', 'spine_02_MainPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r'] + SPINE02_SUB
KNEE = [p + s for s in ('_l', '_r') for p in ('calf_MainPoint', 'calf_AdditionalPoint', 'calf_LimbDir')]
FEET = ['foot_MainPoint_l', 'foot_MainPoint_r']
ALL = UPPER + KNEE + FEET


def ss(a, b, x):
    if x <= a: return 0.0
    if x >= b: return 1.0
    t = (x - a) / float(b - a); return t * t * (3 - 2 * t)


def env(f):                       # whole first two steps
    return ss(1, 6, f) * (1.0 - ss(OUT0, OUT1, f))


def env_peak(f):                  # push peak around PEAK
    return ss(PEAK - 8, PEAK, f) * (1.0 - ss(PEAK + 2, OUT1 - 3, f))


def rot(k, d):
    k = np.asarray(k, dtype=np.float64); k = k / np.linalg.norm(k)
    s, c = math.sin(math.radians(d)), math.cos(math.radians(d))
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + s * K + (1 - c) * K @ K


def hz(v):
    v = np.array(v, dtype=np.float64); v[1] = 0.0; return v


tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
ids = {}
for o in mv.get_objects():
    n = mv.get_object_name(o)
    if n in ALL:
        ids[n] = bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
missing = [n for n in ALL if n not in ids]
assert not missing, missing
LAST = int(G.get('LAST', OUT1 + 2))
src = {f: {n: np.array([float(c) for c in dv.get_data_value(i, f)], dtype=np.float64) for n, i in ids.items()}
       for f in range(0, LAST + 1)}
UPV = np.array([0.0, 1.0, 0.0])
travel = hz(src[LAST]['pelvis_MainPoint'] - src[0]['pelvis_MainPoint'])
FWD0 = travel / np.linalg.norm(travel)

# per-frame raw signals, smoothed (3-tap) so the step alternation has no corners
raw = {'side': [], 'tw': [], 'hd': []}
for f in range(LAST + 1):
    P = src[f]
    lat = hz(P['thigh_MainPoint_l'] - P['thigh_MainPoint_r']); lat /= np.linalg.norm(lat)
    fwd = np.cross(lat, UPV); fwd = fwd if np.dot(fwd, FWD0) > 0 else -fwd
    raw['side'].append(math.tanh((P['foot_MainPoint_r'][1] - P['foot_MainPoint_l'][1]) / 4.0))   # + = left foot lower (stance)
    sh = hz(P['clavicle_MainPoint_l'] - P['clavicle_MainPoint_r']); sh /= np.linalg.norm(sh)
    raw['tw'].append(math.degrees(math.atan2(float(np.dot(np.cross(lat, sh), UPV)), float(np.dot(lat, sh)))))
    raw['hd'].append(float(np.dot(P['hand_MainPoint_l'] - P['hand_MainPoint_r'], fwd)))        # + = left hand ahead


def smooth(a):
    a = np.array(a); b = a.copy(); b[1:-1] = 0.25 * a[:-2] + 0.5 * a[1:-1] + 0.25 * a[2:]; return b


sig = {k: smooth(v) for k, v in raw.items()}
pitch_of = lambda f: PITCH * env_peak(f)
out, log = {}, []
for f in range(LAST + 1):
    P = src[f]; D = {n: v.copy() for n, v in P.items()}
    e, ep = env(f), env_peak(f)
    lat = hz(P['thigh_MainPoint_l'] - P['thigh_MainPoint_r']); lat /= np.linalg.norm(lat)
    fwd = np.cross(lat, UPV); fwd = fwd if np.dot(fwd, FWD0) > 0 else -fwd
    # SWAY
    s = SWAY * e * float(sig['side'][f])
    for n in UPPER: D[n] = D[n] + s * lat
    for n in KNEE: D[n] = D[n] + 0.5 * s * lat
    piv = D['spine_02_MainPoint']
    # YAW (amplify current twist direction) + PITCH (forward) + ROLL (forward-arm shoulder down), one rotation about spine_02
    yaw = YAW * e * math.tanh(float(sig['tw'][f]) / 5.0)
    pit = pitch_of(f)
    roll = ROLL * e * math.tanh(float(sig['hd'][f]) / 10.0)     # + = left arm ahead -> left shoulder down
    Ry = rot(UPV, yaw)
    Rp = rot(lat, pit)
    if np.dot(Rp @ (D['neck_01_MainPoint'] - piv), fwd) < np.dot(D['neck_01_MainPoint'] - piv, fwd):
        Rp = rot(lat, -pit)
    Rr = rot(fwd, roll)
    if (Rr @ (D['clavicle_MainPoint_l'] - piv))[1] > (D['clavicle_MainPoint_l'] - piv)[1]:
        Rr = rot(fwd, -roll)
    R = Rr @ Rp @ Ry
    for n in SPINE02_SUB: D[n] = piv + R @ (D[n] - piv)
    # head: undo the yaw (gaze ahead) and lag the pitch by DRAG frames
    nk = D['neck_01_MainPoint']
    lag = pitch_of(f - DRAG) - pit
    Rh = rot(lat, abs(lag))
    if np.sign(lag) != np.sign(np.dot(Rh @ (D['head_MainPoint'] - nk) - (D['head_MainPoint'] - nk), fwd)) and lag != 0:
        Rh = rot(lat, -abs(lag))
    Rh = Rh @ rot(UPV, -yaw)
    for n in NECK_SUB: D[n] = nk + Rh @ (D[n] - nk)
    # ELBOW: rear arm further back about the shoulder (sagittal plane)
    hd = float(sig['hd'][f])
    for s_, w in (('l', max(0.0, math.tanh(-hd / 10.0))), ('r', max(0.0, math.tanh(hd / 10.0)))):
        a = ELBOW * ep * w
        if a <= 1e-3: continue
        sh_ = D['upperarm_MainPoint_' + s_]
        Ra = rot(lat, a)
        if np.dot(Ra @ (D['lowerarm_MainPoint_' + s_] - sh_), fwd) > np.dot(D['lowerarm_MainPoint_' + s_] - sh_, fwd):
            Ra = rot(lat, -a)
        for n in ARM[s_][1:]: D[n] = sh_ + Ra @ (D[n] - sh_)
    out[f] = {n: D[n] for n in UPPER + KNEE}
    if f % 3 == 0:
        log.append((f, round(s, 2), round(yaw, 1), round(pit, 1), round(roll, 1)))

if G.get('DRY', False):
    result = {'dry': True, 'log': log}
else:
    def change(me, ue, su):
        de = me.data_editor()
        for f, pose in out.items():
            for n, v in pose.items():
                de.set_data_value(ids[n], f, [float(c) for c in v])
            su.run_update(set(ids.values()), f)

    ok = scene.modify_update('runstart w2v vita', change)
    err = 0.0; feet = 0.0
    for f, pose in out.items():
        for n, v in pose.items():
            err = max(err, float(np.linalg.norm(np.array([float(c) for c in dv.get_data_value(ids[n], f)]) - v)))
        for n in FEET:
            feet = max(feet, float(np.linalg.norm(np.array([float(c) for c in dv.get_data_value(ids[n], f)]) - src[f][n])))
    result = {'ok': bool(ok), 'frames': len(out), 'max_err_cm': round(err, 3), 'feet_moved_cm': round(feet, 3), 'log': log}
