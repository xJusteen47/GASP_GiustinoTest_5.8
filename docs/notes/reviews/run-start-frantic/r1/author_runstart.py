"""Frantic run start r1 (Overlord, all kits): 27 frames @30 fps, in-place root, UEFN mannequin skeleton.

Usage: blender -b --factory-startup --python author_runstart.py -- <idle.fbx> <outdir>
Builds the pose from the idle frame 1, drives feet/hands with IK targets keyed at the key frames of
brief.md (timing B: f1-10 stamping almost in place, f11-27 three travelling run steps), bakes to FK,
exports FBX + .blend, renders side and 3/4 cells (every frame) and writes metrics.json.
Blender world: forward = -Y, character left = +X, up = +Z, metres.
"""
import bpy, sys, os, math, json
from mathutils import Vector, Matrix, Euler

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

rest = {n: wpos(n).copy() for n in ('pelvis', 'thigh_l', 'thigh_r', 'foot_l', 'foot_r', 'upperarm_l', 'upperarm_r')}
foot_rest_rot = {s: (MW @ P['foot_' + s].matrix).to_quaternion() for s in 'lr'}
ANKLE_Z = rest['foot_l'].z  # 0.094 flat on ground

# ------------------------------------------------------------------ key tables
# feet: frame -> (y, z, toe-down pitch deg); x fixed per side. y<0 = in front of the body.
FX = {'l': 0.13, 'r': -0.13}
G = ANKLE_Z
FOOT = {
 'l': {1: (0.02, G, 0), 3: (0.04, G, 0), 4: (0.00, 0.22, 20), 5: (-0.06, 0.30, 25), 6: (-0.02, G, 0),
       8: (0.02, G, 0), 9: (0.04, G, 0), 10: (0.08, 0.20, 30), 11: (-0.05, 0.38, 25), 12: (-0.30, 0.25, 5),
       13: (-0.28, G, 0), 16: (0.10, 0.12, 30), 18: (0.30, 0.38, 50), 21: (-0.05, 0.40, 20), 23: (-0.32, 0.22, 0),
       25: (-0.30, G, 0), 27: (-0.05, G, 0)},
 'r': {1: (-0.12, 0.32, 25), 3: (-0.06, G, 0), 5: (-0.03, G, 0), 6: (-0.02, G, 0), 7: (-0.06, 0.24, 20),
       8: (-0.12, 0.30, 25), 9: (-0.08, G, 0), 12: (0.15, 0.12, 30), 14: (0.30, 0.35, 50), 16: (-0.05, 0.40, 20),
       17: (-0.32, 0.22, 0), 19: (-0.30, G, 0), 22: (0.10, 0.12, 30), 24: (0.30, 0.36, 45), 27: (-0.02, 0.38, 20)},
}
CONTACTS = {'l': [6, 13, 25], 'r': [3, 9, 19]}
# pelvis height (m), lateral sway toward stance foot (m), yaw deg (- = left hip forward)
PELV = {1: (0.84, 0.00, 6), 3: (0.80, -0.02, 6), 4: (0.84, 0.0, 0), 6: (0.80, 0.02, -6), 7: (0.84, 0.0, 0),
        9: (0.80, -0.02, 6), 11: (0.87, 0.0, 0), 13: (0.83, 0.02, -6), 16: (0.88, 0.0, 0), 19: (0.83, -0.02, 6),
        22: (0.88, 0.0, 0), 25: (0.845, 0.02, -6), 27: (0.865, 0.01, -3)}
PELV_CONTACT = {3, 6, 9, 13, 19, 25}
# torso lean deg (total, spread pelvis 25% / spine 75%); one undershoot at the hand-over then settle
LEAN = {1: 30, 3: 27, 6: 26, 9: 26, 11: 28, 13: 24, 19: 21, 23: 17, 25: 13, 27: 14}
# arms: 'F' forward pump, 'B' back pump, 's' scramble amplitude, 'r' run amplitude; 1 frame after the contact
ARMS = {'l': {1: 'Fs', 4: 'Fs', 7: 'Bs', 10: 'Fs', 14: 'Br', 20: 'Fr', 26: 'Br', 27: 'Br'},
        'r': {1: 'Bs', 4: 'Bs', 7: 'Fs', 10: 'Bs', 14: 'Fr', 20: 'Br', 26: 'Fr', 27: 'Fr'}}
HAND_OFS = {'Fs': (-0.05, -0.34, -0.04), 'Bs': (0.03, 0.30, -0.20),   # wide frantic: fist up near the chin / far back
            'Fr': (-0.05, -0.30, -0.14), 'Br': (0.03, 0.28, -0.24)}

