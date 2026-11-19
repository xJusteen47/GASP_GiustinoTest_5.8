"""Frantic run start r3 (Overlord, all kits): 27 frames @30 fps, root in place, UEFN mannequin skeleton.

Usage: blender -b --factory-startup --python author_runstart_r3.py -- <idle.fbx> <outdir>
Implements the r2 spec of r1/REVIEW-fable.md with the r3 changes of r2/REVIEW-fable.md: FK arms (swing S + elbow flexion E), fists, lean measured
pelvis->neck_01, slow pelvis load f1-10, short choppy run steps f11-27. Legs: analytic two-bone IK keyed as FK
on every frame (Blender IK does not work on UE-imported bone tails). Blender world: -Y forward, +X left, metres.
"""
import bpy, sys, os, math, json
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index('--') + 1:]
IDLE, OUT = argv[0], argv[1]
os.makedirs(OUT, exist_ok=True)
F0, F1 = 1, 27
D = math.radians

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=IDLE)
sc = bpy.context.scene
sc.render.fps = 30
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
sc.frame_set(1)
base = {pb.name: pb.matrix_basis.copy() for pb in arm.pose.bones}
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.rotation_mode = 'QUATERNION'
    pb.matrix_basis = base[pb.name]
vl = bpy.context.view_layer
vl.update()
MW = arm.matrix_world
P = arm.pose.bones
def wpos(n): return MW @ P[n].head

rest = {n: wpos(n).copy() for n in ('pelvis', 'foot_l', 'foot_r')}
foot_rest_rot = {s: (MW @ P['foot_' + s].matrix).to_quaternion() for s in 'lr'}
G = rest['foot_l'].z            # ankle height with the foot flat (0.094)
IDLE_PZ = rest['pelvis'].z      # 0.906
PY = rest['pelvis'].y           # pelvis y, "ahead of the pelvis" is measured from here
FX = {'l': 0.13, 'r': -0.13}

# ------------------------------------------------------------------ spec tables
# lean L (deg, pelvis->neck_01 from vertical), spec item 5
LEAN = {1: 18, 3: 21, 6: 25, 9: 28, 11: 27, 13: 26, 19: 22, 25: 17, 26: 15, 27: 16}   # r3 item 5
# pelvis height, spec items 7 and 10
PZ_A = {1: IDLE_PZ - .05, 3: IDLE_PZ - .07, 6: IDLE_PZ - .09, 9: IDLE_PZ - .10, 10: IDLE_PZ - .07, 11: IDLE_PZ - .03}  # r3 item 8
PZ_B = {11: IDLE_PZ - .03, 13: IDLE_PZ - .075, 16: IDLE_PZ - .03, 19: IDLE_PZ - .07, 22: IDLE_PZ - .03,
        25: IDLE_PZ - .06, 27: IDLE_PZ - .045}
RUN_CONTACT = {13, 19, 25}
RIPPLE = {3, 6, 9}
# hips sway (m) and yaw (deg, - = left hip forward), r1 values
SWAY = {1: (0, 6), 3: (-.02, 6), 4: (0, 0), 6: (.02, -6), 7: (0, 0), 9: (-.02, 6), 11: (0, 0), 13: (.02, -6),
        16: (0, 0), 19: (-.02, 6), 22: (0, 0), 25: (.02, -6), 27: (.01, -3)}
# feet: frame -> (y, z, toe-down pitch). Spec items 8, 9. Run contact y = PY - 0.18, toe-off y = PY + 0.15,
# stance slides 0.11 m/frame (linear, keys on every stance frame).
CY, TY = PY - 0.18, PY + 0.15
def stance(c):  # contact frame c .. toe-off c+3
    return {c: (CY, G, 0), c + 1: (CY + .11, G, 0), c + 2: (CY + .22, G, 0), c + 3: (TY, .14, 30)}
