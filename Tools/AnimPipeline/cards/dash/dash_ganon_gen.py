"""L3 boost dash, Ganondorf-style (owner 2026-11-19: "L3 = Smash Ultimate's dash"). Round r1, authored offline in Blender (cloud).

Reference dossier: docs/notes/reference-dossiers/dash-ganon.md. Identity traits (rule 13a):
  1) compact load before the push (knees deep, hunched, arms tucked) with a dust burst;
  2) the lunge: torso pitched ~45-50 deg, wide leg split, rear leg straight back, fists low forward, held ~4 frames;
  3) heavy settle into the run: the body rises as the front foot catches under the hips, no float.
Style: "Ganondorf inside Astro Bot", medium-heavy (memory: animation north star).

Usage (Blender 4.x/5.x or the bpy module, no GPU needed):
  blender -b --factory-startup --python dash_ganon_gen.py -- <skeleton.fbx> <outdir> [--variant r1]
  python dash_ganon_gen.py <skeleton.fbx> <outdir>            (with `pip install bpy`)
<skeleton.fbx> = any FBX of the UEFN mannequin skeleton whose frame 2 has fists, e.g.
docs/notes/reviews/run-start-frantic/r3/AS_RunStart_Frantic_r3.fbx (its rest pose is the skeleton's bind pose, its fingers are fists).
Writes <outdir>/AS_GSTN_Dash_Ganon_<variant>.fbx (60 fps, root motion on the root), metrics.json, frames.json (world joints per
frame for the stick renders: dash_ganon_plot.py) and prints max 5 lines.

Frames: index i in this file = clip frame i at 60 fps, designer frame i+1 (frame data counts from 1).
Axes of the numbers below: CHARACTER axes, metres: F = forward, L = character's left, U = up; the root at the origin (component
space). Blender world: +X = left, -Y = forward, Z = up. The sim moves the root: ROOT_SPEED below = the sim's BoostDash
(stage 2 MaxSpeed 610 cm/s, Acceleration 1400 x AccelMultiplier 8 = 11200 cm/s2: top speed in ~3.3 frames), so a foot that is
planted on the ground moves BACK in component space by the root's step every frame.
"""
import bpy, sys, os, math, json
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
SKEL, OUT = argv[0], argv[1]
VARIANT = argv[argv.index('--variant') + 1] if '--variant' in argv else 'r1'
NAME = f'AS_GSTN_Dash_Ganon_{VARIANT}'
os.makedirs(OUT, exist_ok=True)
FPS = 60
N = 25                      # frames 0..24 (0.4 s); even 60 fps intervals
D = math.radians


def V(f, l, u):
    """Character axes (forward, left, up) -> Blender world."""
    return Vector((l, -f, u))


# ------------------------------------------------------------------ root motion = the sim's boost dash
TOP, ACC = 6.10, 112.0      # m/s, m/s2
def root_speed(i):
    return min(TOP, ACC * i / FPS)
ROOT = [0.0]
for i in range(1, N):
    ROOT.append(ROOT[-1] + 0.5 * (root_speed(i - 1) + root_speed(i)) / FPS)


# ------------------------------------------------------------------ curves
def seg(tab, f):
    ks = sorted(tab)
    if f <= ks[0]: return ks[0], ks[0], 0.0, ks
    if f >= ks[-1]: return ks[-1], ks[-1], 0.0, ks
    for a, b in zip(ks, ks[1:]):
        if a <= f <= b: return a, b, (f - a) / (b - a), ks


def tup(v): return v if isinstance(v, tuple) else (v,)


def catmull(tab, f):
    """Smooth through the keys (moving holds, arcs). Tangents clamped at extremes would be nicer; keys are placed so the
    overshoot stays small (metrics report the curves per frame)."""
    a, b, t, ks = seg(tab, f)
    if a == b: return tab[a]
    i = ks.index(a)
    p0, p1, p2, p3 = (tup(tab[ks[j]]) for j in (max(i - 1, 0), i, i + 1, min(i + 2, len(ks) - 1)))
    r = tuple(0.5 * (2 * q1 + (-q0 + q2) * t + (2 * q0 - 5 * q1 + 4 * q2 - q3) * t * t + (-q0 + 3 * q1 - 3 * q2 + q3) * t ** 3)
              for q0, q1, q2, q3 in zip(p0, p1, p2, p3))
    return r if isinstance(tab[a], tuple) else r[0]


