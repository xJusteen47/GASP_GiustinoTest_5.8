"""Copy the owner's Cascadeur key poses bone for bone into a take (no IK, no retarget loss), with a timing plan.

    python casc_fk_take.py <plan.json> [--out=<move>_bones.json]
        -> writes <move>_bones.json (ue_component, the format ue_write_take.py reads); prints 5 lines max

Why: the owner's Cascadeur rig IS the UE mannequin skeleton (same bone names; every local translation equal to the UE one
with Y negated). casc_to_author.py reduced each pose to a few IK targets and lost the pelvis shift (up to 33 cm), the hand
and palm rotations (47-103 deg), the clavicles, the foot yaw/roll and the head roll; the legs were then rebuilt by IK.
Here each bone keeps the owner's own rotation. Cascadeur's raw keys (owner_keys_*.json, `global_rotation` row-major 3x3,
`global_position` cm) obey G_child = G_parent @ L_child (column vectors). With P the swap of Y and Z (Cascadeur Y-up,
+Z forward, +X = character's left  ->  UE component +X left, +Y forward, +Z up) and S = diag(1,-1,1) (the mirrored local Y):
    p_ue = P p_casc + root_offset          G_ue = P G_casc S
which keeps every bone at its skeleton length (checked below against the key poses).

plan.json:
  {"move": "overlord_jab2_fk", "keys_file": "<raw owner keys json>", "frames": 53,
   "keys": [{"frame": 0, "key": "7"}, {"frame": 5, "key": "16", "ease": "out"}, ...],
   "root_forward": {"<frame>": cm, ...}   optional: root bone travel along +Y (root motion), linearly interpolated;
                                          the pelvis keeps its world path, so it is moved back by the same amount
   "foot_paths": [{"bone": "foot_l", "lift": 4, "keys": {"0": "pose", "9": "pose", "14": [x, y]}}]   optional: feet on the
                                          ground (no slide) and real steps; see foot_path(). Legs re-solved by two-bone IK
                                          in the owner's knee plane
   "keys[].shift": [x, y] cm   optional: moves the whole pose of that key (pelvis) in component space, e.g. the final idle
                                          of a move with root motion sits where the root ended
   "offsets": {"<frame>": {"<bone>": [rx, ry, rz] deg}}   optional: extra LOCAL rotation (follow-through, overshoot),
                                          rotation vector in the bone's UE local axes, interpolated like the keys}
Between keys every bone's LOCAL rotation is slerped (pelvis translation lerped) with the key's ease
("inout" smoothstep default, "in" = t^2, "out" = 1-(1-t)^2, "linear", "hold" = keep the previous pose).
"""
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TMPL = os.path.join(HERE, "sheets", "raw", "AS_Pilot_GanonFTilt_V2_frame0.json")
P = np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]], float)
S = np.diag([1.0, -1.0, 1.0])


def m2q(m):
    """3x3 -> (x, y, z, w)."""
    t = np.trace(m)
    if t > 0:
        s = math.sqrt(t + 1) * 2; w = s / 4
        x = (m[2, 1] - m[1, 2]) / s; y = (m[0, 2] - m[2, 0]) / s; z = (m[1, 0] - m[0, 1]) / s
    else:
        i = int(np.argmax(np.diag(m))); j, k = (i + 1) % 3, (i + 2) % 3
        s = math.sqrt(1 + m[i, i] - m[j, j] - m[k, k]) * 2
        q = [0.0, 0.0, 0.0]; q[i] = s / 4; q[j] = (m[j, i] + m[i, j]) / s; q[k] = (m[k, i] + m[i, k]) / s
        w = (m[k, j] - m[j, k]) / s; x, y, z = q
    q = np.array([x, y, z, w]); return q / np.linalg.norm(q)


def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def slerp(a, b, t):
    d = float(np.dot(a, b))
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        q = a + (b - a) * t; return q / np.linalg.norm(q)
    th = math.acos(d)
    return (math.sin((1 - t) * th) * a + math.sin(t * th) * b) / math.sin(th)


