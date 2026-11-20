# Run start v4r5 (runstart-v5-desktop, 2026-11-20): owner clarified that the "stomp" he wanted belongs to the RUN STOP, so the run start
# loses its planted first step: v4r4 f0-9 (knee lift, stomp, standstill ~0.33 s) are cut and the clip starts on the loaded crouch
# right after the stomp (v4r4 f10), straight into the frantic rev-up. Everything else of v4r4 (elbows, hands, arms, tail) is unchanged.
# Run in Cascadeur (run_python, exec) on RunStart_v4r5.casc (= copy of v4r4). Every rig point (IK main/additional/direction/limb-dir
# points and the roots) is read at f+SHIFT and written at f; the range is then cut to LAST-SHIFT. Result = short dict.
import numpy as np

SHIFT = int(globals().get('SHIFT_OVERRIDE', 10))
LAST = 68                     # v4r4 last frame (69 f at 30 fps)
SCENE_END = 'runstart/RunStart_v4r5.casc'

tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
ids = {}
for o in mv.get_objects():
    n = mv.get_object_name(o)
    if not n or n.startswith('EdgeView') or 'constraint' in n or n == 'Points':
        continue
    if not ('Point' in n or 'LimbDir' in n or n in ('root', 'ik_foot_root', 'ik_hand_root')):
        continue
    try:
        ids[n] = bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
    except Exception:
        pass
src = {f: {n: np.array([float(c) for c in dv.get_data_value(i, f + SHIFT)], dtype=np.float32) for n, i in ids.items()}
       for f in range(0, LAST - SHIFT + 1)}

def change(me, ue, su):
    de = me.data_editor()
    for f, pose in src.items():
        for n, v in pose.items():
            de.set_data_value(ids[n], f, v)
        su.run_update(set(ids.values()), f)

ok = scene.modify_update('runstart v4r5 trim planted step', change)
err = 0.0
for f, pose in src.items():
    for n, v in pose.items():
        err = max(err, float(np.linalg.norm(np.array([float(c) for c in dv.get_data_value(ids[n], f)]) - v)))
result = {'ok': bool(ok), 'points': len(ids), 'frames': len(src), 'max_err_cm': round(err, 3)}
