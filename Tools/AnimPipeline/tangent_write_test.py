# Write test of key tangent settings on the dense copy (jab_dense.casc), key at frame 17.
import csc, json
app = csc.app.get_application()
vs = app.get_scene_manager().current_scene()
assert vs.get_path_name().endswith("jab_dense.casc"), vs.get_path_name()
mv = scene.model_viewer(); lv = scene.layers_viewer(); dv = scene.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
lid = lv.layer_id_by_obj_id(ids["hand_MainPoint_r"])
def hz(f):
    for d in dv.get_all_data_id(ids["hand_r"]):
        try:
            if dv.get_data_name(d) == "Position":
                return round(float(dv.get_data_value(d, f)[2]), 2)
        except Exception:
            pass
before_k = lv.layer(lid).key(8)
before = {"tangents": str(before_k.tangents), "lw": before_k.left_weight_velocity, "rw": before_k.right_weight_velocity,
          "hand_z_9_16": [hz(f) for f in range(9, 17)]}
I = csc.layers.layer.Interpolation
def mod(model, update, su):
    le = model.layers_editor()
    def fn(s):
        s.interval.interpolation = I.BEZIER       # replace the AI interval 8->17
        s.key.tangents = csc.layers.layer.Tangents.UserDefined
        s.key.right_weight_velocity = 0.05        # leave key 8 almost flat (slow start)
    le.change_section(8, lid, fn)
    def fn17(s):
        s.key.tangents = csc.layers.layer.Tangents.UserDefined
        s.key.left_weight_velocity = 0.9          # long ease-in into key 17
    le.change_section(17, lid, fn17)
ok = scene.modify_update("Tangent test", mod)
k8 = lv.layer(lid).key(8); k17 = lv.layer(lid).key(17)
after = {"interp_8": str(lv.layer(lid).interval(8).interpolation), "k8": [str(k8.tangents), k8.left_weight_velocity, k8.right_weight_velocity],
         "k17": [str(k17.tangents), k17.left_weight_velocity, k17.right_weight_velocity], "hand_z_9_16": [hz(f) for f in range(9, 17)]}
print(json.dumps({"ok": ok, "before": before, "after": after}))