def rv(v):
    v = np.radians(np.asarray(v, float)); a = np.linalg.norm(v)
    if a < 1e-9:
        return np.eye(3)
    k = v / a; K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * K @ K


EASE = {"inout": lambda t: t * t * (3 - 2 * t), "in": lambda t: t * t, "out": lambda t: 1 - (1 - t) ** 2,
        "linear": lambda t: t, "hold": lambda t: 0.0}


def parents(frame):
    """Parent of each bone from the data: the bone whose G @ L and p + G @ l reproduce the child (no hard-coded tree)."""
    par = {}
    names = list(frame)
    for c in names:
        if c == "root":
            continue
        Gc = np.array(frame[c]["global_rotation"]).reshape(3, 3); Lc = np.array(frame[c]["local_rotation"]).reshape(3, 3)
        pc = np.array(frame[c]["global_position"]); lc = np.array(frame[c]["local_position"])
        best = None
        for p in names:
            if p == c:
                continue
            Gp = np.array(frame[p]["global_rotation"]).reshape(3, 3); pp = np.array(frame[p]["global_position"])
            e = np.abs(Gp @ Lc - Gc).max() + np.linalg.norm(pp + Gp @ lc - pc)
            if best is None or e < best[0]:
                best = (e, p)
        par[c] = best[1]
    return par


def _children(par, b):
    return [c for c, p in par.items() if p == b]


def ik_leg(Rc, Pc, foot, par, order, T, Rf_new):
    """Two-bone IK on thigh+calf so `foot` reaches T in the current knee plane; the foot takes rotation Rf_new and every
    bone under the thigh (twist bones included) follows, so nothing stays behind."""
    calf = par[foot]; thigh = par[calf]
    a, k = Pc[thigh], Pc[calf]
    l1 = np.linalg.norm(k - a); l2 = np.linalg.norm(Pc[foot] - k)
    pole = k + (k - (a + Pc[foot]) / 2)
    d = T - a; dist = min(np.linalg.norm(d), l1 + l2 - 1e-3); dn = d / np.linalg.norm(d)
    x = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist); h = math.sqrt(max(l1 * l1 - x * x, 0))
    pv = pole - a; pv = pv - dn * np.dot(pv, dn); pv = pv / (np.linalg.norm(pv) + 1e-9)
    K = a + dn * x + pv * h; T = a + dn * dist
    Q1 = _rot_between(k - a, K - a)
    Q2 = _rot_between(Q1 @ (Pc[foot] - k), T - K)
    D = Rf_new @ Rc[foot].T
    oR = {b: Rc[b] for b in order if _under(par, b, thigh)}; oP = {b: Pc[b] for b in oR}
    for b in oR:
        if _under(par, b, foot):
            Rc[b] = D @ oR[b]; Pc[b] = T + D @ (oP[b] - oP[foot])
        elif _under(par, b, calf):
            Rc[b] = Q2 @ Q1 @ oR[b]; Pc[b] = K + Q2 @ Q1 @ (oP[b] - k)
        else:
            Rc[b] = Q1 @ oR[b]; Pc[b] = a + Q1 @ (oP[b] - a)
    return float(np.linalg.norm(d)) - (l1 + l2)   # > 0: target out of reach by that much