def eased(tab, f, kinds=None):
    """Key to key with an ease per segment: kinds[b] = 'in' | 'out' | 'io' (default) for the segment ending on key b."""
    a, b, t, _ = seg(tab, f)
    va, vb = tup(tab[a]), tup(tab[b])
    if a == b: return tab[a]
    k = (kinds or {}).get(b, 'io')
    u = t * t if k == 'in' else (1 - (1 - t) ** 2 if k == 'out' else (t if k == 'lin' else t * t * (3 - 2 * t)))
    r = tuple(x + (y - x) * u for x, y in zip(va, vb))
    return r if isinstance(tab[a], tuple) else r[0]


# ------------------------------------------------------------------ THE POSES (r1 numbers; Fable tunes them)
# Lean = pelvis -> neck_01 from vertical, deg, + = forward (trait 2: 45-50 in the lunge). Load hunched (extra upper-spine curl),
# push = the fastest change (ease-out into the lunge), lunge a MOVING hold 52 -> 49, settle 32 -> 25 into the run.
LEAN = {0: 6, 2: 18, 4: 36, 5: 38, 6: 46, 8: 55, 11: 51, 13: 44, 15: 38, 17: 33, 20: 28, 24: 25}
LEAN_K = {2: 'out', 4: 'out', 6: 'in', 8: 'out', 11: 'lin', 13: 'io', 15: 'io', 17: 'io', 20: 'out', 24: 'io'}
HUNCH = {0: 0, 3: 10, 5: 14, 7: 5, 9: 0, 12: 0, 16: 3, 24: 3}         # extra flexion on spine_04/05 (deg): compact load (trait 1)
# Pelvis height (m; bind pose 0.856, ankle 0.094). Load deep (trait 1), lunge low and long, catch dip then rise (trait 3).
PZ = {0: 0.88, 2: 0.81, 4: 0.72, 5: 0.71, 7: 0.76, 9: 0.79, 11: 0.79, 14: 0.77, 16: 0.76, 17: 0.745, 19: 0.80, 21: 0.83,
      24: 0.80}
PZ_K = {2: 'out', 4: 'out', 5: 'io', 7: 'in', 9: 'out', 11: 'lin', 14: 'io', 16: 'in', 17: 'out', 19: 'out', 21: 'io', 24: 'in'}
PF = {0: 0.0, 4: -0.03, 6: 0.04, 9: 0.06, 12: 0.06, 16: 0.03, 19: 0.06, 24: 0.05}   # pelvis forward of the root (m)
# Hip yaw (deg, + = left hip forward: the left leg leads the lunge), shoulders counter it (trait 2: arrow, not a twist)
YAW = {0: 0, 4: -6, 7: 10, 10: 14, 13: 10, 16: 2, 20: -10, 24: -14}
# Head WORLD pitch from vertical (neck_01 -> head line), deg: eyes forward, 1-2 frames late on the lean (overlap)
HEAD = {0: 8, 3: 14, 6: 24, 9: 38, 12: 38, 15: 30, 18: 22, 21: 16, 24: 14}   # r1b: chin forward in the lunge (arrow)
SHRUG = {0: 0, 4: 12, 6: 8, 9: 3, 14: 0, 24: 0}                  # clavicles up/forward in the load (hunched)

# Feet. Ankle targets (F, L, U); "planted" keys move back with the root so the foot stays still on the ground.
G = 0.094                    # ankle height, foot flat
LOAD_SLIDE = (0, 3)          # frames 1-4: both feet slide forward with the body (Smash: the dust sells it), no lift
# Right foot = the PUSH foot: planted in the world from frame 3 to toe-off at 7, then trails STRAIGHT behind (trait 2)
R_PLANT = (3, 7)             # world plant [first, last] (ball planted on its toes from 5)
R_AIR = {8: (-0.60, -0.13, 0.30), 11: (-0.66, -0.13, 0.34), 14: (-0.48, -0.14, 0.34), 17: (-0.18, -0.13, 0.38),
         20: (0.14, -0.12, 0.33), 24: (0.40, -0.12, 0.16)}
