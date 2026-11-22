# Test one Cascadeur action on jab_phys_test.casc (a throwaway copy of v5) and report what it changed.
# Params (prepended by csc_run.py): ACTION (str), FRAME (int, current frame), OBJS (list of point names, [] = none,
# ["*"] = every point), INTERVAL ((first, last) timeline selection on all layers, or None), DRY (True = setup only).
import csc, json
import numpy as np
ACTION = globals().get("ACTION"); FRAME = globals().get("FRAME", 0); OBJS = globals().get("OBJS", [])
INTERVAL = globals().get("INTERVAL"); DRY = globals().get("DRY", False)
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == globals().get("TAB", "jab_phys_test.casc")][0]
sm.set_current_scene(tab)
ds = tab.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer(); bv = ds.behaviour_viewer(); lv = ds.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
N = dv.get_animation_size()
layer_ids = list(lv.all_layer_ids())
WATCH = ["hand_MainPoint_l", "hand_MainPoint_r", "lowerarm_MainPoint_l", "lowerarm_MainPoint_r", "pelvis_MainPoint",
         "head_MainPoint", "spine_04_MainPoint", "foot_MainPoint_l", "foot_MainPoint_r", "ball_MainPoint_l", "ball_MainPoint_r"]
def pos_id(n):
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == "Position":
                return d
        except Exception:
            pass
PID = {n: pos_id(n) for n in WATCH}
ap = bv.get_behaviour_by_name(ids["Center of mass"], "AutoPhysics")
prio_id = bv.get_behaviour_data(ap, "priority_frame")
def interp(lid, f):
    try:
        return str(lv.layer(lid).interval(f).interpolation).split(".")[-1]
    except Exception as e:
        return "?"
def snap():
    return {"pos": {n: np.array([dv.get_data_value(PID[n], f) for f in range(N)], dtype=np.float64) for n in WATCH},
            "prio": [bool(dv.get_data_value(prio_id, f)) for f in range(N)],
            "keys": {str(l): sorted(lv.layer(l).key_frame_indices()) for l in layer_ids},
            "interp": [interp(layer_ids[0], f) for f in range(N)],
            "n": dv.get_animation_size()}
# setup: frame, object selection, timeline interval
ds.set_current_frame(FRAME)
pts = [n for n in ids if mv.get_object_type_name(ids[n]) == "Point"] if OBJS == ["*"] else OBJS
sel_err = None
try:
    ds.selector().select({ids[n] for n in pts})
except Exception as e:
    sel_err = str(e)[:200]
int_err = None
if INTERVAL:
    try:
        ds.get_layers_selector().set_full_selection_by_parts(layer_ids, INTERVAL[0], INTERVAL[1])
    except Exception as e:
        int_err = str(e)[:200]
lsel = ds.get_layers_selector().selection()
setup = {"frame": ds.get_current_frame(), "n_selected": len(ds.selector().selected().ids), "sel_err": sel_err, "int_err": int_err,
         "timeline_sel_empty": lsel.is_empty(), "timeline_sel_frames": sorted(set(int(c.frame_index) for c in lsel.cell_indices())) if not lsel.is_empty() else None,
         "interp_0_10": [interp(layer_ids[0], f) for f in range(11)]}
before = snap()
err = None
if not DRY:
    try:
        app.get_action_manager().call_action(ACTION)
    except Exception as e:
        err = str(e)[:300]
after = snap()
diff = {}
for n in WATCH:
    d = np.linalg.norm(after["pos"][n] - before["pos"][n], axis=1)
    ch = [i for i in range(len(d)) if d[i] > 0.01]
    if ch:
        diff[n] = {"frames": ch, "max_cm": round(float(d.max()), 2), "at": int(d.argmax())}
res = {"action": ACTION, "setup": setup, "error": err, "pos_changed": diff,
       "prio_frames_before": [i for i, v in enumerate(before["prio"]) if v], "prio_frames_after": [i for i, v in enumerate(after["prio"]) if v],
       "keys_changed": {l: {"before": before["keys"][l], "after": after["keys"][l]} for l in before["keys"] if before["keys"][l] != after["keys"][l]},
       "interp_changed": {f: [before["interp"][f], after["interp"][f]] for f in range(N) if before["interp"][f] != after["interp"][f]},
       "anim_size": [before["n"], after["n"]]}
print(json.dumps(res, default=str))
