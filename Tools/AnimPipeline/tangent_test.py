import os
# Cascadeur (tangent_test.casc, a scratch copy of the discarded jab_v10 trial): can a strike be shaped with SPARSE keys and
# the key API (interpolation type, tangent mode, left/right easing weights) instead of dense keys?
# Timeline at 4x: keys at 0 (guard), 16 (anticipation), 40 (contact), 56 (hold end), 80 (guard), so frames 0..80 read the
# 1x curve (keys 0/4/10/14/20) at 240 fps. VARIANT selects how the build interval 16->40 and the contact key are set.
# Prints the lead hand's forward progress per 4x frame and the impact-spacing numbers.
import csc, json, time
import numpy as np
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
tab = [s for s in sm.scenes() if s.name() == "tangent_test.casc"][0]
sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
layer_ids = list(lv.all_layer_ids())
VARIANT = globals().get("VARIANT") or "A"
SETUP = globals().get("SETUP") in (True, "1", "True")
I = csc.layers.layer.Interpolation
KEYS = [0, 16, 40, 56, 80]

if SETUP:
    T = json.load(open(base + "v11_targets.json"))
    g = {n: np.array(v[0]) for n, v in T["targets"].items()}
    c = {n: np.array(v[1]) for n, v in T["targets"].items()}
    a = {n: v.copy() for n, v in g.items()}
    for n in ("hand_MainPoint_l", "hand_DirectionPoint_l", "hand_AdditionalPoint_l", "lowerarm_MainPoint_l", "lowerarm_AdditionalPoint_l", "lowerarm_LimbDir_l"):
        a[n] = a[n] + np.array([0.0, -2.0, -8.0])          # anticipation: fist 8 cm back, 2 cm down
    h = {n: g[n] + 0.97 * (c[n] - g[n]) for n in g}        # hold: 97% of contact
    POSES = {0: g, 16: a, 40: c, 56: h, 80: g}

    def wipe(model, update, su):
        le = model.layers_editor()
        for lid in layer_ids:
            for f in sorted(lv.layer(lid).key_frame_indices()):
                if f not in KEYS:
                    le.unset_section(int(f), lid)
            for f in KEYS:
                le.set_fixed_interpolation_or_key_if_need(lid, f, True)
    ok1 = ds.modify_update("tangent test: sparse keys", wipe)

    def push(model, update, su):
        su.generate_update()
        for f, P in POSES.items():
            dids = set()
            for n, v in P.items():
                node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
                node.set_value(np.array(v, dtype=np.float32), f)
                dids.add(node.data_id())
            for n, p in T["feet"].items():
                node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
                node.set_value(np.array(p, dtype=np.float32), f)
                dids.add(node.data_id())
            su.run_update(dids, f)
    ok2 = ds.modify_update("tangent test: poses", push)
    print("setup", ok1, ok2, sorted(lv.layer(layer_ids[0]).key_frame_indices()))

# ---- variant: interpolation type on every interval, then tangents/weights on the build and the contact key
KIND = {"A": I.BEZIER, "B": I.BEZIER, "C": I.CLAMPED_BEZIER, "D": I.LOW_AMPLITUDE_BEZIER, "E": I.BEZIER, "L": I.LINEAR}[VARIANT[0]]
W16 = float(globals().get("W16") or 0) or None      # optional right weight of key 16 and left weight of key 40
W40 = float(globals().get("W40") or 0) or None
TAN = csc.layers.layer.Tangents

def shape(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in KEYS[:-1]:
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", KIND))
        def key16(s):
            s.key.tangents = TAN.Continuous
            s.key.right_weight_velocity = W16 or (0.9 if VARIANT in ("B", "E") else 1.0 / 3.0)
            s.key.left_weight_velocity = 1.0 / 3.0
        def key40(s):
            s.key.tangents = TAN.UserDefined if VARIANT == "E" else TAN.Continuous
            s.key.left_weight_velocity = W40 or (0.05 if VARIANT in ("B", "E") else 1.0 / 3.0)
            s.key.right_weight_velocity = 1.0 / 3.0
        le.change_section(16, lid, key16)
        le.change_section(40, lid, key40)
ok3 = ds.modify_update("tangent test: variant " + VARIANT, shape)

def pos(n, f):
    for d in dv.get_all_data_id(ids[n]):
        if dv.get_data_name(d) == "Position":
            return np.array([float(x) for x in dv.get_data_value(d, f)])
P = np.array([pos("hand_MainPoint_l", f) for f in range(81)])
z = P[:, 2]
z16, z40 = z[16], z[40]
prog = (z - z16) / (z40 - z16)
build = prog[16:41]
sp = np.abs(np.diff(z[16:41]))
last2 = (z40 - z[32]) / (z40 - z16)                       # last 2 x1 frames = last 8 x4 frames
k = lv.layer(layer_ids[0]).key(40)
print(json.dumps({"variant": VARIANT, "ok": ok3, "keys": sorted(lv.layer(layer_ids[0]).key_frame_indices()),
                  "key40": {"tangents": str(k.tangents), "lw": round(float(k.left_weight_velocity), 3), "rw": round(float(k.right_weight_velocity), 3)},
                  "progress_x1_frames": [round(float(prog[f]), 3) for f in range(16, 57, 4)],
                  "progress_x4_build": [round(float(x), 3) for x in build],
                  "last2_share": round(float(last2), 3), "speed_peak_x4_frame": int(16 + 1 + np.argmax(sp)),
                  "overshoot_after_contact_cm": round(float(max(0.0, (z[40:57] - z40).max())), 2),
                  "dip_before_anticipation_cm": round(float(max(0.0, (z16 - z[0:17]).max())), 2) if z16 < z[0] else None,
                  "hold_wobble_cm": round(float(z[40:57].max() - z[40:57].min()), 2), "sec": round(time.time() - t0, 1)}))