# Left foot = the LEAD foot: knee drive from the load, lunge thigh forward-up, heel-first contact at 15, planted to 21
L_AIR = {4: (0.06, 0.13, 0.13), 6: (0.16, 0.13, 0.24), 8: (0.30, 0.13, 0.32), 11: (0.36, 0.13, 0.30), 13: (0.43, 0.13, 0.19),
         15: (0.40, 0.13, G + 0.012)}
L_PLANT = (15, 20)          # flat 15-20, on the toes 21-22 (TOEFLAT), toe-off after 22
L_AFTER = {24: (-0.40, 0.13, 0.24)}                 # after the toe-off the foot kicks back up
STANCE = {'l': (0.05, 0.13), 'r': (-0.05, -0.13)}  # start stance (F, L)
# Foot pitch (deg, + = toe down / heel up) and how flat the toes stay on the ground (1 = ball flat, toes bent)
PITCH = {'r': {0: 0, 3: 0, 5: 18, 7: 48, 9: 68, 12: 70, 15: 45, 18: 18, 21: 0, 24: -12},
         'l': {0: 0, 3: 0, 4: 18, 6: 30, 9: 22, 12: 8, 14: -8, 15: -14, 16: 0, 20: 0, 21: 22, 22: 42, 24: 55}}
TOEFLAT = {'r': range(5, 8), 'l': range(21, 23)}

# Hands: fist targets relative to spine_05 (F, L, U), elbow poles relative to the shoulder. Load: tucked by the hips; lunge: both
# fists low and forward, the right (contralateral to the lead leg) further; then the run swing settles 2-3 frames after the catch.
HAND = {'l': {0: (0.02, 0.22, -0.40), 2: (-0.02, 0.21, -0.36), 4: (-0.06, 0.19, -0.30), 5: (-0.06, 0.19, -0.30),
              7: (0.04, 0.18, -0.30), 9: (0.12, 0.17, -0.32), 12: (0.10, 0.17, -0.33), 15: (-0.04, 0.19, -0.36),
              18: (-0.22, 0.20, -0.30), 21: (-0.12, 0.20, -0.36), 24: (0.16, 0.17, -0.28)},
        'r': {0: (0.02, -0.22, -0.40), 2: (-0.02, -0.21, -0.36), 4: (-0.06, -0.19, -0.30), 5: (-0.06, -0.19, -0.30),
              7: (0.12, -0.17, -0.28), 9: (0.26, -0.15, -0.30), 12: (0.27, -0.15, -0.31), 15: (0.22, -0.16, -0.27),
              18: (0.18, -0.17, -0.27), 21: (0.02, -0.19, -0.37), 24: (-0.20, -0.20, -0.31)}}
ELBOW_POLE = {'l': (-0.45, 0.12, -0.20), 'r': (-0.45, -0.12, -0.20)}   # r1b: back and down, not out (chicken wings in r1)
KNEE_POLE = {'l': (0.6, 0.06, 0.0), 'r': (0.6, -0.06, 0.0)}


# ------------------------------------------------------------------ scene
if bpy.app.version >= (2, 80):
    bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SKEL)
sc = bpy.context.scene
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
sc.frame_set(2)
P = arm.pose.bones
FIST = {pb.name: pb.matrix_basis.copy() for pb in P
        if pb.name.split('_')[0] in ('index', 'middle', 'ring', 'pinky', 'thumb')}


def _foot_yaw(s):
    d = arm.matrix_world @ P[f'ball_{s}'].head - arm.matrix_world @ P[f'foot_{s}'].head
    return math.degrees(math.atan2(d.x, -d.y))


def _flat_frame(s):
    """A frame of the skeleton take where foot s is flat on the ground (ankle 0.094, ball 0.032)."""
    for f in range(2, 40):
        sc.frame_set(f); bpy.context.view_layer.update(); w = arm.matrix_world
        if abs((w @ P[f'foot_{s}'].head).z - 0.094) < 0.004 and abs((w @ P[f'ball_{s}'].head).z - 0.032) < 0.004:
            return f
    raise RuntimeError(f'no flat frame for foot {s} in {SKEL}')


