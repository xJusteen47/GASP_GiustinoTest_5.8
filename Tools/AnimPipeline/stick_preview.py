"""Offline look at a card's `author` key poses: no Unreal, no bridge.

    python stick_preview.py <card.json> [--plant] [--roll=arm_r@6:-90,arm_r@4:-90] [--out=<png>]

Solves every key pose with animpipe/author.py (needs sheets/raw/idle_neutral_legbones.json and the template frame-0
cache) and draws one grid: a column per key pose, rows = side (forward to the right), front (character's left on the
right) and top (forward up). Right arm red, left arm blue, right leg orange, left leg teal.

--plant   rewrites leg_l/leg_r of every pose for the feet in the card's `planted` list so ankle and foot orientation stay
          on the idle ones whatever the pelvis does (targets are in the pelvis frame, so they have to be recomputed).
--roll    writes `hand_rot` = <deg> about the solved forearm axis for <arm>@<frame>: a forearm roll (pronation), the one
          hand rotation that cannot be read off a stick figure. The printed palm normal tells which way the palm faces.
Both edit the card in place (numbers only): the card stays the source, this is not a per-move script.
Prints at most 5 lines; details go to <out>.txt.
"""
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from animpipe import author as A

SIDE = {"l": 1.0, "r": -1.0}


def rotvec(m):
    """Rotation matrix -> rotation vector in degrees."""
    a = math.acos(max(-1.0, min(1.0, (np.trace(m) - 1) / 2)))
    if a < 1e-6:
        return [0.0, 0.0, 0.0]
    ax = np.array([m[2, 1] - m[1, 2], m[0, 2] - m[2, 0], m[1, 0] - m[0, 1]]) / (2 * math.sin(a))
    return [round(float(x), 1) for x in np.degrees(a) * ax]


def palm(P, s):
    """Unit palm normal (out of the palm) of hand s; sign fixed on the idle, where palms face the thighs."""
    h = P[f"hand_{s}"]
    n = np.cross(P[f"index_01_{s}"] - h, P[f"pinky_01_{s}"] - h)
    return n / np.linalg.norm(n)


def plant(po, pose, sides):
    pv = pose.get("pelvis", {})
    D = A.rv(pv.get("rot", [0, 0, 0])); pp = po.P0["pelvis"] + np.array(pv.get("loc", [0, 0, 0]), float)
    for s in sides:
        toe = po.P0[f"ball_{s}"] - po.P0[f"foot_{s}"]; toe[2] = 0; toe /= np.linalg.norm(toe)
        leg = {"foot": (D.T @ (po.P0[f"foot_{s}"] - pp)).round(1).tolist(),
               "pole": (D.T @ (po.P0[f"calf_{s}"] + 45 * toe - pp)).round(1).tolist()}
        pose[f"leg_{s}"] = leg
        P, R = po.solve(pose)
        leg["foot_rot"] = rotvec(po.R0[f"foot_{s}"] @ R[f"foot_{s}"].T)


