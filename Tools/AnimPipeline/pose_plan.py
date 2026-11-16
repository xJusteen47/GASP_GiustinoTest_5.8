"""Compile a pose plan into a card with a dense `author` block, offline (no Unreal), and draw its key poses.

    python pose_plan.py <plan.json> [--out=<png>] [--frames=0,6,12] [--no-card]

Why: `author` poses give hands in the chest frame and feet in the pelvis frame and ease to a stop on every key. That is
fine for an attack from idle; it is unusable for a body that lies down, rolls or gets up, where hands and feet stay on the
floor while the trunk turns 90-270 degrees and the motion must flow through the keys. A plan is the same pose data plus:
  - world targets: arm_x {"hand_w", "pole_w"}, leg_x {"foot_w", "pole_w"} in component space (root on the floor, z up,
    y forward, x = character's left), converted to the chest/pelvis frames on every frame, so a planted limb stays planted;
  - pelvis "rot_e": {"yaw", "pitch", "roll", "twist"} in degrees instead of a rotation vector: R = Rz(yaw) Rx(pitch)
    Ry(roll) Rz(twist). pitch +90 = lying on the back (head toward -y), +180 = upside down, +270 = face down, 360 = upright;
  - spline timing: values pass through the keys without stopping and without overshooting them (monotone cubic);
    "hold": true on a key stops everything there (zero tangent);
  - a limb given as the string "idle_w" sits where the standing idle has it in the world (planted idle foot).
A limb goes through a segment in world space when both its keys give world targets, else in its body frame (it follows
the trunk). A limb left out of a key hangs as in the idle (its solved position becomes the target).

plan = {"card": {<card fields: move, class, anim, rig, fps, timing_csv, poses...>}, "card_path": "cards/<kit>/<move>.json",
        "take": "AS_Pilot_<Name>", "frames": N, "keys": {"<frame>": pose}, "floor": [bones checked against z = 0]}
Output: the card (author.poses for every frame), <out>.png (columns = keys, rows = side, behind, top) and a floor report:
per key the lowest bone surface (bone z minus its radius; < -2 cm is under the floor, > 6 cm on a lying key is floating).
Prints at most 5 lines.
"""
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from animpipe import author as A

LIMBS = {"arm_l": ("hand", "upperarm_l", "lowerarm_l", "hand_l", "hand_rot"), "arm_r": ("hand", "upperarm_r", "lowerarm_r", "hand_r", "hand_rot"),
         "leg_l": ("foot", "thigh_l", "calf_l", "foot_l", "foot_rot"), "leg_r": ("foot", "thigh_r", "calf_r", "foot_r", "foot_rot")}
VECS = ("spine", "head", "clavicle_l", "clavicle_r", "fingers", "wrist_out")
# rough half thickness of the body around each bone, cm (floor report only)
RADIUS = {"pelvis": 11, "spine_03": 11, "spine_05": 11, "head": 9, "hand_l": 3, "hand_r": 3, "lowerarm_l": 4, "lowerarm_r": 4,
          "calf_l": 5, "calf_r": 5, "foot_l": 6, "foot_r": 6, "ball_l": 3, "ball_r": 3}


def rot_e(e):
    def ax(v, d):
        return A.rv((np.array(v, float) * d).tolist())
    return ax([0, 0, 1], e.get("yaw", 0)) @ ax([1, 0, 0], e.get("pitch", 0)) @ ax([0, 1, 0], e.get("roll", 0)) @ ax([0, 0, 1], e.get("twist", 0))


def rotvec(m):
    q = A.m2q(m); w = max(-1.0, min(1.0, float(q[3]))); a = 2 * math.acos(w); s = math.sqrt(max(1 - w * w, 0))
    return [0.0, 0.0, 0.0] if s < 1e-8 else (np.array(q[:3], float) / s * math.degrees(a)).tolist()


def body(k):
    """Pose dict without limbs for the solver, from a key or an interpolated frame."""
    pv = k.get("pelvis", {})
    rot = rotvec(rot_e(pv["rot_e"])) if "rot_e" in pv else pv.get("rot", [0, 0, 0])
    p = {"pelvis": {"loc": pv.get("loc", [0, 0, 0]), "rot": rot}}
    for v in VECS:
        if k.get(v) is not None:
            p[v] = k[v]
    return p