FOOT = {'l': {1: (0.0, G, 0), 3: (0.02, G, 0), 4: (-0.02, .24, 20), 5: (-0.04, .32, 25), 6: (0.0, G, 0),
              8: (0.02, G, 0), 9: (0.03, G, 0), 10: (0.06, .20, 25), 11: (CY + .02, .35, 15), 12: (CY - .015, .20, 5),
              **stance(13), 18: (PY + .10, .38, 50), 19: (PY + .03, .38, 40), 20: (PY - .05, .37, 30),
              21: (PY - .12, .36, 20), 22: (CY + .03, .35, 15), 23: (CY - .01, .30, 5), 24: (CY - .005, .18, 0),
              **{k: v for k, v in stance(25).items() if k <= 27}},
        'r': {1: (-0.08, .32, 25), 3: (-0.04, G, 0), 6: (-0.02, G, 0), 7: (-0.06, .24, 20), 8: (-0.08, .33, 25),
              9: (-0.04, G, 0), 10: (0.0, G, 0), 11: (0.06, .12, 25), 12: (0.13, .26, 45), 13: (PY + .10, .38, 50),
              14: (PY - .02, .38, 35), 15: (PY - .10, .37, 25), 16: (CY + .03, .35, 15), 17: (CY - .01, .30, 5),
              18: (CY - .005, .18, 0), **stance(19), 23: (PY + .19, .26, 45), 24: (PY + .10, .38, 50),
              25: (PY + .03, .38, 40), 26: (PY - .03, .38, 30), 27: (PY - .06, .38, 20)}}
CONTACTS = {'l': {6, 13, 25}, 'r': {3, 9, 19}}
# arms, spec items 1-3: (S swing deg, + forward; E elbow flexion deg) at extremes, 1 frame after contacts
SF, SB, RF, RB = (30, 90), (-50, 95), (35, 90), (-62, 95)   # r3 item 2, S from world vertical
ARMS = {'l': {1: (-10, 92), 4: SF, 7: SB, 10: SF, 14: RB, 20: RF, 26: RB, 32: RF},
        'r': {1: (10, 92), 4: SB, 7: SF, 10: SB, 14: RF, 20: RB, 26: RF, 32: RB}}
def abd_for(S):  # r3 item 4: 6 deg forward, flaring to 12 deg on the back swing
    return 6.0 if S >= 0 else 6.0 + 6.0 * min(1.0, -S / 50.0)
FA_IN = 6  # forearm toward the midline

def seg(tab, f):
    ks = sorted(tab)
    if f <= ks[0]: return ks[0], ks[0], 0.0, ks
    if f >= ks[-1]: return ks[-1], ks[-1], 0.0, ks
    for a, b in zip(ks, ks[1:]):
        if a <= f <= b: return a, b, (f - a) / (b - a), ks
def tup(v): return v if isinstance(v, tuple) else (v,)
def catmull(tab, f):
    a, b, t, ks = seg(tab, f)
    if a == b: return tab[a]
    i = ks.index(a)
    p0, p1, p2, p3 = (tup(tab[ks[j]]) for j in (max(i - 1, 0), i, i + 1, min(i + 2, len(ks) - 1)))
    r = tuple(0.5 * (2 * q1 + (-q0 + q2) * t + (2 * q0 - 5 * q1 + 4 * q2 - q3) * t * t + (-q0 + 3 * q1 - 3 * q2 + q3) * t ** 3)
              for q0, q1, q2, q3 in zip(p0, p1, p2, p3))
    return r if isinstance(tab[a], tuple) else r[0]
def linear(tab, f, idx):
    a, b, t, _ = seg(tab, f)
    return tup(tab[a])[idx] + (tup(tab[b])[idx] - tup(tab[a])[idx]) * t
def eased(tab, f, idx, contacts):
    a, b, t, _ = seg(tab, f)
    va, vb = tup(tab[a])[idx], tup(tab[b])[idx]
    if a == b: return va
    u = t * t if b in contacts else (1 - (1 - t) ** 2 if a in contacts else t * t * (3 - 2 * t))
    return va + (vb - va) * u
def sine(tab, f, idx):  # sine ease between extremes, no holds
    a, b, t, _ = seg(tab, f)
    va, vb = tup(tab[a])[idx], tup(tab[b])[idx]
    return va + (vb - va) * (1 - math.cos(math.pi * t)) / 2
def pulse(f, peaks, amp):  # +amp on the peak frame, back to 0 in 3 frames
    v = 0.0
    for p in peaks:
        if p <= f <= p + 3: v += amp * (1 - (f - p) / 3)
    return v

def pelvis_z(f):
    if f <= 11:
        z = catmull(PZ_A, f)
        if f in RIPPLE: z -= 0.015
        return z
    return eased(PZ_B, f, 0, RUN_CONTACT)

