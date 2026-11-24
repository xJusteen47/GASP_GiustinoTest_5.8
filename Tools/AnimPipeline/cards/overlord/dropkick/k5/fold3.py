"""Dropkick K5 pose round 3 (2026-11-21): the LYING fold (startup keys 14-15, loop 0/6/11).
Review k5r2b (FAIL, posa): with the torso laid back 82 deg, round 2's elbows-forward fold built a tent (top elbow ~20-25 cm above
the chest, higher than the head) = a raised guard. Target of round 3 (review "Round 3" 1-3): upper arms down along the ribs,
elbows OUT to the sides (|side| 18-22 cm) and only slightly forward, forearms resting ON the chest stacked left over right, hands
on the opposite arm, the whole arm block within ~11 cm of the sternum. Clearance is bought sideways (wider elbows), never by
lifting anything off the chest: when the check cannot pass with the forearms touching, the fit reports the margin instead.

Same machinery as fold.py (build-check shapes, anatomical chest frame); only the cost and the start change.
  python cards/overlord/dropkick/k5/fold3.py fit <card.json> <key> [key ...]   -> k5/fold3_fit.json + %TEMP%/k5r3/fit3.png
  python cards/overlord/dropkick/k5/fold3.py eval <card.json> <out.png> [keys]  -> block height, elbows, clearance per key
"""
import copy, json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fold as F                      # noqa: E402  (its module-level setup needs cwd = Tools/AnimPipeline)
from animpipe import author as A      # noqa: E402

ARM = ("upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r")
BLOCK = float(os.environ.get("FOLD3_BLOCK", 11.0))     # arm block <= sternum + BLOCK (cm, forward of the chest, shape surface)
_metrics0 = F.metrics


def _fwd_extent(c, shapes):
    best = -1e9
    for s in shapes:
        if s[0] == "cap":
            best = max(best, c(s[1])[1] + s[3], c(s[2])[1] + s[3])
        else:
            best = max(best, max(c(p)[1] for p in s[1]))
    return best


def metrics(po, pose):
    m, P, R, W = _metrics0(po, pose)
    c = F.chest(P)
    m["st"] = max(_fwd_extent(c, F.shapes(P, R, b)) for b in ("spine_04", "spine_05"))       # sternum (chest surface)
    m["blk"] = max(_fwd_extent(c, W[b] if b in W else F.shapes(P, R, b)) for b in ARM)        # top of the arm block
    for s in "lr":
        m[f"mid_{s}"] = c((P[f"lowerarm_{s}"] + P[f"hand_{s}"]) / 2)
    return m, P, R, W


def cost(m, tight=False, terms=None):
    cl = m["cl"]; P = m["P"]; T = {}
    add = lambda k, v: T.__setitem__(k, T.get(k, 0.0) + float(v))
    for k, v in cl.items():                                                # the build check (margin 1 cm)
        if v < 1.0:
            add("clear", 40 * (1.0 - v) ** 2)
    add("block", 6.0 * max(m["blk"] - (m["st"] + BLOCK), 0) ** 2)         # nothing stands up off the chest
    for s, o in (("l", "r"), ("r", "l")):
        e, h = P[f"lowerarm_{s}"], P[f"hand_{s}"]
        add("elb_side", 2.0 * (max(abs(e[0]) - 22, 0) ** 2 + max(18 - abs(e[0]), 0) ** 2))   # elbows out at the sides
        add("elb_fwd", 2.0 * max(e[1] - 16, 0) ** 2)                                         # only slightly forward
        add("elb_low", 0.5 * max(e[2] + 14, 0) ** 2)                                         # upper arm down along the ribs
        fa = min(cl[f"lowerarm_{s}|spine_04"], cl[f"lowerarm_{s}|spine_05"], cl[f"lowerarm_{s}|spine_03"])
        add("on_chest", 2.0 * max(fa - 2.0, 0) ** 2)                                          # each forearm ON the chest
        add("tuck", 0.3 * max(min(cl[f"hand_{s}|upperarm_{o}"], cl[f"hand_{s}|lowerarm_{o}"]) - 2.0, 0) ** 2)  # hand on the other arm
        add("level", 0.5 * max(abs(h[2] - e[2]) - 8, 0) ** 2)                                # forearm roughly across, not up
        fist = m[f"fist_{s}"]
        add("fist_up", 1.0 * max(fist[:, 2].max() + 2.0, 0) ** 2)                            # fists below the collarbones
        add("fist_out", 1.0 * max(abs(fist[:, 0]).max() - 24.0, 0) ** 2)
    add("stack", 1.0 * max(8.0 - (m["mid_l"][2] - m["mid_r"][2]), 0) ** 2)                   # left forearm above the right one
    if terms is not None:
        terms.update({k: round(v, 1) for k, v in T.items()})
    return sum(T.values())


