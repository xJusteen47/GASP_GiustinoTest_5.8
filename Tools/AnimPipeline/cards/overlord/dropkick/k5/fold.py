"""Dropkick K5 pose round 2 (2026-11-21): refit the crossed arms as a Bison FOLD, with the build check's clearance inside the fit.
Review k5b (FAIL, posa): elbows at the sides and hands pushed out = a guard. Here the ELBOWS come forward to mid-chest height, the
forearms lie flat and level across the chest (left over right), each fist rests on the opposite upper arm, and no fist is ahead of
the forearms in a side view. Clearance is bought by moving the elbows forward, never by pushing the hands out.

Offline clearance = clearance.py's shapes (physbodies.json) on the Poser's component transforms, so the fit sees what the
build check will see. Usage (from Tools/AnimPipeline):
  python cards/overlord/dropkick/k5/fold.py fit <card.json> <key> [key ...]   -> prints the arm specs per key (max 5 lines)
  python cards/overlord/dropkick/k5/fold.py eval <card.json> <out.png> [keys] -> clearance + metrics per key, 3-view drawing
"""
import sys, os, json, math, copy
sys.path.insert(0, os.getcwd())
import numpy as np
from animpipe import author as A

HERE = os.path.dirname(os.path.abspath(__file__))
PR = json.load(open(os.path.join(os.getcwd(), "physbodies.json")))["prims"]
SH = {}
for p in PR:
    SH.setdefault(p["bone"], []).append(p)
PAIRS = []
for s, o in (("r", "l"), ("l", "r")):
    for a in (f"hand_{s}", f"lowerarm_{s}"):
        for b in ("head", "neck_01", "neck_02", "spine_05", "spine_04", "spine_03", f"clavicle_{o}", f"upperarm_{o}"):
            PAIRS.append((a, b))
PAIRS += [("hand_l", "hand_r"), ("lowerarm_l", "lowerarm_r"), ("hand_l", "lowerarm_r"), ("hand_r", "lowerarm_l")]


def shapes(P, R, bone):
    out = []
    q = A.m2q(R[bone])
    for s in SH.get(bone, []):
        M = R[bone] @ A.q2m(np.array(s["quat"]))
        c = P[bone] + R[bone] @ np.array(s["trans"])
        ax = [M[:, i] for i in range(3)]
        if "CAPSULE" in s["type"].upper():
            r = s["extent"][0]; h = max(s["extent"][2] - r, 0.0)
            out.append(("cap", c - ax[2] * h, c + ax[2] * h, r))
        else:
            e = np.array(s["extent"])
            pts = np.array([c + ax[0] * (i * e[0]) + ax[1] * (j * e[1]) + ax[2] * (k * e[2])
                            for i in np.linspace(-1, 1, 5) for j in np.linspace(-1, 1, 3) for k in np.linspace(-1, 1, 4)])
            out.append(("box", pts, c, ax, e))
    return out


def segpt(a, b, p):
    ab = b - a; t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
    return np.linalg.norm(p - (a + np.outer(t, ab) if p.ndim > 1 else a + ab * t), axis=-1)


def gap(X, Y):
    best = 1e9
    for a in X:
        for b in Y:
            if a[0] == "cap" and b[0] == "cap":
                pts = a[1] + np.outer(np.linspace(0, 1, 9), a[2] - a[1])
                g = segpt(b[1], b[2], pts).min() - a[3] - b[3]
            elif a[0] == "box" and b[0] == "cap":
                g = segpt(b[1], b[2], a[1]).min() - b[3]
            elif a[0] == "cap" and b[0] == "box":
                g = segpt(a[1], a[2], b[1]).min() - a[3]
            else:
                d = np.stack([(a[1] - b[2]) @ v for v in b[3]], 1); q = np.abs(d) - b[4]
                g = (np.linalg.norm(np.maximum(q, 0), axis=1) + np.minimum(q.max(1), 0)).min()
            best = min(best, g)
    return best


