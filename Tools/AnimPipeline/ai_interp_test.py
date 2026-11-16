import os
# Cascadeur: load jab_ai_test.casc (copy of the owner's STEP blockout), switch every key interval to AI (Inbetweening)
# interpolation, report where the lead hand / pelvis go on the in-between frames, save the copy.
import csc, json, time
import numpy as np
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
NAME = "jab_ai_test.casc"
if not any(s.name() == NAME for s in sm.scenes()):
    app.get_data_source_manager().load_scene(base + NAME)
tab = [s for s in sm.scenes() if s.name() == NAME][0]
sm.set_current_scene(tab)
ds = tab.domain_scene(); lv = ds.layers_viewer(); mv = ds.model_viewer(); dv = ds.data_viewer()
layer_ids = list(lv.all_layer_ids())
keys = sorted(lv.layer(layer_ids[0]).key_frame_indices())
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
def pos(n, f):
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == "Position":
                return np.array(dv.get_data_value(d, f))
        except Exception:
            pass
before = {f: pos("hand_l", f).round(1).tolist() for f in range(0, dv.get_animation_size())}
I = csc.layers.layer.Interpolation
t0 = time.time()
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in keys[:-1]:
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.AI))
ok = ds.modify_update("AI inbetweening on every interval", interp)
after = {f: pos("hand_l", f).round(1).tolist() for f in range(0, dv.get_animation_size())}
interp_now = [str(lv.layer(layer_ids[0]).interval(f).interpolation).split(".")[-1] for f in range(dv.get_animation_size())]
tab.save(base + NAME)
print(json.dumps({"ok": ok, "keys": keys, "sec": round(time.time() - t0, 1), "interp": interp_now,
                  "hand_l_before": before, "hand_l_after": after}))
