# Placeholder jab for the pipeline round-trip test (crude poses, not final animation).
# Runs inside Cascadeur through the MCP script runner (scene, csc in scope).
# Cascadeur space: Y up, character faces +Z, its right side is -X. Units cm.
import csc, json
import numpy as np

KEYS = [0, 1, 3, 8, 17, 37]

guard = {  # frame 0 / 37: guard, right shoulder slightly back
    "hand_MainPoint_r": (-14, 138, 18), "lowerarm_MainPoint_r": (-24, 112, 2),
    "upperarm_MainPoint_r": (-17.7, 132.9, -6.0),
    "hand_MainPoint_l": (14, 142, 22), "lowerarm_MainPoint_l": (23, 115, 4),
    "upperarm_MainPoint_l": (17.7, 132.9, -1.0),
}
contact = {  # frame 1: right arm extended forward, torso twisted (right shoulder forward, left back)
    "hand_MainPoint_r": (-7, 138, 58), "lowerarm_MainPoint_r": (-12, 136, 31),
    "upperarm_MainPoint_r": (-15.5, 133.0, 6.0),
    "hand_MainPoint_l": (14, 142, 18), "lowerarm_MainPoint_l": (23, 115, 0),
    "upperarm_MainPoint_l": (18.5, 132.9, -7.0),
}
def lerp(a, b, t):
    return {k: tuple(a[k][i] + (b[k][i] - a[k][i]) * t for i in range(3)) for k in a}
overshoot = {k: (v[0], v[1], v[2] + (5 if k.endswith("_r") else 0)) for k, v in contact.items()}  # ~10% past contact
settle = lerp(guard, contact, 0.85)
ret = lerp(guard, contact, 0.0)
ret = {k: (v[0], v[1], v[2] + (2 if k.startswith("upperarm_MainPoint_r") else 0)) for k, v in ret.items()}  # shoulder neutral
POSES = {0: guard, 1: contact, 3: overshoot, 8: settle, 17: ret, 37: guard}

mv = scene.model_viewer(); lv = scene.layers_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
layer_ids = list(lv.all_layer_ids())
log = {}

def add_keys(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for f in KEYS:
            le.set_fixed_interpolation_or_key_if_need(lid, f, True)
    model.fit_animation_size_by_layers()
ok1 = scene.modify_update("Jab: keys", add_keys)

def set_poses(model, update, su):
    su.generate_update()
    for f, pose in POSES.items():
        data_ids = set()
        for name, p in pose.items():
            node = update.get_object_by_id(ids[name]).root_group().node_deep("Position")
            node.set_value(np.array(p, dtype=np.float32), f)
            data_ids.add(node.data_id())
        su.run_update(data_ids, f)
ok2 = scene.modify_update("Jab: poses", set_poses)

I = csc.layers.layer.Interpolation
PLAN = {0: I.STEP, 1: I.BEZIER, 3: I.BEZIER, 8: I.AI, 17: I.AI}
def set_interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        for pos, mode in PLAN.items():
            def fn(section, mode=mode):
                section.interval.interpolation = mode
            le.change_section(pos, lid, fn)
ok3 = scene.modify_update("Jab: interpolation", set_interp)

L = lv.layer(lv.layer_id_by_obj_id(ids["hand_MainPoint_r"]))
log.update(ok_keys=ok1, ok_poses=ok2, ok_interp=ok3, anim_size=scene.data_viewer().get_animation_size(),
           keys=sorted(L.key_frame_indices), interp={p: str(L.interval(p).interpolation) for p in PLAN})
print(json.dumps(log, default=str))
