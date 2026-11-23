# Run start v4r4 = extra pose round 4 (owner, 2026-11-20): v4r3 elbows too high ("zombie": upper arm stuck, only forearms move).
# Run in Cascadeur (run_python, exec) on RunStart_v4r4.casc (= copy of v4r3). Input DPSI_JSON from runstart_elbow_targets_v4r4.py:
# per side and frame, the rotation (rad) of the elbow about the shoulder->hand axis that brings the elbow height (along the torso
# up axis) to the v4r2 height at the same hand phase. The forearm and hand points rotate RIGIDLY about that axis through the
# shoulder: the fist position is unchanged by construction (it lies on the axis), wrist bend relative to the forearm unchanged;
# nothing else moves (torso, legs, clavicle, upper arm root untouched).
import json, math
import numpy as np

SCENE_END = 'runstart/RunStart_v4r4.casc'
DPSI = json.load(open(DPSI_JSON))
ROT = {s: ['lowerarm_MainPoint_' + s, 'lowerarm_AdditionalPoint_' + s, 'lowerarm_LimbDir_' + s,
           'hand_AdditionalPoint_' + s, 'hand_DirectionPoint_' + s] for s in 'lr'}
KEEP = {s: ['upperarm_MainPoint_' + s, 'hand_MainPoint_' + s] for s in 'lr'}
ALL = sum(ROT.values(), []) + sum(KEEP.values(), [])

tab = app.get_scene_manager().current_scene()
assert str(tab.get_path_name()).replace("\\", "/").endswith(SCENE_END), tab.get_path_name()
scene = tab.domain_scene()
mv = scene.model_viewer(); bv = mv.behaviour_viewer(); dv = mv.data_viewer()
def pid(n):
    o = mv.get_objects(n)[0]
    return bv.get_behaviour_data(bv.get_behaviour_by_name(o, 'Transform'), 'global_position')
ids = {n: pid(n) for n in ALL}
frames = list(range(0, 69))
orig = {f: {n: np.array([float(c) for c in dv.get_data_value(ids[n], f)]) for n in ALL} for f in frames}

def rodrigues(k, a):
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * K @ K

target = {}
for f in frames:
    D = {}
    for s in 'lr':
        a = DPSI[s][f]
        if abs(a) < 1e-6: continue
        S, H = orig[f]['upperarm_MainPoint_' + s], orig[f]['hand_MainPoint_' + s]
        k = (H - S) / np.linalg.norm(H - S); R = rodrigues(k, a)
        for n in ROT[s]:
            D[n] = S + R @ (orig[f][n] - S)
    target[f] = D

def change(me, ue, su):
    de = me.data_editor()
    for f in frames:
        if not target[f]: continue
        for n, v in target[f].items():
            de.set_data_value(ids[n], f, np.array(v, dtype=np.float32))
        su.run_update(set(ids.values()), f)

ok = scene.modify_update('runstart v4r4 elbows to v4r2 height', change)
err = 0.0; hand = 0.0; uarm = 0.0
for f in frames:
    now = {n: np.array([float(c) for c in dv.get_data_value(ids[n], f)]) for n in ALL}
    for n, v in target[f].items(): err = max(err, float(np.linalg.norm(now[n] - v)))
    for s in 'lr':
        hand = max(hand, float(np.linalg.norm(now['hand_MainPoint_' + s] - orig[f]['hand_MainPoint_' + s])))
        uarm = max(uarm, float(np.linalg.norm(now['upperarm_MainPoint_' + s] - orig[f]['upperarm_MainPoint_' + s])))
result = {'ok': bool(ok), 'max_target_err_cm': round(err, 3), 'hand_drift_cm': round(hand, 3), 'shoulder_drift_cm': round(uarm, 3)}