def chest(P):
    up = P["neck_01"] - P["spine_03"]; up /= np.linalg.norm(up)
    side = P["upperarm_l"] - P["upperarm_r"]; side -= (side @ up) * up; side /= np.linalg.norm(side)
    fwd = np.cross(up, side)
    return lambda v: np.array([(v - P["spine_05"]) @ side, (v - P["spine_05"]) @ fwd, (v - P["spine_05"]) @ up])


def metrics(po, pose):
    P, R = po.solve(pose); c = chest(P)
    W = {b: shapes(P, R, b) for b in {x for pr in PAIRS for x in pr}}
    cl = {f"{a}|{b}": gap(W[a], W[b]) for a, b in PAIRS}
    m = {"cl": cl, "P": {b: c(P[b]) for b in ("upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r", "spine_03")}}
    for s in "lr":
        fist = np.array([c(p) for p in W[f"hand_{s}"][0][1]])
        m[f"fist_{s}"] = fist          # hand box points, chest frame
    return m, P, R, W


TGT = {"l": {"elb_up": -13.0, "lvl": 0.0}, "r": {"elb_up": -18.0, "lvl": 0.0}}   # left forearm over the right one


def cost(m, tight=False, terms=None):
    cl = m["cl"]; P = m["P"]; T = {}
    add = lambda k, v: T.__setitem__(k, T.get(k, 0.0) + float(v))
    for k, v in cl.items():                                   # the build check: no penetration (margin 1 cm)
        if v < 1.0:
            add("clear", 40 * (1.0 - v) ** 2)
    for s, o in (("l", "r"), ("r", "l")):
        e, h = P[f"lowerarm_{s}"], P[f"hand_{s}"]
        add("elb_h", 1.0 * (e[2] - TGT[s]["elb_up"]) ** 2)               # elbows at mid-chest height
        add("level", 2.0 * (h[2] - e[2] - TGT[s]["lvl"]) ** 2)           # forearm level
        fa = min(cl[f"lowerarm_{s}|spine_04"], cl[f"lowerarm_{s}|spine_05"], cl[f"lowerarm_{s}|spine_03"])
        add("on_chest", 1.0 * max(fa - (1.5 if tight else 2.5), 0) ** 2)  # forearm lies on the chest (or on the other arm)
        add("tuck", 0.5 * max(cl[f"hand_{s}|upperarm_{o}"] - 3.0, 0) ** 2)  # fist by the opposite upper arm
        fist = m[f"fist_{s}"]
        add("fist_fwd", 3.0 * max(fist[:, 1].max() - e[1] + 1.0, 0) ** 2)   # side view: no fist ahead of the elbows
        add("fist_out", 1.0 * max(abs(fist[:, 0]).max() - 24.0, 0) ** 2)    # front view: fist not poking out past the arm
        add("fist_up", 1.0 * max(fist[:, 2].max() - 0.0, 0) ** 2)           # fists below the collarbone line (not a chin guard)
        add("compact", 0.05 * max(e[1] - 18, 0) ** 2)                       # keep the fold compact
        add("elb_in", 1.0 * max(abs(e[0]) - float(os.environ.get("FOLD_ELB_IN", 15)), 0) ** 2)                  # elbows in front of the body, not flared
    if terms is not None:
        terms.update({k: round(v, 1) for k, v in T.items()})
    return sum(T.values())

# Start of the fit (chest frame of the card: x = side +left, y = forward, z = up, from spine_05). The pole is set ON the wanted
# elbow, so the search moves elbows and wrists directly (a far pole only fixes a plane and drifts).
INIT = {"arm_l": {"hand": [-10.0, 24.0, -12.0], "pole": [12.0, 25.0, -13.0]},
        "arm_r": {"hand": [10.0, 18.0, -17.0], "pole": [-12.0, 21.0, -17.0]}}
CLAV0 = 12.0


