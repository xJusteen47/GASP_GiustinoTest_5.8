import os
# Pass 3 on a copy (jab_v3.casc): compact recovery (elbows tucked at 5 and 8), stronger lean/twist at contact, re-planted feet.
import csc, json, time
import numpy as np
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
src = [s for s in sm.scenes() if s.name() == "jab_v2.casc"][0]
sm.set_current_scene(src)
src.save(base + "jab_v3.casc")
vs = [s for s in sm.scenes() if s.name() == "jab_v3.casc"][0]
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
    "chest": ["spine_04_MainPoint", "spine_04_AdditionalPoint"],
    "neck": ["neck_01_MainPoint", "neck_01_AdditionalPoint"],
    "head": ["head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint"],
    "sho_r": ["clavicle_MainPoint_r", "clavicle_AdditionalPoint_r", "upperarm_MainPoint_r"],
    "elb_l": ["lowerarm_MainPoint_l", "lowerarm_AdditionalPoint_l"],
    "elb_r": ["lowerarm_MainPoint_r", "lowerarm_AdditionalPoint_r"],
    "hand_l": ["hand_MainPoint_l", "hand_DirectionPoint_l", "hand_AdditionalPoint_l"],
    "hand_r": ["hand_MainPoint_r", "hand_DirectionPoint_r", "hand_AdditionalPoint_r"],
}
# elbows tuck toward the body midline (x toward 0) and down; character's left is +X, right is -X
def tuck(px, amount_x, down):
    return V(-np.sign(px) * amount_x, -down, 0)
PLAN = {1: {"chest": V(0, -0.5, 3), "neck": V(0, -0.5, 3), "head": V(0, -0.5, 3), "sho_r": V(0, 0, -3)},
        3: {"chest": V(0, -0.5, 3), "neck": V(0, -0.5, 3), "head": V(0, -0.5, 3.5), "sho_r": V(0, 0, -3)},
        5: {"chest": V(0, 0, 2), "neck": V(0, 0, 2), "head": V(0, 0, 2), "elb_l": "tuck:3:4"},
        8: {"elb_l": "tuck:6:4", "elb_r": "tuck:6:4", "hand_l": "tuck:3:1", "hand_r": "tuck:3:1"}}
targets = {}
for f, deltas in PLAN.items():
    targets[f] = {}
    for g, dlt in deltas.items():
        for n in G[g]:
            p = pos(n, f)
            if isinstance(dlt, str):
                _, ax, dn = dlt.split(":")
                # one shared offset per group, decided by the group's main point
                gp = pos(G[g][0], f)
                d = tuck(gp[0], float(ax), float(dn))
            else:
                d = dlt
            targets[f][n] = p + d
# feet: back to their frame-0 positions on every key (plant)
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
ok = ds.modify_update("v3: compact recovery, lean, plant", push)
vs.save(base + "jab_v3.casc")
print(json.dumps({"ok": ok, "keys": KEYS, "elb_l_x": {f: round(float(pos("lowerarm_l", f)[0]), 1) for f in (5, 8)},
                  "elb_r_x": {f: round(float(pos("lowerarm_r", f)[0]), 1) for f in (8,)},
                  "head_z": {f: round(float(pos("head", f)[2]), 1) for f in (0, 1, 3, 5, 8)},
                  "foot_l": {f: [round(float(x), 2) for x in pos("foot_l", f)] for f in (0, 1, 8)},
                  "current": sm.current_scene().name(), "sec": round(time.time() - t0, 1)}))