def frames_of(poser, pose):
    """(P, Dpel, Dch) of the trunk for a limbless pose."""
    P, R = poser.solve(pose)
    Dpel = A.rv(pose["pelvis"]["rot"])
    return P, Dpel, A.rv(pose.get("spine", [0, 0, 0])) @ Dpel


def hermite(ts, vs, holds, t):
    """Value at t of a Catmull-Rom spline through (ts, vs); zero tangent at the ends and on held keys."""
    vs = np.array(vs, float)
    if t <= ts[0]:
        return vs[0]
    if t >= ts[-1]:
        return vs[-1]
    i = max(j for j in range(len(ts) - 1) if ts[j] <= t)

    def tan(j):
        if j == 0 or j == len(ts) - 1 or holds[j]:
            return np.zeros_like(vs[0])
        # monotone tangents (PCHIP): a value never overshoots its keys, so a limb that lands on the floor does not dip
        # under it on the next frames and a value that stops changing stops without a hand-set "hold"
        h0, h1 = ts[j] - ts[j - 1], ts[j + 1] - ts[j]
        d0 = (vs[j] - vs[j - 1]) / h0; d1 = (vs[j + 1] - vs[j]) / h1
        w1, w2 = 2 * h1 + h0, h1 + 2 * h0
        ok = d0 * d1 > 1e-12
        return np.where(ok, (w1 + w2) / (w1 / np.where(ok, d0, 1.0) + w2 / np.where(ok, d1, 1.0)), 0.0)
    h = ts[i + 1] - ts[i]; s = (t - ts[i]) / h
    return ((2 * s ** 3 - 3 * s ** 2 + 1) * vs[i] + (s ** 3 - 2 * s ** 2 + s) * h * tan(i)
            + (-2 * s ** 3 + 3 * s ** 2) * vs[i + 1] + (s ** 3 - s ** 2) * h * tan(i + 1))


