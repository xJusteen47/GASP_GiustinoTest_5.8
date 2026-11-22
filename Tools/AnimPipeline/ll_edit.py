# Cascadeur: move hand_MainPoint_l at frame FRAME by DY cm (Y up) on the Live Link test copy, solve, print wall time.
import csc, json, time
import numpy as np
FRAME = globals().get("FRAME", 10); DY = globals().get("DY", 5.0)
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_livelink_test.casc"][0]
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
names = ["hand_MainPoint_l", "hand_DirectionPoint_l", "hand_AdditionalPoint_l"]
def pos(n, f):
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == "Position":
                return np.array(dv.get_data_value(d, f), dtype=np.float32)
        except Exception:
            pass
old = {n: pos(n, FRAME) for n in names}
def push(model, update, su):
    su.generate_update()
    dids = set()
    for n in names:
        node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
        node.set_value(old[n] + np.array([0, DY, 0], dtype=np.float32), FRAME)
        dids.add(node.data_id())
    su.run_update(dids, FRAME)
ok = ds.modify_update("livelink latency test", push)
print(json.dumps({"ok": ok, "t_done": time.time(), "hand_before": old["hand_MainPoint_l"].round(2).tolist(),
                  "hand_after": pos("hand_MainPoint_l", FRAME).round(2).tolist()}))
