"""Straighten the fists: per frame and hand, rotate the Cascadeur hand points (Direction, Additional) about the wrist so the
fist axis (wrist -> middle knuckle, measured on the UE bones of the source take) lines up with the forearm, with a small
natural curl toward the palm and no sideways bend. Positions (wrist, elbow) are untouched.
UE component space -> Cascadeur space: the same numbers with Y and Z swapped (UE X left, Y fwd, Z up; Cascadeur X left, Y up, Z fwd).
Usage: python wrist_fix.py <UE take with cached bones> <targets in> <targets out> [curl_deg=10] [frames=all|a-b]"""
import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def rot_between(a, b):
    a, b = unit(a), unit(b)
    v = np.cross(a, b); c = float(np.dot(a, b))
    if c < -0.9999:
        ax = unit(np.cross(a, [1.0, 0, 0]) if abs(a[0]) < 0.9 else np.cross(a, [0, 1.0, 0]))
        return 2 * np.outer(ax, ax) - np.eye(3)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1.0 / (1.0 + c))

P = np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]], dtype=float)      # UE <-> Cascadeur axis swap (its own inverse)

def correction(fr, s, curl):
    tf = lambda b: L.bone_tf(fr, b)[0]
    wr, el = tf(f"hand_{s}"), tf(f"lowerarm_{s}")
    fa = unit(wr - el)
    ha = unit(tf(f"middle_01_{s}") - wr)
    across = unit(tf(f"index_01_{s}") - tf(f"pinky_01_{s}"))
    tip = tf(f"middle_03_{s}") - tf(f"middle_01_{s}")
    ac = unit(across - np.dot(across, fa) * fa)
    pn = np.cross(fa, ac)
    if np.dot(pn, tip) < 0:
        pn = -pn
    t = math.radians(curl)
    h_des = unit(math.cos(t) * fa + math.sin(t) * pn)
    return P @ rot_between(ha, h_des) @ P

if __name__ == "__main__":
    take, tin, tout = sys.argv[1], sys.argv[2], sys.argv[3]
    curl = float(sys.argv[4]) if len(sys.argv) > 4 else 10.0
    rng = sys.argv[5] if len(sys.argv) > 5 else "all"
    bones = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))["frames"]
    T = json.load(open(tin))
    frames = T["frames"]
    sel = set(frames) if rng == "all" else set(range(int(rng.split("-")[0]), int(rng.split("-")[1]) + 1))
    changed = 0
    for i, f in enumerate(frames):
        if f not in sel:
            continue
        for s in "lr":
            R = correction(bones[str(f)], s, curl)
            w = np.array(T["targets"][f"hand_MainPoint_{s}"][i])
            for n in (f"hand_DirectionPoint_{s}", f"hand_AdditionalPoint_{s}"):
                p = np.array(T["targets"][n][i])
                T["targets"][n][i] = (w + R @ (p - w)).tolist()
            changed += 1
    # temporal smoothing of the hand offsets (Direction/Additional relative to the wrist) outside the snap: each frame's
    # correction is computed on its own, so tiny orientation noise would wobble the knuckle path (arc jitter check)
    sm = [a for a in sys.argv[6:] if a.startswith("--smooth=")]
    if sm:
        s0, s1 = (int(x) for x in sm[0].split("=")[1].split("-"))
        for s in "lr":
            for n in (f"hand_DirectionPoint_{s}", f"hand_AdditionalPoint_{s}"):
                W = np.array(T["targets"][f"hand_MainPoint_{s}"]); Q = np.array(T["targets"][n]); off = Q - W
                ln = np.linalg.norm(off, axis=1)
                new = off.copy()
                for _ in range(2):
                    src = new.copy()
                    for i in range(len(frames)):
                        if s0 <= frames[i] <= s1 and 0 < i < len(frames) - 1:
                            new[i] = 0.25 * src[i - 1] + 0.5 * src[i] + 0.25 * src[i + 1]
                new = new / np.linalg.norm(new, axis=1, keepdims=True) * ln[:, None]
                T["targets"][n] = (W + new).tolist()
    json.dump(T, open(tout, "w"))
    print(json.dumps({"from": take, "in": tin, "out": tout, "curl_deg": curl, "hands_changed": changed}))