def foot_y(s, f):
    ft = FOOT[s]; a, b, _, _ = seg(ft, f)
    stance_seg = ft[a][1] <= G + 1e-6 and ft[b][1] <= G + .05 and f >= 13
    return linear(ft, f, 0) if stance_seg else catmull({k: v[0] for k, v in ft.items()}, f)

# ------------------------------------------------------------------ helpers
SPINE = ['spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05']
def rot_world(n, R):
    pb = P[n]; h = pb.head.copy()
    pb.matrix = Matrix.Translation(h) @ R @ Matrix.Translation(-h) @ pb.matrix
    vl.update()
def aim(n, child, target):
    cur = wpos(child) - wpos(n); want = target - wpos(n)
    if cur.length > 1e-6 and want.length > 1e-6:
        rot_world(n, cur.rotation_difference(want).to_matrix().to_4x4())
def two_bone(up, mid, end, target, pole, L1, L2):
    H = wpos(up); d = target - H; Dd = min(d.length, (L1 + L2) * 0.995); d.normalize()
    pv = pole - H; n = pv - d * pv.dot(d); n = n.normalized() if n.length > 1e-6 else Vector((0, -1, 0))
    a_ = (L1 * L1 - L2 * L2 + Dd * Dd) / (2 * Dd); h = math.sqrt(max(L1 * L1 - a_ * a_, 0))
    aim(up, mid, H + d * a_ + n * h); aim(mid, end, H + d * Dd)
def lean_meas():
    v = wpos('neck_01') - wpos('pelvis')
    return math.degrees(math.atan2(-v.y, v.z))
def side_dir(angle_from_down_fwd, abd_out, sg):
    """Unit vector at angle from straight down, + toward forward (-Y), tilted abd_out deg away from the body."""
    a, b = D(angle_from_down_fwd), D(abd_out)
    return Vector((sg * math.sin(b), -math.sin(a) * math.cos(b), -math.cos(a) * math.cos(b))).normalized()

LEN = {}
for s in 'lr':
    LEN['leg_' + s] = ((wpos('calf_' + s) - wpos('thigh_' + s)).length, (wpos('foot_' + s) - wpos('calf_' + s)).length)
    LEN['arm_' + s] = ((wpos('lowerarm_' + s) - wpos('upperarm_' + s)).length, (wpos('hand_' + s) - wpos('lowerarm_' + s)).length)

FINGERS = ['index', 'middle', 'ring', 'pinky']
CURL = (75, 90, 60)
def tip(s, fn):
    a, b = wpos(f'{fn}_02_{s}'), wpos(f'{fn}_03_{s}')
    return b + (b - a) * 0.8
def make_fist(s):
    hand = wpos('hand_' + s)
    across = (wpos('pinky_01_' + s) - wpos('index_01_' + s)).normalized()
    fwd = (wpos('middle_01_' + s) - hand).normalized()
    centre_a, centre_b = hand + fwd * 0.05 + across.cross(fwd) * 0.03, hand + fwd * 0.05 - across.cross(fwd) * 0.03
    # palm side = the side the relaxed fingers already curl toward
    v12 = wpos('index_02_' + s) - wpos('index_01_' + s); v13 = tip(s, 'index') - wpos('index_01_' + s)
    palm = (v13 - v12 * (v13.dot(v12) / v12.length_squared))
    centre = centre_a if palm.dot(across.cross(fwd)) > 0 else centre_b
    for fn in FINGERS:
        for j, ang in zip(('01', '02', '03'), CURL):
            n = f'{fn}_{j}_{s}'; best = None
            for sgn in (1, -1):
                saved = P[n].matrix_basis.copy()
                rot_world(n, Matrix.Rotation(D(ang * sgn), 4, across))
                dist = (tip(s, fn) - centre).length
                P[n].matrix_basis = saved; vl.update()
                if best is None or dist < best[0]: best = (dist, sgn)
            rot_world(n, Matrix.Rotation(D(ang * best[1]), 4, across))
    for j, ang in (('01', 30), ('02', 40)):
        n = f'thumb_{j}_{s}'
        tdir = (wpos(f'thumb_03_{s}') - wpos(n)).normalized()
        ax = tdir.cross(palm.normalized()) if palm.length > 1e-6 else across
        if ax.length < 1e-6: ax = across
        ax.normalize(); best = None
        for sgn in (1, -1):
            saved = P[n].matrix_basis.copy()
            rot_world(n, Matrix.Rotation(D(ang * sgn), 4, ax))
            dist = (wpos(f'thumb_03_{s}') - wpos('middle_01_' + s)).length
            P[n].matrix_basis = saved; vl.update()
            if best is None or dist < best[0]: best = (dist, sgn)
        rot_world(n, Matrix.Rotation(D(ang * best[1]), 4, ax))

