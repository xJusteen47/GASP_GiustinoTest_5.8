"""Offline check of v8_targets.json (Cascadeur space) before going through Cascadeur/UE: rear elbow vs the chest back plane,
rear forearm and hand vs the chest/head capsules (rough radii from the physics asset), per frame."""
import json, sys, math
import numpy as np
T = json.load(open(sys.argv[1] if len(sys.argv) > 1 else "v8_targets.json"))
fr = [0] + T["frames"]
src = json.load(open("v7_src_points.json"))["points"]
def P(n, k):
    if k == 0:
        return np.array(T["targets"][n][0]) if False else None
    return np.array(T["targets"][n][k - 1])
def segd(a0, a1, b0, b1):
    best = 1e9
    for t in np.linspace(0, 1, 15):
        p = a0 + (a1 - a0) * t; ab = b1 - b0
        u = float(np.clip(np.dot(p - b0, ab) / max(np.dot(ab, ab), 1e-9), 0, 1))
        best = min(best, float(np.linalg.norm(p - (b0 + ab * u))))
    return best
rows = []
for k in T["frames"]:
    g = lambda n: np.array(T["targets"][n][T["frames"].index(k)])
    ul, ur, ch, sp2, nk = g("upperarm_MainPoint_l"), g("upperarm_MainPoint_r"), g("spine_04_MainPoint"), g("spine_02_MainPoint"), g("neck_01_MainPoint")
    side = ul - ur; side[1] = 0; side /= np.linalg.norm(side)
    fwd = np.cross(side, np.array([0.0, 1.0, 0.0]))     # Cascadeur (X left, Y up, Z fwd): chest facing = side x up
    el, hr, hd = g("lowerarm_MainPoint_r"), g("hand_MainPoint_r"), g("head_MainPoint")
    back = float(np.dot(el - ch, -fwd)) - 12.8
    out_ = float(np.dot(el - ch, -side))
    fore = min(segd(el, hr, sp2, ch), segd(el, hr, ch, nk)) - 4.8 - 13.0
    rows.append((k, round(back, 1), round(out_, 1), round(fore, 1)))
print("k, rear elbow behind back plane (cm, >0 = behind), elbow out to the side from chest centre (cm), forearm-chest gap proxy (cm)")
for r in rows[:22]:
    print(r)