# Flat-foot world rotations. The skeleton take's bind pose is its first frame (run start r3), whose RIGHT foot is turned ~47 deg out
# (the twisted idle noted in r3/README): take each foot's rotation from a frame where it is flat, then turn the right foot so it
# points like the left one, mirrored (the ball sits ~8 deg inside the ankle line on the UEFN feet).
FLAT, YAW_L = {}, None
for s in 'lr':
    sc.frame_set(_flat_frame(s)); bpy.context.view_layer.update()
    FLAT[s] = {b: (arm.matrix_world.to_3x3().normalized() @ P[f'{b}_{s}'].matrix.to_3x3()).normalized() for b in ('foot', 'ball')}
    if s == 'l':
        YAW_L = _foot_yaw('l')
    else:
        fix = Matrix.Rotation(math.radians(-YAW_L - _foot_yaw('r')), 3, 'Z')
        FLAT['r'] = {b: fix @ m for b, m in FLAT['r'].items()}
sc.frame_set(2)
arm.animation_data_clear()
for pb in P:
    pb.rotation_mode = 'QUATERNION'
    pb.matrix_basis = Matrix.Identity(4)
vl = bpy.context.view_layer
arm.location = (0, 0, 0)
vl.update()
MW = arm.matrix_world
MWI = MW.inverted()


def wpos(n): return MW @ P[n].head


def rot_world(n, R):
    pb = P[n]; h = pb.head.copy()
    pb.matrix = Matrix.Translation(h) @ R @ Matrix.Translation(-h) @ pb.matrix
    vl.update()


def aim(n, child, target):
    cur = wpos(child) - wpos(n); want = target - wpos(n)
    if cur.length > 1e-6 and want.length > 1e-6:
        rot_world(n, cur.rotation_difference(want).to_matrix().to_4x4())


def two_bone(up, mid, end, target, pole):
    L1 = (REST[mid] - REST[up]).length; L2 = (REST[end] - REST[mid]).length
    H = wpos(up); d = target - H; reach = d.length; Dd = min(reach, (L1 + L2) * 0.995); d.normalize()
    pv = pole - H; n = pv - d * pv.dot(d); n = n.normalized() if n.length > 1e-6 else Vector((0, -1, 0))
    a_ = (L1 * L1 - L2 * L2 + Dd * Dd) / (2 * Dd); h = math.sqrt(max(L1 * L1 - a_ * a_, 0))
    aim(up, mid, H + d * a_ + n * h); aim(mid, end, H + d * Dd)
    return max(0.0, reach - (L1 + L2) * 0.995)


def set_world_rot(n, R3):
    """Bone's armature-space rotation = R3 (world axes; the armature only scales), head kept."""
    pb = P[n]; M = pb.matrix.copy(); loc = M.to_translation()
    M = R3.to_4x4(); M.translation = loc; pb.matrix = M; vl.update()


def line_yaw(a, b):
    v = wpos(b) - wpos(a); return math.degrees(math.atan2(v.y, v.x))


def lean_of(a, b):
    v = wpos(b) - wpos(a); return math.degrees(math.atan2(-v.y, v.z))


def fk_sync(ik, src):
    """ik_foot_*/ik_hand_* follow the real bone: the game's leg IK pulls the feet to them (crouch air v7 lesson)."""
    pb = P[ik]; pb.matrix = P[src].matrix.copy(); vl.update()


REST = {pb.name: wpos(pb.name).copy() for pb in P}
REST_ROT = {pb.name: pb.matrix.to_3x3().normalized() for pb in P}
REST_LEAN = lean_of('pelvis', 'neck_01')
_afb = REST['foot_l'] - REST['ball_l']                       # ankle relative to the ball, foot flat (left is flat in the bind pose)
ANK_FROM_BALL = {'l': _afb, 'r': Vector((-_afb.x, _afb.y, _afb.z))}


def rx(deg): return Matrix.Rotation(D(deg), 3, 'X')           # + = top (or toe) forward/down
def rz(deg): return Matrix.Rotation(D(deg), 3, 'Z')


