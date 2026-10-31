import csc, json
import numpy as np
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_v8.casc"][0]; sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
def did(n, key):
    for d in dv.get_all_data_id(ids[n]):
        if dv.get_data_name(d) == key:
            return d
r = dv.get_data_value(did("index_01_Box_l", "Local Rotation"), 1)
attrs = [a for a in dir(r) if not a.startswith("_")]
def q(n, f, key="Local Rotation"):
    v = dv.get_data_value(did(n, key), f)
    for m in ("to_quaternion", "quaternion", "as_quaternion"):
        if hasattr(v, m):
            x = getattr(v, m); x = x() if callable(x) else x
            for comp in (("x", "y", "z", "w"), ("w", "x", "y", "z")):
                if all(hasattr(x, c) for c in comp):
                    return {c: round(float(getattr(x, c)() if callable(getattr(x, c)) else getattr(x, c)), 4) for c in comp}
            return [a for a in dir(x) if not a.startswith("_")]
    return str(v)
names = ["index_01_Box", "index_02_Box", "middle_01_Box", "middle_02_Box", "thumb_01_Box"]
vals = {f"{n}_{s}": {f: q(f"{n}_{s}", f) for f in (0, 1, 8, 17)} for n in names for s in "lr"}
print(json.dumps({"attrs": attrs, "vals": vals}))
