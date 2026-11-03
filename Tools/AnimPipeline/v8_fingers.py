import os
# Cascadeur (jab_v8.casc): closed fists on both hands, every frame. Each finger Box controller (index/middle/ring/pinky
# 01-03, thumb 01-03) gets the local rotation of the LEFT hand's box at frame 1 (the blockout's punching fist). Left and right
# boxes have the same local rotations in the relaxed pose, so the same values give a mirrored fist. Then save.
import csc, json, time
t0 = time.time()
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
tab = [s for s in sm.scenes() if s.name() == "jab_v8.casc"][0]; sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
N = 38
boxes = [f"{f}_{i}_Box" for f in ("index", "middle", "ring", "pinky", "thumb") for i in ("01", "02", "03")]
def did(n, key):
    for d in dv.get_all_data_id(ids[n]):
        if dv.get_data_name(d) == key:
            return d
fist = {b: dv.get_data_value(did(f"{b}_l", "Local Rotation"), 1) for b in boxes}
err = []
def push(model, update, su):
    su.generate_update()
    for f in range(N):
        dids = set()
        for b in boxes:
            for side in "lr":
                node = update.get_object_by_id(ids[f"{b}_{side}"]).root_group().node_deep("Local Rotation")
                node.set_value(fist[b], f)
                dids.add(node.data_id())
        su.run_update(dids, f)
ok = ds.modify_update("v8: closed fists on both hands", push)
def q(n, f):
    x = dv.get_data_value(did(n, "Local Rotation"), f).to_quaternion()
    return [round(float(getattr(x, c)() if callable(getattr(x, c)) else getattr(x, c)), 3) for c in ("x", "y", "z", "w")]
check = {f: {"index_01_r": q("index_01_Box_r", f), "index_01_l": q("index_01_Box_l", f)} for f in (0, 1, 18, 37)}
tab.save(base + "jab_v8.casc")
print(json.dumps({"ok": ok, "check": check, "sec": round(time.time() - t0, 1)}))
