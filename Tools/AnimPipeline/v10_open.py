import os
# Cascadeur: open jab_v9.casc and save it at once as jab_v10.casc (v9 on disk is never written).
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
if not any(s.name() == "jab_v10.casc" for s in sm.scenes()):
    if not any(s.name() == "jab_v9.casc" for s in sm.scenes()):
        app.get_data_source_manager().load_scene(base + "jab_v9.casc")
    tab = [s for s in sm.scenes() if s.name() == "jab_v9.casc"][0]
    tab.save(base + "jab_v10.casc")
tab = [s for s in sm.scenes() if s.name() == "jab_v10.casc"][0]
sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer()
names = [mv.get_object_name(o) for o in mv.get_objects()]
fing = sorted(n for n in names if any(k in n.lower() for k in ("index", "middle", "ring", "pinky", "thumb", "finger", "fist")))
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
info = {}
for n in fing[:12]:
    info[n] = sorted({dv.get_data_name(d) for d in dv.get_all_data_id(ids[n])})[:12]
print(json.dumps({"tabs": [s.name() for s in sm.scenes()], "path": str(tab.get_path_name()), "n_objects": len(names), "finger_objects": fing[:60], "n_finger": len(fing), "data": info}))
