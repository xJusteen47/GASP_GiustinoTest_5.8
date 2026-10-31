# Dense-key test: frames 1..8 keyed every frame from our own easing curve, LINEAR between,
# on a copy of jab_test (saved as jab_dense.casc). Cascadeur space: Y up, forward +Z, right = -X.
import csc, json, math, os
import numpy as np

app = csc.app.get_application()
vs = app.get_scene_manager().current_scene()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
assert vs.get_path_name().endswith("jab_test.casc"), vs.get_path_name()
vs.save(base + "jab_dense.casc")  # from here on the tab is the copy

mv = scene.model_viewer(); lv = scene.layers_viewer(); dv = scene.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
layer_ids = list(lv.all_layer_ids())
tools = sorted(t.name() for t in app.get_tools_manager().tools() if hasattr(t, "name"))

contact = {"hand_MainPoint_r": (-7, 138, 58), "lowerarm_MainPoint_r": (-12, 136, 31), "upperarm_MainPoint_r": (-15.5, 133.0, 6.0),
           "hand_MainPoint_l": (14, 142, 18), "lowerarm_MainPoint_l": (23, 115, 0), "upperarm_MainPoint_l": (18.5, 132.9, -7.0)}
guard = {"hand_MainPoint_r": (-14, 138, 18), "lowerarm_MainPoint_r": (-24, 112, 2), "upperarm_MainPoint_r": (-17.7, 132.9, -6.0),
         "hand_MainPoint_l": (14, 142, 22), "lowerarm_MainPoint_l": (23, 115, 4), "upperarm_MainPoint_l": (17.7, 132.9, -1.0)}
settle = {k: tuple(guard[k][i] + (contact[k][i] - guard[k][i]) * 0.85 for i in range(3)) for k in contact}

def smoothstep(x):
    x = min(1.0, max(0.0, x)); return x * x * (3 - 2 * x)
def overshoot(t):  # cm added along +Z on the right arm chain: peak 6 cm at t=0.3, gone by t=0.6
    return 6.0 * math.sin(math.pi * t / 0.6) if t < 0.6 else 0.0
def pose_at(f):
    t = (f - 1) / 7.0
    s = smoothstep((t - 0.3) / 0.7)  # contact -> settle after the overshoot
    o = overshoot(t)
    p = {}
    for k in contact:
        v = [contact[k][i] + (settle[k][i] - contact[k][i]) * s for i in range(3)]
        if k.endswith("_r"):
            v[2] += o
        p[k] = tuple(v)
    return p
FRAMES = list(range(1, 9))
curve = {f: round(pose_at(f)["hand_MainPoint_r"][2], 2) for f in FRAMES}

def add_keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in FRAMES:
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
ok1 = scene.modify_update("Dense: keys", add_keys)

def set_poses(model, update, su):
    su.generate_update()
    for f in FRAMES:
        d = set()
        for name, p in pose_at(f).items():
            node = update.get_object_by_id(ids[name]).root_group().node_deep("Position")
            node.set_value(np.array(p, dtype=np.float32), f)
            d.add(node.data_id())
        su.run_update(d, f)
ok2 = scene.modify_update("Dense: poses", set_poses)

I = csc.layers.layer.Interpolation
def set_interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in FRAMES[:-1]:
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.LINEAR))
ok3 = scene.modify_update("Dense: linear", set_interp)

def pos(n, f):
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == "Position":
                return [round(float(x), 2) for x in dv.get_data_value(d, f)]
        except Exception:
            pass
hand = {f: pos("hand_r", f)[2] for f in range(0, 10)}
feet = {f: (pos("foot_l", f), pos("foot_r", f)) for f in (0, 1, 3, 5, 8)}
vs.save(base + "jab_dense.casc")
loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(vs)
st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
loader.export_joints(base + "jab_dense.fbx")
L = lv.layer(lv.layer_id_by_obj_id(ids["hand_MainPoint_r"]))
print(json.dumps({"ok": [ok1, ok2, ok3], "tools": tools, "keys": sorted(L.key_frame_indices()),
                  "target_hand_point_z": curve, "solved_hand_r_joint_z": hand, "feet": feet,
                  "fbx": os.path.getsize(base + "jab_dense.fbx")}, default=str))
