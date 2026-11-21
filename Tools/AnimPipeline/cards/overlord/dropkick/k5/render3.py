"""Dropkick K5 round 3: offline silhouettes of whole poses in the COMPONENT frame (the camera views of the capture, roughly), from
the build check's collision shapes. Side (looking along X, forward to the right) and two quarter views (+-45 deg about Z).
Shows the arm block against the head (review acceptance: below the top of the head on the side view, head visible on the quarter).
  python cards/overlord/dropkick/k5/render3.py <card.json> <out.png> <key,key,...> [poses_override.json]
Prints, per key, the arm block top and the head top above the pelvis line in the side view (cm). Max 5 lines.
"""
import copy, json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fold as F                      # noqa: E402
from animpipe import author as A      # noqa: E402

ARMS = ("upperarm", "lowerarm", "hand")
COL = {"l": (40, 80, 230), "r": (220, 50, 40)}


def views():
    out = [("side az90", np.eye(3))]
    for name, deg in (("quarter +45", 45), ("quarter -45", -45)):
        a = math.radians(deg); c, s = math.cos(a), math.sin(a)
        out.append((name, np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])))
    return out


def render(po, poses, keys, out):
    from PIL import Image, ImageDraw
    C = 300; V = views()
    G = Image.new("RGB", (C * len(keys), C * len(V)), "white"); d = ImageDraw.Draw(G)
    rows = []
    for i, k in enumerate(keys):
        P, R = po.solve(poses[k])
        bones = sorted({p["bone"] for p in F.PR})
        sh = [(b, s) for b in bones if b in R for s in F.shapes(P, R, b)]
        ctr = P["spine_05"]
        for row, (name, Mv) in enumerate(V):
            # view: horizontal = rotated Y (forward), vertical = Z, depth = rotated X
            proj = lambda v: Mv.T @ (v - ctr)
            ox, oy = i * C + C // 2, row * C + C // 2
            items = []
            for b, s in sh:
                pts = [s[1] + (s[2] - s[1]) * t for t in np.linspace(0, 1, 8)] if s[0] == "cap" else list(s[1])
                r = s[3] if s[0] == "cap" else 1.2
                side = b[-1] if any(b.startswith(a) for a in ARMS) else None
                col = COL[side] if side else ((235, 200, 120) if b == "head" else (190, 190, 190))
                for p in pts:
                    q = proj(p); items.append((q[0], q, r, col))
            face = R["head"] @ po.R0["head"].T @ np.array([0.0, 1.0, 0.0])      # idle face = +Y: the nose marker
            nose = P["head"] + np.array([0, 0, 0.0]) + (R["head"] @ po.R0["head"].T @ np.array([0, 0, 9.0])) + face * 11
            items.append((proj(nose)[0] - 0.01, proj(nose), 2.5, (0, 0, 0)))
            for _, q, r, col in sorted(items, key=lambda t: t[0]):
                x, y = ox + q[1] * 1.3, oy - q[2] * 1.3; rr = r * 1.3
                d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=col, outline=(80, 80, 80))
            d.text((i * C + 4, row * C + 4), f"key {k} {name}", fill="black")
        if True:
            top = lambda b: max(p[2] + (s[3] if s[0] == "cap" else 0) for bb, s in sh if bb == b
                                for p in ([s[1], s[2]] if s[0] == "cap" else list(s[1])))
            arm = max(top(f"{a}_{s}") for a in ARMS for s in "lr")
            face = R["head"] @ po.R0["head"].T @ np.array([0.0, 1.0, 0.0])
            rows.append(f"k{k} face elev {math.degrees(math.asin(face[2])):.0f} deg toward +X {face[0]:.2f} | side view: arm block top {arm - P['pelvis'][2]:.1f} cm, head top {top('head') - P['pelvis'][2]:.1f} cm (above the pelvis)")
    G.save(out)
    return rows


if __name__ == "__main__":
    card = json.load(open(sys.argv[1])); au = card["author"]; po = A.Poser(au.get("thumb", "along"))
    poses = copy.deepcopy(au["poses"])
    if len(sys.argv) > 4:
        for k, v in json.load(open(sys.argv[4])).items():
            poses[k].update({a: v[a] for a in v if a in ("arm_l", "arm_r", "clavicle_l", "clavicle_r", "head", "neck_chain")})
    for r in render(po, poses, sys.argv[3].split(","), sys.argv[2])[:5]:
        print(r)
