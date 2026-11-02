import os
# Pass 1: retime the owner's blockout to the agreed timing on a copy (jab_v1.casc), plant the feet, set interpolation.
import csc, json, time
import numpy as np
t0 = time.time()
app = csc.app.get_application()
vs = app.get_scene_manager().current_scene()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
assert vs.get_path_name().endswith("jab_blockout.casc"), vs.get_path_name()
vs.save(base + "jab_v1.casc")  # tab now = v1; the blockout file stays as saved

mv = scene.model_viewer(); dv = scene.data_viewer(); lv = scene.layers_viewer()
objs = mv.get_objects()
ids = {mv.get_object_name(o): o for o in objs}
layer_ids = list(lv.all_layer_ids())

# 1) snapshot every readable data value at the owner's key frames
SRC = [0, 1, 3, 8, 19]
snap = {f: {} for f in SRC}
n_data = 0
for o in objs:
    for d in dv.get_all_data_id(o):
        try:
            vals = {f: dv.get_data_value(d, f) for f in SRC}
        except Exception:
            continue
        n_data += 1
        for f in SRC:
            snap[f][d] = vals[f]

# target frame <- owner frame
MAP = {0: 0, 1: 1, 3: 1, 5: 3, 8: 8, 17: 19, 37: 0}
NEW_KEYS = sorted(MAP)

def keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in NEW_KEYS:
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
        le.unset_section(19, lid)
    model.fit_animation_size_by_layers()
ok_keys = scene.modify_update("v1: keys", keys)

errors = 0
def copy(model, update, su):
    global errors
    de = model.data_editor()
    for dst, src in MAP.items():
        for d, v in snap[src].items():
            try:
                de.set_data_value(d, dst, v)
            except Exception:
                errors += 1
ok_copy = scene.modify_update("v1: retime poses", copy)

# 2) plant both feet: foot/ball controllers keep their contact-frame (owner frame 1) position on every key
FEET = [n for n in ids if mv.get_object_type_name(ids[n]) == "Point" and (n.startswith("foot_") or n.startswith("ball_"))]
feet_pos = {}
for n in FEET:
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == "Position":
                feet_pos[n] = np.array(dv.get_data_value(d, 1), dtype=np.float32)
        except Exception:
            pass
def plant(model, update, su):
    su.generate_update()
    for f in NEW_KEYS:
        dids = set()
        for n, p in feet_pos.items():
            node = update.get_object_by_id(ids[n]).root_group().node_deep("Position")
            node.set_value(p, f)
            dids.add(node.data_id())
        su.run_update(dids, f)
ok_plant = scene.modify_update("v1: plant feet", plant)

# 3) interpolation: snap into contact, hold, smooth retract, ease into the guard, smooth tail
I = csc.layers.layer.Interpolation; T = csc.layers.layer.Tangents
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        def step(s): s.interval.interpolation = I.STEP
        le.change_section(0, lid, step)                    # 0 -> 1 snap, no blend (rule 1)
        for f in (1, 3, 5, 8, 17):
            def bez(s): s.interval.interpolation = I.CLAMPED_BEZIER  # clamped: no overshoot between equal keys
            le.change_section(f, lid, bez)
        def ease_in(s):
            s.key.tangents = T.UserDefined; s.key.left_weight_velocity = 0.6   # long ease-in into the guard
        le.change_section(17, lid, ease_in)
ok_interp = scene.modify_update("v1: interpolation", interp)
vs.save(base + "jab_v1.casc")
L = lv.layer(layer_ids[0])
print(json.dumps({"ok": [ok_keys, ok_copy, ok_plant, ok_interp], "n_data": n_data, "copy_errors": errors, "feet": list(feet_pos),
                  "keys": sorted(L.key_frame_indices()), "anim_size": dv.get_animation_size(), "sec": round(time.time() - t0, 1)}))
