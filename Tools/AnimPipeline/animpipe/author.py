"""Author a source take from pose data when no clip exists yet (first version of a new move, no owner Step poses).

    python -m animpipe author <card.json> [--no-write]

The card's `author` block describes each key pose as a few joint targets; this module turns them into component-space
bones per frame (eased between the poses) and hands them to ue_write_take.py, which writes the take the build samples
(`source_anim`). Everything is data in the card: no per-move script (brief §2.7).

author = {"take": "AS_Pilot_<Name>", "frames": N (clip length, frames 0..N-1), "poses": {"<frame>": pose}}
pose (all optional; missing keys = idle):
  pelvis   {"loc": [x, y, z] cm, "rot": [rx, ry, rz] deg}   world offset and rotation of the pelvis
  spine    [rx, ry, rz]   total rotation of the trunk above the pelvis, spread over spine_01..05
  head     [rx, ry, rz]   extra rotation of neck_01, neck_02, head (spread)
  clavicle_l / clavicle_r  [rx, ry, rz]   shrug / protraction
  arm_l / arm_r  {"hand": [x, y, z], "pole": [x, y, z], "hand_rot": [rx, ry, rz]}  hand target and elbow pole, in the
           CHEST frame (offset from spine_05, axes turned with the trunk); the hand keeps its idle angle to the forearm
  leg_l / leg_r  {"foot": [x, y, z], "pole": [x, y, z], "foot_rot": [rx, ry, rz]}  ankle target and knee pole, in the
           PELVIS frame (offset from the pelvis bone, axes turned with the pelvis)
Axes are Unreal component space: X = character's left, Y = forward, Z = up. A rotation [rx, ry, rz] is a rotation vector
in degrees about those axes (+rx tips up -> back, i.e. a forward lean is negative; it swings a hanging limb forward).
  wrist_out [deg, 0, 0]   bends both hands at the wrist away from the forearm line, toward the outside of the body
           (owner: the villain hand reads only when it breaks the arm line); blended per frame
  fingers  [open, 0, 0]   0 = the template's fists (default), 1 = the overlord "villain hand": open and flat, fingers
           straight and pressed together (joints 01-03 straightened, metacarpals as the template); blended per frame.
Fingers keep the template's fists unless `fingers` opens them. Limbs are two-bone IK in the plane of the pole; each bone's roll comes from mapping its
idle (aim, hinge) frame onto the new one, so elbows and knees keep their hinge.
"""
import json, math, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, TOOLS)
WORK = TOOLS   # ue_write_take.py and its caches (sheets/raw) live next to the tools
IDLE = os.path.join(WORK, "sheets", "raw", "idle_neutral_legbones.json")
TMPL = os.path.join(WORK, "sheets", "raw", "AS_Pilot_GanonFTilt_V2_frame0.json")

PARENT = {"pelvis": None, "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03",
          "spine_05": "spine_04", "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02"}
for s in "lr":
    PARENT.update({f"clavicle_{s}": "spine_05", f"upperarm_{s}": f"clavicle_{s}", f"lowerarm_{s}": f"upperarm_{s}",
                   f"hand_{s}": f"lowerarm_{s}", f"upperarm_twist_01_{s}": f"upperarm_{s}", f"upperarm_twist_02_{s}": f"upperarm_{s}",
                   f"lowerarm_twist_01_{s}": f"lowerarm_{s}", f"lowerarm_twist_02_{s}": f"lowerarm_{s}",
                   f"thigh_{s}": "pelvis", f"calf_{s}": f"thigh_{s}", f"foot_{s}": f"calf_{s}", f"ball_{s}": f"foot_{s}"})
    for fg in ("index", "middle", "ring", "pinky"):
        PARENT[f"{fg}_metacarpal_{s}"] = f"hand_{s}"
        PARENT[f"{fg}_01_{s}"] = f"{fg}_metacarpal_{s}"
        PARENT[f"{fg}_02_{s}"] = f"{fg}_01_{s}"
        PARENT[f"{fg}_03_{s}"] = f"{fg}_02_{s}"
SPINE = ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05"]
NECK = ["neck_01", "neck_02", "head"]


# --- quaternions (x, y, z, w) and rotation matrices ---
def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def m2q(m):
    t = np.trace(m)
    if t > 0:
        s = math.sqrt(t + 1) * 2
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    else:
        i = int(np.argmax(np.diag(m))); j, k = (i + 1) % 3, (i + 2) % 3
        s = math.sqrt(1 + m[i, i] - m[j, j] - m[k, k]) * 2
        q = [0.0] * 4
        q[i] = 0.25 * s; q[j] = (m[j, i] + m[i, j]) / s; q[k] = (m[k, i] + m[i, k]) / s; q[3] = (m[k, j] - m[j, k]) / s
    q = np.array(q); return q / np.linalg.norm(q)


