import os
# Pass 5 on a copy (jab_v5.casc): stronger push at contact (Sakurai "too much is just right"), owner-approved.
# Deltas are added on top of v4 at frames 1 and 3 (and 60% at 5 for the lagging torso). Feet stay planted.
import csc, json, time
import numpy as np
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
src = [s for s in sm.scenes() if s.name() == "jab_v4.casc"][0]
sm.set_current_scene(src)
src.save(base + "jab_v5.casc")
vs = [s for s in sm.scenes() if s.name() == "jab_v5.casc"][0]
ds = vs.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
KEYS = sorted(lv.layer(list(lv.all_layer_ids())[0]).key_frame_indices())
def pos(name, f):
    for d in dv.get_all_data_id(ids[name]):
        try:
            if dv.get_data_name(d) == "Position":
                return np.array(dv.get_data_value(d, f), dtype=np.float32)
        except Exception:
            pass
def V(x=0, y=0, z=0): return np.array([x, y, z], dtype=np.float32)
G = {
    "pelvis": ["pelvis_MainPoint", "pelvis_AdditionalPoint"],
    "spine2": ["spine_02_MainPoint", "spine_02_AdditionalPoint"],
    "chest": ["spine_04_MainPoint", "spine_04_AdditionalPoint"],
    "neck": ["neck_01_MainPoint", "neck_01_AdditionalPoint"],
    "head": ["head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint"],
    "sho_l": ["clavicle_MainPoint_l", "clavicle_AdditionalPoint_l", "upperarm_MainPoint_l"],
    "arm_l": ["lowerarm_MainPoint_l", "lowerarm_AdditionalPoint_l", "hand_MainPoint_l", "hand_DirectionPoint_l", "hand_AdditionalPoint_l"],
    "sho_r": ["clavicle_MainPoint_r", "clavicle_AdditionalPoint_r", "upperarm_MainPoint_r"],
    "forearm_r": ["lowerarm_MainPoint_r", "lowerarm_AdditionalPoint_r", "hand_MainPoint_r", "hand_DirectionPoint_r", "hand_AdditionalPoint_r"],
}
PUSH = {"pelvis": V(0, -3, 3), "spine2": V(0, -3, 5), "chest": V(0, -3, 7), "neck": V(0, -3, 8), "head": V(0, -3, 8.5),
        "sho_l": V(0, -3, 7), "arm_l": V(0, -3, 9), "sho_r": V(0, -2, -5), "forearm_r": V(0, 5, -1)}
PLAN = {1: (1.0, PUSH), 3: (1.0, PUSH), 5: (0.6, {k: v for k, v in PUSH.items() if k in ("pelvis", "spine2", "chest", "neck", "head", "sho_r")})}
before = {f: {"hand_l_z": round(float(pos("hand_l", f)[2]), 1), "head_z": round(float(pos("head", f)[2]), 1),
              "pelvis_y": round(float(pos("pelvis", f)[1]), 1), "hand_r_y": round(float(pos("hand_r", f)[1]), 1)} for f in (1, 3, 5)}
targets = {}
for f, (k, deltas) in PLAN.items():
    targets[f] = {n: pos(n, f) + d * k for g, d in deltas.items() for n in G[g]}
FEET = [n for n in ids if mv.get_object_type_name(ids[n]) == "Point" and (n.startswith("foot_") or n.startswith("ball_"))]
feet0 = {n: pos(n, 0) for n in FEET}
for f in KEYS:
    targets.setdefault(f, {}).update(feet0)
def push(model, update, su):
    su.generate_update()
    for f in sorted(targets):
        dids = set()
        for n, p in targets[f].items():
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(p, f)
            dids.add(node.data_id())
        su.run_update(dids, f)
ok = ds.modify_update("v5: stronger push at contact", push)
vs.save(base + "jab_v5.casc")
after = {f: {"hand_l_z": round(float(pos("hand_l", f)[2]), 1), "head_z": round(float(pos("head", f)[2]), 1),
             "pelvis_y": round(float(pos("pelvis", f)[1]), 1), "hand_r_y": round(float(pos("hand_r", f)[1]), 1)} for f in (1, 3, 5)}
print(json.dumps({"ok": ok, "before": before, "after": after, "sec": round(time.time() - t0, 1)}))
