# Read-only quality metrics on the current Cascadeur scene: per-frame joint angular speed,
# hand path reversals and foot drift, per interval. Prints JSON.
import csc, json, math
mv = scene.model_viewer(); dv = scene.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
joints = [n for n, o in ids.items() if mv.get_object_type_name(o) == "Joint"]
N = dv.get_animation_size()

def data_id(name, dname):
    for d in dv.get_all_data_id(ids[name]):
        try:
            if dv.get_data_name(d) == dname:
                return d
        except Exception:
            pass

def quat(r):
    q = r.to_quaternion()
    if all(hasattr(q, a) for a in ("w", "x", "y", "z")):
        g = lambda v: float(v() if callable(v) else v)
        return (g(q.w), g(q.x), g(q.y), g(q.z))
    try:
        return tuple(float(v) for v in q.coeffs())
    except Exception:
        return tuple(float(v) for v in list(q))

rot_ids = {j: data_id(j, "Local Rotation") for j in joints}
pos_ids = {j: data_id(j, "Position") for j in ["hand_r", "hand_l", "foot_l", "foot_r", "ball_l", "ball_r", "pelvis", "head"]}
Q = {j: [quat(dv.get_data_value(d, f)) for f in range(N)] for j, d in rot_ids.items() if d is not None}
P = {j: [[float(x) for x in dv.get_data_value(d, f)] for f in range(N)] for j, d in pos_ids.items()}

def ang(q1, q2):
    dot = abs(sum(a * b for a, b in zip(q1, q2)))
    return math.degrees(2 * math.acos(min(1.0, dot)))

speed = {j: [ang(q[f], q[f + 1]) for f in range(N - 1)] for j, q in Q.items()}  # deg per frame

def interval_report(a, b):
    rep = {}
    worst = []
    for j, s in speed.items():
        seg = s[a:b]
        if not seg:
            continue
        acc = [abs(seg[i + 1] - seg[i]) for i in range(len(seg) - 1)]
        worst.append((max(seg), max(acc) if acc else 0.0, j))
    worst.sort(reverse=True)
    rep["top_ang_speed_deg_per_frame"] = [(round(w[0], 1), round(w[1], 1), w[2]) for w in worst[:6]]
    hz = [P["hand_r"][f][2] for f in range(a, b + 1)]
    v = [hz[i + 1] - hz[i] for i in range(len(hz) - 1)]
    rep["hand_r_z"] = [round(x, 1) for x in hz]
    rep["hand_r_z_reversals"] = sum(1 for i in range(len(v) - 1) if v[i] * v[i + 1] < -1e-3 and abs(v[i]) > 0.2 and abs(v[i + 1]) > 0.2)
    for ft in ("foot_l", "foot_r", "ball_l", "ball_r"):
        pts = P[ft][a:b + 1]
        rep[ft + "_drift_cm"] = round(max(math.dist(pts[0], p) for p in pts), 2)
    rep["pelvis_y_range"] = round(max(p[1] for p in P["pelvis"][a:b + 1]) - min(p[1] for p in P["pelvis"][a:b + 1]), 2)
    return rep

out = {"frames": N, "joints": len(Q)}
for a, b in [(0, 1), (1, 3), (3, 8), (8, 17), (17, 37)]:
    out[f"{a}-{b}"] = interval_report(a, b)
print(json.dumps(out))