def foot_path(frames_c, fp, par, order, nfr, ground_z):
    """Drive a foot along keyed ground positions. keys {"<frame>": "pose" | [x, y]}: "pose" = where the FK pose has the foot
    on that frame, [x, y] = that spot on the ground. Equal consecutive spots = planted (position AND rotation held); a move
    follows the FK arc of the poses (owner: arcs are FK), plus a blended correction to the spots and `lift` cm (default 0). Frames outside the first..last key keep the FK pose, blended over 2 frames."""
    foot = fp["bone"]; lift = float(fp.get("lift", 0))
    ks = sorted((int(f), v) for f, v in fp["keys"].items())
    spots = []
    for f, v in ks:
        p = frames_c[f][1][foot]
        spots.append((f, p.copy() if v == "pose" else np.array([v[0], v[1], ground_z]), frames_c[f][0][foot].copy(),
                      p.copy(), frames_c[f][0][foot].copy()))
    worst = 0.0
    for f in range(max(0, ks[0][0] - 2), min(nfr, ks[-1][0] + 3)):
        Rc, Pc = frames_c[f]
        if f < ks[0][0] or f > ks[-1][0]:
            e = spots[0] if f < ks[0][0] else spots[-1]
            w = 1 - abs(f - e[0]) / 3; T = Pc[foot] + (e[1] - Pc[foot]) * w; Rn = _slerp_m(Rc[foot], e[2], w)
        else:
            i = max(j for j in range(len(spots)) if spots[j][0] <= f)
            if i == len(spots) - 1:
                T, Rn = spots[i][1], spots[i][2]
            else:
                (fa, pa, ra, fpa, fra), (fb, pb, rb, fpb, frb) = spots[i], spots[i + 1]
                t = (f - fa) / (fb - fa); te = t * t * (3 - 2 * t)
                if np.linalg.norm(pb - pa) < 0.5:
                    T, Rn = pa, ra
                else:   # step in the air: the FK arc of the owner's poses (owner: arcs are FK), shifted by a blended
                        # correction so take-off and landing sit on the planted spots; extra lift only if asked
                    T = Pc[foot] + (pa - fpa) * (1 - te) + (pb - fpb) * te + np.array([0, 0, lift * math.sin(math.pi * t)])
                    Rn = _slerp_m(ra @ fra.T, rb @ frb.T, te) @ Rc[foot]
        worst = max(worst, ik_leg(Rc, Pc, foot, par, order, T, Rn))
    return worst


def _under(par, b, top):
    while b in par:
        if b == top:
            return True
        b = par[b]
    return b == top


def _rot_between(a, b):
    a = a / np.linalg.norm(a); b = b / np.linalg.norm(b); v = np.cross(a, b); s = np.linalg.norm(v); c = float(np.dot(a, b))
    if s < 1e-9:
        return np.eye(3)
    K = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + K + K @ K * ((1 - c) / s ** 2)


