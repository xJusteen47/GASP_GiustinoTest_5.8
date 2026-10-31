import os
# Cascadeur: AutoPhysics setup on the v6 tab for a MANUAL "Snap to AutoPhysics" on frames 3-17 (not scriptable):
# planted feet Enforce, priority frames on the contact and the guard, compensation only on the off-hand, secondary motion
# only on the hands. Positions are untouched. Saved as jab_v6_physready.casc (jab_v6.casc stays the keyed final).
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
tab = [s for s in sm.scenes() if s.name() == "jab_v6.casc"][0]
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); bv = ds.behaviour_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
N = dv.get_animation_size(); ALL = set(range(N))
def did(obj, beh, prop):
    b = bv.get_behaviour_by_name(ids[obj], beh)
    return None if b.is_null() else bv.get_behaviour_data(b, prop)
points = [n for n, o in ids.items() if mv.get_object_type_name(o) == "Point"]
feet = [n for n in points if n.startswith("foot_") or n.startswith("ball_")]
hands = ["hand_MainPoint_l", "hand_MainPoint_r"]
writes = []
for n in feet:
    writes.append((n, "FulcrumPoint", "fulcrum_state", ALL, 1))                  # Enforce
writes.append(("Center of mass", "AutoPhysics", "priority_frame", {1, 3, 17}, True))
for n in points:
    writes.append((n, "CompensationMotion", "muscle_stiffness", ALL, 15.0 if n == "hand_MainPoint_r" else 100.0))
    writes.append((n, "SecondaryMotion", "local_blending", ALL, 40.0 if n in hands else 100.0))
def mod(model, update, su):
    de = model.data_editor()
    for n, b, p, frames, v in writes:
        d = did(n, b, p)
        if d is not None and not d.is_null():
            de.set_data_value(d, frames, v)
ok = ds.modify("v6: AutoPhysics setup for a manual snap", mod)
tab.save(base + "jab_v6_physready.casc")
chk = {"foot_l_state": dv.get_data_value(did("foot_MainPoint_l", "FulcrumPoint", "fulcrum_state"), 10),
       "hand_r_comp": dv.get_data_value(did("hand_MainPoint_r", "CompensationMotion", "muscle_stiffness"), 10),
       "hand_l_comp": dv.get_data_value(did("hand_MainPoint_l", "CompensationMotion", "muscle_stiffness"), 10),
       "pelvis_sec": dv.get_data_value(did("pelvis_MainPoint", "SecondaryMotion", "local_blending"), 10),
       "prio": [f for f in range(N) if dv.get_data_value(did("Center of mass", "AutoPhysics", "priority_frame"), f)]}
print(json.dumps({"ok": ok, "writes": len(writes), "check": {k: (v if isinstance(v, list) else str(v)) for k, v in chk.items()},
                  "tab": sm.current_scene().name()}))
