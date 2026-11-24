"""K8 offline pose numbers per key (cwd Tools/AnimPipeline): python cards/overlord/dropkick/k8/measure.py <card.json> [frames]
Per key, in the chest frame (x side, y forward, z up; fold.chest): elbows x vs shoulders x (1A/2A: elbows outside the torso),
head x/y vs the shoulder midpoint (hump centred), and in the component frame the angle between heel->ball and world forward
(+Y) and the sole normal vs forward (feet: flat soles toward the target = sole angle small)."""
import contextlib, io, json, os, sys
import numpy as np
sys.path.insert(0, os.getcwd()); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "k5"))
from animpipe import author as A
with contextlib.redirect_stdout(io.StringIO()):
    import fold as F


def numbers(po, pose):
    P, R = po.solve(pose); c = F.chest(P)
    sh = max(abs(c(P["upperarm_l"])[0]), abs(c(P["upperarm_r"])[0]))
    el, er = c(P["lowerarm_l"]), c(P["lowerarm_r"])
    hd = c(P["head"]); nk = c(P["neck_01"])
    fwd = np.array([0, 1.0, 0])
    toe = P["ball_l"] - P["foot_l"]; toe /= np.linalg.norm(toe)
    shin = P["foot_l"] - P["calf_l"]; shin /= np.linalg.norm(shin)
    sole_n = -np.cross(np.cross(shin, toe), toe); sole_n /= np.linalg.norm(sole_n)   # normal of the sole, pointing away from the shin side
    return dict(sh=sh, elbL=el[0], elbR=-er[0], out_l=el[0] - sh, out_r=-er[0] - sh, elb_z=(el[2] + er[2]) / 2,
                head_x=hd[0], head_up=hd[2] - nk[2], toe_fwd=float(np.degrees(np.arccos(np.clip(toe @ fwd, -1, 1)))),
                sole_fwd=float(np.degrees(np.arccos(np.clip(-sole_n @ fwd, -1, 1)))), shin_fwd=float(np.degrees(np.arccos(np.clip(shin @ fwd, -1, 1)))))


if __name__ == "__main__":
    card = json.load(open(sys.argv[1])); au = card["author"]; po = A.Poser(au.get("thumb", "along"))
    keys = sys.argv[2].split(",") if len(sys.argv) > 2 else sorted(au["poses"], key=int)
    for k in keys:
        n = numbers(po, au["poses"][k])
        print(f"k{k} " + " ".join(f"{a}={b:.1f}" for a, b in n.items()))