def rv(v):
    """Rotation matrix of a rotation vector in degrees."""
    v = np.radians(np.asarray(v, float)); a = np.linalg.norm(v)
    if a < 1e-9:
        return np.eye(3)
    k = v / a; K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * K @ K


def _rot_to_quat(p, y, r):
    import legibility as L
    return L.rot_to_quat(p, y, r)


def base():
    """Idle component transforms (rotation matrices) and the local frames FK uses: rotations from the idle, translations
    from the template (the skeleton's lengths, as ue_write_take keeps them); fingers' rotations from the template (fists)."""
    idle = json.load(open(IDLE))["frames"]["0"]
    tm = json.load(open(TMPL))
    P = {b: np.array(v[:3]) for b, v in idle.items()}
    R = {b: q2m(_rot_to_quat(*v[3:6])) for b, v in idle.items()}
    loc_t, loc_r = {}, {}
    for b, p in PARENT.items():
        if p is None:
            continue
        loc_t[b] = np.array(tm["local"][b][:3])
        if b.split("_")[0] in ("index", "middle", "ring", "pinky"):
            loc_r[b] = q2m(np.array(tm["local"][b][3:7]))
        else:
            loc_r[b] = R[p].T @ R[b]
    return P, R, loc_t, loc_r


def _slerp_m(a, b, t):
    qa, qb = m2q(a), m2q(b)
    if np.dot(qa, qb) < 0:
        qb = -qb
    q = qa * (1 - t) + qb * t
    return q2m(q / np.linalg.norm(q))


def _frame(aim, hinge):
    a = aim / np.linalg.norm(aim); h = hinge - a * np.dot(hinge, a); h /= np.linalg.norm(h)
    return np.column_stack([a, h, np.cross(a, h)])


def two_bone(S, T, pole, l1, l2):
    d = T - S; dist = np.linalg.norm(d); u = d / dist
    dist = min(dist, 0.995 * (l1 + l2)); T = S + u * dist
    w = (pole - S) - u * np.dot(pole - S, u); w /= max(np.linalg.norm(w), 1e-9)
    x = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist); h = math.sqrt(max(l1 * l1 - x * x, 0.0))
    return S + u * x + w * h, T


