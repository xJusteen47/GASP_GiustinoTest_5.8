import csc, json
mv = scene.model_viewer(); dv = scene.data_viewer(); lv = scene.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
def val(name, data, f):
    for d in dv.get_all_data_id(ids[name]):
        try:
            if dv.get_data_name(d) == data:
                return [round(float(x), 1) for x in dv.get_data_value(d, f)]
        except Exception as e:
            return "ERR " + str(e)[:60]
frames = [0, 1, 2, 3, 5, 8, 12, 17, 27, 37]
L = lv.layer(lv.layer_id_by_obj_id(ids["hand_MainPoint_r"]))
out = {
    "anim_size": dv.get_animation_size(),
    "keys": sorted(L.key_frame_indices()),
    "interp": {p: str(L.interval(p).interpolation) for p in [0, 1, 3, 8, 17]},
    "hand_r_joint": {f: val("hand_r", "Position", f) for f in frames},
    "hand_r_point": {f: val("hand_MainPoint_r", "Position", f) for f in frames},
    "root_joint": {f: val("root", "Position", f) for f in [0, 1, 17, 37]},
    "foot_r_joint": {f: val("foot_r", "Position", f) for f in [0, 1, 8, 37]},
    "fps": csc.app.get_application().get_scene_manager().current_scene().get_setting_handler().get_float_value("Scene", "Fps"),
}
print(json.dumps(out, default=str))
