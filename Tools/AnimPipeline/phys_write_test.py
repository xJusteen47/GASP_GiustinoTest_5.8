import os
# Cascadeur: load jab_phys_test2.casc (fresh copy of v5) if needed and test writes of AutoPhysics behaviour data.
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
NAME = "jab_phys_test2.casc"
if not any(s.name() == NAME for s in sm.scenes()):
    app.get_data_source_manager().load_scene(base + NAME)
tab = [s for s in sm.scenes() if s.name() == NAME][0]
sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); bv = ds.behaviour_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
N = dv.get_animation_size()
def did(obj, beh, prop):
    return bv.get_behaviour_data(bv.get_behaviour_by_name(ids[obj], beh), prop)
TESTS = [("hand_MainPoint_r", "CompensationMotion", "muscle_stiffness", 15.0, "all"),
         ("hand_MainPoint_l", "SecondaryMotion", "local_blending", 30.0, "all"),
         ("pelvis_MainPoint", "SecondaryMotion", "local_blending", 100.0, "all"),
         ("hand_MainPoint_l", "SecondaryMotion", "damping", 5.0, "all"),
         ("foot_MainPoint_l", "FulcrumPoint", "fulcrum_state", 1, "all"),
         ("Center of mass", "AutoPhysics", "priority_frame", True, [1, 3, 17])]
before = {}
for o, b, p, v, fr in TESTS:
    d = did(o, b, p)
    before[f"{o}.{b}.{p}"] = {"mode": str(dv.get_data(d).mode), "f0": str(dv.get_data_value(d, 0)), "f17": str(dv.get_data_value(d, 17))}
def mod(model, update, su):
    de = model.data_editor()
    for o, b, p, v, fr in TESTS:
        d = did(o, b, p)
        if dv.get_data(d).mode == csc.model.DataMode.Static:
            de.set_data_value(d, v)
        else:
            frames = set(range(N)) if fr == "all" else set(fr)
            de.set_data_value(d, frames, v)
ok = ds.modify("phys write test", mod)
after = {}
for o, b, p, v, fr in TESTS:
    d = did(o, b, p)
    after[f"{o}.{b}.{p}"] = {"f0": str(dv.get_data_value(d, 0)), "f1": str(dv.get_data_value(d, 1)), "f17": str(dv.get_data_value(d, 17)), "f20": str(dv.get_data_value(d, 20))}
print(json.dumps({"ok": ok, "before": before, "after": after}))
