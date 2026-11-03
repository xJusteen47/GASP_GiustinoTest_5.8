# Read-only: list open tabs and the action-manager / behaviour APIs available for the physics-tools check.
import csc, json
app = csc.app.get_application(); sm = app.get_scene_manager()
out = {"tabs": [], "current": sm.current_scene().name() if sm.current_scene() else None}
for s in sm.scenes():
    out["tabs"].append({"name": s.name(), "path": str(s.get_path_name()), "frames": s.domain_scene().data_viewer().get_animation_size()})
am = app.get_action_manager()
out["action_manager"] = [n for n in dir(am) if not n.startswith("_")]
for n in out["action_manager"]:
    out["doc_am_" + n] = (getattr(type(am), n).__doc__ or "")[:400]
out["app"] = [n for n in dir(app) if not n.startswith("_")]
out["scene_api"] = [n for n in dir(scene) if not n.startswith("_")]
mv = scene.model_viewer()
out["model_viewer"] = [n for n in dir(mv) if not n.startswith("_")]
print(json.dumps(out, default=str))
