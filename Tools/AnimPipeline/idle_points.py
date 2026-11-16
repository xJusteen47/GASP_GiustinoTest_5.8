"""Rig points (Cascadeur IK points) for a pose known only as Unreal bones: the project's standing idle, the start and FAF
target of every grounded move (owner, 31/10: "the guard you start from is always an idle where the character stands
upright"; crouched and aerial moves excepted).

Each Cascadeur point is bound to the Unreal bone it rides on. Its offset in that bone's frame is measured on a pose known
both ways (jab v11 frame 0: v11_targets.json and the v11 bone samples), then re-applied to the idle's bones.
UE component space (X left, Y forward, Z up) -> Cascadeur (X left, Y up, Z forward): swap Y and Z.
Usage: python idle_points.py [anim path] [take] [frame]  ->  idle_points.json"""
import sys, os, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

HERE = os.path.dirname(os.path.abspath(__file__))
P = np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]], dtype=float)

def bone_of(n):
    s = n[-2:] if n.endswith(("_l", "_r")) else ""
    base = n.split("_")[0]
    table = {"hand": "hand", "lowerarm": "lowerarm", "upperarm": "upperarm", "clavicle": "clavicle", "thigh": "thigh",
             "calf": "calf", "foot": "foot", "ball": "ball", "pelvis": "pelvis", "head": "head"}
    if n.startswith("spine_02"):
        return "spine_02"
    if n.startswith("spine_04"):
        return "spine_04"
    if n.startswith("neck_01"):
        return "neck_01"
    return table[base] + s

def qmat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])

def frame_of(fr, b):
    p, q = L.bone_tf(fr, b)
    return P @ p, P @ qmat(q) @ P          # bone origin and rotation, in Cascadeur space

def offsets(points, fr):
    out = {}
    for n, v in points.items():
        o, R = frame_of(fr, bone_of(n))
        out[n] = R.T @ (np.array(v) - o)
    return out

def apply(offs, fr):
    out = {}
    for n, d in offs.items():
        o, R = frame_of(fr, bone_of(n))
        out[n] = (o + R @ d).tolist()
    return out

if __name__ == "__main__":
    anim = sys.argv[1] if len(sys.argv) > 1 else "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
    take = sys.argv[2] if len(sys.argv) > 2 else "idle_neutral"
    f = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    T = json.load(open(os.path.join(HERE, "v11_targets.json")))
    pts = {n: v[0] for n, v in T["targets"].items()}; pts.update(T["feet"])
    v11 = json.load(open(os.path.join(L.RAW, "v11_legbones.json")))["frames"]["0"]
    offs = offsets(pts, v11)
    back = apply(offs, v11)
    err = max(float(np.linalg.norm(np.array(back[n]) - np.array(pts[n]))) for n in pts)
    idle = L.sample(anim, take)[str(f)]
    res = apply(offs, idle)
    json.dump({"source": anim, "frame": f, "points": res, "selfcheck_cm": round(err, 3)}, open(os.path.join(HERE, "idle_points.json"), "w"), indent=1)
    floor = min(res[n][1] for n in res if n.startswith(("ball_MainPoint", "foot_Self0")))
    print(json.dumps({"selfcheck_cm": round(err, 3), "pelvis_y": round(res["pelvis_MainPoint"][1], 1), "floor_y": round(floor, 1),
                      "feet": {n: [round(x, 1) for x in res[n]] for n in ("foot_MainPoint_l", "foot_MainPoint_r", "ball_MainPoint_l", "ball_MainPoint_r")},
                      "hands": {n: [round(x, 1) for x in res[n]] for n in ("hand_MainPoint_l", "hand_MainPoint_r")}}))
