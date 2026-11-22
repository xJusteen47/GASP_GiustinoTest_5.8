import os
# Pass 4 on a copy (jab_v4.casc): breakdowns at 11 and 14 so 8 -> 17 starts fast and decelerates into the guard
# (v3 held the high guard until 14 and swung the lead arm out in the last 3 frames).
import csc, json, time
import numpy as np
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
src = [s for s in sm.scenes() if s.name() == "jab_v3.casc"][0]
sm.set_current_scene(src)
src.save(base + "jab_v4.casc")
vs = [s for s in sm.scenes() if s.name() == "jab_v4.casc"][0]
ds = vs.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
layer_ids = list(lv.all_layer_ids())
PTS = [n for n, o in ids.items() if mv.get_object_type_name(o) in ("Point", "Limb Direction")
       and not (n.startswith("foot_") or n.startswith("ball_"))]
def pos(name, f):
    for d in dv.get_all_data_id(ids[name]):
        try:
            if dv.get_data_name(d) == "Position":
                return np.array(dv.get_data_value(d, f), dtype=np.float32)
        except Exception:
            pass
a = {n: pos(n, 8) for n in PTS}; b = {n: pos(n, 17) for n in PTS}
BREAK = {11: 0.65, 14: 0.92}
def keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in BREAK:
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
ok1 = ds.modify_update("v4: breakdown keys", keys)
def push(model, update, su):
    su.generate_update()
    for f, t in BREAK.items():
        dids = set()
        for n in PTS:
            if a[n] is None or b[n] is None:
                continue
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(a[n] + (b[n] - a[n]) * t, f)
            dids.add(node.data_id())
        su.run_update(dids, f)
ok2 = ds.modify_update("v4: breakdown poses", push)
I = csc.layers.layer.Interpolation
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in BREAK:
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.CLAMPED_BEZIER))
ok3 = ds.modify_update("v4: interpolation", interp)
vs.save(base + "jab_v4.casc")
hl = {f: round(float(pos("hand_l", f)[0]), 1) for f in range(8, 18)}
print(json.dumps({"ok": [ok1, ok2, ok3], "keys": sorted(lv.layer(layer_ids[0]).key_frame_indices()), "hand_l_x_8_17": hl,
                  "sec": round(time.time() - t0, 1)}))
