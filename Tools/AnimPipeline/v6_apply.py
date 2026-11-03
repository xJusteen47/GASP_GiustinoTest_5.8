import os
# Cascadeur: write v6_targets.json into tab TAB (a copy of jab_v5.casc saved as jab_v6.casc), one key per frame 3..37,
# feet re-planted on their frame-0 positions, rig solved per frame, LINEAR between the dense keys, then save.
# Params: TAB (default "jab_v6.casc").
import csc, json, time
import numpy as np
t0 = time.time()
TAB = globals().get("TAB") or "jab_v6.casc"
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == TAB][0]
assert str(tab.get_path_name()).endswith("/Saved/AnimPipeline/" + TAB), tab.get_path_name()
sm.set_current_scene(tab)
ds = tab.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
layer_ids = list(lv.all_layer_ids())
T = json.load(open(base + (globals().get("TARGETS") or "v6_targets.json")))
FR = T["frames"]

def keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in FR:
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
ok1 = ds.modify_update("v6: dense keys 3..37", keys)

def push(model, update, su):
    su.generate_update()
    for i, f in enumerate(FR):
        dids = set()
        for n, tr in T["targets"].items():
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(np.array(tr[i], dtype=np.float32), f)
            dids.add(node.data_id())
        for n, p in T["feet"].items():
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(np.array(p, dtype=np.float32), f)
            dids.add(node.data_id())
        su.run_update(dids, f)
ok2 = ds.modify_update("v6: arm-led recovery and living tail", push)

I = csc.layers.layer.Interpolation
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in FR[:-1]:
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.LINEAR))
ok3 = ds.modify_update("v6: linear between dense keys", interp)
SAVE_AS = globals().get("SAVE_AS") or TAB
assert SAVE_AS not in ("jab_v5.casc", "jab_blockout.casc", "jab_v6_tailtry.casc"), SAVE_AS
tab.save(base + SAVE_AS)   # save-as renames the tab to SAVE_AS

def pos(n, f):
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == "Position":
                return [round(float(x), 2) for x in dv.get_data_value(d, f)]
        except Exception:
            pass
err = {}
for n in ("hand_MainPoint_l", "hand_MainPoint_r", "pelvis_MainPoint", "head_MainPoint", "lowerarm_MainPoint_l"):
    i_err = [float(np.linalg.norm(np.array(pos(n, f)) - np.array(T["targets"][n][i]))) for i, f in enumerate(FR)]
    err[n] = round(max(i_err), 2)
print(json.dumps({"ok": [ok1, ok2, ok3], "keys": sorted(lv.layer(layer_ids[0]).key_frame_indices()),
                  "max_target_error_cm": err, "sec": round(time.time() - t0, 1)}))