def fing_curl(s, fn):  # measured bend (deg) of base, middle, tip joints
    pts = [wpos(f'{fn}_metacarpal_{s}'), wpos(f'{fn}_01_{s}'), wpos(f'{fn}_02_{s}'), wpos(f'{fn}_03_{s}'), tip(s, fn)]
    return [round(math.degrees((pts[i + 1] - pts[i]).angle(pts[i + 2] - pts[i + 1])), 0) for i in range(3)]

def line_yaw(a, b):
    v = wpos(a) - wpos(b); return math.degrees(math.atan2(v.y, v.x))
def square_up():
    rot_world('pelvis', Matrix.Rotation(D(-line_yaw('thigh_l', 'thigh_r')), 4, 'Z'))
    rot_world('spine_01', Matrix.Rotation(D(-line_yaw('upperarm_l', 'upperarm_r')), 4, 'Z'))
def seg_lean(a, b):
    v = wpos(b) - wpos(a); return math.degrees(math.atan2(-v.y, v.z))

# ------------------------------------------------------------------ per-frame pose, keyed as FK
KEYED = None
metrics = []
for f in range(F0, F1 + 1):
    for pb in P: pb.matrix_basis = base[pb.name]
    vl.update()
    square_up()
    L = catmull(LEAN, f)
    sway, yaw = catmull(SWAY, f)
    pb = P['pelvis']; M = pb.matrix.copy()
    M.translation = MW.inverted() @ Vector((rest['pelvis'].x + sway, PY, pelvis_z(f))); pb.matrix = M; vl.update()
    rot_world('pelvis', Matrix.Rotation(D(yaw), 4, 'Z') @ Matrix.Rotation(D(L * 0.55), 4, 'X'))
    for n in SPINE:
        R = Matrix.Rotation(D(L * 0.45 / 5), 4, 'X')
        if n in ('spine_04', 'spine_05'): R = Matrix.Rotation(D(3), 4, 'X') @ R          # small flexion (r3: 3)
        if n == 'spine_04': R = Matrix.Rotation(D(-yaw * 1.6), 4, 'Z') @ R
        rot_world(n, R)
    for _ in range(3):  # make measured lean match keyed: correct from the hips (rigid, keeps the back straight)
        c = L - lean_meas()
        rot_world('pelvis', Matrix.Rotation(D(c), 4, 'X'))
    # chest nod after each run contact (spec 11), after the lean match so it shows as a nod
    rot_world('spine_05', Matrix.Rotation(D(pulse(f, (14, 20, 26), 1.5)), 4, 'X'))
    for _ in range(2):  # straight strong back (r3 gate 6): bend between lower and upper back <= 11 deg, then re-match lean
        bend = seg_lean('spine_03', 'neck_01') - seg_lean('pelvis', 'spine_03')
        if abs(bend) > 11: rot_world('spine_03', Matrix.Rotation(D(-(bend - math.copysign(11, bend))), 4, 'X'))
        rot_world('pelvis', Matrix.Rotation(D(L - lean_meas()), 4, 'X'))
    for s, sg in (('l', 1), ('r', -1)):  # hunch: clavicles 12 up, 10 forward
        rot_world('clavicle_' + s, Matrix.Rotation(D(-4 * sg), 4, 'Z') @ Matrix.Rotation(D(-8 * sg), 4, 'Y'))
    rot_world('head', Matrix.Rotation(D(-0.25 * L + pulse(f, (15, 21, 27), 2)), 4, 'X'))
    hd, nk, pv = wpos('head'), wpos('neck_01'), wpos('pelvis'); ln_ = (nk - pv).normalized()
    ahead = -(hd - (pv + ln_ * (hd - pv).dot(ln_))).y
    if ahead > 0.02:  # head 2 cm ahead of the chest line (r3 item 7): pull it back on the neck
        rot_world('neck_01', Matrix.Rotation(D(-math.degrees(math.atan2(ahead - 0.02, (hd - nk).length))), 4, 'X'))
    # legs
    for s in 'lr':
        ft = FOOT[s]
        y = foot_y(s, f)
        z = max(eased(ft, f, 1, CONTACTS[s]), G)
        pitch = catmull({k: v[2] for k, v in ft.items()}, f)
        two_bone('thigh_' + s, 'calf_' + s, 'foot_' + s, Vector((FX[s], y, z)),
                 Vector((FX[s] * 1.2, y - 0.9, 0.6)), *LEN['leg_' + s])
        q = Matrix.Rotation(D(pitch), 4, 'X').to_quaternion() @ foot_rest_rot[s]
        fb = P['foot_' + s]; Mf = q.to_matrix().to_4x4(); Mf.translation = fb.matrix.translation; fb.matrix = Mf; vl.update()
    # arms in FK
    for s, sg in (('l', 1), ('r', -1)):
        S, E = sine(ARMS[s], f, 0), sine(ARMS[s], f, 1)
        sh = wpos('upperarm_' + s); L1, L2 = LEN['arm_' + s]
        el = sh + side_dir(S, abd_for(S), sg) * L1
        aim('upperarm_' + s, 'lowerarm_' + s, el)
        aim('lowerarm_' + s, 'hand_' + s, wpos('lowerarm_' + s) + side_dir(S + E, -FA_IN, sg) * L2)
        dS = sine(ARMS[s], max(F0, f - 1), 0) - sine(ARMS[s], max(F0, f - 2), 0)   # wrist lags 1 frame
        rot_world('hand_' + s, Matrix.Rotation(D(8 * max(-1, min(1, dS / 15))), 4, 'X'))
        make_fist(s)
    if KEYED is None:
        KEYED = [pb.name for pb in P]
    for n in KEYED:
        P[n].keyframe_insert('rotation_quaternion', frame=f)
    P['pelvis'].keyframe_insert('location', frame=f)
    # ---- metrics for this frame
    row = dict(f=f, lean_keyed=round(L, 1), lean_meas=round(lean_meas(), 1), pelvis_z=round(wpos('pelvis').z, 3),
               pelvis_y=round(wpos('pelvis').y, 3))
    for s, sg in (('l', 1), ('r', -1)):
        sh, el, wr = wpos('upperarm_' + s), wpos('lowerarm_' + s), wpos('hand_' + s)
        u = (el - sh).normalized(); fo = (wr - el).normalized()
        row['S_' + s] = round(math.degrees(math.atan2(-u.y, -u.z)), 1)
        row['E_' + s] = round(math.degrees(u.angle(fo)), 1)
        row['abd_' + s] = round(math.degrees(math.asin(max(-1, min(1, u.x * sg)))), 1)
        row['elbow_below_sh_' + s] = round(sh.z - el.z, 3)
        row['S_world_' + s] = row['S_' + s]
        fist = (wr + wpos('middle_01_' + s)) / 2
        row['fist_below_sh_' + s] = round(sh.z - fist.z, 3)
        row['fist_to_head_' + s] = round((fist - wpos('head')).length, 3)
        pv_, nk_ = wpos('pelvis'), wpos('neck_01'); ly, lz = nk_.y - pv_.y, nk_.z - pv_.z; ln = math.hypot(ly, lz)
        row['elbow_behind_back_' + s] = round(((el.y - pv_.y) * lz - (el.z - pv_.z) * ly) / ln, 3)
        fw, bw = wpos('foot_' + s), wpos('ball_' + s)
        row['ankle_' + s] = [round(fw.y, 3), round(fw.z, 3)]
        row['ball_z_' + s] = round(bw.z, 3)
        th = (wpos('calf_' + s) - wpos('thigh_' + s)).normalized()
        row['thigh_from_vert_' + s] = round(math.degrees(th.angle(Vector((0, 0, -1)))), 1)
        row['knee_' + s] = round(math.degrees((wpos('thigh_' + s) - wpos('calf_' + s)).angle(fw - wpos('calf_' + s))), 1)
        row['curl_middle_' + s] = fing_curl(s, 'middle')
    hd, nk, pv = wpos('head'), wpos('neck_01'), wpos('pelvis')
    line = (nk - pv).normalized(); ext = pv + line * (hd - pv).dot(line)
    row['head_ahead_of_chest_line'] = round(-(hd - ext).y, 3)
    s3 = wpos('spine_03'); a1 = (s3 - wpos('pelvis')); a2 = (wpos('neck_01') - s3); a1.x = a2.x = 0
    row['back_bend_deg'] = round(math.degrees(a1.angle(a2)), 1)
    row['feet_apart_y'] = round(abs(row['ankle_l'][0] - row['ankle_r'][0]), 3)
    metrics.append(row)