def compile_plan(plan):
    poser = A.Poser(plan.get("thumb", "along"))
    ts = sorted(int(k) for k in plan["keys"])
    keys = [plan["keys"][str(t)] for t in ts]
    holds = [bool(k.get("hold")) for k in keys]
    # per key: both representations of every limb target
    K = []
    Pidle, _ = poser.solve({})
    for k in keys:
        b = body(k); P, Dpel, Dch = frames_of(poser, b)
        Pf, _ = poser.solve({**b, **{l: {kk: vv for kk, vv in k[l].items() if not kk.endswith("_w")} for l in LIMBS
                                     if isinstance(k.get(l), dict) and LIMBS[l][0] in k[l]}})
        row = {}
        for l, (tk, a, m, e, rk) in LIMBS.items():
            O, D = (P["spine_05"], Dch) if tk == "hand" else (P["pelvis"], Dpel)
            sp = k.get(l) or {}
            if sp == "idle_w":      # the limb's idle place in the world (a planted foot of the standing idle), whatever the trunk does
                row[l] = {"world": True, "Tw": Pidle[e], "Pw": Pidle[m], "Tl": D.T @ (Pidle[e] - O), "Pl": D.T @ (Pidle[m] - O), "rot": np.zeros(3)}
                continue
            world = (tk + "_w") in sp
            if world:
                Tw, Pw = np.array(sp[tk + "_w"], float), np.array(sp.get("pole_w", sp.get("pole", [0, 0, 0])), float)
                if "pole_w" not in sp:
                    Pw = O + D @ Pw
            elif tk in sp:
                Tw, Pw = O + D @ np.array(sp[tk], float), O + D @ np.array(sp["pole"], float)
            else:
                Tw, Pw = Pf[e], Pf[m]
            row[l] = {"world": world, "Tw": Tw, "Pw": Pw, "Tl": D.T @ (Tw - O), "Pl": D.T @ (Pw - O), "rot": np.array(sp.get(rk, [0, 0, 0]), float)}
        K.append((b, row))
    out = {}
    for f in range(int(plan["frames"])):
        def sp(get, dim=3):
            return hermite(ts, [get(i) for i in range(len(ts))], holds, f)
        pose = {"pelvis": {"loc": sp(lambda i: K[i][0]["pelvis"]["loc"]).tolist()}}
        if any("rot_e" in k.get("pelvis", {}) for k in keys):
            e = {n: float(sp(lambda i, n=n: [keys[i].get("pelvis", {}).get("rot_e", {}).get(n, 0)])[0]) for n in ("yaw", "pitch", "roll", "twist")}
            pose["pelvis"]["rot"] = rotvec(rot_e(e))
        else:
            pose["pelvis"]["rot"] = sp(lambda i: K[i][0]["pelvis"]["rot"]).tolist()
        for v in VECS:
            if any(v in K[i][0] for i in range(len(ts))):
                pose[v] = sp(lambda i, v=v: K[i][0].get(v, [0, 0, 0])).tolist()
        P, Dpel, Dch = frames_of(poser, pose)
        i = max([j for j in range(len(ts) - 1) if ts[j] <= f] or [0])
        for l, (tk, a, m, e, rk) in LIMBS.items():
            O, D = (P["spine_05"], Dch) if tk == "hand" else (P["pelvis"], Dpel)
            if K[i][1][l]["world"] and K[min(i + 1, len(ts) - 1)][1][l]["world"]:
                T = D.T @ (sp(lambda j: K[j][1][l]["Tw"]) - O); Po = D.T @ (sp(lambda j: K[j][1][l]["Pw"]) - O)
            else:
                T = sp(lambda j: K[j][1][l]["Tl"]); Po = sp(lambda j: K[j][1][l]["Pl"])
            pose[l] = {tk: np.round(T, 2).tolist(), "pole": np.round(Po, 2).tolist()}
            r = sp(lambda j: K[j][1][l]["rot"])
            if np.abs(r).max() > 1e-6:
                pose[l][rk] = np.round(r, 2).tolist()
        # flat feet: a leg solved to a floor target keeps the idle ankle angle to the SHIN, so a bent knee digs the toes in.
        # "flat": true on a leg key (always on for "idle_w") turns the foot back to its idle world orientation, blended.
        flat = {l: float(np.clip(sp(lambda j, l=l: [1.0 if (keys[j].get(l) == "idle_w" or (isinstance(keys[j].get(l), dict) and keys[j][l].get("flat"))) else 0.0])[0], 0, 1))
                for l in ("leg_l", "leg_r")}
        if any(flat.values()):
            Pq, Rq = poser.solve({**pose, **{l: {k2: v2 for k2, v2 in pose[l].items() if k2 != "foot_rot"} for l in flat}})
            for l, w in flat.items():
                if w > 0:
                    e = LIMBS[l][3]
                    need = A._slerp_m(np.eye(3), poser.R0[e] @ Rq[e].T, w)
                    pose[l]["foot_rot"] = np.round(rotvec(need), 2).tolist()
        for kk in list(pose):
            if kk in VECS or kk == "pelvis":
                pose[kk] = json.loads(json.dumps(pose[kk]), parse_float=lambda x: round(float(x), 3))
        out[str(f)] = pose
    return poser, ts, out


