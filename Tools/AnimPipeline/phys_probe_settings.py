# Read-only: AutoPhysics tool settings through the scene SettingsHandler(section, key), and the layer/section API.
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == globals().get("TAB", "jab_phys_test.casc")][0]
ds = tab.domain_scene()
sh = tab.get_setting_handler()
out = {"settings": {}}
for sec, key, kind in [("AutoPhysics", "Secondary motion", "bool"), ("AutoPhysics", "Secondary alpha", "float"),
                       ("AutoPhysics", "Compensation motion", "bool"), ("AutoPhysics", "Smooth rotation", "bool"),
                       ("AutoPhysics", "Separation of motion", "bool"), ("Secondary motion", "Floor collision", "bool"),
                       ("Physics corrector", "Key frame weight", "float"), ("Trajectory smoothness", "Ballistic frame weight", "float"),
                       ("Scene", "FixingInterpolationOnInrevalChange", "bool")]:
    try:
        v = sh.get_bool_value(sec, key) if kind == "bool" else sh.get_float_value(sec, key)
    except Exception as e:
        v = "ERR " + str(e)[:120]
    out["settings"][f"{sec}/{key}"] = v
lv = ds.layers_viewer()
lid = list(lv.all_layer_ids())[0]
layer = lv.layer(lid)
out["layer_api"] = {n: (getattr(type(layer), n).__doc__ or "").split("\n")[0][:200] for n in dir(layer) if not n.startswith("_")}
out["layers_viewer_api"] = {n: (getattr(type(lv), n).__doc__ or "").split("\n")[0][:200] for n in dir(lv) if not n.startswith("_")}
out["n_layers"] = len(list(lv.all_layer_ids()))
print(json.dumps(out, default=str))