F.metrics, F.cost = metrics, cost
F.CLAV0 = 0.0
INIT = {"arm_l": {"hand": [-7.0, 20.0, -11.0], "pole": [20.0, 12.0, -19.0]},
        "arm_r": {"hand": [7.0, 19.0, -22.0], "pole": [-20.0, 12.0, -25.0]}}


def report(po, pose, k):
    m = metrics(po, pose)[0]; P = m["P"]
    return (f"k{k} blk-st {m['blk'] - m['st']:.1f} elbL{np.round(P['lowerarm_l'], 0).tolist()} elbR{np.round(P['lowerarm_r'], 0).tolist()} "
            f"hL{np.round(P['hand_l'], 0).tolist()} hR{np.round(P['hand_r'], 0).tolist()} min_cl {min(m['cl'].values()):.2f}")


if __name__ == "__main__":
    mode, path = sys.argv[1], sys.argv[2]
    card = json.load(open(path)); au = card["author"]; po = A.Poser(au.get("thumb", "along"))
    outdir = os.path.join(os.environ.get("TEMP", "."), "k5r3"); os.makedirs(outdir, exist_ok=True)
    if mode == "fit":
        res = {}
        for k in sys.argv[3:]:
            p, J = F.fit(po, au["poses"][k], init=INIT, iters=int(os.environ.get("FOLD_ITERS", 80)))
            res[k] = {"arm_l": p["arm_l"], "arm_r": p["arm_r"], "clavicle_l": p["clavicle_l"], "clavicle_r": p["clavicle_r"],
                      "J": round(float(J), 2), "x": F.fit.last_x}
        json.dump(res, open(os.path.join(HERE, "fold3_fit.json"), "w"), indent=1)
        poses = copy.deepcopy(au["poses"])
        for k, v in res.items():
            poses[k].update({a: v[a] for a in ("arm_l", "arm_r", "clavicle_l", "clavicle_r")})
        F.draw(po, poses, list(res), os.path.join(outdir, "fit3.png"))
        for k in list(res)[:4]:
            print(report(po, poses[k], k))
    else:
        keys = sys.argv[4].split(",") if len(sys.argv) > 4 else sorted(au["poses"], key=int)
        rows = [report(po, au["poses"][k], k) for k in keys]
        F.draw(po, au["poses"], keys, sys.argv[3])
        open(sys.argv[3] + ".txt", "w").write("\n".join(rows))
        for r in rows[:5]:
            print(r)


def fit_path(card_path, key, prev, nxt, ts=(0.2, 0.4, 0.6, 0.8)):
    """Key 14 sits between the upright fold (13) and the lying fold (15) with one frame on each side, and the build's retime samples
    the blends: the first build (r3) failed clearance at f14 (-0.34 cm, lowerarm_r|spine_03) because the right forearm swept through
    the belly on the way from forward-in to the side. Here every clearance pair is the minimum over the key AND its blends with the
    neighbours (author._lerp, same as the build), so the fit picks a key 14 whose whole path stays clear."""
    card = json.load(open(card_path)); au = card["author"]; po = A.Poser(au.get("thumb", "along"))
    pa, pb = au["poses"][prev], au["poses"][nxt]
    def metrics_path(po_, pose):
        m, P, R, W = metrics(po_, pose)
        for nb in (pa, pb):
            for t in ts:
                cl = _metrics0(po_, A._lerp(nb, pose, t))[0]["cl"]
                m["cl"] = {k: min(v, cl[k]) for k, v in m["cl"].items()}
        return m, P, R, W
    F.metrics = metrics_path
    try:
        p, J = F.fit(po, au["poses"][key], init=INIT, iters=int(os.environ.get("FOLD_ITERS", 80)))
    finally:
        F.metrics = metrics
    m = metrics_path(po, p)[0]
    return p, J, m
