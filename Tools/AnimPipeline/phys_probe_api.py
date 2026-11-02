import os
# Load jab_phys_test.casc (a copy of v5) as its own tab and dump the APIs relevant to AutoPhysics:
# behaviour viewer/editor, setting manager, tools manager. Read-only on every other tab.
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
NAME = "jab_phys_test.casc"
if not any(s.name() == NAME for s in sm.scenes()):
    app.get_data_source_manager().load_scene(base + NAME)
tab = [s for s in sm.scenes() if s.name() == NAME][0]
sm.set_current_scene(tab)
ds = tab.domain_scene()
def api(o, pfx):
    names = [n for n in dir(o) if not n.startswith("_")]
    return {n: (getattr(type(o), n).__doc__ or "").split("\n")[0][:200] for n in names}
out = {"current": sm.current_scene().name(), "path": str(tab.get_path_name())}
out["dsm"] = api(app.get_data_source_manager(), "dsm")
out["scene_manager"] = api(sm, "sm")
out["behaviour_viewer"] = api(ds.behaviour_viewer(), "bv")
out["setting_manager"] = api(app.get_setting_manager(), "set")
tm = app.get_tools_manager()
out["tools_manager"] = api(tm, "tm")
out["domain_scene"] = api(ds, "ds")
print(json.dumps(out, default=str))