def ankle_target(s, i):
    """(ankle world pos, planted ball world pos or None) for foot s at frame i."""
    F0, L0 = STANCE[s]
    if i <= LOAD_SLIDE[1]:
        return V(F0, L0, G), None
    if s == 'r':
        a, b = R_PLANT
        if i <= b:
            # planted from frame a: the ball stays where it was on frame a (world), i.e. back by the root's travel
            ball0 = V(F0, L0, G) - ANK_FROM_BALL['r']
            ball = ball0 - V(ROOT[i] - ROOT[a], 0, 0)
            pitch = catmull(PITCH['r'], i)
            if i in TOEFLAT['r']:
                return ball + rx(pitch) @ ANK_FROM_BALL['r'], ball
            return ball + ANK_FROM_BALL['r'], ball
        tab = dict(R_AIR); last = ankle_target('r', b)[0]
        tab[b] = (-last.y, last.x, last.z)
        return V(*catmull(tab, i)), None
    a, b = L_PLANT
    if i < a:
        tab = {LOAD_SLIDE[1]: (F0, L0, G), **L_AIR}
        return V(*catmull(tab, i)), None
    land = V(*L_AIR[a]); land.z = G
    if i <= b:
        return land - V(ROOT[i] - ROOT[a], 0, 0), None
    ball = land - ANK_FROM_BALL['l'] - V(ROOT[b] - ROOT[a], 0, 0)       # the ball stays where it was on the last flat frame
    toe = {k: ball + rx(catmull(PITCH['l'], k)) @ ANK_FROM_BALL['l'] for k in TOEFLAT['l']}
    if i in toe:
        return toe[i], ball
    k1 = max(TOEFLAT['l'])                                              # toe-off -> the foot kicks back up
    tab = {b: tuple(land - V(ROOT[b] - ROOT[a], 0, 0)), k1: tuple(toe[k1]), 24: tuple(V(*L_AFTER[24]))}
    return Vector(catmull(tab, i)), None


def pose(i):
    for pb in P:
        pb.matrix_basis = FIST.get(pb.name, Matrix.Identity(4))
    vl.update()
    # pelvis: height, forward, lean share, yaw
    L = eased(LEAN, i, LEAN_K)
    pb = P['pelvis']; M = pb.matrix.copy()
    M.translation = MWI @ Vector((REST['pelvis'].x, REST['pelvis'].y - catmull(PF, i), eased(PZ, i, PZ_K))); pb.matrix = M; vl.update()
    yaw = catmull(YAW, i)
    # square up the bind pose (hips 6 deg, shoulders 4 deg twisted, r3/README), then the authored yaw
    rot_world('pelvis', rz(-line_yaw('thigh_r', 'thigh_l')).to_4x4())
    rot_world('spine_01', rz(-line_yaw('upperarm_r', 'upperarm_l')).to_4x4())
    rot_world('pelvis', (rz(-yaw) @ rx((L - REST_LEAN) * 0.5)).to_4x4())
    hunch = catmull(HUNCH, i)
    for n in ('spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05'):
        extra = hunch / 2 if n in ('spine_04', 'spine_05') else 0.0
        rot_world(n, rx((L - REST_LEAN) * 0.5 / 5 + extra).to_4x4())
    rot_world('spine_03', rz(yaw * 0.8).to_4x4())              # shoulders counter the hips
    for _ in range(2):                                          # match the measured lean (the hunch bends the line)
        rot_world('pelvis', rx(L - lean_of('pelvis', 'neck_01')).to_4x4())
    for s, sg in (('l', 1), ('r', -1)):
        sh = catmull(SHRUG, i)
        rot_world(f'clavicle_{s}', (Matrix.Rotation(D(-sh * sg), 3, 'Y') @ rz(sh * 0.4 * sg)).to_4x4())
    # head: world pitch of the neck_01 -> head line
    want = catmull(HEAD, i)
    for _ in range(2):                                          # neck_02 moves the line too: two passes converge
        for n in ('neck_01', 'neck_02'):
            rot_world(n, rx((want - lean_of('neck_01', 'head')) * 0.5).to_4x4())
    # arms
    for s in 'lr':
        tgt = wpos('spine_05') + V(*catmull(HAND[s], i))
        two_bone(f'upperarm_{s}', f'lowerarm_{s}', f'hand_{s}', tgt, wpos(f'upperarm_{s}') + V(*ELBOW_POLE[s]))
    # legs
    stretch = {}
    for s in 'lr':
        ank, ball = ankle_target(s, i)
        knee_pole = wpos(f'thigh_{s}') + V(*KNEE_POLE[s])
        stretch[s] = two_bone(f'thigh_{s}', f'calf_{s}', f'foot_{s}', ank, knee_pole)
        pitch = catmull(PITCH[s], i)
        set_world_rot(f'foot_{s}', rx(pitch) @ FLAT[s]['foot'])
        flat = i in TOEFLAT[s]
        set_world_rot(f'ball_{s}', (FLAT[s]['ball'] if flat else rx(pitch) @ FLAT[s]['ball']))
        fk_sync(f'ik_foot_{s}', f'foot_{s}')
        fk_sync(f'ik_hand_{s}', f'hand_{s}')
    return stretch


