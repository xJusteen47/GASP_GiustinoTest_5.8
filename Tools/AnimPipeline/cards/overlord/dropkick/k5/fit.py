import sys, json, os, copy
sys.path.insert(0, os.getcwd())
import numpy as np
from animpipe import author as A
card = json.load(open(sys.argv[1])); au = card["author"]; po = A.Poser(au.get("thumb","along"))
base = copy.deepcopy(au["poses"][sys.argv[2]])
GOAL = {"l": {"hand": np.array([-6., 19., -14.]), "lowerarm": np.array([20., 13., -21.])},
        "r": {"hand": np.array([6., 15., -19.]), "lowerarm": np.array([-20., 10., -24.])}}
def frame(P):
    up = P["neck_01"]-P["spine_03"]; up/=np.linalg.norm(up)
    side = P["upperarm_l"]-P["upperarm_r"]; side-=(side@up)*up; side/=np.linalg.norm(side)
    fwd = np.cross(up, side); return lambda b: np.array([(P[b]-P["spine_05"])@side, (P[b]-P["spine_05"])@fwd, (P[b]-P["spine_05"])@up])
pose = copy.deepcopy(base)
for it in range(8):
    P,_ = po.solve(pose); c = frame(P)
    for s in "lr":
        a = pose[f"arm_{s}"]
        a["hand"] = list(np.array(a["hand"]) + 0.8*(GOAL[s]["hand"] - c(f"hand_{s}")))
        a["pole"] = list(np.array(a["pole"]) + 1.5*(GOAL[s]["lowerarm"] - c(f"lowerarm_{s}")))
P,_ = po.solve(pose); c = frame(P)
for s in "lr":
    print(s, "hand", np.round(pose[f"arm_{s}"]["hand"],1).tolist(), "pole", np.round(pose[f"arm_{s}"]["pole"],1).tolist(),
          "-> hand", np.round(c(f"hand_{s}"),1), "elb", np.round(c(f"lowerarm_{s}"),1))
