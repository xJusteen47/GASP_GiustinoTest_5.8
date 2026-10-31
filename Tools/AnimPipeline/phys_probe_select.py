# Read-only: selection / interval APIs and SettingsManager key format (does a real key return its ini value?).
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_phys_test.casc"][0]
ds = tab.domain_scene()
def api(o):
    out = {}
    for n in dir(o):
        if n.startswith("_"):
            continue
        try:
            out[n] = (getattr(type(o), n).__doc__ or "").split("\n")[0][:230]
        except Exception:
            out[n] = "?"
    return out
out = {"selector_type": str(type(ds.selector())), "selector": api(ds.selector()),
       "layers_selector_type": str(type(ds.get_layers_selector())), "layers_selector": api(ds.get_layers_selector()),
       "view_scene": api(tab)}
setm = app.get_setting_manager()
keys = {"float": ["AutoPhysics/Secondary alpha", "AutoPhysics/Secondary%20alpha", "AutoPhysics.Secondary alpha", "Secondary alpha",
                  "Physics corrector/Key frame weight", "Trajectory smoothness/Ballistic frame weight"],
        "bool": ["Secondary motion/Floor collision", "Fulcrum Motion Cleaning/Adjust to surface", "Scene/FixingInterpolationOnInrevalChange"]}
res = {}
for k in keys["float"]:
    try: res[k] = setm.get_float_value(k)
    except Exception as e: res[k] = "ERR " + str(e)[:100]
for k in keys["bool"]:
    try: res[k] = setm.get_bool_value(k)
    except Exception as e: res[k] = "ERR " + str(e)[:100]
out["settings_reads"] = res
print(json.dumps(out, default=str))
