# Read-only: API of the editors behind the physics-related tools.
import csc, json
app = csc.app.get_application()
tm = app.get_tools_manager(); sm = app.get_scene_manager(); tab = [s for s in sm.scenes() if s.name() == "jab_phys_test.casc"][0]
out = {}
for key in ("AutoPhysicsTool", "FixFootTool", "ShowFulcrumPointsTool", "InbetweeningTool", "Timeline", "LiveLinkTool"):
    t = tm.get_tool(key)
    rep = {"tool_name": None}
    try:
        rep["tool_name"] = t.name() if callable(t.name) else t.name
    except Exception as e:
        rep["tool_name"] = "ERR " + str(e)[:100]
    try:
        e = t.editor(tab)
        rep["editor_repr"] = str(e)
        if e is not None:
            api = {}
            for n in dir(e):
                if n.startswith("_"):
                    continue
                try:
                    api[n] = (getattr(type(e), n).__doc__ or "").split("\n")[0][:220]
                except Exception:
                    api[n] = "?"
            rep["editor_api"] = api
    except Exception as ex:
        rep["editor_err"] = str(ex)[:200]
    out[key] = rep
print(json.dumps(out, default=str))