def fit(po, pose, tight=False, iters=60, init=None):
    keys = [("arm_l", "hand"), ("arm_l", "pole"), ("arm_r", "hand"), ("arm_r", "pole")]
    src = init or pose
    # + the shoulders rounding forward (clavicle protraction about the chest's up axis, degrees): the upper arm is only 26.7 cm,
    # so elbows at mid-chest height cannot reach in front of the forearm's clearance line from a square shoulder.
    # The search runs in the ANATOMICAL chest frame (side, forward, up from the shoulders and neck); the card's arm frame is the
    # trunk-turned component frame, ~24 deg off in yaw on this rig: M maps one to the other.
    P0, _ = po.solve(pose); upw = P0["neck_01"] - P0["spine_03"]; upw /= np.linalg.norm(upw)
    sd = P0["upperarm_l"] - P0["upperarm_r"]; sd -= (sd @ upw) * upw; sd /= np.linalg.norm(sd)
    Dch = A.rv(pose.get("spine", [0, 0, 0])) @ A.rv(pose["pelvis"]["rot"])
    M = Dch.T @ np.column_stack([sd, np.cross(upw, sd), upw])
    x = np.concatenate([np.array(src[a][k], float) if init else M.T @ np.array(src[a][k], float) for a, k in keys] + [np.array([CLAV0, CLAV0, 30.0, 30.0])])
    if os.environ.get("FOLD_X0"):                 # start from a solved key (anatomical x), so every key holds the same fold
        x = np.array(json.load(open(os.environ["FOLD_X0"]))["x"], float)
    def setx(x):
        p = copy.deepcopy(pose)
        for i, (a, k) in enumerate(keys):
            p[a][k] = [round(float(v), 2) for v in M @ x[3 * i:3 * i + 3]]
        # fists bent back at the wrist around the opposite arm (about the chest up axis, degrees 0-90)
        p["arm_l"]["hand_rot"] = [round(float(v), 2) for v in upw * np.clip(x[14], -90, 90)]
        p["arm_r"]["hand_rot"] = [round(float(v), 2) for v in -upw * np.clip(x[15], -90, 90)]
        p["clavicle_l"] = [round(float(v), 2) for v in upw * np.clip(x[12], 0, 25)]
        p["clavicle_r"] = [round(float(v), 2) for v in -upw * np.clip(x[13], 0, 25)]
        return p
    f = lambda x: cost(metrics(po, setx(x))[0], tight) + 0.03 * (np.clip(x[12], 0, 25) ** 2 + np.clip(x[13], 0, 25) ** 2)
    rng = np.random.default_rng(7); x0 = x.copy(); gbest, gx = 1e18, x
    for start in range(int(os.environ.get("FOLD_STARTS", 1))):
      x = x0 + (0 if start == 0 else rng.normal(0, 1, len(x0)) * np.array([4.0] * 12 + [5, 5, 20, 20]))
      best = f(x); step = np.array([3.0, 3, 3, 3, 3, 3] * 2 + [4.0, 4.0, 10.0, 10.0])
      for _ in range(iters):
        improved = False
        for i in range(len(x)):
            for sg in (1, -1):
                y = x.copy(); y[i] += sg * step[i]; v = f(y)
                if v < best:
                    best, x, improved = v, y, True
                    break
        if not improved:
            step *= 0.5
            if step[0] < 0.25:
                break
      if best < gbest:
          gbest, gx = best, x
    x, best = gx, gbest
    fit.last_x = [round(float(v), 3) for v in x]
    tm = {}; cost(metrics(po, setx(x))[0], tight, tm); print("terms", tm, "clav/wrist deg", np.round(x[12:], 0).tolist())
    return setx(x), best


