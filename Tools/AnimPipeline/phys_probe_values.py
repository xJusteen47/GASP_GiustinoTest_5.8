# Read-only: values and property kinds of the AutoPhysics-related behaviours in jab_phys_test.casc.
import csc, json, re
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_phys_test.casc"][0]
ds = tab.domain_scene()
mv = ds.model_viewer(); bv = ds.behaviour_viewer(); dv = ds.data_viewer()
names = {o: mv.get_object_name(o) for o in mv.get_objects()}
ids = {v: k for k, v in names.items()}
FR = [0, 1, 3, 8, 17, 37]
def val(x):
    try:
        if hasattr(x, "__len__") and not isinstance(x, str):
            return [round(float(v), 3) for v in x]
        if isinstance(x, float):
            return round(x, 4)
        return x if isinstance(x, (int, bool, str)) else str(x)
    except Exception:
        return str(x)
def describe(b):
    rep = {"owner": names.get(bv.get_behaviour_owner(b))}
    for p in bv.get_behaviour_property_names(b):
        t = str(bv.get_property_type(b, p))
        r = {"kind": t}
        try:
            if "Data" in t and "Range" not in t:
                d = bv.get_behaviour_data(b, p)
                r["per_frame"] = {f: val(dv.get_data_value(d, f)) for f in FR}
            elif "Setting" in t and "Range" not in t:
                s = bv.get_behaviour_setting(b, p)
                r["value"] = val(dv.get_setting_value(s)) if hasattr(dv, "get_setting_value") else "no get_setting_value"
            elif "Object" in t and "Range" in t:
                r["objects"] = [names.get(o) for o in bv.get_behaviour_objects_range(b, p)][:40]
            elif "Object" in t:
                r["object"] = names.get(bv.get_behaviour_object(b, p))
            elif "String" in t:
                r["value"] = bv.get_behaviour_string(b, p)
        except Exception as e:
            r["err"] = str(e)[:200]
        rep[p] = r
    return rep
out = {"data_viewer_api": [n for n in dir(dv) if not n.startswith("_")]}
for o, n in names.items():
    for b in bv.get_behaviours(o):
        bn = bv.get_behaviour_name(b)
        if bn in ("AutoPhysics", "CenterOfMass", "FulcrumGroup"):
            out.setdefault(bn, []).append(describe(b))
for pt in ("foot_MainPoint_l", "foot_MainPoint_r", "ball_MainPoint_l", "hand_MainPoint_r", "hand_MainPoint_l", "pelvis_MainPoint"):
    if pt not in ids:
        out.setdefault("missing", []).append(pt); continue
    for bn in ("FulcrumPoint", "CompensationMotion", "SecondaryMotion", "SeparationOfMotion", "AutoPhysicsApply"):
        try:
            b = bv.get_behaviour_by_name(ids[pt], bn)
            out.setdefault("points", {}).setdefault(pt, {})[bn] = describe(b)
        except Exception as e:
            out.setdefault("points", {}).setdefault(pt, {})[bn] = str(e)[:200]
dyn = []
for o, n in names.items():
    for b in bv.get_behaviours(o):
        if bv.get_behaviour_name(b) == "Dynamic":
            try:
                bname = bv.get_behaviour_string(b, "behaviourName")
            except Exception:
                bname = "?"
            if re.search(r"(?i)phys|fulcrum|second|compens|separ|smooth|motion|inert|priority", bname or ""):
                dyn.append({"owner": n, "name": bname})
out["dynamic_physics"] = dyn[:60]
out["fulcrum_named_points"] = sorted(n for n in names.values() if re.search(r"(?i)fulcrum|toe|heel|Self\dPoint|ball", n))
print(json.dumps(out, default=str))