act = arm.animation_data.action
act.name = 'AS_RunStart_Frantic_r3'
sc.frame_start, sc.frame_end = F0, F1
r = metrics
def rng(k): return [min(x[k] for x in r), max(x[k] for x in r)]
summary = dict(
    lean_max_err=max(abs(x['lean_keyed'] - x['lean_meas']) for x in r),
    E_range={s: rng('E_' + s) for s in 'lr'}, S_range={s: rng('S_' + s) for s in 'lr'},
    abd_max={s: max(abs(x['abd_' + s]) for x in r) for s in 'lr'},
    elbow_below_sh_min={s: min(x['elbow_below_sh_' + s] for x in r) for s in 'lr'},
    pelvis_z={f: next(x['pelvis_z'] for x in r if x['f'] == f) for f in (1, 3, 6, 9, 10, 11, 13, 16, 19, 25)},
    contact_ahead={f'{s}{c}': round(next(x for x in r if x['f'] == c)['pelvis_y'] - next(x for x in r if x['f'] == c)['ankle_' + s][0], 3)
                   for s in 'lr' for c in sorted(CONTACTS[s]) if c >= 13},
    feet_apart_max_run=max(x['feet_apart_y'] for x in r if x['f'] >= 11),
    inplace_spread={s: round(max(abs(x['ankle_' + s][0] - FOOT[s][3 if s == 'r' else 1][0]) for x in r if x['f'] <= 10), 3) for s in 'lr'},
    inplace_lift_max={s: max(x['ankle_' + s][1] for x in r if x['f'] <= 10) for s in 'lr'},
    min_ankle_z=min(min(x['ankle_l'][1], x['ankle_r'][1]) for x in r),
    min_ball_z=min(min(x['ball_z_l'], x['ball_z_r']) for x in r),
    knee_range={s: rng('knee_' + s) for s in 'lr'},
    thigh_drive={f'{s}{x["f"]}': x['thigh_from_vert_' + s] for x in r for s in 'lr' if x['f'] in (11, 16, 17, 22, 23)},
    head_ahead=rng('head_ahead_of_chest_line'),
    fist_below_sh_min={s: min(x['fist_below_sh_' + s] for x in r) for s in 'lr'},
    fist_to_head_min={s: min(x['fist_to_head_' + s] for x in r) for s in 'lr'},
    back_extremes={f'{s}{e}': dict(behind_back=r[e - 1]['elbow_behind_back_' + s], below_sh=r[e - 1]['elbow_below_sh_' + s], S=r[e - 1]['S_' + s])
                   for s in 'lr' for e, v in ARMS[s].items() if e <= F1 and v[0] < -30},
    back_bend_max=max(x['back_bend_deg'] for x in r),
    curl_middle_f1={s: r[0]['curl_middle_' + s] for s in 'lr'})
