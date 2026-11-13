import csc, json
app = csc.app.get_application()
sm = app.get_scene_manager()
def hand(ds, name, f):
    mv = ds.model_viewer(); dv = ds.data_viewer()
    for o in mv.get_objects():
        if mv.get_object_name(o) == name:
            for d in dv.get_all_data_id(o):
                try:
                    if dv.get_data_name(d) == "Position":
                        return [round(float(x), 1) for x in dv.get_data_value(d, f)]
                except Exception:
                    pass
out = {"mcp_scene_hand_l_f1": hand(scene, "hand_l", 1), "mcp_scene_hand_r_f1": hand(scene, "hand_r", 1),
       "mcp_scene_size": scene.data_viewer().get_animation_size()}
for s in sm.scenes():
    ds = s.domain_scene()
    out[s.name()] = {"path": str(s.get_path_name()), "size": ds.data_viewer().get_animation_size(),
                     "hand_l_f1": hand(ds, "hand_l", 1), "hand_r_f1": hand(ds, "hand_r", 1),
                     "is_mcp_scene": ds == scene, "current": s.name() == sm.current_scene().name()}
print(json.dumps(out, default=str))