def draw(poser, dense, cols, path, title):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    chains = [(["pelvis", "spine_01", "spine_03", "spine_05", "neck_01", "head"], "k", 5),
              (["clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "middle_01_r"], "tab:red", 3), (["clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "middle_01_l"], "tab:blue", 3),
              (["thigh_r", "calf_r", "foot_r", "ball_r"], "tab:orange", 4), (["thigh_l", "calf_l", "foot_l", "ball_l"], "teal", 4),
              (["upperarm_r", "upperarm_l"], "gray", 2), (["thigh_r", "thigh_l"], "gray", 2)]
    views = [("side (forward ->)", lambda p: (p[1], p[2])), ("behind (game camera)", lambda p: (-p[0], p[2])), ("top (forward up)", lambda p: (-p[0], p[1]))]
    fig, axs = plt.subplots(3, len(cols), figsize=(2.6 * len(cols), 8.2), squeeze=False)
    for c, f in enumerate(cols):
        P, R = poser.solve(dense[str(f)])
        hc = P["head"] + R["head"] @ np.array([8.0, 0, 0]) if False else P["head"] + (P["head"] - P["neck_01"]) / np.linalg.norm(P["head"] - P["neck_01"]) * 9
        for r, (name, pr) in enumerate(views):
            ax = axs[r][c]
            # draw far limbs first so the near side (right for the side view from the character's right) is on top
            for ch, col, lw in chains:
                xy = [pr(P[b]) for b in ch if b in P]
                ax.plot([p[0] for p in xy], [p[1] for p in xy], color=col, lw=lw, solid_capstyle="round")
            ax.add_patch(plt.Circle(pr(hc), 9, fill=False, color="k", lw=2))
            if r < 2:
                ax.axhline(0, color="saddlebrown", lw=1)
                ax.set_ylim(-10, 200)
            else:
                ax.set_ylim(-130, 130)
            ax.set_xlim(-130, 130); ax.set_aspect("equal"); ax.set_xticks([]); ax.set_yticks([])
            if r == 0:
                ax.set_title(f"f{f}", fontsize=11)
            if c == 0:
                ax.set_ylabel(name, fontsize=8)
    fig.suptitle(title, fontsize=10)
    fig.tight_layout(); fig.savefig(path, dpi=70); plt.close(fig)


def main():
    path = sys.argv[1]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[2:] if a.startswith("--"))
    plan = json.load(open(path, encoding="utf-8"))
    poser, ts, dense = compile_plan(plan)
    card = dict(plan["card"]); card["author"] = {"take": plan["take"], "frames": int(plan["frames"]), "thumb": plan.get("thumb", "along"), "poses": dense}
    card["_plan"] = os.path.relpath(os.path.abspath(path), HERE).replace("\\", "/")
    n = int(plan["frames"])
    # the build keys the rig only on the card's poses and re-spaces between them: a flowing clip keys every frame
    card.setdefault("poses", [{"name": f"f{f}", "frame": f, "source": None} for f in (range(n) if len(ts) > 2 else (0, n - 1))])
    cp = os.path.join(HERE, plan["card_path"])
    if "no-card" not in opt:
        os.makedirs(os.path.dirname(cp), exist_ok=True)
        json.dump(card, open(cp, "w", encoding="utf-8"), indent=1)
        # a reaction has no hitbox: the windows still come from a CSV (card.windows), so the last frame is the "active" one
        from animpipe import config as _cfg
        rows = "frame,extension_pct,reach_px,active\n" + "".join(f"{i + 1},0.0,0.0,{i == n - 1}\n" for i in range(n))
        for d in (os.path.join(_cfg.DATA, os.path.dirname(card["timing_csv"])), os.path.join(os.path.dirname(cp), "timing")):
            os.makedirs(d, exist_ok=True)
            open(os.path.join(d, os.path.basename(card["timing_csv"])), "w").write(rows)
    cols = [int(x) for x in opt["frames"].split(",")] if opt.get("frames") else ts
    out = opt.get("out") or os.path.splitext(path)[0] + ".png"
    draw(poser, dense, cols, out, f"{card['move']}  ({plan['frames']} frames, keys {ts})")
    low = []
    for f in range(int(plan["frames"])):
        P, _ = poser.solve(dense[str(f)])
        b, z = min(((b, float(P[b][2]) - r) for b, r in RADIUS.items()), key=lambda x: x[1])
        low.append((f, b, round(z, 1)))
    worst = min(low, key=lambda x: x[2]); top = max(low, key=lambda x: x[2])
    print(f"plan {card['move']}: {plan['frames']} frames, keys {ts} -> {plan['card_path']}")
    print("floor at keys: " + ", ".join(f"f{f} {b} {z}" for f, b, z in low if f in ts))
    print(f"lowest f{worst[0]} {worst[1]} {worst[2]} cm, highest lowest-point f{top[0]} {top[1]} {top[2]} cm")
    print("preview:", out)


if __name__ == "__main__":
    main()
