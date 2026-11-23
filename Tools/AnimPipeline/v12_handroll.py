"""Jab v12, direct-to-Unreal: the hand's Direction/Additional points follow the forearm's turn (minimal rotation of the
guard forearm onto the current one) instead of keeping the guard's world offsets. With the fixed offsets the solved fist
rolled 94-131 deg in one frame against its forearm (motion_check arm_twist, v12 first write); v11 hid it because
Cascadeur interpolated the hand. The lead fist keeps the v6 contact twist on frames 1-2 and blends back over 3-6.
Usage: python v12_handroll.py <targets in> <targets out>"""
import json, sys
import numpy as np

def unit(v): return v / max(np.linalg.norm(v), 1e-9)
def rot_between(a, b):
    a, b = unit(a), unit(b); v = np.cross(a, b); c = float(np.dot(a, b))
    if c < -0.9999: return -np.eye(3)
    K = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + K + K @ K / (1 + c)

d = json.load(open(sys.argv[1])); T = d["targets"]; n = len(d["frames"])
P = lambda name, k: np.array(T[name][k], float)
for s in "lr":
    fa0 = P(f"hand_MainPoint_{s}", 0) - P(f"lowerarm_MainPoint_{s}", 0)
    pts = [f"hand_DirectionPoint_{s}", f"hand_AdditionalPoint_{s}"]
    off0 = {p: P(p, 0) - P(f"hand_MainPoint_{s}", 0) for p in pts}
    offc = {p: P(p, 2) - P(f"hand_MainPoint_{s}", 2) for p in pts}
    fac = P(f"hand_MainPoint_{s}", 2) - P(f"lowerarm_MainPoint_{s}", 2)
    for k in range(n):
        if s == "l" and k in (1, 2):
            continue
        h = P(f"hand_MainPoint_{s}", k); fa = h - P(f"lowerarm_MainPoint_{s}", k)
        for p in pts:
            o = rot_between(fa0, fa) @ off0[p]
            if s == "l" and 3 <= k <= 6:
                w = (k - 2) / 4.0
                o = unit((1 - w) * (rot_between(fac, fa) @ offc[p]) + w * o) * np.linalg.norm(off0[p])
            T[p][k] = (h + o).tolist()
json.dump(d, open(sys.argv[2], "w"))
print("written", sys.argv[2])