# ------------------------------------------------------------------ bake
sc.render.fps = FPS
sc.frame_start, sc.frame_end = 1, N
metrics, frames = [], {}
JOINTS = ['pelvis', 'spine_03', 'spine_05', 'neck_01', 'head', 'clavicle_l', 'clavicle_r'] + \
         [f'{b}_{s}' for s in 'lr' for b in ('upperarm', 'lowerarm', 'hand', 'thigh', 'calf', 'foot', 'ball')]
for i in range(N):
    sc.frame_set(i + 1)
    arm.location = (0, 0, 0); vl.update()
    st = pose(i)
    for pb in P:
        pb.keyframe_insert('location', frame=i + 1); pb.keyframe_insert('rotation_quaternion', frame=i + 1)
    j = {n: tuple(round(x, 4) for x in wpos(n)) for n in JOINTS}
    frames[i] = j
    metrics.append(dict(frame=i + 1, lean=round(lean_of('pelvis', 'neck_01'), 1), head=round(lean_of('neck_01', 'head'), 1),
                        pelvis_z=round(wpos('pelvis').z, 3), ball_z={s: round(wpos(f'ball_{s}').z, 3) for s in 'lr'},
                        ankle_z={s: round(wpos(f'foot_{s}').z, 3) for s in 'lr'},
                        split=round(abs(wpos('foot_l').y - wpos('foot_r').y), 3),
                        stretch={s: round(v, 3) for s, v in st.items()},
                        ball_world_f={s: round(-wpos(f'ball_{s}').y + ROOT[i], 3) for s in 'lr'},
                        fist_from_spine={s: round(_d, 3) for s, _d in (
                            (s, (wpos(f'hand_{s}') - wpos('spine_03')).length) for s in 'lr')}))
    arm.location = (0, -ROOT[i] * 1.0, 0)
    arm.keyframe_insert('location', frame=i + 1)
arm.location = (0, 0, 0)

# planted-foot slide (world): ball stays put while planted
def slide(s, a, b):
    xs = [m['ball_world_f'][s] for m in metrics[a:b + 1]]
    return round(max(xs) - min(xs), 3)
summary = dict(frames=N, fps=FPS, root_travel_m=round(ROOT[-1], 3), root_speed_end=TOP,
               lean_max=max(m['lean'] for m in metrics), lean_lunge=[m['lean'] for m in metrics[8:12]],
               pelvis_min=min(m['pelvis_z'] for m in metrics), ball_min=min(min(m['ball_z'].values()) for m in metrics),
               split_lunge=[m['split'] for m in metrics[8:12]],
               slide_r_plant=slide('r', *R_PLANT), slide_l_plant=slide('l', *L_PLANT),
               max_stretch=max(max(m['stretch'].values()) for m in metrics),
               fist_min=min(min(m['fist_from_spine'].values()) for m in metrics))
json.dump(dict(summary=summary, per_frame=metrics, root=[round(x, 4) for x in ROOT]), open(os.path.join(OUT, 'metrics.json'), 'w'), indent=1)
json.dump(dict(fps=FPS, root=ROOT, frames=frames), open(os.path.join(OUT, 'frames.json'), 'w'))

# ------------------------------------------------------------------ export (same settings as the run start r3 that imported fine)
for o in sc.objects: o.select_set(False)
arm.select_set(True); vl.objects.active = arm
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, NAME + '.fbx'), use_selection=True, object_types={'ARMATURE'},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0)
print(NAME, 'frames', N, 'root', round(ROOT[-1], 3), 'm')
print('lean lunge', summary['lean_lunge'], 'pelvis min', summary['pelvis_min'], 'ball min', summary['ball_min'])
print('slide plant r/l', summary['slide_r_plant'], summary['slide_l_plant'], 'stretch', round(summary['max_stretch'], 3),
      'fist min', summary['fist_min'])
