"""Stick strip of a *_bones.json take: rows side / back / top, every frame (or every Nth). python strip_bones.py <bones.json> <out.png> [first] [last]"""
import json, sys, numpy as np
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
F = json.load(open(sys.argv[1]))["frames"]; n = len(F)
a = int(sys.argv[3]) if len(sys.argv) > 3 else 0; b = int(sys.argv[4]) if len(sys.argv) > 4 else n - 1
SEG = [("pelvis","spine_03"),("spine_03","spine_05"),("spine_05","neck_01"),("neck_01","head"),
       ("spine_05","upperarm_l"),("upperarm_l","lowerarm_l"),("lowerarm_l","hand_l"),("hand_l","middle_01_l"),
       ("spine_05","upperarm_r"),("upperarm_r","lowerarm_r"),("lowerarm_r","hand_r"),("hand_r","middle_01_r"),
       ("pelvis","thigh_l"),("thigh_l","calf_l"),("calf_l","foot_l"),("foot_l","ball_l"),
       ("pelvis","thigh_r"),("thigh_r","calf_r"),("calf_r","foot_r"),("foot_r","ball_r")]
fr = list(range(a, b + 1)); cols = len(fr)
VIEWS = [("side", lambda p: (p[1], p[2])), ("back", lambda p: (-p[0], p[2])), ("top", lambda p: (-p[0], p[1]))]
fig, ax = plt.subplots(3, cols, figsize=(1.3 * cols, 4.6))
for j, f in enumerate(fr):
    P = {k: np.array(v[0]) for k, v in F[str(f)].items()}
    for i, (nm, pr) in enumerate(VIEWS):
        A = ax[i][j]; A.set_aspect("equal"); A.axis("off")
        for s, e in SEG:
            c = "r" if e.endswith("_r") and ("arm" in e or "hand" in e or "middle" in e) else ("b" if e.endswith("_r") else "k")
            x0, y0 = pr(P[s]); x1, y1 = pr(P[e]); A.plot([x0, x1], [y0, y1], c, lw=2)
        if nm != "top": A.axhline(P["root"][2], color="g", lw=0.6)
        A.set_xlim(-90, 90) if nm != "side" else A.set_xlim(-80, 100)
        A.set_ylim(-10, 190) if nm != "top" else A.set_ylim(-80, 80)
        if i == 0: A.set_title(str(f), fontsize=8)
plt.subplots_adjust(0, 0, 1, 0.95, 0.02, 0.02); plt.savefig(sys.argv[2], dpi=80)
print("wrote", sys.argv[2])