def main():
    path = sys.argv[1]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[2:] if a.startswith("--"))
    card = json.load(open(path)); au = card["author"]
    po = A.Poser(au.get("thumb", "along"))
    P0, _ = po.solve({})
    sign = {s: (1.0 if palm(P0, s)[0] * -SIDE[s] > 0 else -1.0) for s in "lr"}
    keys = sorted(au["poses"], key=int)
    dirty = False
    if "plant" in opt:
        sides = [s for s in "lr" if f"foot_{s}_ik_ctrl" in card.get("planted", [])]
        for k in keys:
            plant(po, au["poses"][k], sides)
        dirty = True
    for item in filter(None, opt.get("roll", "").split(",")):
        arm, rest = item.split("@"); k, deg = rest.split(":"); s = arm[-1]
        P, _ = po.solve(au["poses"][k])
        ax = P[f"hand_{s}"] - P[f"lowerarm_{s}"]; ax /= np.linalg.norm(ax)
        au["poses"][k][arm]["hand_rot"] = [round(float(x), 1) for x in ax * float(deg)]
        dirty = True
    if dirty:
        json.dump(card, open(path, "w"), indent=1)

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    chains = [("k", A.SPINE and ["pelvis"] + A.SPINE + A.NECK), ("k", ["clavicle_l", "spine_05", "clavicle_r"]),
              ("k", ["thigh_l", "pelvis", "thigh_r"])]
    for s, ca, cl in (("r", "tab:red", "tab:orange"), ("l", "tab:blue", "teal")):
        chains += [(ca, [f"clavicle_{s}", f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}", f"middle_01_{s}", f"middle_03_{s}"]),
                   (ca, [f"index_03_{s}", f"index_01_{s}", f"hand_{s}", f"pinky_01_{s}", f"pinky_03_{s}"]),
                   (cl, [f"thigh_{s}", f"calf_{s}", f"foot_{s}", f"ball_{s}"])]
    views = [("lato", lambda p: (p[1], p[2])), ("fronte", lambda p: (p[0], p[2])), ("alto", lambda p: (-p[0], p[1]))]
    fig, axs = plt.subplots(3, len(keys), figsize=(2.6 * len(keys), 8.4), squeeze=False)
    rep, worst = [], 0.0
    names = {str(p["frame"]): p["name"] for p in card.get("poses", [])}
    for c, k in enumerate(keys):
        P, R = po.solve(au["poses"][k])
        drift = max(float(np.linalg.norm(P[f"foot_{s}"] - P0[f"foot_{s}"])) for s in "lr")
        worst = max(worst, drift)
        pr = palm(P, "r") * sign["r"]; pl = palm(P, "l") * sign["l"]
        rep.append(f"f{k} {names.get(k, '')}: hand_r {P['hand_r'].round(1).tolist()} palm_r {pr.round(2).tolist()} "
                   f"hand_l {P['hand_l'].round(1).tolist()} palm_l {pl.round(2).tolist()} elbow_r {P['lowerarm_r'].round(1).tolist()} "
                   f"head_fwd {(R['head'] @ po.R0['head'].T @ np.array([0, 1, 0])).round(2).tolist()} foot_drift {drift:.1f}")
        for r, (vn, pj) in enumerate(views):
            ax = axs[r][c]
            for col, ch in chains:
                pts = [pj(P[b]) for b in ch if b in P]
                ax.plot([p[0] for p in pts], [p[1] for p in pts], color=col, lw=2.2 if col != "k" else 1.6)
            hx, hy = pj(P["head"] + R["head"] @ po.R0["head"].T @ np.array([0, 2, 9.0]))
            ax.add_patch(plt.Circle((hx, hy), 10, fill=False, color="k"))
            nx, ny = pj(P["head"] + R["head"] @ po.R0["head"].T @ np.array([0, 16, 8.0]))
            ax.plot([hx, nx], [hy, ny], color="k", lw=1)          # nose line: where the face points
            ax.set_aspect("equal"); ax.set_xticks([]); ax.set_yticks([])
            if r < 2:
                ax.set_xlim(-75, 85); ax.set_ylim(-2, 175); ax.axhline(0, color="gray", lw=0.6)
            else:
                ax.set_xlim(-80, 80); ax.set_ylim(-60, 90)
            if r == 0:
                ax.set_title(f"f{k} {names.get(k, '')}", fontsize=8)
            if c == 0:
                ax.set_ylabel(vn)
    out = opt.get("out") or os.path.join(HERE, "sheets", f"{card['move']}_stick.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.tight_layout(); fig.savefig(out, dpi=70)
    open(os.path.splitext(out)[0] + ".txt", "w").write("\n".join(rep))
    print(f"stick {card['move']}: {len(keys)} key poses, worst foot drift {worst:.1f} cm" + (" (card rewritten)" if dirty else ""))
    print(out)


if __name__ == "__main__":
    main()
