# Cascadeur (jab_v8.casc): finger Box controllers, their local rotations at guard (0) and contact (1) for both hands.
import csc, json
import numpy as np
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_v8.casc"][0]; sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
def data(n):
    out = {}
    for d in dv.get_all_data_id(ids[n]):
        try:
            out[dv.get_data_name(d)] = d
        except Exception:
            pass
    return out
boxes = sorted(n for n in ids if "_Box_" in n and any(n.startswith(f) for f in ("index", "middle", "ring", "pinky", "thumb")))
res = {}
for n in boxes:
    dd = data(n)
    row = {}
    for key in ("Local Rotation", "Rotation"):
        if key in dd:
            try:
                row[key] = {f: [round(float(x), 4) for x in np.array(dv.get_data_value(dd[key], f)).ravel()] for f in (0, 1, 17)}
            except Exception as e:
                row[key] = str(e)[:80]
    res[n] = row
print(json.dumps({"boxes": boxes, "names_box0": sorted(data(boxes[0]).keys()), "values": {k: res[k] for k in ("index_01_Box_l", "index_01_Box_r", "middle_02_Box_l", "middle_02_Box_r", "thumb_01_Box_l", "thumb_01_Box_r")}}))