def _seg(tab, f):
    ks = sorted(tab)
    if f <= ks[0]: return ks[0], ks[0], 0.0, ks
    if f >= ks[-1]: return ks[-1], ks[-1], 0.0, ks
    for a, b in zip(ks, ks[1:]):
        if a <= f <= b: return a, b, (f - a) / (b - a), ks

def _num(v): return v if isinstance(v, tuple) else (v,)

def interp_table(tab, f, ease=None):
    """Catmull-Rom through the keys (fluid arcs); ease(a, b) may return 'in'/'out' to override a channel segment."""
    a, b, t, ks = _seg(tab, f)
    if a == b: return tab[a]
    i = ks.index(a)
    p0 = _num(tab[ks[max(i - 1, 0)]]); p1 = _num(tab[a]); p2 = _num(tab[b]); p3 = _num(tab[ks[min(i + 2, len(ks) - 1)]])
    t2, t3 = t * t, t * t * t
    cr = tuple(0.5 * (2 * q1 + (-q0 + q2) * t + (2 * q0 - 5 * q1 + 4 * q2 - q3) * t2 + (-q0 + 3 * q1 - 3 * q2 + q3) * t3)
               for q0, q1, q2, q3 in zip(p0, p1, p2, p3))
    out = cr if isinstance(tab[a], tuple) else cr[0]
    return out

def eased(tab, f, idx, contacts):
    """Height channel: accelerate into a contact (ease-in, heavy stomp), snap out of it (ease-out)."""
    a, b, t, ks = _seg(tab, f)
    va, vb = _num(tab[a])[idx], _num(tab[b])[idx]
    if a == b: return va
    if b in contacts: u = t * t
    elif a in contacts: u = 1 - (1 - t) ** 2
    else: u = t * t * (3 - 2 * t)
    return va + (vb - va) * u

SPINE = ['spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05']
def rot_world(n, R):
    pb = P[n]; h = pb.head.copy()
    pb.matrix = Matrix.Translation(h) @ R @ Matrix.Translation(-h) @ pb.matrix
    vl.update()

def aim(n, child, target_world):
    """Rotate bone n about its head so that its child joint points at target_world (keeps twist minimal)."""
    cur = wpos(child) - wpos(n); want = target_world - wpos(n)
    if cur.length < 1e-6 or want.length < 1e-6: return
    rot_world(n, cur.rotation_difference(want).to_matrix().to_4x4())

def two_bone(upper, mid, end, target, pole, L1, L2):
    H = wpos(upper); d = target - H; D = min(d.length, (L1 + L2) * 0.999); d.normalize()
    pv = pole - H; n = (pv - d * pv.dot(d))
    n = n.normalized() if n.length > 1e-6 else Vector((0, -1, 0))
    a_ = (L1 * L1 - L2 * L2 + D * D) / (2 * D); h = math.sqrt(max(L1 * L1 - a_ * a_, 0))
    knee = H + d * a_ + n * h
    aim(upper, mid, knee)
    aim(mid, end, H + d * D)

LEN = {}
for s in 'lr':
    LEN['leg_' + s] = ((wpos('calf_' + s) - wpos('thigh_' + s)).length, (wpos('foot_' + s) - wpos('calf_' + s)).length)
    LEN['arm_' + s] = ((wpos('lowerarm_' + s) - wpos('upperarm_' + s)).length, (wpos('hand_' + s) - wpos('lowerarm_' + s)).length)

ARM_T = {}
for s in 'lr':
    ARM_T[s] = {f: HAND_OFS[k] for f, k in ARMS[s].items()}
    ARM_T[s + '_pole'] = {f: ((0.25, 0.15, -0.5) if k[0] == 'F' else (0.2, 0.5, 0.15)) for f, k in ARMS[s].items()}

KEYED = ['pelvis'] + SPINE + ['clavicle_l', 'clavicle_r', 'neck_01', 'head'] + \
        [f'{b}_{s}' for s in 'lr' for b in ('thigh', 'calf', 'foot', 'upperarm', 'lowerarm')]
