import bpy, sys, os, math, json
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, outdir, tag = argv[0], argv[1], argv[2]
os.makedirs(outdir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
sc = bpy.context.scene
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
act = arm.animation_data.action
f0, f1 = [int(round(v)) for v in act.frame_range]
fps = sc.render.fps / sc.render.fps_base
nfr = f1 - f0 + 1
dur = (nfr - 1) / fps

def bone(suffix):
    return next(b for b in arm.pose.bones if b.name.split(':')[-1] == suffix)

LF, RF = bone('LeftFoot'), bone('RightFoot')
LT, RT = bone('LeftToeBase'), bone('RightToeBase')
LA, RA = bone('LeftArm'), bone('RightArm')
HIP = bone('Hips')
mw = arm.matrix_world

rows = []
for f in range(f0, f1 + 1):
    sc.frame_set(f)
    W = lambda b, tail=False: mw @ (b.tail if tail else b.head)
    rows.append(dict(
        f=f,
        lz=min(W(LF).z, W(LT).z), rz=min(W(RF).z, W(RT).z),
        la=(W(LA, True) - W(LA)).normalized(), ra=(W(RA, True) - W(RA)).normalized(),
        hip=W(HIP)))

# forward axis = hip travel if it moves, else -Y (Mixamo default facing)
travel = rows[-1]['hip'] - rows[0]['hip']
travel.z = 0
fwd = travel.normalized() if travel.length > 0.3 else Vector((0, -1, 0))

def contacts(key):
    # stance phases = cyclic runs of frames below 20% of the foot-height range; contact = first frame of each run
    z = [r[key] for r in rows]
    if abs(z[0] - z[-1]) < 0.01: z = z[:-1]  # looping clip: last frame repeats first
    lo, hi = min(z), max(z); thr = lo + 0.2 * (hi - lo)
    low = [v <= thr for v in z]; n = len(z)
    return [rows[i]['f'] for i in range(n) if low[i] and not low[i - 1]]

cl, cr = contacts('lz'), contacts('rz')
steps = len(cl) + len(cr)
cyc = nfr - 1 if abs(rows[0]['lz'] - rows[-1]['lz']) < 0.01 else nfr
sps = steps / (cyc / fps)

def amp(k):  # largest angle between any two upper-arm directions over the clip
    ds = [r[k] for r in rows]
    return max(math.degrees(d1.angle(d2)) for d1 in ds for d2 in ds)

def pitch(d):  # sagittal angle from straight down, + = forward
    return math.degrees(math.atan2(d.dot(fwd), -d.z))
pf = [pitch(r[k]) for r in rows for k in ('la', 'ra')]

res = dict(clip=tag, frames=nfr, fps=round(fps, 3), duration_s=round(dur, 3),
           left_contacts=cl, right_contacts=cr, steps=steps, steps_per_s=round(sps, 2),
           arm_swing_L_deg=round(amp('la'), 1), arm_swing_R_deg=round(amp('ra'), 1),
           arm_fwd_max_deg=round(max(pf), 1), arm_back_max_deg=round(min(pf), 1),
           root_travel_m=round(travel.length, 2), speed_mps=round(travel.length / dur, 2))
with open(os.path.join(outdir, tag + '.json'), 'w') as fh:
    json.dump(res, fh)
print('OK', tag, nfr, round(sps, 2))
if '--norender' in argv: sys.exit(0)

# ---- render side-view grid
# clip FBX has no mesh: bring in the X Bot skin from the same pack and drive it with this clip's action
xbot = os.path.join(os.path.dirname(fbx), 'X Bot.fbx')
before = set(sc.objects)
bpy.ops.import_scene.fbx(filepath=xbot)
xarm = next(o for o in set(sc.objects) - before if o.type == 'ARMATURE')
xarm.matrix_world = arm.matrix_world.copy()
if xarm.animation_data is None: xarm.animation_data_create()
xarm.animation_data.action = act
try:
    xarm.animation_data.action_slot = act.slots[0]
except Exception:
    pass
arm.hide_render = True
mat = bpy.data.materials.new('grey')
for o in sc.objects:
    if o.type == 'MESH':
        o.data.materials.clear(); o.data.materials.append(mat)
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, 0))
try:
    sc.render.engine = 'BLENDER_WORKBENCH'
except TypeError:
    pass
w = bpy.data.worlds.new('w'); sc.world = w; w.color = (0.8, 0.8, 0.8)
try:
    w.use_nodes = True; bg = next(n for n in w.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs[0].default_value = (0.8, 0.8, 0.8, 1); bg.inputs[1].default_value = 1.0
except Exception:
    pass
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); sc.collection.objects.link(sun)
sun.data.energy = 3; sun.rotation_euler = (0.8, 0.2, 0.6)
sh = sc.display.shading
sh.light = 'STUDIO'; sh.color_type = 'SINGLE'; sh.single_color = (0.6, 0.6, 0.6)
sh.show_shadows = True
sc.render.resolution_x = sc.render.resolution_y = 512
sc.render.film_transparent = False
cam_d = bpy.data.cameras.new('cam'); cam_d.type = 'ORTHO'; cam_d.ortho_scale = 2.6
cam = bpy.data.objects.new('cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
side = fwd.cross(Vector((0, 0, 1))).normalized()  # character's right
frames = [f0 + round(i * (nfr - 1) / 8) for i in range(8)]
paths = []
for i, f in enumerate(frames):
    sc.frame_set(f)
    h = mw @ HIP.head
    c = Vector((h.x, h.y, 1.0)) + side * 6.0
    cam.location = c
    cam.rotation_euler = (-side).to_track_quat('-Z', 'Y').to_euler()
    p = os.path.join(outdir, f'{tag}_{i}.png'); sc.render.filepath = p
    bpy.ops.render.render(write_still=True); paths.append((p, f))
with open(os.path.join(outdir, tag + '_frames.json'), 'w') as fh:
    json.dump(paths, fh)
print('OK', tag, nfr, round(sps, 2))
