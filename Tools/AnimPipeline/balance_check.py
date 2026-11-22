"""Balance (owner, 31/10 on Ganondorf v1: "completely unbalanced and weightless").
Per frame, from the UE bone samples (component space X left, Y forward, Z up):
  planted feet   a foot whose ankle and ball stay within PLANT_TOL cm of their height in frame 0
  support base   each planted foot as a rectangle heel..toe (heel 0.45 x ankle-ball behind the ankle, toe 0.35 beyond the
                 ball), +-FOOT_HALF_W wide; two planted feet = the convex hull of both
  centre of mass ground projection of the volume-weighted capsules (motion_check.com); distance outside the base, cm
  counterbalance kicks: from the anticipation key to contact the upper body's centre of mass (torso, head, arms) moves
                 back against the attack RELATIVE to the hips by >= counterbalance_min_cm, while the hips drive forward
Declared exceptions (move sheet "balance_exempt_frames"): lunges, falls, airborne frames."""
import math
import numpy as np
import legibility as L

PLANT_TOL = 2.5
FOOT_HALF_W = 4.5

def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def foot_quad(fr, s, toes_only=False):
    an = L.bone_tf(fr, f"foot_{s}")[0][:2]; ba = L.bone_tf(fr, f"ball_{s}")[0][:2]
    d = ba - an; L_ = np.linalg.norm(d); a = unit(d); n = np.array([-a[1], a[0]])
    heel = (ba - a * 0.2 * L_) if toes_only else (an - a * 0.45 * L_); toe = ba + a * 0.35 * L_
    return [heel + n * FOOT_HALF_W, heel - n * FOOT_HALF_W, toe - n * FOOT_HALF_W, toe + n * FOOT_HALF_W]

def hull(pts):
    pts = sorted(map(tuple, pts))
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    return [np.array(p) for p in lo[:-1] + up[:-1]]

def outside(poly, p):
    """Signed distance of p from a convex polygon (counter-clockwise): > 0 outside, < 0 inside, cm."""
    best_in = 1e9; out = 0.0
    n = len(poly)
    inside = True
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        e = b - a; nrm = np.array([e[1], -e[0]]) / max(np.linalg.norm(e), 1e-9)   # outward for CCW
        d = float(np.dot(p - a, nrm))
        if d > 0:
            inside = False
        best_in = min(best_in, -d)
    if inside:
        return -best_in
    # distance to the polygon boundary
    dmin = 1e9
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        t = float(np.clip(np.dot(p - a, b - a) / max(np.dot(b - a, b - a), 1e-9), 0, 1))
        dmin = min(dmin, float(np.linalg.norm(p - (a + (b - a) * t))))
    return dmin

def upper_com(fr):
    tot, acc = 0.0, np.zeros(3)
    for part, prims in L.part_prims(fr).items():
        if part.startswith(("thigh", "shin", "foot")):
            continue
        for pr in prims:
            if pr[0] == "cap":
                a, b, r = pr[1], pr[2], pr[3]
                v = math.pi * r * r * np.linalg.norm(b - a) + 4.0 / 3.0 * math.pi * r ** 3; c = (a + b) / 2
            else:
                c, e = pr[1], pr[3]; v = 8 * e[0] * e[1] * e[2]
            tot += v; acc += v * c
    return acc / tot

def rows(frames, com_fn):
    n = len(frames)
    f0 = frames["0"]
    h0 = {s: (L.bone_tf(f0, f"foot_{s}")[0][2], L.bone_tf(f0, f"ball_{s}")[0][2]) for s in "lr"}
    out = []
    for f in range(n):
        fr = frames[str(f)]
        # support: a foot whose ball is on the floor; flat if its ankle is down too, on its toes (ball area only) if not
        planted = [s for s in "lr" if L.bone_tf(fr, f"ball_{s}")[0][2] <= h0[s][1] + PLANT_TOL]
        flat = {s: L.bone_tf(fr, f"foot_{s}")[0][2] <= h0[s][0] + PLANT_TOL for s in planted}
        c = com_fn(fr)[:2]
        if not planted:
            out.append({"frame": f, "planted": [], "outside_cm": None}); continue
        poly = hull([p for s in planted for p in foot_quad(fr, s, toes_only=not flat[s])])
        out.append({"frame": f, "planted": planted, "on_toes": [s for s in planted if not flat[s]], "outside_cm": round(outside(poly, c), 1)})
    return out

def check(frames, sheet, TH, com_fn, attack_dir, anticipation_key):
    R = rows(frames, com_fn)
    exempt = set(sheet.get("balance_exempt_frames", []))
    flags = []
    bad = [r for r in R if r["outside_cm"] is not None and r["outside_cm"] > TH.get("balance_outside_max_cm", 2.0) and r["frame"] not in exempt]
    if bad:
        worst = max(bad, key=lambda r: r["outside_cm"])
        flags.append({"check": "off balance (centre of mass outside the feet)", "frames": [r["frame"] for r in bad],
                      "worst_cm": worst["outside_cm"], "worst_frame": worst["frame"], "on_one_foot": [r["frame"] for r in bad if len(r["planted"]) == 1]})
    # counterbalance: kicks only (the punch's own forward weight shift is judged by the weight rule)
    cb = None
    if str(sheet.get("strike_limb", "")).startswith("leg"):
        c0 = sheet["contact"][0]
        a = unit(np.array([attack_dir[0], attack_dir[1], 0.0]))
        # the upper body (torso, head, arms) moves back RELATIVE to the hips while they drive the kick out
        p0 = L.bone_tf(frames[str(anticipation_key)], "pelvis")[0]; p1 = L.bone_tf(frames[str(c0)], "pelvis")[0]
        u0 = upper_com(frames[str(anticipation_key)]) - p0; u1 = upper_com(frames[str(c0)]) - p1
        cb = round(float(np.dot(u1 - u0, a)), 1)
        if cb > -TH.get("counterbalance_min_cm", 2.0):
            flags.append({"check": "no counterbalance (upper body does not lean back against the kick)", "upper_body_vs_hips_cm_along_attack": cb})
    return R, cb, flags