class Poser:
    def __init__(self):
        self.P0, self.R0, self.lt, self.lr = base()
        self.hinge0 = {}
        for s in "lr":
            for a, m, e in ((f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"), (f"thigh_{s}", f"calf_{s}", f"foot_{s}")):
                u = self.P0[m] - self.P0[a]; f = self.P0[e] - self.P0[m]
                self.hinge0[a] = np.cross(u, f) / np.linalg.norm(np.cross(u, f))

    def solve(self, pose):
        P, R = {}, {}
        pv = pose.get("pelvis", {})
        R["pelvis"] = rv(pv.get("rot", [0, 0, 0])) @ self.R0["pelvis"]
        P["pelvis"] = self.P0["pelvis"] + np.array(pv.get("loc", [0, 0, 0]), float)
        Dpel = rv(pv.get("rot", [0, 0, 0]))
        sp = np.array(pose.get("spine", [0, 0, 0]), float); hd = np.array(pose.get("head", [0, 0, 0]), float)

        def fk(b, delta=None):
            p = PARENT[b]
            P[b] = P[p] + R[p] @ self.lt[b]
            R[b] = R[p] @ self.lr[b]
            if delta is not None:
                # world-axis rotation about the bone's own pivot; children inherit it
                R[b] = rv(delta) @ R[b]
        for b in SPINE:
            fk(b, sp / len(SPINE))
        for b in NECK:
            fk(b, hd / len(NECK))
        Dch = rv(sp) @ Dpel
        for s in "lr":
            fk(f"clavicle_{s}", pose.get(f"clavicle_{s}"))
            fk(f"upperarm_{s}")
            self._limb(P, R, f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}", pose.get(f"arm_{s}"), P["spine_05"], Dch, "hand_rot")
            wo = float((pose.get("wrist_out") or [0])[0])
            if wo:
                fa = P[f"hand_{s}"] - P[f"lowerarm_{s}"]; out = Dch @ np.array([1.0 if s == "l" else -1.0, 0, 0])
                ax = np.cross(fa, out); n = np.linalg.norm(ax)
                if n > 1e-6:
                    R[f"hand_{s}"] = rv(ax / n * wo) @ R[f"hand_{s}"]
            for t in ("upperarm_twist_01", "upperarm_twist_02"):
                fk(f"{t}_{s}")
            for t in ("lowerarm_twist_01", "lowerarm_twist_02"):
                fk(f"{t}_{s}")
            fo = float(np.clip((pose.get("fingers") or [0])[0], 0, 1))
            for fg in ("index", "middle", "ring", "pinky"):
                for j in ("metacarpal", "01", "02", "03"):
                    b = f"{fg}_{j}_{s}"
                    if fo > 0 and j != "metacarpal":
                        keep = self.lr[b]; self.lr[b] = _slerp_m(keep, np.eye(3), fo)
                        fk(b); self.lr[b] = keep
                    else:
                        fk(b)
            fk(f"thigh_{s}")
            self._limb(P, R, f"thigh_{s}", f"calf_{s}", f"foot_{s}", pose.get(f"leg_{s}"), P["pelvis"], Dpel, "foot_rot")
            fk(f"ball_{s}")
        return P, R

    def _limb(self, P, R, a, m, e, spec, origin, D, rot_key):
        l1, l2 = np.linalg.norm(self.lt[m]), np.linalg.norm(self.lt[e])
        if not spec:
            P[m] = P[a] + R[a] @ self.lt[m]; R[m] = R[a] @ self.lr[m]
            P[e] = P[m] + R[m] @ self.lt[e]; R[e] = R[m] @ self.lr[e]
            return
        key = "hand" if "hand" in spec else "foot"
        T = origin + D @ np.array(spec[key], float); pole = origin + D @ np.array(spec["pole"], float)
        K, T = two_bone(P[a], T, pole, l1, l2)
        u, f = K - P[a], T - K
        h = np.cross(u, f); h = h / np.linalg.norm(h)
        u0 = self.P0[m] - self.P0[a]; f0 = self.P0[e] - self.P0[m]
        R[a] = _frame(u, h) @ _frame(u0, self.hinge0[a]).T @ self.R0[a]
        R[m] = _frame(f, h) @ _frame(f0, self.hinge0[a]).T @ self.R0[m]
        P[m] = K
        P[e] = T
        R[e] = rv(spec.get(rot_key, [0, 0, 0])) @ (R[m] @ self.lr[e])


def _lerp(a, b, t):
    """Blend two pose dicts (same shape); missing keys count as zero/idle."""
    if isinstance(a, dict) or isinstance(b, dict):
        a, b = a or {}, b or {}
        return {k: _lerp(a.get(k), b.get(k), t) for k in set(a) | set(b)}
    if a is None and b is None:
        return None
    a = np.zeros(3) if a is None else np.array(a, float); b = np.zeros(3) if b is None else np.array(b, float)
    return (a + (b - a) * t).tolist()


def frames(author):
    keys = sorted((int(k), v) for k, v in author["poses"].items())
    out = {}
    for f in range(int(author["frames"])):
        if f <= keys[0][0]:
            out[f] = keys[0][1]; continue
        if f >= keys[-1][0]:
            out[f] = keys[-1][1]; continue
        (fa, pa), (fb, pb) = next((keys[i], keys[i + 1]) for i in range(len(keys) - 1) if keys[i][0] <= f < keys[i + 1][0])
        t = (f - fa) / (fb - fa); t = t * t * (3 - 2 * t)
        out[f] = _lerp(pa, pb, t)
    return out


def run(card_path, opt):
    from animpipe.card import load
    c = load(card_path)
    au = c["author"]
    poser = Poser()
    S = {}
    for f, pose in frames(au).items():
        P, R = poser.solve(pose)
        S[str(f)] = {b: [P[b].tolist(), m2q(R[b]).tolist()] for b in P}
    name = au["take"]
    rel = f"{c['move']}_bones.json"
    json.dump({"source": card_path, "space": "ue_component", "frames": S}, open(os.path.join(WORK, rel), "w"))
    args = [sys.executable, os.path.join(WORK, "ue_write_take.py"), rel, name] + (["--no-write"] if "no-write" in opt else [])
    r = subprocess.run(args, cwd=WORK, capture_output=True, text=True)
    log = os.path.join(WORK, "animpipe", f"{c['move']}_author.txt")
    os.makedirs(os.path.dirname(log), exist_ok=True)
    open(log, "w").write(r.stdout + "\n" + r.stderr)
    tail = [l for l in r.stdout.splitlines() if l.startswith(("bone", "write", "asset", "read-back", "moved"))][-4:]
    print(f"author {c['move']}: {len(S)} frames -> {name} (exit {r.returncode})")
    for l in tail:
        print(l[:200])
    print(log)
