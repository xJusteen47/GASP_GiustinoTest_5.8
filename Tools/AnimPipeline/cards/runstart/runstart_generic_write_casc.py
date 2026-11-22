# Writes a generic run start pose npz (runstart_generic_compose.py) into the CURRENT Cascadeur tab (run_python, exec).
# Scene must end with runstart/RunStart_Generic_<TAG>.casc (same rig as RunLoop_Relaxed_v0_gaspZ). Points are written as global
# positions, boxes as local rotation/position, on frames 0..N-1; keys after N-1 are deleted by the caller (keys.delete) before export.
import numpy as np
import csc

G = globals()
TAG = G.get('TAG', 'g1')
NPZ = G.get('NPZ', r'C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Saved/AnimPipeline/runstart/generic_%s_pose.npz' % TAG)
tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith('runstart/RunStart_Generic_%s.casc' % TAG), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
D = np.load(NPZ)
pts, boxes = {}, {}
for o in mv.get_objects():
    n = mv.get_object_name(o)
    if not n:
        continue
    try:
        tr = bv.get_behaviour_by_name(o, 'Transform')
    except Exception:
        continue
    try:
        if 'P|' + n in D.files:
            pts[n] = bv.get_behaviour_data(tr, 'global_position')
        elif 'R|' + n in D.files:
            boxes[n] = (bv.get_behaviour_data(tr, 'local_rotation'), bv.get_behaviour_data(tr, 'local_position'))
    except Exception:
        pass
N = D['P|root'].shape[0]


def change(me, ue, su):
    de = me.data_editor()
    ids = set(pts.values()) | {x for r_p in boxes.values() for x in r_p}
    for u in range(N):
        for n, i in pts.items():
            de.set_data_value(i, u, [float(c) for c in D['P|' + n][u]])
        for n, (ri, pi) in boxes.items():
            q = [float(c) for c in D['R|' + n][u]]
            de.set_data_value(ri, u, csc.math.Rotation.from_quaternion(*q))
            de.set_data_value(pi, u, [float(c) for c in D['T|' + n][u]])
        su.run_update(ids, u)


ok = scene.modify_update('runstart generic %s write' % TAG, change)
err = 0.0
for u in range(0, N, 5):
    for n, i in pts.items():
        err = max(err, float(np.linalg.norm(np.array([float(c) for c in dv.get_data_value(i, u)]) - D['P|' + n][u])))
result = {'ok': bool(ok), 'N': N, 'points': len(pts), 'boxes': len(boxes), 'max_err_cm': round(err, 3)}