def _slerp_m(A, B, t):
    return q2m(slerp(m2q(A), m2q(B), t))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--") and "=" in a)
    plan = json.load(open(args[0]))
    raw = json.load(open(os.path.join(os.path.dirname(os.path.abspath(args[0])), plan["keys_file"])))["frames"]
    tm = json.load(open(TMPL)); bones = list(tm["local"])
    root_off = np.array(tm["local"]["root"][:3], float)
    k0 = raw[plan["keys"][0]["key"]]
    par = parents({b: k0[b] for b in bones})
    order = []
    def visit(b):
        if b in order: return
        if b in par: visit(par[b])
        order.append(b)
    for b in bones: visit(b)

    def key_local(k):
        """{bone: (local quat in UE axes, local translation in UE axes)} for one owner key."""
        f = raw[k]; out = {}
        for b in bones:
            if b == "root":
                continue
            G = P @ np.array(f[b]["global_rotation"]).reshape(3, 3) @ S
            if par.get(b) and par[b] != "root":
                Gp = P @ np.array(f[par[b]]["global_rotation"]).reshape(3, 3) @ S
                L = Gp.T @ G
                t = S @ np.array(f[b]["local_position"], float)
            else:   # pelvis: world placement relative to the root (root stays put unless root_forward moves it)
                L = G; t = P @ np.array(f[b]["global_position"], float)
            out[b] = (m2q(L), t)
        return out

    def key_shift(k):
        d = key_local(k["key"]); sh = k.get("shift")
        if sh:
            q, t = d["pelvis"]; d["pelvis"] = (q, t + np.array([sh[0], sh[1], 0.0]))
        return d

    keys = [(int(k["frame"]), key_shift(k), k.get("ease", "inout")) for k in plan["keys"]]
    offs = sorted((int(f), v) for f, v in plan.get("offsets", {}).items())
    rootf = sorted((int(f), float(v)) for f, v in plan.get("root_forward", {}).items())

    def interp_scalar(tab, f):
        if not tab: return 0.0
        if f <= tab[0][0]: return tab[0][1]
        if f >= tab[-1][0]: return tab[-1][1]
        for (fa, a), (fb, b) in zip(tab, tab[1:]):
            if fa <= f <= fb: return a + (b - a) * (f - fa) / (fb - fa)

    def offset_at(f, b):
        if not offs: return np.eye(3)
        vals = [(fa, np.array(v.get(b, [0, 0, 0]), float)) for fa, v in offs]
        if f <= vals[0][0]: return rv(vals[0][1])
        if f >= vals[-1][0]: return rv(vals[-1][1])
        for (fa, a), (fb, c) in zip(vals, vals[1:]):
            if fa <= f <= fb:
                t = EASE["inout"]((f - fa) / (fb - fa)); return rv(a + (c - a) * t)

    out = {}; frames_c = {}; nfr = int(plan["frames"]); worst = 0.0
    for f in range(nfr):
        if f <= keys[0][0]: A, B, t = keys[0][1], keys[0][1], 0.0
        elif f >= keys[-1][0]: A, B, t = keys[-1][1], keys[-1][1], 0.0
        else:
            i = next(i for i in range(len(keys) - 1) if keys[i][0] <= f < keys[i + 1][0])
            A, B = keys[i][1], keys[i + 1][1]
            t = EASE[keys[i + 1][2]]((f - keys[i][0]) / (keys[i + 1][0] - keys[i][0]))
        rf = interp_scalar(rootf, f)
        Rc, Pc = {}, {}
        rq = np.array(tm["local"]["root"][3:7], float)
        Rc["root"] = q2m(rq); Pc["root"] = root_off + np.array([0, rf, 0])
        for b in order:
            if b == "root": continue
            qa, ta = A[b]; qb, tb = B[b]
            L = q2m(slerp(qa, qb, t)) @ offset_at(f, b); tl = ta + (tb - ta) * t
            p = par.get(b, "root")
            if b == "pelvis" or p == "root":   # world path kept: pelvis relative to the moving root
                Rc[b] = L; Pc[b] = root_off + tl
            else:
                Rc[b] = Rc[p] @ L; Pc[b] = Pc[p] + Rc[p] @ tl
        frames_c[f] = (Rc, Pc)
    gz = float(min(frames_c[0][1]["foot_l"][2], frames_c[0][1]["foot_r"][2]))
    reach = 0.0
    for fp in plan.get("foot_paths", []):
        reach = max(reach, foot_path(frames_c, fp, par, order, nfr, gz))
    for f in range(nfr):
        Rc, Pc = frames_c[f]
        out[str(f)] = {b: [Pc[b].round(4).tolist(), m2q(Rc[b]).round(6).tolist()] for b in bones}
    # exactness check at the key frames: hands/feet/head against the owner's key, in UE component space
    for fk, _, _ in keys:
        kd = next(k for k in plan["keys"] if int(k["frame"]) == fk); kname = kd["key"]
        sh = np.array([*(kd.get("shift") or [0, 0]), 0.0])
        if offs: break
        for b in ("hand_r", "hand_l", "head"):
            exp = P @ np.array(raw[kname][b]["global_position"]) + root_off
            got = np.array(out[str(fk)][b][0]) - sh
            worst = max(worst, float(np.linalg.norm(exp - got)))
    dst = opt.get("out", os.path.join(HERE, f"{plan['move']}_bones.json"))
    json.dump({"source": os.path.abspath(args[0]), "space": "ue_component", "frames": out}, open(dst, "w"))
    print(f"{plan['move']}: {nfr} frames, {len(keys)} owner keys copied bone for bone -> {dst}")
    if plan.get("foot_paths"):
        print(f"foot paths: worst out-of-reach {max(reach, 0):.1f} cm (0 = every target reached)")
    print(f"key-pose error (hands, head vs owner, no offsets): {worst:.3f} cm" if not offs else "offsets on: key check skipped")


if __name__ == "__main__":
    main()
