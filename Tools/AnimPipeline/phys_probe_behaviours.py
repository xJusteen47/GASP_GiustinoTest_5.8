# Read-only scan of jab_phys_test.casc: every behaviour type with its property names, plus the objects whose
# behaviours mention physics / fulcrum / centre of mass / secondary / compensation / stiffness / damping.
import csc, json, re
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_phys_test.casc"][0]
ds = tab.domain_scene()
mv = ds.model_viewer(); bv = ds.behaviour_viewer()
KW = re.compile(r"(?i)phys|fulcrum|fucrum|mass|secondary|compens|stiff|damp|muscle|priority|inertia|separation|smooth|blend")
types = {}
hits = []
for o in mv.get_objects():
    oname = mv.get_object_name(o)
    try:
        bs = bv.get_behaviours(o)
    except Exception as e:
        hits.append({"obj": oname, "err": str(e)}); continue
    for b in bs:
        bn = bv.get_behaviour_name(b)
        try:
            props = bv.get_behaviour_property_names(b)
        except Exception:
            props = []
        if bn not in types:
            types[bn] = {"count": 0, "props": props}
        types[bn]["count"] += 1
        if KW.search(bn) or any(KW.search(p) for p in props) or KW.search(oname):
            hits.append({"obj": oname, "type": mv.get_object_type_name(o), "beh": bn, "props": props})
print(json.dumps({"n_types": len(types), "types": types, "hits": hits[:200]}, default=str))
