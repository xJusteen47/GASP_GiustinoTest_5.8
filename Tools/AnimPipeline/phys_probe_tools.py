# Read-only: list Cascadeur tools and the API of anything physics-related; try likely setting keys.
import csc, json, re
app = csc.app.get_application()
tm = app.get_tools_manager()
out = {}
tools = tm.tools()
def tname(t):
    for a in ("name", "get_name", "id", "key"):
        if hasattr(t, a):
            try:
                v = getattr(t, a)
                return str(v() if callable(v) else v)
            except Exception:
                pass
    return str(t)
out["tools"] = sorted(tname(t) for t in tools)
out["tool_obj_api"] = sorted(set(n for t in tools[:3] for n in dir(t) if not n.startswith("_")))
for key in ("AutoPhysicsTool", "AutoPhysics", "Auto Physics", "TweenMachine", "PhysicsTool", "FixFoot"):
    try:
        t = tm.get_tool(key)
        out["get_tool_" + key] = {"repr": str(t), "api": [n for n in dir(t) if not n.startswith("_")]}
    except Exception as e:
        out["get_tool_" + key] = "ERR " + str(e)[:150]
setm = app.get_setting_manager()
probe = {}
for k in ("AutoPhysicsTool.Secondary motion", "AutoPhysics.SecondaryMotion", "AutoPhysicsTool.Compensation motion",
          "AutoPhysicsTool.Smooth rotation", "AutoPhysicsTool.Separation of motion", "AutoPhysicsTool.Enabled"):
    try:
        probe[k] = setm.get_bool_value(k)
    except Exception as e:
        probe[k] = "ERR " + str(e)[:120]
out["settings_probe"] = probe
print(json.dumps(out, default=str))