for f in range(F0, F1 + 1):
    for pb in P: pb.matrix_basis = base[pb.name]
    vl.update()
    _, sway, yaw = interp_table(PELV, f)
    z = eased(PELV, f, 0, PELV_CONTACT)
    lean = interp_table(LEAN, f)
    pb = P['pelvis']; M = pb.matrix.copy()
    M.translation = MW.inverted() @ Vector((rest['pelvis'].x + sway, rest['pelvis'].y, z)); pb.matrix = M; vl.update()
    rot_world('pelvis', Matrix.Rotation(D(yaw), 4, 'Z') @ Matrix.Rotation(D(lean * 0.25), 4, 'X'))
    for n in SPINE:
        R = Matrix.Rotation(D(lean * 0.75 / 5), 4, 'X')
        if n == 'spine_04': R = Matrix.Rotation(D(-yaw * 1.6), 4, 'Z') @ R
        rot_world(n, R)
    for s, sg in (('l', 1), ('r', -1)):
        rot_world('clavicle_' + s, Matrix.Rotation(D(6), 4, 'X') @ Matrix.Rotation(D(-6 * sg), 4, 'Y'))
    lean_h = interp_table(LEAN, max(F0, f - 1))           # head 1 frame late
    rot_world('neck_01', Matrix.Rotation(D(-lean_h * 0.35), 4, 'X'))
    rot_world('head', Matrix.Rotation(D(-lean_h * 0.25), 4, 'X'))
    for s, sg in (('l', 1), ('r', -1)):
        ft = FOOT[s]
        y = interp_table({k: v[0] for k, v in ft.items()}, f)
        zf = eased(ft, f, 1, set(CONTACTS[s]))
        pitch = interp_table({k: v[2] for k, v in ft.items()}, f)
        tgt = Vector((FX[s], y, max(zf, ANKLE_Z)))
        two_bone('thigh_' + s, 'calf_' + s, 'foot_' + s, tgt, Vector((FX[s] * 1.3, y - 0.8, 0.55)), *LEN['leg_' + s])
        q = Matrix.Rotation(D(pitch), 4, 'X').to_quaternion() @ foot_rest_rot[s]
        fb = P['foot_' + s]; Mf = q.to_matrix().to_4x4(); Mf.translation = fb.matrix.translation; fb.matrix = Mf; vl.update()
        sh = wpos('upperarm_' + s)
        ox, oy, oz = interp_table(ARM_T[s], f); px, py, pz = interp_table(ARM_T[s + '_pole'], f)
        two_bone('upperarm_' + s, 'lowerarm_' + s, 'hand_' + s, sh + Vector((ox * sg, oy, oz)),
                 sh + Vector((px * sg, py, pz)), *LEN['arm_' + s])
    for n in KEYED:
        P[n].keyframe_insert('rotation_quaternion', frame=f)
    P['pelvis'].keyframe_insert('location', frame=f)
cal = 'analytic two-bone IK (no Blender IK)'
sc.frame_start, sc.frame_end = F0, F1
act = arm.animation_data.action
act.name = 'AS_RunStart_Frantic_r1'

# ------------------------------------------------------------------ metrics
m = dict(frames=F1 - F0 + 1, fps=30, pole_cal=cal, per_frame=[])
up0 = {}
for f in range(F0, F1 + 1):
    sc.frame_set(f); vl.update()
    row = dict(f=f, pelvis_z=round(wpos('pelvis').z, 3))
    for s in 'lr':
        fw = MW @ P['foot_' + s].head; bw = MW @ P['ball_' + s].head
        row['foot_' + s] = [round(fw.y, 3), round(fw.z, 3)]; row['ball_z_' + s] = round(bw.z, 3)
        ua = (MW @ P['lowerarm_' + s].head) - (MW @ P['upperarm_' + s].head)
        row['uarm_' + s] = [round(v, 3) for v in ua.normalized()]
        el = (MW @ P['upperarm_' + s].head - MW @ P['lowerarm_' + s].head).normalized()
        fa = (MW @ P['hand_' + s].head - MW @ P['lowerarm_' + s].head).normalized()
        row['elbow_deg_' + s] = round(math.degrees(el.angle(fa)), 1)
        row['hand_' + s] = [round(v, 3) for v in (MW @ P['hand_' + s].head)]
    m['per_frame'].append(row)
def swing(s):
    ds = [Vector(r['uarm_' + s]) for r in m['per_frame']]
    return round(max(math.degrees(a.angle(b)) for a in ds for b in ds), 1)
m['arm_swing_deg'] = {s: swing(s) for s in 'lr'}
m['contacts'] = CONTACTS
m['steps_per_s_scramble'] = round(3 / ((9 - 3) / 30 + 3 / 30), 2)
m['steps_per_s_run'] = round(30 / 6, 2)
m['min_ball_z'] = min(min(r['ball_z_l'], r['ball_z_r']) for r in m['per_frame'])
m['min_ankle_z'] = min(min(r['foot_l'][1], r['foot_r'][1]) for r in m['per_frame'])
json.dump(m, open(os.path.join(OUT, 'metrics.json'), 'w'), indent=1)

# ------------------------------------------------------------------ export
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); vl.objects.active = arm
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, 'AS_RunStart_Frantic_r1.fbx'), use_selection=True,
                         object_types={'ARMATURE'}, add_leaf_bones=False, bake_anim=True,
                         bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'runstart_frantic_r1.blend'))

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