json.dump(dict(summary=summary, per_frame=r), open(os.path.join(OUT, 'metrics.json'), 'w'), indent=1)
m = dict(arm_swing_deg={s: round(summary['S_range'][s][1] - summary['S_range'][s][0], 1) for s in 'lr'},
         min_ball_z=summary['min_ball_z'])

# ------------------------------------------------------------------ export
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); vl.objects.active = arm
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, 'AS_RunStart_Frantic_r3.fbx'), use_selection=True,
                         object_types={'ARMATURE'}, add_leaf_bones=False, bake_anim=True,
                         bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'runstart_frantic_r3.blend'))

# ------------------------------------------------------------------ proxy body for renders (not exported)
sc.frame_set(F0); vl.update()
mat = bpy.data.materials.new('grey')
SEG = [('pelvis', 'thigh_l', 'thigh_r', 0.10), ('pelvis', 'pelvis', 'spine_02', 0.13), ('spine_02', 'spine_02', 'spine_04', 0.14),
       ('spine_04', 'spine_04', 'neck_01', 0.15), ('neck_01', 'neck_01', 'head', 0.05)]
for s in 'lr':
    SEG += [('clavicle_' + s, 'spine_05', 'upperarm_' + s, 0.06), ('upperarm_' + s, 'upperarm_' + s, 'lowerarm_' + s, 0.06),
            ('lowerarm_' + s, 'lowerarm_' + s, 'hand_' + s, 0.05), ('thigh_' + s, 'thigh_' + s, 'calf_' + s, 0.085),
            ('calf_' + s, 'calf_' + s, 'foot_' + s, 0.06), ('foot_' + s, 'foot_' + s, 'ball_' + s, 0.045)]
