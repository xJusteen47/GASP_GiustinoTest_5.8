import csc, json
mv = scene.model_viewer(); lv = scene.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
L = lv.layer(lv.layer_id_by_obj_id(ids["hand_MainPoint_r"]))
out = {}
for f in sorted(L.key_frame_indices()):
    k = L.key(f)
    out[f] = {"tangents": str(k.tangents), "lw": k.left_weight_velocity, "rw": k.right_weight_velocity,
              "ik_fk": str(k.common.ik_fk), "fixation": str(k.common.fixation),
              "interp_after": str(L.interval(f).interpolation)}
print(json.dumps(out))
