import os
# Pass 2 on a copy (jab_v2.casc): recovery into the fighting stance, pushed contact poses, moving hold, overlap, tail settle.
# Cascadeur space: Y up, punch toward +Z, character's right = -X. Units cm. Body parts move as rigid groups
# (main + additional + direction points together) so hand and head orientation is kept and twist flips are avoided.
import csc, json, time
import numpy as np
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
src = [s for s in sm.scenes() if s.name() == "jab_v1.casc"][0]
sm.set_current_scene(src)
src.save(base + "jab_v2.casc")          # tab becomes v2; v1 file stays on disk
vs = [s for s in sm.scenes() if s.name() == "jab_v2.casc"][0]
ds = vs.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
objs = mv.get_objects(); ids = {mv.get_object_name(o): o for o in objs}
layer_ids = list(lv.all_layer_ids())

G = {
    "pelvis": ["pelvis_MainPoint", "pelvis_AdditionalPoint"],
    "spine2": ["spine_02_MainPoint", "spine_02_AdditionalPoint"],
    "chest": ["spine_04_MainPoint", "spine_04_AdditionalPoint"],
    "neck": ["neck_01_MainPoint", "neck_01_AdditionalPoint"],
    "head": ["head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint"],
    "sho_l": ["clavicle_MainPoint_l", "clavicle_AdditionalPoint_l", "upperarm_MainPoint_l"],
    "arm_l": ["lowerarm_MainPoint_l", "lowerarm_AdditionalPoint_l", "hand_MainPoint_l", "hand_DirectionPoint_l", "hand_AdditionalPoint_l"],
    "sho_r": ["clavicle_MainPoint_r", "clavicle_AdditionalPoint_r", "upperarm_MainPoint_r"],
    "elb_r": ["lowerarm_MainPoint_r", "lowerarm_AdditionalPoint_r"],
    "hand_r": ["hand_MainPoint_r", "hand_DirectionPoint_r", "hand_AdditionalPoint_r"],
}
def V(x=0, y=0, z=0): return np.array([x, y, z], dtype=np.float32)
CONTACT = {"pelvis": V(0, -3, 2), "spine2": V(0, -3, 3), "chest": V(0, -2.5, 5), "neck": V(0, -2, 5.5), "head": V(0, -2, 6),
           "sho_l": V(0, -2, 6), "arm_l": V(0, -2, 7), "sho_r": V(0, -2, -3), "elb_r": V(-6, 6, 2), "hand_r": V(0, 9, 5)}
def scaled(d, k, **extra):
    out = {g: v * k for g, v in d.items()}
    for g, v in extra.items():
        out[g] = out.get(g, V()) + v
    return out
# frame -> (base frame to read from, deltas)
PLAN = {
    1: (1, CONTACT),
    3: (3, scaled(CONTACT, 1.0, arm_l=V(0, 0, 1.5), chest=V(0, 0, 1.0), head=V(0, 0, 1.5), neck=V(0, 0, 1.2), hand_r=V(0, -1, 0))),  # moving hold + overlap
    5: (5, {**scaled(CONTACT, 0.7), "arm_l": V(0, -1.5, 3), "sho_l": V(0, -1.5, 4)}),   # arm already retracting, torso/head lag
    8: (8, {"pelvis": V(0, -1.5, 1), "spine2": V(0, -1.5, 1.5), "chest": V(0, -1, 2.5), "neck": V(0, -1, 3), "head": V(0, -1, 3.5)}),  # head/torso still settling
    23: (0, {"pelvis": V(0, -1.2, 0.5), "spine2": V(0, -1.0, 0.5), "chest": V(0, -0.8, 0.6), "neck": V(0, -0.7, 0.8), "head": V(0, -0.6, 1.0)}),
    30: (0, {"pelvis": V(0, 0.4, 0), "spine2": V(0, 0.3, 0), "chest": V(0, 0.3, 0), "head": V(0, 0.2, -0.3)}),
}

# 1) frame 17 becomes the owner's fighting stance (frame 0) and new tail keys 23, 30 start from it
snap0 = {}
for o in objs:
    for d in dv.get_all_data_id(o):
        try:
            snap0[d] = dv.get_data_value(d, 0)
        except Exception:
            pass
def keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in (23, 30):
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
ok_keys = ds.modify_update("v2: tail keys", keys)
def copy0(model, update, su):
    de = model.data_editor()
    for f in (17, 23, 30):
        for d, v in snap0.items():
            try:
                de.set_data_value(d, f, v)
            except Exception:
                pass
ok_copy = ds.modify_update("v2: stance at 17 and tail", copy0)

# 2) pushed poses: read base positions, add group deltas, write and solve per frame
def pos(name, f):
    for d in dv.get_all_data_id(ids[name]):
        try:
            if dv.get_data_name(d) == "Position":
                return np.array(dv.get_data_value(d, f), dtype=np.float32)
        except Exception:
            pass
targets = {}
for f, (bf, deltas) in PLAN.items():
    targets[f] = {}
    for g, dlt in deltas.items():
        for n in G[g]:
            targets[f][n] = pos(n, bf) + dlt
def push(model, update, su):
    su.generate_update()
    for f, pts in targets.items():
        dids = set()
        for n, p in pts.items():
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(p, f)
            dids.add(node.data_id())
        su.run_update(dids, f)
ok_push = ds.modify_update("v2: pushed poses", push)

# 3) interpolation
I = csc.layers.layer.Interpolation; T = csc.layers.layer.Tangents
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        le.change_section(0, lid, lambda s: setattr(s.interval, "interpolation", I.STEP))
        for f in (1, 3, 5, 8, 17, 23, 30):
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.CLAMPED_BEZIER))
        def ease_in(s):
            s.key.tangents = T.UserDefined; s.key.left_weight_velocity = 0.6
        le.change_section(17, lid, ease_in)
ok_interp = ds.modify_update("v2: interpolation", interp)
vs.save(base + "jab_v2.casc")
L = lv.layer(layer_ids[0])
print(json.dumps({"ok": [ok_keys, ok_copy, ok_push, ok_interp], "keys": sorted(L.key_frame_indices()),
                  "hand_l_z": {f: round(float(pos("hand_l", f)[2]), 1) for f in (0, 1, 3, 5, 8, 17)},
                  "hand_r_y": {f: round(float(pos("hand_r", f)[1]), 1) for f in (0, 1, 3, 8, 17)},
                  "pelvis_y": {f: round(float(pos("pelvis", f)[1]), 1) for f in (0, 1, 8, 12, 17, 23, 30, 37)},
                  "current": sm.current_scene().name(), "sec": round(time.time() - t0, 1)}))
