"""Offline prototype of the v6 timing on Unreal bone data of v5 (sheets/v5_timing.json). No Cascadeur needed.
Prints per-frame speeds (world and relative to the chest) for v5 and v6, deviations from the guard, and draws
sheets/v6_proto_speeds.png. Usage: python -I v6_proto.py [param overrides as json]"""
import sys, os, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v6_motion as vm

HERE = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(HERE, "sheets", "v5_timing.json")))
B2G = {"pelvis": "pelvis", "spine_03": "spine2", "spine_05": "chest", "neck_01": "neck", "head": "head",
       "upperarm_l": "sho_l", "lowerarm_l": "elbow_l", "hand_l": "hand_l",
       "upperarm_r": "sho_r", "lowerarm_r": "elbow_r", "hand_r": "hand_r"}
main = {g: np.array(d["pos"][b], dtype=float) for b, g in B2G.items()}
params = dict(vm.DEFAULT)
if len(sys.argv) > 1:
    for k, v in json.loads(sys.argv[1]).items():
        params[k] = tuple(v)
new, dbg = vm.build(main, params, up=np.array([0.0, 0.0, 1.0]))

def rel(tr, g, base="chest"):
    return {f: tr[g][f] - tr[base][f] for f in range(len(tr[g]))}

cols = ["pelvis", "chest", "head", "hand_l", "elbow_l", "hand_r", "elbow_r"]
def table(tr, title):
    print(title)
    print("  f " + " ".join(f"{c:>8s}" for c in cols) + "   hand_l/ch elbow_l/ch hand_r/ch elbow_r/ch")
    rl, el, rr, er = rel(tr, "hand_l"), rel(tr, "elbow_l"), rel(tr, "hand_r"), rel(tr, "elbow_r")
    for f in range(3, 38):
        w = [np.linalg.norm(tr[c][f] - tr[c][f - 1]) for c in cols]
        r = [np.linalg.norm(x[f] - x[f - 1]) for x in (rl, el, rr, er)]
        print(f"{f:3d} " + " ".join(f"{v:8.2f}" for v in w) + "   " + " ".join(f"{v:9.2f}" for v in r))
table(main, "v5")
table(new, "v6 prototype")
print("\ndeviation from guard (cm) at 17 / 20 / 25 / 30:")
for g in cols:
    print(f"  {g:8s}", " ".join(f"{np.linalg.norm(new[g][f] - main[g][0]):6.2f}" for f in (17, 20, 25, 30)))
print("hand_l position v6 at 8 vs v5 at 8:", np.round(new["hand_l"][8], 1), np.round(main["hand_l"][8], 1))
try:
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(2, 1, figsize=(11, 7), sharex=True)
    F = list(range(1, 38))
    for i, (tr, name) in enumerate(((main, "v5"), (new, "v6 proto"))):
        for c in cols:
            ax[i].plot(F, [np.linalg.norm(tr[c][f] - tr[c][f - 1]) for f in F], label=c)
        ax[i].set_title(name + " world speed (cm/frame)"); ax[i].set_ylim(0, 16); ax[i].grid(alpha=.3)
        ax[i].axvline(17, color="k", lw=.6); ax[i].axvline(3, color="k", lw=.6)
    ax[0].legend(ncol=6, fontsize=8)
    fig.tight_layout(); fig.savefig(os.path.join(HERE, "sheets", "v6_proto_speeds.png"), dpi=90)
except Exception as e:
    print("plot skipped:", e)
