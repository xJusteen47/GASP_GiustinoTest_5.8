# Read-only: list open Cascadeur scenes and describe the current one (keys, interpolation, fps, length).
import csc, json
app = csc.app.get_application()
sm = app.get_scene_manager()
cur = sm.current_scene()
out = {"scenes": []}
for s in sm.scenes():
    try:
        out["scenes"].append({"name": s.name(), "path": str(s.get_path_name()), "current": s.name() == cur.name()})
    except Exception as e:
        out["scenes"].append(str(e))
mv = scene.model_viewer(); lv = scene.layers_viewer(); dv = scene.data_viewer()
out["current"] = {"name": cur.name(), "path": str(cur.get_path_name()),
                  "fps": cur.get_setting_handler().get_float_value("Scene", "Fps"),
                  "anim_size": dv.get_animation_size()}
objs = mv.get_objects()
types = {}
for o in objs:
    t = mv.get_object_type_name(o); types[t] = types.get(t, 0) + 1
out["types"] = types
layers = []
for lid in lv.all_layer_ids():
    try:
        L = lv.layer(lid)
        ks = sorted(L.key_frame_indices())
        layers.append({"id": str(lid)[:8], "n_obj": len(L.obj_ids), "keys": ks,
                       "interp": {k: str(L.interval(k).interpolation).split(".")[-1] for k in ks}})
    except Exception as e:
        layers.append({"id": str(lid)[:8], "err": str(e)[:80]})
out["layers"] = layers
print(json.dumps(out, default=str))
