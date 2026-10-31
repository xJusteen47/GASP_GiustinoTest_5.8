import os
# Cascadeur: apply a move's targets with SPARSE keys (owner, 31/10: no dense keys where the key API can shape the curve).
# TARGETS json: "targets" {point: [per-frame xyz]}, "feet" {point: xyz} (constant), "keyplan":
#   keys     frames keyed on every layer
#   layers   optional {layer name: {"keys": [...], "lag": n}}: that layer's own key frames, its values taken n frames late
#            (overlap: the head settling after the chest)
#   linear   [[a, b], ...] intervals made LINEAR (the 1-frame snap); every other interval is CLAMPED_BEZIER (no overshoot)
#   weights  {frame: {"lw": x, "rw": y}} easing handle lengths on every layer's key at that frame (tangent_test.py: long
#            incoming handle on the arrival key + short outgoing on the departure key = slow start, fast arrival)
# Every key not in the plan is removed. Then the tab is saved (SAVE_AS, never an earlier version or the owner's file).
import csc, json, time
import numpy as np
t0 = time.time()
TAB = globals().get("TAB")
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == TAB][0]
assert str(tab.get_path_name()).endswith("/Saved/AnimPipeline/" + TAB), tab.get_path_name()
sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
T = json.load(open(base + globals().get("TARGETS")))
KP = T["keyplan"]
N = len(T["frames"])

def lname(lid):
    h = lv.layer(lid).header; n = getattr(h, "name", None); return str(n() if callable(n) else n)
LAYERS = {lname(l): l for l in lv.all_layer_ids()}
owner = {}
for nm, lid in LAYERS.items():
    for o in lv.layer(lid).obj_ids:
        owner[mv.get_object_name(o)] = nm
def mk(keys, lags=((0, 0),), clamped=()):
    """keys = the poses' source frames; lags = [(from_frame, lag), ...]: a lagged layer keeps those poses lag frames later
    (overlap). Returns the layer's key frames and, per key, the source frame its values come from."""
    lagf = lambda k: [l for s0, l in sorted(lags) if k >= s0][-1]
    src = {0: 0, N - 1: N - 1}
    for k in keys:
        f = k + lagf(k)
        if 0 < f < N - 1 and f not in src:
            src[f] = k
    return {"keys": sorted(src), "src": src, "clamped": {tuple(x) for x in clamped}, "weights": None}
plan = {nm: mk(KP["keys"]) for nm in LAYERS}
for nm, v in (KP.get("layers") or {}).items():
    lags = v.get("lags") or [(0, int(v.get("lag", 0)))]
    plan[nm] = mk(v.get("keys") or KP["keys"], lags, v.get("clamped", []))
    if "weights" in v:                 # this layer's own easing weights (instead of the global ones)
        plan[nm]["weights"] = {int(k): w for k, w in v["weights"].items()}
missing = [n for n in list(T["targets"]) + list(T["feet"]) if n not in owner]
assert not missing, missing

def keys(model, update, su):
    le = model.layers_editor()
    for nm, lid in LAYERS.items():
        want = set(plan[nm]["keys"])
        for f in want:
            le.set_fixed_interpolation_or_key_if_need(lid, int(f), True)
        for f in sorted(lv.layer(lid).key_frame_indices()):
            if f not in want:
                le.unset_section(int(f), lid)
ok1 = ds.modify_update("sparse: keys", keys)

def push(model, update, su):
    su.generate_update()
    frames = sorted({f for p in plan.values() for f in p["keys"]})
    for f in frames:
        dids = set()
        for n, tr in T["targets"].items():
            p = plan[owner[n]]
            if f not in p["keys"]:
                continue
            src = p["src"][f]
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(np.array(tr[src], dtype=np.float32), f)
            dids.add(node.data_id())
        for n, v in T["feet"].items():
            if f not in plan[owner[n]]["keys"]:
                continue
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(np.array(v, dtype=np.float32), f)
            dids.add(node.data_id())
        if dids:
            su.run_update(dids, f)
ok2 = ds.modify_update("sparse: poses", push)

I = csc.layers.layer.Interpolation
LIN = {tuple(x) for x in KP.get("linear", [])}
BEZ = {tuple(x) for x in KP.get("bezier", [])}     # pass-through keys: continuous Bezier (clamped kinks world-space paths)
W = {int(k): v for k, v in (KP.get("weights") or {}).items()}
def shape(model, update, su):
    le = model.layers_editor()
    for nm, lid in LAYERS.items():
        ks = plan[nm]["keys"]
        sr = plan[nm]["src"]
        for a, b in zip(ks[:-1], ks[1:]):
            iv = (sr[a], sr[b])
            kind = I.LINEAR if iv in LIN else I.CLAMPED_BEZIER if iv in plan[nm].get("clamped", ()) else I.BEZIER if iv in BEZ else I.CLAMPED_BEZIER
            le.change_section(a, lid, lambda s, kind=kind: setattr(s.interval, "interpolation", kind))
        for f0, w in (plan[nm]["weights"] if plan[nm]["weights"] is not None else W).items():
            f = next((k for k, v in plan[nm]["src"].items() if v == f0), None)
            if f in ks:
                def setw(s, w=w):
                    if "lw" in w: s.key.left_weight_velocity = float(w["lw"])
                    if "rw" in w: s.key.right_weight_velocity = float(w["rw"])
                le.change_section(f, lid, setw)
ok3 = ds.modify_update("sparse: clamped, linear snap, weights", shape)
SAVE_AS = globals().get("SAVE_AS") or TAB
assert not SAVE_AS.startswith(("jab_", "ganon_ftilt_blockout")), SAVE_AS
tab.save(base + SAVE_AS)

def pos(n, f):
    for d in dv.get_all_data_id(ids[n]):
        if dv.get_data_name(d) == "Position":
            return np.array([float(x) for x in dv.get_data_value(d, f)])
err = {}
for n in ("foot_MainPoint_l", "hand_MainPoint_l", "hand_MainPoint_r", "pelvis_MainPoint", "head_MainPoint"):
    p = plan[owner[n]]
    err[n] = round(max(float(np.linalg.norm(pos(n, f) - np.array(T["targets"][n][p["src"][f]]))) for f in p["keys"]), 2)
print(json.dumps({"ok": [ok1, ok2, ok3], "layers": {nm: len(lv.layer(l).key_frame_indices()) for nm, l in LAYERS.items()},
                  "key_error_cm": err, "anim_size": dv.get_animation_size(), "sec": round(time.time() - t0, 1)}))