def attach(o, bone):
    pb = P[bone]
    o.parent = arm; o.parent_type = 'BONE'; o.parent_bone = bone
    o.matrix_parent_inverse = (MW @ pb.matrix @ Matrix.Translation((0, pb.bone.length, 0))).inverted()
    o.data.materials.append(mat)
def wm(o, M):
    o.matrix_world = M
for bone, a, b, r in SEG:
    pa, pbp = wpos(a), wpos(b)
    v = pbp - pa
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=max(v.length, 0.01), vertices=16)
    o = bpy.context.object
    M = Matrix.Translation((pa + pbp) / 2) @ v.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    attach(o, bone); o.matrix_world = M
    for p in (pa, pbp):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=p, segments=16, ring_count=8)
        attach(bpy.context.object, bone); bpy.context.object.matrix_world = Matrix.Translation(p)
for s in 'lr':  # fists and toes
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.065, location=wpos('hand_' + s) + Vector((0, 0, -0.06)))
    o = bpy.context.object; M = o.matrix_world.copy(); attach(o, 'hand_' + s); o.matrix_world = M
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.045, location=wpos('ball_' + s) + Vector((0, -0.06, 0)))
    o = bpy.context.object; M = o.matrix_world.copy(); attach(o, 'ball_' + s); o.matrix_world = M
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.12, location=wpos('head') + Vector((0, -0.02, 0.10)))
o = bpy.context.object; M = o.matrix_world.copy(); attach(o, 'head'); o.matrix_world = M
# nose to read the head direction
bpy.ops.mesh.primitive_cone_add(radius1=0.03, depth=0.08, location=wpos('head') + Vector((0, -0.14, 0.10)), rotation=(D(90), 0, 0))
o = bpy.context.object; M = o.matrix_world.copy(); attach(o, 'head'); o.matrix_world = M

bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
# ground marks every 25 cm along Y to read foot slide
for i in range(-8, 9):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0.45, i * 0.25, 0.002), scale=(0.02, 0.004, 0.002))
w = bpy.data.worlds.new('w'); sc.world = w; w.color = (0.8, 0.8, 0.8)
try:
    w.use_nodes = True; bg = next(n for n in w.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs[0].default_value = (0.8, 0.8, 0.8, 1)
except Exception:
    pass
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); sc.collection.objects.link(sun)
sun.data.energy = 3; sun.rotation_euler = (0.8, 0.2, 0.6)
try:
    sc.render.engine = 'BLENDER_WORKBENCH'
except TypeError:
    pass
sh = sc.display.shading; sh.light = 'STUDIO'; sh.color_type = 'SINGLE'; sh.single_color = (0.6, 0.6, 0.6); sh.show_shadows = True
sc.render.resolution_x = sc.render.resolution_y = 512
cam_d = bpy.data.cameras.new('cam'); cam_d.type = 'ORTHO'; cam_d.ortho_scale = 2.3
cam = bpy.data.objects.new('cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
VIEWS = {'side': Vector((1, 0, 0)), 'q34': Vector((0.7, -0.7, 0.25)).normalized()}
for vname, d in VIEWS.items():
    cam.location = Vector((0, -0.05, 0.85)) + d * 6
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    for f in range(F0, F1 + 1):
        sc.frame_set(f)
        sc.render.filepath = os.path.join(OUT, 'cells', f'{vname}_{f:02d}.png')
        bpy.ops.render.render(write_still=True)
print('OK frames', F1 - F0 + 1, 'swing', m['arm_swing_deg'], 'min ball z', m['min_ball_z'])
