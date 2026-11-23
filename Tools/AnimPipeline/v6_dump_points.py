# Cascadeur, read-only: dump every Point / Limb Direction position for all frames of tab TAB to OUT (json).
# Params: TAB (tab name), OUT (absolute json path).
import csc, json
TAB = globals().get("TAB"); OUT = globals().get("OUT")
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == TAB][0]
ds = tab.domain_scene()
mv = ds.model_viewer(); dv = ds.data_viewer()
N = dv.get_animation_size()
res = {"tab": TAB, "frames": N, "points": {}, "types": {}}
for o in mv.get_objects():
    t = mv.get_object_type_name(o)
    if t not in ("Point", "Limb Direction"):
        continue
    n = mv.get_object_name(o)
    for d in dv.get_all_data_id(o):
        try:
            if dv.get_data_name(d) == "Position":
                res["points"][n] = [[float(x) for x in dv.get_data_value(d, f)] for f in range(N)]
                res["types"][n] = t
                break
        except Exception:
            pass
lv = ds.layers_viewer()
res["keys"] = sorted(lv.layer(list(lv.all_layer_ids())[0]).key_frame_indices())
json.dump(res, open(OUT, "w"))
print(json.dumps({"points": len(res["points"]), "frames": N, "keys": res["keys"]}))
