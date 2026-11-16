"""Wrist check (owner, v8: "the right fist stays bent inward; the front fist stays fixed forward at frame 3 while the elbow goes
back"). Per frame and hand, from the UE bone samples (component space, cached by legibility.py):
  flexion   = bend of the fist axis (wrist -> middle knuckle) toward the palm, about the knuckle line (+ = toward the palm)
  deviation = sideways bend toward the thumb (+, radial) or the little finger (-, ulnar), about the palm normal
  both measured against the forearm axis (elbow -> wrist).
  frozen    = the hand's world rotation changes much less than the forearm's for 2+ frames while the forearm turns fast:
              the fist stays aimed while the arm moves (the frame-3 problem).
Usage: python wrist_check.py <take> [<take> ...]   -> prints a per-frame table; motion_check.py uses wrist() for the rules."""
import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def wrist(frames):
    out = {"l": [], "r": []}
    n = len(frames)
    for s in "lr":
        prev = None
        for f in range(n):
            fr = frames[str(f)]
            el = L.bone_tf(fr, f"lowerarm_{s}")[0]; wr, qh = L.bone_tf(fr, f"hand_{s}")
            _, qf = L.bone_tf(fr, f"lowerarm_{s}")
            k = L.bone_tf(fr, f"middle_metacarpal_{s}")[0] * 0.5 + L.bone_tf(fr, f"middle_01_{s}")[0] * 0.5
            fa = unit(wr - el)                                           # forearm axis
            ha = unit(L.bone_tf(fr, f"middle_01_{s}")[0] - wr)            # fist axis: wrist -> middle knuckle
            across = unit(L.bone_tf(fr, f"index_01_{s}")[0] - L.bone_tf(fr, f"pinky_01_{s}")[0])   # knuckle line, toward the thumb side
            tip = L.bone_tf(fr, f"middle_03_{s}")[0] - L.bone_tf(fr, f"middle_01_{s}")[0]
            # forearm frame: axis, across (orthogonalised), palm normal (side the curled fingers point to)
            ac = unit(across - np.dot(across, fa) * fa)
            pn = np.cross(fa, ac)
            if np.dot(pn, tip) < 0:
                pn = -pn
            flex = math.degrees(math.atan2(np.dot(ha, pn), np.dot(ha, fa)))
            dev = math.degrees(math.atan2(np.dot(ha, ac), np.dot(ha, fa)))
            rot_h = rot_f = None
            if prev is not None:
                rot_h = L.qangle(prev[0], qh); rot_f = L.qangle(prev[1], qf)
            out[s].append({"frame": f, "flexion": round(flex, 1), "deviation": round(dev, 1),
                           "hand_rot": None if rot_h is None else round(rot_h, 1), "forearm_rot": None if rot_f is None else round(rot_f, 1)})
            prev = (qh, qf)
    return out

def frozen_frames(rows, min_forearm_deg=15.0, max_ratio=0.35, min_run=2):
    """Frames where the forearm turns fast (> min_forearm_deg/frame) but the hand turns < max_ratio of that, for min_run+ frames."""
    hits = [r["frame"] for r in rows if r["forearm_rot"] and r["forearm_rot"] > min_forearm_deg and r["hand_rot"] is not None
            and r["hand_rot"] < max_ratio * r["forearm_rot"]]
    runs, cur = [], []
    for f in hits:
        if cur and f == cur[-1] + 1:
            cur.append(f)
        else:
            if len(cur) >= min_run:
                runs.append(cur)
            cur = [f]
    if len(cur) >= min_run:
        runs.append(cur)
    return runs

if __name__ == "__main__":
    for take in sys.argv[1:]:
        frames = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))["frames"]
        w = wrist(frames)
        print(f"== {take}  (flexion + toward palm / deviation + toward thumb; deg)  frames 0..12, 18, 30")
        for s in "lr":
            sel = [r for r in w[s] if r["frame"] <= 12 or r["frame"] in (18, 30)]
            print(f"  hand_{s} flex/dev:", " ".join(f"{r['frame']}:{r['flexion']:+.0f}/{r['deviation']:+.0f}" for r in sel))
            print(f"  hand_{s} rot hand/forearm:", " ".join(f"{r['frame']}:{r['hand_rot']}/{r['forearm_rot']}" for r in w[s][1:9]))
            print(f"  hand_{s} frozen runs:", frozen_frames(w[s]))