def draw(po, poses, keys, out):
    from PIL import Image, ImageDraw
    C = 260; G = Image.new("RGB", (C * len(keys), C * 3), "white"); d = ImageDraw.Draw(G)
    col = {"l": (40, 80, 230), "r": (220, 50, 40)}
    for i, k in enumerate(keys):
        m, P, R, W = metrics(po, poses[k]); c = chest(P)
        for row, (ax, sx, name) in enumerate((((1, 2), 1, "side"), ((0, 2), 1, "front"), ((0, 1), 1, "top"))):
            ox, oy = i * C + C // 2, row * C + C // 2 + 20
            pt = lambda v: (ox + sx * c(v)[ax[0]] * 2.2, oy - c(v)[ax[1]] * 2.2)
            for b in ("spine_03", "spine_04", "spine_05", "head", "neck_01", "upperarm_l", "upperarm_r", "lowerarm_l", "lowerarm_r", "clavicle_l", "clavicle_r"):
                for s in shapes(P, R, b):
                    cc = col.get(b[-1], (150, 150, 150)) if "arm" in b else (200, 200, 200)
                    for t in np.linspace(0, 1, 8):              # capsule silhouette = discs along the segment
                        x, y = pt(s[1] + (s[2] - s[1]) * t); r = s[3] * 2.2
                        d.ellipse([x - r, y - r, x + r, y + r], fill=cc, outline=(90, 90, 90))
            for s in "lr":
                for p in W[f"hand_{s}"][0][1]:
                    x, y = pt(p); d.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(0, 0, 0) if s == "l" else (120, 0, 0))
            d.text((i * C + 4, row * C + 4), f"key {k} {name}", fill="black")
    G.save(out)


if __name__ == "__main__":
    mode, path = sys.argv[1], sys.argv[2]
    card = json.load(open(path)); au = card["author"]; po = A.Poser(au.get("thumb", "along"))
    if mode == "fit":
        res = {}
        for k in sys.argv[3:]:
            p, J = fit(po, au["poses"][k], tight=k in ("7", "11", "13"), init=INIT, iters=int(os.environ.get("FOLD_ITERS", 60)))
            res[k] = {"arm_l": p["arm_l"], "arm_r": p["arm_r"], "clavicle_l": p["clavicle_l"], "clavicle_r": p["clavicle_r"], "J": round(float(J), 2), "x": fit.last_x}
        json.dump(res, open(os.path.join(HERE, "fold_fit.json"), "w"), indent=1)
        print("cost per key", {k: v["J"] for k, v in res.items()}, "-> k5/fold_fit.json")
        poses = copy.deepcopy(au["poses"])
        for k, v in res.items():
            poses[k].update({a: v[a] for a in ("arm_l", "arm_r", "clavicle_l", "clavicle_r")})
        out = os.path.join(os.environ.get("TEMP", "."), "k5r2", "fit.png"); os.makedirs(os.path.dirname(out), exist_ok=True)
        draw(po, poses, list(res), out)
        for k in res:
            m = metrics(po, poses[k])[0]; P = m["P"]
            print(f"k{k} elbL{np.round(P['lowerarm_l'],0).tolist()} hL{np.round(P['hand_l'],0).tolist()} elbR{np.round(P['lowerarm_r'],0).tolist()} "
                  f"hR{np.round(P['hand_r'],0).tolist()} min_cl {min(m['cl'].values()):.1f} fistfwd {m['fist_l'][:,1].max():.0f}/{m['fist_r'][:,1].max():.0f}")
    else:
        keys = sys.argv[4].split(",") if len(sys.argv) > 4 else sorted(au["poses"], key=int)
        rows = []
        for k in keys:
            m = metrics(po, au["poses"][k])[0]
            neg = {kk: round(v, 1) for kk, v in m["cl"].items() if v < 1.0}
            P = m["P"]
            rows.append(f"k{k} J{cost(m):.1f} elbL{np.round(P['lowerarm_l'],0).tolist()} hL{np.round(P['hand_l'],0).tolist()} "
                        f"elbR{np.round(P['lowerarm_r'],0).tolist()} hR{np.round(P['hand_r'],0).tolist()} <1cm {neg}")
        draw(po, au["poses"], keys, sys.argv[3])
        with open(sys.argv[3] + ".txt", "w") as f:
            f.write("\n".join(rows))
        for r in rows[:5]:
            print(r[:400])
