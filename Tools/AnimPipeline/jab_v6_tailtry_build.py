import os
# Pass 6 on a copy (jab_v6.casc): organic follow-through in the tail (17 -> 37).
# Owner's note on v5: arms stayed fixed after the move and the torso moved like a doll pushed from outside.
# Cause: the v2 tail translated pelvis/torso/head only, so the hands stayed pinned in world space.
# Fix: one key per frame 18..36 from damped curves; overlap chain pelvis -> spine -> chest -> neck -> head,
# shoulders/arms ride on the chest delta plus their own lagged motion. Motion continues the recoil direction
# (weight back past the stance, punching shoulder back), then returns; it fades to exactly zero at 37.
import csc, json, math, time
import numpy as np
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
src = [s for s in sm.scenes() if s.name() == "jab_v5.casc"][0]
sm.set_current_scene(src)
src.save(base + "jab_v6.casc")
vs = [s for s in sm.scenes() if s.name() == "jab_v6.casc"][0]
ds = vs.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
layer_ids = list(lv.all_layer_ids())
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
    "arm_r": ["lowerarm_MainPoint_r", "lowerarm_AdditionalPoint_r", "hand_MainPoint_r", "hand_DirectionPoint_r", "hand_AdditionalPoint_r"],
}
# x toward the body midline for the lead fist: midline ~ pelvis x
mid_x = float(pos("pelvis_MainPoint", 17)[0]); lead_x = float(pos("hand_MainPoint_l", 17)[0])
toward_mid = -1.0 if lead_x > mid_x else 1.0
# amplitude (cm) and delay (frames) per group; arms also inherit the chest motion (carried=True)
SPEC = {
    "pelvis": (V(0, -1.5, -2.0), 0, False),
    "spine2": (V(0, -1.2, -2.5), 1, False),
    "chest":  (V(0, -1.0, -3.0), 2, False),
    "neck":   (V(0, -1.0, -3.2), 3, False),
    "head":   (V(0, -1.2, -3.5), 4, False),
    "sho_l":  (V(0, -0.8, -2.5), 2, True),
    "sho_r":  (V(0, -0.8, 1.5), 2, True),
    "arm_l":  (V(1.5 * toward_mid, -2.5, -1.0), 4, True),
    "arm_r":  (V(0, -2.0, 0.5), 5, True),
}
T, TAU, END = 18.0, 9.0, 20.0           # period, decay, tail length (17 -> 37)
def wave(t):
    if t <= 0:
        return 0.0
    fade = 1.0 - (lambda x: x * x * (3 - 2 * x))(min(1.0, max(0.0, (t - 13.0) / 7.0)))
    return math.sin(math.pi * t / (T / 2) * 0.5) * math.exp(-t / TAU) * fade   # one swing out and back
def delta(group, t):
    amp, delay, carried = SPEC[group]
    d = amp * wave(t - delay)
    if carried:
        d = d + SPEC["chest"][0] * wave(t - SPEC["chest"][1])
    return d
stance = {n: pos(n, 17) for g in G for n in G[g]}
FRAMES = list(range(18, 37))
def keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in (23, 30):
            le.unset_section(f, lid)
        for f in FRAMES:
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
ok1 = ds.modify_update("v6: dense tail keys", keys)
FEET = [n for n in ids if mv.get_object_type_name(ids[n]) == "Point" and (n.startswith("foot_") or n.startswith("ball_"))]
feet0 = {n: pos(n, 0) for n in FEET}
def push(model, update, su):
    su.generate_update()
    for f in FRAMES:
        t = f - 17
        dids = set()
        for g, names in G.items():
            dg = delta(g, t)
            for n in names:
                node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
                node.set_value(stance[n] + dg, f)
                dids.add(node.data_id())
        for n, p in feet0.items():
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(p, f); dids.add(node.data_id())
        su.run_update(dids, f)
ok2 = ds.modify_update("v6: follow-through curves", push)
I = csc.layers.layer.Interpolation; T_ = csc.layers.layer.Tangents
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in [17] + FRAMES:
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.LINEAR))
        def no_ease(s):  # let the recoil carry through 17 into the tail instead of stopping dead
            s.key.tangents = T_.Continuous; s.key.left_weight_velocity = 0.3333; s.key.right_weight_velocity = 0.3333
        le.change_section(17, lid, no_ease)
ok3 = ds.modify_update("v6: interpolation", interp)
vs.save(base + "jab_v6.casc")
curve = {f: [round(float(x), 2) for x in delta("head", f - 17)] for f in (18, 20, 22, 24, 26, 28, 30, 33, 36)}
hl = {f: [round(float(x), 1) for x in pos("hand_l", f)] for f in (17, 21, 25, 29, 33, 37)}
print(json.dumps({"ok": [ok1, ok2, ok3], "keys": sorted(lv.layer(layer_ids[0]).key_frame_indices()), "head_delta": curve, "hand_l": hl,
                  "sec": round(time.time() - t0, 1)}))
