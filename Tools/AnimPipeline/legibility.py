"""Body-part legibility check of an AnimSequence on the UEFN mannequin.

Why: the old silhouette test only asked "how much of the arm sticks out of the body". It passed v7, but from 3/4 the
owner saw the raised rear (right) arm half-cover the lead fist, the rear elbow swing out behind the back, and the rear
hand open. This tool asks instead how well each body part reads on its own.

Model: the physics-asset primitives (physbodies.json, the same shapes clearance.py uses) are grouped into parts. For
every frame and view, each part is ray-cast into its own mask and depth buffer: 256 px, the same orthographic framing as
capture_ue.py 'depth' (200 cm wide, aimed at CENTER), plus the game camera (perspective).

Per part, frame and view:
  distinct   share of the part's projected area NOT covered by any other part. It counts occlusion by nearer parts and
             merging with parts behind it.
  distinct_x the same, ignoring the part's own chain neighbours (the fist vs its own forearm at the wrist, etc.).
  outline    share of the outer silhouette boundary owned by the part (front-most part at each boundary pixel).
Per arm and view:
  negative space between the arm and torso+head: hole area inside their convex hull, and the min 2D gap.
Pose checks:
  rear elbow vs the torso: behind the back plane in 3D (chest frame), and its signed 2D distance to the torso
             outline in the side and rear views (cm, + = outside).
  finger curl: summed joint angles of index..pinky (metacarpal -> 01 -> 02 -> 03), mean per finger, degrees.

Strike block (move sheet "strike", a card supplies it through sheet_extra; see strike_of):
  limb         arm_l (default) | arm_r | leg_l | leg_r | legs   the attacking part
  hiders       parts that may hide it (rule B); default: the other arm for a punch, both arms for a kick; [] = rule B off
  pose_checks  elbow_behind (C), rear_arm_cover (C2: rear elbow tucked against the body, side view), closed_fist (D);
               default all three for a punch, none for a kick (the villain hand is open on purpose, the arms balance away)
  The old strike_limb / strike_side fields still select a kick when there is no strike block.

Usage:
  python legibility.py measure <AnimPath> <take> [contact=first-last] [faf=17]   sample (cached), measure, judge, heatmap
  python legibility.py spot <take> <frame> <view>                        mesh vs capsules (UE SceneCapture depth)
Outputs: sheets/<take>_legibility.png, sheets/<take>_legibility.json, sheets/<take>_legibility.txt"""
import sys, os, json, math, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

HERE = os.path.dirname(os.path.abspath(__file__))
SHEETS = os.path.join(HERE, "sheets")
RAW = os.path.join(SHEETS, "raw")
PR = json.load(open(os.path.join(HERE, "physbodies.json")))["prims"]
SHAPES = {}
for p in PR:
    SHAPES.setdefault(p["bone"], []).append(p)

PARTS = {
    "head": ["head", "neck_01", "neck_02"],
    "torso": ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_l", "clavicle_r"],
    "upperarm_l": ["upperarm_l"], "lowerarm_l": ["lowerarm_l"], "hand_l": ["hand_l"],
    "upperarm_r": ["upperarm_r"], "lowerarm_r": ["lowerarm_r"], "hand_r": ["hand_r"],
    "thigh_l": ["thigh_l"], "shin_l": ["calf_l", "foot_l"], "thigh_r": ["thigh_r"], "shin_r": ["calf_r", "foot_r"],
}
PNAMES = list(PARTS)
NEIGH = {"head": ["torso"], "torso": ["head", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r"],
         "upperarm_l": ["torso", "lowerarm_l"], "lowerarm_l": ["upperarm_l", "hand_l"], "hand_l": ["lowerarm_l"],
         "upperarm_r": ["torso", "lowerarm_r"], "lowerarm_r": ["upperarm_r", "hand_r"], "hand_r": ["lowerarm_r"],
         "thigh_l": ["torso", "shin_l"], "shin_l": ["thigh_l"], "thigh_r": ["torso", "shin_r"], "shin_r": ["thigh_r"]}
ARMS = {"l": ["upperarm_l", "lowerarm_l", "hand_l"], "r": ["upperarm_r", "lowerarm_r", "hand_r"]}
FINGERS = ["index", "middle", "ring", "pinky"]
FINGER_BONES = [f"{f}_{k}_{s}" for s in "lr" for f in FINGERS for k in ("metacarpal", "01", "02", "03")]
BONES = sorted(set(SHAPES) | set(FINGER_BONES) | {"upperarm_l", "upperarm_r", "lowerarm_r", "lowerarm_l", "ball_l", "ball_r", "upperarm_twist_01_l", "upperarm_twist_02_l",
      "lowerarm_twist_01_l", "lowerarm_twist_02_l", "upperarm_twist_01_r", "upperarm_twist_02_r", "lowerarm_twist_01_r", "lowerarm_twist_02_r"})

# Views. UE component space: X = character's left, Y = forward, Z = up. az 0 = camera in front, +az turns toward the
# character's left (capture_ue._place). Ortho framing = capture_ue 'depth'; the 3/4 video is az 315.
CENTER = np.array([0.0, 15.0, 95.0])
RES = 256
ORTHO_W = 200.0
EL = 10.0
VIEWS = [{"name": f"az{a}", "kind": "ortho", "az": a, "el": EL} for a in range(0, 360, 45)]
# Game camera: the project's gameplay camera is GSTNCamera's Follow mode (docs/architecture/camera.md 2.3,
# Plugins/GSTNCamera/.../GSTN_CameraTypes.h FGSTN_CameraView): pivot = actor location, arm 650 cm, pitch -25 deg, FOV 65,
# free orbit that never turns with the character. The actor location sits 88 cm above the feet (SandboxCharacter_CMC: capsule
# half-height 86, mesh at z -88). The orbit is free, so the check samples behind the character +-15 and +-30 deg.
# FOV 24 is a centred crop of the real 65 deg view: same camera position, so the overlaps are the same, with more pixels.
# The character turns freely, so the gameplay cameras are evaluated on a full ring of yaws relative to its facing
# (az 0 = camera in front, 180 = behind):
#   f<az>: Follow, the documented "Mario 3D World" far camera (D1): el 25, 650 cm;
#   h<az>: a higher rail/section angle (designer sections have no preset): el 40, 1000 cm;
#   p090 / p270: the PvP Duel camera, side-on (FGSTN_CameraDuelTuning: pitch -12, FOV 55, arm 450..1800): el 12, 450 cm.
# FOVs are centred crops of the real views (same camera positions, so the same overlaps).
GAME_PIVOT = (0.0, 0.0, 88.0)
RING = tuple(range(0, 360, 45))
for _a in RING:
    VIEWS.append({"name": f"f{_a:03d}", "kind": "persp", "az": _a, "el": 25.0, "dist": 650.0, "fov": 24.0, "center": GAME_PIVOT, "game": "follow"})
for _a in RING:
    VIEWS.append({"name": f"h{_a:03d}", "kind": "persp", "az": _a, "el": 40.0, "dist": 1000.0, "fov": 16.0, "center": GAME_PIVOT, "game": "rail"})
for _a in (90, 270):
    VIEWS.append({"name": f"p{_a:03d}", "kind": "persp", "az": _a, "el": 12.0, "dist": 450.0, "fov": 34.0, "center": GAME_PIVOT, "game": "pvp"})
# the earlier behind +-15/+-30 sample of the Follow camera, kept as named views for the large shots only
EXTRA_VIEWS = {f"g{_y:+d}" if _y else "g0": {"name": f"g{_y:+d}" if _y else "g0", "kind": "persp", "az": 180 + _y, "el": 25.0,
               "dist": 650.0, "fov": 24.0, "center": GAME_PIVOT} for _y in (-30, -15, 0, 15, 30)}
VNAMES = [v["name"] for v in VIEWS]
GAME_VIEWS = [v["name"] for v in VIEWS if v.get("game") in ("follow", "rail")]
PVP_VIEWS = [v["name"] for v in VIEWS if v.get("game") == "pvp"]
REAR_ARM = ["upperarm_r", "lowerarm_r", "hand_r"]
# The attacking limb. Punch (default): the lead fist, its forearm, the lead arm; the rear arm is what may hide it.
# Kick (strike limb leg_l, Ganondorf ftilt; legs, the overlord dair): the kicking foot(s), the shin(s), the leg(s); either
# arm may hide it (the counterbalancing arms must stay off the kick). configure(take) switches before measuring.
# ATTS / FORES / ATT_BONES / ATT_JOINTS are lists (two for "legs"); ATT and ATT_FORE are the first, for labels.
# REAR is the side of the non-attacking arm whose elbow rules C/C2 watch (the result keys keep the name elbow_r_*).
LIMBS = ("arm_l", "arm_r", "leg_l", "leg_r", "legs")
POSE_CHECKS_ALL = ("elbow_behind", "rear_arm_cover", "closed_fist")
_PARTS0 = {k: list(v) for k, v in PARTS.items()}
_NEIGH0 = {k: list(v) for k, v in NEIGH.items()}
ATT, ATT_FORE, ATT_BONE, ATT_JOINT = "hand_l", "lowerarm_l", "hand_l", "lowerarm_l"
ATTS, FORES, ATT_BONES, ATT_JOINTS = [ATT], [ATT_FORE], [ATT_BONE], [ATT_JOINT]
ATT_CHAIN = list(ARMS["l"])
HIDERS = list(REAR_ARM)
REAR = "r"
POSE_CHECKS = set(POSE_CHECKS_ALL)
STRIKE = {"limb": "arm_l", "hiders": list(REAR_ARM), "pose_checks": list(POSE_CHECKS_ALL)}
SHEET = {}
KICK = False

def sheet_entry(take, anim=None):
    """The move-sheet entry of a take. checks.py keys the entries by move name ("overlord_dair_v4") and passes the take
    (asset name minus AS_), so a key lookup alone missed every card-driven move and the dair / ftilt v3.1 were judged as
    punches. Key first (old hand-written entries), then the entry whose anim is this asset (the last one wins)."""
    try:
        ms = json.load(open(os.path.join(HERE, "move_sheets.json")))
    except Exception:
        return {}
    if isinstance(ms.get(take), dict):
        return ms[take]
    hits = [v for k, v in ms.items() if isinstance(v, dict) and not k.startswith("_") and v.get("anim")
            and (v["anim"] == anim or v["anim"].rsplit("/", 1)[-1] == f"AS_{take}")]
    return hits[-1] if hits else {}

def _hider_parts(name, kick_sides):
    """A hider entry: a part name (hand_r, torso, ...) or a group (arm_l, arm_r, arms, leg_l, leg_r, legs)."""
    if name in ("arm_l", "arm_r"):
        return list(ARMS[name[-1]])
    if name == "arms":
        return ARMS["l"] + ARMS["r"]
    if name in ("leg_l", "leg_r", "legs"):
        sides = "lr" if name == "legs" else name[-1]
        return [p for s in sides for p in ([f"thigh_{s}", f"shin_{s}"] + ([f"foot_{s}"] if s in kick_sides else []))]
    return [name]

def strike_of(sh):
    """The strike block of a move sheet with its defaults: {limb, hiders (part names), pose_checks}. The card supplies
    sheet["strike"] through sheet_extra; without it the old strike_limb / strike_side fields pick the limb, so the move
    sheets written before the block still behave as they did (a leg = kick defaults)."""
    st = dict(sh.get("strike") or {})
    limb = st.get("limb") or sh.get("strike_limb") or "arm_l"
    if limb not in LIMBS:
        raise ValueError(f"strike limb {limb!r}: one of {LIMBS}")
    kick = limb.startswith("leg")
    sides = ("lr" if limb == "legs" else limb[-1]) if kick else ""
    if "hiders" in st:
        hiders = [p for h in st["hiders"] for p in _hider_parts(h, sides)]
    else:
        hiders = ARMS["l"] + ARMS["r"] if kick else list(ARMS["r" if limb == "arm_l" else "l"])
    own = ([f"{q}_{s}" for s in sides for q in ("thigh", "shin", "foot")] if kick else list(ARMS[limb[-1]]))
    hiders = [p for p in dict.fromkeys(hiders) if p not in own]
    pc = list(st.get("pose_checks", [] if kick else POSE_CHECKS_ALL))
    bad = [p for p in pc if p not in POSE_CHECKS_ALL]
    if bad:
        raise ValueError(f"strike pose_checks {bad}: one of {POSE_CHECKS_ALL}")
    return {"limb": limb, "hiders": hiders, "pose_checks": pc}

def configure(take, anim=None, sheet=None):
    """Set the attacking limb, the hiders and the pose checks from the move sheet (sheet= for tests). Rebuilt from the
    import-time parts every call, so a worker process or a second take never stacks the kick split twice."""
    global ATT, ATT_FORE, ATT_BONE, ATT_JOINT, ATTS, FORES, ATT_BONES, ATT_JOINTS, ATT_CHAIN, HIDERS, REAR, POSE_CHECKS
    global STRIKE, SHEET, KICK, TAKE
    TAKE = take
    SHEET = sheet if sheet is not None else sheet_entry(take, anim)
    STRIKE = strike_of(SHEET)
    limb = STRIKE["limb"]
    PARTS.clear(); PARTS.update({k: list(v) for k, v in _PARTS0.items()})
    NEIGH.clear(); NEIGH.update({k: list(v) for k, v in _NEIGH0.items()})
    KICK = limb.startswith("leg")
    if KICK:
        sides = "lr" if limb == "legs" else limb[-1]
        for s in sides:
            PARTS[f"shin_{s}"] = [f"calf_{s}"]; PARTS[f"foot_{s}"] = [f"foot_{s}"]
            NEIGH[f"shin_{s}"] = [f"thigh_{s}", f"foot_{s}"]; NEIGH[f"foot_{s}"] = [f"shin_{s}"]
        ATTS = [f"foot_{s}" for s in sides]; FORES = [f"shin_{s}" for s in sides]
        ATT_BONES = [f"ball_{s}" for s in sides]; ATT_JOINTS = [f"calf_{s}" for s in sides]
        ATT_CHAIN = [f"{q}_{s}" for s in sides for q in ("thigh", "shin", "foot")]
        if limb == "legs":   # both feet strike together (dair stomp): the two legs read as one limb, touching is not hiding
            for p in ATT_CHAIN:
                NEIGH[p] = list(dict.fromkeys(NEIGH.get(p, []) + [q for q in ATT_CHAIN if q != p]))
        REAR = "r"
    else:
        s = limb[-1]
        ATTS, FORES, ATT_BONES, ATT_JOINTS = [f"hand_{s}"], [f"lowerarm_{s}"], [f"hand_{s}"], [f"lowerarm_{s}"]
        ATT_CHAIN = list(ARMS[s])
        REAR = "r" if s == "l" else "l"
    PNAMES[:] = list(PARTS)
    ATT, ATT_FORE, ATT_BONE, ATT_JOINT = ATTS[0], FORES[0], ATT_BONES[0], ATT_JOINTS[0]
    HIDERS = [p for p in STRIKE["hiders"] if p in PARTS]
    POSE_CHECKS = set(STRIKE["pose_checks"])
    return KICK


# ---------------------------------------------------------------- math (same conventions as clearance.py)
def rot_to_quat(p, y, r):
    d = math.pi / 360.0
    SP, CP = math.sin(p * d), math.cos(p * d)
    SY, CY = math.sin(y * d), math.cos(y * d)
    SR, CR = math.sin(r * d), math.cos(r * d)
    return np.array([CR * SP * SY - SR * CP * CY, -CR * SP * CY - SR * CP * SY, CR * CP * SY - SR * SP * CY, CR * CP * CY + SR * SP * SY])

def qmul(a, b):
    x1, y1, z1, w1 = a; x2, y2, z2, w2 = b
    return np.array([w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                     w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2])

def qrot(q, v):
    u = q[:3]; w = q[3]
    return 2 * np.dot(u, v) * u + (w * w - np.dot(u, u)) * v + 2 * w * np.cross(u, v)

def qangle(a, b):
    """Angle of the relative rotation between two orientations, degrees."""
    d = abs(float(np.dot(a, b)) / (np.linalg.norm(a) * np.linalg.norm(b)))
    return math.degrees(2 * math.acos(min(1.0, d)))


# ---------------------------------------------------------------- sampling (UE bridge, cached)
def sample(anim, take):
    path = os.path.join(RAW, f"{take}_legbones.json")
    if os.path.exists(path):
        d = json.load(open(path))
        if d.get("anim") == anim:
            return d["frames"]
    import ue
    n = int(ue.call("read_anim_sequence", {"assetPath": anim}).get("numFrames") or 38)
    frames = {}
    for s0 in range(0, n, 8):
        r = ue.call("sample_pose", {"assetPath": anim, "boneNames": BONES, "frames": list(range(s0, min(n, s0 + 8))), "space": "component"})
        for s in r["samples"]:
            frames[str(s["frame"])] = {b["name"]: [b["location"][k] for k in "xyz"] + [b["rotation"][k] for k in ("pitch", "yaw", "roll")]
                                       for b in s["bones"]}
    os.makedirs(RAW, exist_ok=True)
    json.dump({"anim": anim, "frames": frames}, open(path, "w"))
    return frames

def bone_tf(fr, b):
    v = fr[b]
    return np.array(v[:3]), rot_to_quat(v[3], v[4], v[5])

def part_prims(fr):
    out = {}
    for part, bones in PARTS.items():
        lst = []
        for b in bones:
            L, Q = bone_tf(fr, b)
            for s in SHAPES.get(b, []):
                c = L + qrot(Q, np.array(s["trans"]))
                q = qmul(Q, np.array(s["quat"]))
                ax = [qrot(q, np.array(v, dtype=float)) for v in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
                t = s["type"].upper()
                if "CAPSULE" in t or "SPHYL" in t:
                    r = s["extent"][0]; h = max(s["extent"][2] - r, 0.0)
                    lst.append(("cap", c - ax[2] * h, c + ax[2] * h, r))
                elif "SPHERE" in t:
                    lst.append(("cap", c, c, s["extent"][0]))
                else:
                    lst.append(("box", c, np.array(ax), np.array(s["extent"])))
        out[part] = lst
    return out


# ---------------------------------------------------------------- cameras and ray casting
def camera(view):
    az, el = math.radians(view["az"]), math.radians(view["el"])
    dist = view.get("dist", 400.0)
    center = np.array(view.get("center", CENTER))
    loc = center + dist * np.array([math.cos(el) * math.sin(az), math.cos(el) * math.cos(az), math.sin(el)])
    f = (center - loc) / np.linalg.norm(center - loc)
    yaw, pitch = math.atan2(f[1], f[0]), math.asin(f[2])
    right = np.array([-math.sin(yaw), math.cos(yaw), 0.0])                     # UE: left-handed, Y right of X
    up = np.array([-math.sin(pitch) * math.cos(yaw), -math.sin(pitch) * math.sin(yaw), math.cos(pitch)])
    return loc, f, right, up

_RAYS = {}
def rays(view):
    if view["name"] in _RAYS:
        return _RAYS[view["name"]]
    loc, f, right, up = camera(view)
    i = (np.arange(RES) + 0.5) / RES * 2 - 1                                       # -1..1 left -> right
    j = 1 - (np.arange(RES) + 0.5) / RES * 2                                       # 1..-1 top -> bottom
    X, Y = np.meshgrid(i, j)
    X = X.reshape(-1, 1); Y = Y.reshape(-1, 1)
    if view["kind"] == "ortho":
        O = loc + X * (ORTHO_W / 2) * right + Y * (ORTHO_W / 2) * up
        D = np.broadcast_to(f, O.shape).copy()
    else:
        t = math.tan(math.radians(view["fov"]) / 2)
        D = f + X * t * right + Y * t * up
        D /= np.linalg.norm(D, axis=1, keepdims=True)
        O = np.broadcast_to(loc, D.shape).copy()
    _RAYS[view["name"]] = (O, D, loc, f, right, up)
    return _RAYS[view["name"]]

def project(view, p):
    """World point -> (col, row) in pixels, for markers and the elbow check."""
    loc, f, right, up = camera(view)
    d = p - loc
    if view["kind"] == "ortho":
        x, y = np.dot(d, right) / (ORTHO_W / 2), np.dot(d, up) / (ORTHO_W / 2)
    else:
        t = math.tan(math.radians(view["fov"]) / 2); z = np.dot(d, f)
        x, y = np.dot(d, right) / (z * t), np.dot(d, up) / (z * t)
    return (x + 1) / 2 * RES - 0.5, (1 - y) / 2 * RES - 0.5

def cast_cap(O, D, a, b, r):
    u = b - a; w0 = O - a
    bb = D @ u; cc = float(u @ u); dd = np.einsum("ij,ij->i", D, w0); ee = w0 @ u
    den = cc - bb * bb
    t = np.where(den > 1e-9, (ee - bb * dd) / np.where(den > 1e-9, den, 1), 0.0) if cc > 1e-9 else np.zeros(len(O))
    t = np.clip(t, 0, 1)
    s = t * bb - dd                                                                 # ray param of the closest point
    P = a + t[:, None] * u
    dist = np.linalg.norm(O + s[:, None] * D - P, axis=1)
    hit = dist < r
    depth = np.where(hit, s - np.sqrt(np.maximum(r * r - dist * dist, 0)), np.inf)
    return hit, depth

def cast_box(O, D, c, ax, e):
    o = (O - c) @ ax.T; d = D @ ax.T
    d = np.where(np.abs(d) < 1e-9, 1e-9, d)
    t1 = (-e - o) / d; t2 = (e - o) / d
    tn = np.minimum(t1, t2).max(axis=1); tf = np.maximum(t1, t2).min(axis=1)
    hit = (tn <= tf) & (tf > 0)
    return hit, np.where(hit, tn, np.inf)

def px_per_cm(view):
    """Image scale: exact for ortho views, at the pivot depth for perspective ones."""
    if view["kind"] == "ortho":
        return RES / ORTHO_W
    return RES / (2 * view["dist"] * math.tan(math.radians(view["fov"]) / 2))

def render(prims, view):
    O, D = rays(view)[:2]
    depth = np.full((len(PNAMES), RES * RES), np.inf, dtype=np.float32)
    for k, part in enumerate(PNAMES):
        for pr in prims[part]:
            h, z = cast_cap(O, D, pr[1], pr[2], pr[3]) if pr[0] == "cap" else cast_box(O, D, pr[1], pr[2], pr[3])
            depth[k] = np.minimum(depth[k], z)
    return depth.reshape(len(PNAMES), RES, RES)


# ---------------------------------------------------------------- 2D helpers (no scipy here)
def boundary(m):
    inner = m.copy()
    inner[1:, :] &= m[:-1, :]; inner[:-1, :] &= m[1:, :]; inner[:, 1:] &= m[:, :-1]; inner[:, :-1] &= m[:, 1:]
    inner[0, :] = inner[-1, :] = inner[:, 0] = inner[:, -1] = False
    return m & ~inner

def hull_mask(m):
    from PIL import Image, ImageDraw
    ys, xs = np.nonzero(boundary(m))
    if len(xs) < 3:
        return m.copy()
    pts = sorted(set(zip(xs.tolist(), ys.tolist())))
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, hi = [], []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(hi) >= 2 and cross(hi[-2], hi[-1], p) <= 0:
            hi.pop()
        hi.append(p)
    img = Image.new("L", (RES, RES), 0)
    ImageDraw.Draw(img).polygon(lo[:-1] + hi[:-1], fill=1, outline=1)
    return np.array(img, dtype=bool)

def signed_dist(m, col, row):
    """Pixels from (col,row) to the mask outline: + outside, - inside."""
    ys, xs = np.nonzero(m)
    if not len(xs):
        return float("nan")
    r, c = int(round(row)), int(round(col))
    inside = 0 <= r < RES and 0 <= c < RES and m[r, c]
    if inside:
        ys, xs = np.nonzero(~m)
        return -float(np.sqrt(((xs - col) ** 2 + (ys - row) ** 2).min()))
    return float(np.sqrt(((xs - col) ** 2 + (ys - row) ** 2).min()))

def min_gap(a, b):
    if not a.any() or not b.any():
        return float("nan")
    if (a & b).any():
        return 0.0
    ya, xa = np.nonzero(boundary(a)); yb, xb = np.nonzero(boundary(b))
    d = (xa[:, None] - xb[None, :]) ** 2 + (ya[:, None] - yb[None, :]) ** 2
    return float(np.sqrt(d.min()))


# ---------------------------------------------------------------- pose metrics
def chest_frame(fr):
    """Body-relative axes from the shoulders: forward (horizontal, toward +Y side) and the chest centre."""
    ul, _ = bone_tf(fr, "upperarm_l"); ur, _ = bone_tf(fr, "upperarm_r"); c5, _ = bone_tf(fr, "spine_05")
    side = ul - ur; side[2] = 0; side /= np.linalg.norm(side)
    fwd = np.array([-side[1], side[0], 0.0])        # up x side: the chest's own facing. Never flipped toward world +Y:
    return c5, fwd                                 # at contact the torso turns past 90 deg and faces the character's right

def finger_curl(fr, s):
    tot = []
    for f in FINGERS:
        qs = [bone_tf(fr, f"{f}_{k}_{s}")[1] for k in ("metacarpal", "01", "02", "03")]
        tot.append(sum(qangle(qs[i], qs[i + 1]) for i in range(3)))
    return float(np.mean(tot)), [round(x, 1) for x in tot]


# ---------------------------------------------------------------- measure
# Frames are measured in parallel (owner, 31/10: "why does the render cost so much?" - 44 frames x 26 views of numpy ray
# casting took ~2 min on 1 core of 24). Each worker process re-applies configure(take) (the kick mode changes module
# globals); results are identical to the single-core run. LEG_WORKERS=1 forces one process; the default leaves 6 cores free.
TAKE = None

def workers():
    n = int(os.environ.get("LEG_WORKERS", "0") or 0)
    return n if n > 0 else max(1, (os.cpu_count() or 2) - 6)

def _init_worker(take, sheet, center=None):
    if take:
        configure(take, sheet=sheet)
    if center is not None:
        CENTER[:] = center

def fit_center(frames, margin=10.0):
    """The ortho views are ORTHO_W wide around CENTER (z 95 = a standing body). An aerial stomp puts the kicking foot
    below z -5, outside the frame: its mask is empty and rule A read it as missing (KeyError on the dair). Only when the
    take leaves the frame is the centre moved to the middle of its vertical range, so takes that fit keep the old framing
    (and the calibration made on it)."""
    zs = [float(bone_tf(fr, b)[0][2]) for fr in frames.values() for b in ("head", "pelvis", "ball_l", "ball_r", "hand_l", "hand_r")]
    lo, hi, half = min(zs), max(zs), ORTHO_W / 2 - margin
    if lo < CENTER[2] - half or hi > CENTER[2] + half:
        CENTER[2] = round((lo + hi) / 2, 1)
        print(f"  ortho centre moved to z {CENTER[2]} (take spans z {lo:.0f}..{hi:.0f})", flush=True)

def measure(frames):
    out = {"views": VNAMES, "parts": PNAMES, "frames": {}}
    keys = sorted(frames, key=int)
    n = min(workers(), len(keys))
    if n <= 1:
        for fs in keys:
            out["frames"][fs] = measure_frame(fs, frames[fs])
            print(f"  frame {fs} done", flush=True)
        return out
    import multiprocessing as mp
    with mp.get_context("spawn").Pool(n, initializer=_init_worker, initargs=(TAKE, SHEET, CENTER.tolist())) as pool:
        for fs, rowf in zip(keys, pool.starmap(measure_frame, [(fs, frames[fs]) for fs in keys], chunksize=1)):
            out["frames"][fs] = rowf
    print(f"  {len(keys)} frames done on {n} processes", flush=True)
    return out

def measure_frame(fs, fr):
    px_cm = ORTHO_W / RES
    if True:
        prims = part_prims(fr)
        c5, fwd = chest_frame(fr)
        elbow = bone_tf(fr, f"lowerarm_{REAR}")[0]
        # back surface of the chest: spine_05 / spine_04 capsule radius around the chest centre, along -fwd
        behind = float(np.dot(elbow - c5, -fwd) - SHAPES["spine_05"][0]["extent"][0])
        sideax = np.array([fwd[1], -fwd[0], 0.0])                                  # toward the character's left
        rowf = {"elbow_r_behind_back_cm": round(behind, 1),
                "elbow_r_body_cm": [round(float(np.dot(elbow - c5, -fwd)), 1), round(float(np.dot(elbow - c5, -sideax)), 1),
                                    round(float(elbow[2] - c5[2]), 1)],                      # back, out (right), up

                "curl_l": round(finger_curl(fr, "l")[0], 1), "curl_r": round(finger_curl(fr, "r")[0], 1),
                "curl_l_fingers": finger_curl(fr, "l")[1], "curl_r_fingers": finger_curl(fr, "r")[1], "views": {}}
        for view in VIEWS:
            depth = render(prims, view)
            masks = np.isfinite(depth)
            U = masks.any(axis=0)
            idmap = np.where(U, depth.argmin(axis=0), -1)
            bnd = boundary(U); nb = max(1, int(bnd.sum()))
            vr = {"parts": {}, "pairs": {}, "neg": {}}
            for k, p in enumerate(PNAMES):
                m = masks[k]; a = int(m.sum())
                if not a:
                    continue
                others = masks[[i for i in range(len(PNAMES)) if i != k]].any(axis=0)
                ox = masks[[i for i, q in enumerate(PNAMES) if i != k and q not in NEIGH[p]]].any(axis=0)
                vr["parts"][p] = {"area": a, "distinct": round(100 * float((m & ~others).sum()) / a, 1),
                                  "distinct_x": round(100 * float((m & ~ox).sum()) / a, 1),
                                  "visible": round(100 * float((idmap == k).sum()) / a, 1),
                                  "outline": round(100 * float((bnd & (idmap == k)).sum()) / nb, 1)}
                for i, q in enumerate(PNAMES):
                    if i == k:
                        continue
                    ov = m & masks[i]
                    n = int(ov.sum())
                    if n and 100 * n / a >= 2:
                        occl = int((ov & (depth[i] < depth[k])).sum())     # q is nearer: q covers p
                        vr["pairs"][f"{p}|{q}"] = [round(100 * n / a, 1), round(100 * occl / a, 1)]
            # share of the attacking fist that only the rear arm hides: covered (either depth order) by the rear arm and by
            # nothing else except its own forearm. Where torso or head already cover the fist, the rear arm is not to blame.
            hl = masks[[PNAMES.index(q) for q in ATTS]].any(axis=0)
            if hl.any() and HIDERS:
                rear = masks[[PNAMES.index(q) for q in HIDERS]].any(axis=0)
                rest = masks[[i for i, q in enumerate(PNAMES) if q not in HIDERS + ATTS + FORES]].any(axis=0)
                vr["fist_rear_steal"] = round(100 * float((hl & rear & ~rest).sum()) / float(hl.sum()), 1)
            # the attacking limb as a whole (forearm + fist, or shin + foot) against everything that is not that limb
            arm = masks[[PNAMES.index(q) for q in FORES + ATTS]].any(axis=0)
            if arm.any():
                notarm = masks[[i for i, q in enumerate(PNAMES) if q not in ATT_CHAIN]].any(axis=0)
                vr["arm_l_distinct"] = round(100 * float((arm & ~notarm).sum()) / float(arm.sum()), 1)
                vr["arm_l_cm2"] = round(float(arm.sum()) / px_per_cm(view) ** 2, 1)
                vr["arm_l_distinct_cm2"] = round(float((arm & ~notarm).sum()) / px_per_cm(view) ** 2, 1)
            ys, xs = np.nonzero(U)
            if len(xs):
                vr["sil_w_over_h"] = round(float(xs.max() - xs.min() + 1) / float(ys.max() - ys.min() + 1), 2)
            body = masks[PNAMES.index("torso")] | masks[PNAMES.index("head")]
            # exaggeration: how far each arm reaches out of the body outline, and where each elbow sits against it (cm)
            bb = boundary(body); by_, bx_ = np.nonzero(bb)
            vr["limb_out_cm"] = {}; vr["elbow_out_cm"] = {}
            for s, arm in ARMS.items():
                am = masks[[PNAMES.index(q) for q in arm]].any(axis=0) & ~body
                ay, ax_ = np.nonzero(am)
                if len(ax_) and len(bx_):
                    step = max(1, len(ax_) // 1500)
                    dd = np.sqrt(((ax_[::step, None] - bx_[None, :]) ** 2 + (ay[::step, None] - by_[None, :]) ** 2).min(axis=1))
                    vr["limb_out_cm"][s] = round(float(dd.max()) / px_per_cm(view), 1)
                else:
                    vr["limb_out_cm"][s] = 0.0
                ecol, erow = project(view, bone_tf(fr, f"lowerarm_{s}")[0])
                vr["elbow_out_cm"][s] = round(signed_dist(body, ecol, erow) / px_per_cm(view), 1)
            if ATT_CHAIN != ARMS["l"]:   # rule F reads key "l" = the attacking limb (kicking leg(s), or arm_r); arm_l kept as "al"
                vr["limb_out_cm"]["al"], vr["elbow_out_cm"]["al"] = vr["limb_out_cm"]["l"], vr["elbow_out_cm"]["l"]
                am = masks[[PNAMES.index(q) for q in ATT_CHAIN]].any(axis=0) & ~body
                ay, ax_ = np.nonzero(am)
                if len(ax_) and len(bx_):
                    step = max(1, len(ax_) // 1500)
                    dd = np.sqrt(((ax_[::step, None] - bx_[None, :]) ** 2 + (ay[::step, None] - by_[None, :]) ** 2).min(axis=1))
                    vr["limb_out_cm"]["l"] = round(float(dd.max()) / px_per_cm(view), 1)
                else:
                    vr["limb_out_cm"]["l"] = 0.0
                # the knee (elbow) that reads best: for "legs" either knee out of the outline is the shape
                vr["elbow_out_cm"]["l"] = round(max(signed_dist(body, *project(view, bone_tf(fr, j)[0])) for j in ATT_JOINTS)
                                                / px_per_cm(view), 1)
            for s, arm in ARMS.items():
                am = masks[[PNAMES.index(q) for q in arm]].any(axis=0)
                holes = hull_mask(am | body) & ~U
                g = min_gap(am, body)
                vr["neg"][s] = {"area_cm2": round(float(holes.sum()) * px_cm * px_cm, 1) if view["kind"] == "ortho" else None,
                                "gap_cm": round(g * px_cm, 1) if view["kind"] == "ortho" and g == g else None}
            if view["kind"] == "ortho":
                col, row = project(view, elbow)
                vr["elbow_r_out_cm"] = round(signed_dist(masks[PNAMES.index("torso")], col, row) * px_cm, 1)
            rowf["views"][view["name"]] = vr
    return rowf


# ---------------------------------------------------------------- judge + heatmap
TH = {}   # filled from legibility_thresholds.json (calibrated on v5, v6 and v7; see jab_log.md)

def load_th():
    p = os.path.join(HERE, "legibility_thresholds.json")
    TH.update(json.load(open(p)) if os.path.exists(p) else {})
    # a kick is judged with the foot's own thresholds (block "kick"), not the ones calibrated on the jab's arm
    if KICK:
        TH.update(TH.get("kick", {}))

def fist_cover_rear(vr):
    """% of the attacking fist overlapped by the rear arm in this view (any depth order) and % occluded by it."""
    ov = oc = 0.0
    for q in REAR_ARM:
        v = vr["pairs"].get(f"hand_l|{q}")
        if v:
            ov += v[0]; oc += v[1]
    return min(ov, 100.0), min(oc, 100.0)

def arm_read_any(fr, v):
    side = max(fr["views"][w].get("arm_l_cm2", 0.0) for w in ("az90", "az270")) or 1.0
    return min(100.0, 100 * fr["views"][v].get("arm_l_distinct_cm2", 0.0) / side)

def game_arm_read(fr, views=None):
    """Per gameplay view: forearm+fist area that reads beside the body, % of the same arm's area in the better side view."""
    side = max(fr["views"][v].get("arm_l_cm2", 0.0) for v in ("az90", "az270")) or 1.0
    return {v: round(min(100.0, 100 * fr["views"][v].get("arm_l_distinct_cm2", 0.0) / side), 1) for v in (views or GAME_VIEWS + PVP_VIEWS)}

def fist_extension(res, c0):
    """Attacking fist progress along its guard -> contact line per frame (from the cached bone samples)."""
    take = res.get("take")
    p = os.path.join(RAW, f"{take}_legbones.json") if take else None
    if not p or not os.path.exists(p):
        return {}
    fr = json.load(open(p))["frames"]
    h = {int(k): np.mean([bone_tf(v, b)[0] for b in ATT_BONES], axis=0) for k, v in fr.items()}
    d = h[c0] - h[0]; dd = float(np.dot(d, d)) or 1e-6
    return {k: float(np.dot(v - h[0], d)) / dd for k, v in h.items()}

def judge(res, contact, faf):
    """contact = (first, last) active frame. Rules and thresholds: legibility_thresholds.json (calibration in jab_log.md)."""
    load_th()
    fl = []
    c0, c1 = contact
    # A. at contact the fist must read on its own from every 3/4 view (front and back). The views behind are rule E's job
    #    (the game camera range), where the whole attacking arm is judged.
    for f in range(c0, c1 + 1):
        for v in TH["contact_views"]:
            for att in ATTS:
                vr = res["frames"][str(f)]["views"][v]
                d = vr["parts"].get(att, {}).get("distinct_x", 0.0)    # no mask in this view = not readable
                if d < TH["fist_contact_distinct_min"]:
                    worst = max(((k.split("|")[1], val[0]) for k, val in vr["pairs"].items() if k.startswith(att + "|")
                                 and k.split("|")[1] not in NEIGH[att]), key=lambda x: x[1], default=("?", 0))
                    fl.append({"check": "fist not distinct at contact", "frame": f, "view": v, "fist_distinct_x": d, "with": worst,
                               **({"part": att} if len(ATTS) > 1 else {})})
    # B. while the attacking fist is out (>= steal_min_extension of its guard->contact reach, from contact on), the rear arm
    #    must not be what hides it (where torso/head hide it anyway it is not blamed). Back in the guard, a tucked rear arm
    #    covering the lead fist from the rear-arm side is the guard itself, not the attack. hiders = [] turns the rule off.
    ext = fist_extension(res, c0) if HIDERS else {}
    strike_end = c1 + TH.get("steal_review_strike_extra_frames", 2)
    last = faf
    if KICK:      # the strike and its hold, not the re-chamber (legibility_thresholds.json _kick_steal_note)
        hw = SHEET.get("hold_window") or [c0, c1]
        last = min(faf, hw[1] + 1)
    for f in [f for f in range(c0, last + 1) if HIDERS and ext.get(f, 1.0) >= TH.get("steal_min_extension", 0.15)]:
        for v in TH["steal_views"]:
            # gameplay cameras (f*, h*, p*): the whole time the fist is out. Low diagnostic views (az*): the strike itself
            # (contact + snap back); in the recovery spring they are reported only (see legibility_thresholds.json note)
            if v.startswith("az") and f > strike_end:
                continue
            s = res["frames"][str(f)]["views"][v].get("fist_rear_steal", 0)
            if s >= TH["fist_rear_steal_max"]:
                fl.append({"check": "rear arm hides the fist", "frame": f, "view": v, "steal_pct": s})
    # C. rear elbow: not behind the back plane of the chest (3D), confirmed in the views where it shows (pose check
    #    elbow_behind: a punch's guard rule).
    for f in (sorted(res["frames"], key=int) if "elbow_behind" in POSE_CHECKS else []):
        r = res["frames"][f]
        if r["elbow_r_behind_back_cm"] > TH["elbow_behind_max_cm"]:
            out = {v: r["views"][v]["elbow_r_out_cm"] for v in TH["elbow_views"]}
            fl.append({"check": "rear elbow behind the back", "frame": int(f), "cm": r["elbow_r_behind_back_cm"],
                       "outside_outline_cm": out})
    # C2. rear elbow flaring away from the body during the move, seen from the side where the guard keeps it inside.
    #     (pose check rear_arm_cover; off for kicks: both arms counterbalance away from the torso on purpose, arm_check
    #     keeps the elbows down.)
    for f in (range(c0, faf + 1) if "rear_arm_cover" in POSE_CHECKS else []):
        e = res["frames"][str(f)]["views"][TH["elbow_flare_view"]]["elbow_r_out_cm"]
        if e > TH["elbow_flare_max_cm"]:
            fl.append({"check": "rear elbow away from the body (side view)", "frame": f, "cm": e})
    # E. game camera: in the active frames the attacking arm (forearm + fist) must read beside the body from the camera the
    #    player uses. A share of the arm's own projection is misleading when the arm points away from the camera (60% of a
    #    foreshortened stub is still a stub), so the measure is the arm area that reads on its own from the game camera as a
    #    % of the same arm seen from the side (its full length): mean over the yaw range >= game_arm_read_min, each yaw
    #    >= game_arm_read_floor.
    #    The character turns freely: each ring (Follow, rail) is judged over every yaw except the front one (a punch toward
    #    the camera reads by its size and trail, not its silhouette), and the PvP side views on their own.
    for f in range(c0, c1 + 1):
        vals = game_arm_read(res["frames"][str(f)])
        for ring in ("f", "h"):
            rv = {v: x for v, x in vals.items() if v.startswith(ring) and v[1:] != "000"}
            mean = sum(rv.values()) / len(rv)
            worst = min(rv, key=rv.get)
            if mean < TH["game_arm_read_min"] or rv[worst] < TH["game_arm_read_floor"]:
                fl.append({"check": "attacking arm hidden from the game camera", "frame": f, "camera": {"f": "follow", "h": "rail"}[ring],
                           "mean_pct": round(mean, 1), "worst_yaw": worst, "worst_pct": rv[worst], "by_yaw": rv})
        for v in PVP_VIEWS:
            if vals[v] < TH["pvp_arm_read_min"]:
                fl.append({"check": "attacking arm hidden from the PvP side camera", "frame": f, "view": v, "pct": vals[v]})
    # F. exaggeration: at contact the attacking arm reaches well out of the body and its elbow sits outside the outline
    #    (PvP side view, where the arm shape is judged).
    for f in range(c0, c1 + 1):
        fr = res["frames"][str(f)]
        reach = min(fr["views"][v]["limb_out_cm"]["l"] for v in PVP_VIEWS)
        elbow = min(fr["views"][v]["elbow_out_cm"]["l"] for v in PVP_VIEWS)
        if reach < TH["limb_out_min_cm"] or elbow < TH["elbow_out_min_cm"]:
            fl.append({"check": "attacking arm not exaggerated out of the body", "frame": f, "reach_cm": reach, "elbow_out_cm": elbow})
    # D. fists closed in the guard and at the FAF (pose check closed_fist; off for the villain hand, open on purpose).
    for f in (sorted({0, faf}) if "closed_fist" in POSE_CHECKS else []):
        for s in "lr":
            c = res["frames"][str(f)][f"curl_{s}"]
            if c < TH["curl_closed_min_deg"]:
                fl.append({"check": f"hand_{s} open", "frame": f, "curl_deg": c})
    return fl

def heatmap(res, take, contact, faf, flags):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.colors import LinearSegmentedColormap
    load_th()
    nf = len(res["frames"]); fr = [str(i) for i in range(nf)]
    keys = [(ATT, f"attacking {'foot' if KICK else 'fist'} ({ATT})"), (ATT_FORE, "attacking shin" if KICK else "lead forearm"), (f"upperarm_{REAR}", "rear upper arm"),
            (f"lowerarm_{REAR}", "rear forearm"), (f"hand_{REAR}", "rear hand"), ("head", "head"), ("arm_l", "ATTACKING ARM: forearm + fist area that reads, % of its side view (game rule on rows g*)")]
    cm = LinearSegmentedColormap.from_list("leg", ["#c0151b", "#f2a33a", "#f4e86a", "#5fb85a", "#1d7a37"])
    fig, axs = plt.subplots(len(keys) + 2, 1, figsize=(15, 3.0 * (len(keys) + 2)), constrained_layout=True)
    for ax, (p, label) in zip(axs, keys):
        M = np.array([[(arm_read_any(res["frames"][f], v) if p == "arm_l" else
                        res["frames"][f]["views"][v]["parts"].get(p, {}).get("distinct_x", np.nan)) for f in fr] for v in VNAMES])
        im = ax.imshow(M, aspect="auto", cmap=cm, vmin=0, vmax=100, interpolation="nearest")
        ax.set_yticks(range(len(VNAMES)), VNAMES); ax.set_xticks(range(nf), fr, fontsize=7)
        ax.set_title(f"{take}: {label}, distinct % (red = merges with other parts)", fontsize=10, loc="left")
        for x in (contact[0], faf):
            ax.axvline(x - 0.5, color="k", lw=1.2); ax.axvline(x + 0.5, color="k", lw=1.2)
        if p == ATT:
            for fl in flags:
                if fl["check"] == "fist not distinct at contact":
                    ax.add_patch(plt.Rectangle((fl["frame"] - 0.5, VNAMES.index(fl["view"]) - 0.5), 1, 1, fill=False, ec="k", lw=1.5))
        fig.colorbar(im, ax=ax, fraction=0.015, pad=0.005)
    ax = axs[len(keys)]
    M = np.array([[res["frames"][f]["views"][v].get("fist_rear_steal", np.nan) for f in fr] for v in VNAMES])
    im = ax.imshow(M, aspect="auto", cmap=cm.reversed(), vmin=0, vmax=100, interpolation="nearest")
    ax.set_yticks(range(len(VNAMES)), VNAMES); ax.set_xticks(range(nf), fr, fontsize=7)
    ax.set_title(f"% of the attacking fist hidden ONLY by the rear arm (limit {TH['fist_rear_steal_max']}%, boxed = flag)", fontsize=10, loc="left")
    for x in (contact[0], faf):
        ax.axvline(x - 0.5, color="k", lw=1.2); ax.axvline(x + 0.5, color="k", lw=1.2)
    for fl in flags:
        if fl["check"] == "rear arm hides the fist":
            ax.add_patch(plt.Rectangle((fl["frame"] - 0.5, VNAMES.index(fl["view"]) - 0.5), 1, 1, fill=False, ec="k", lw=1.5))
    fig.colorbar(im, ax=ax, fraction=0.015, pad=0.005)
    ax = axs[-1]
    xs = np.arange(nf)
    ax.plot(xs, [res["frames"][f]["elbow_r_behind_back_cm"] for f in fr], "-o", ms=3, label="rear elbow behind back plane (cm)")
    for v, st in (("az270", "--"), ("az180", ":")):
        ax.plot(xs, [res["frames"][f]["views"][v]["elbow_r_out_cm"] for f in fr], st, label=f"rear elbow outside torso outline, {v} (cm)")
    ax.plot(xs, [res["frames"][f]["views"]["az315"]["elbow_r_out_cm"] for f in fr], "-.", label="rear elbow outside torso outline, az315 (cm)")
    ax.axhline(TH["elbow_behind_max_cm"], color="r", lw=0.8)
    ax2 = ax.twinx()
    ax2.plot(xs, [res["frames"][f]["curl_l"] for f in fr], color="g", lw=1, label="finger curl hand_l (deg)")
    ax2.plot(xs, [res["frames"][f]["curl_r"] for f in fr], color="m", lw=1, label="finger curl hand_r (deg)")
    ax2.axhline(TH["curl_closed_min_deg"], color="m", lw=0.6, ls="--")
    ax.set_xticks(xs, fr, fontsize=7); ax.set_title("pose checks", fontsize=10, loc="left")
    ax.legend(loc="upper left", fontsize=7); ax2.legend(loc="upper right", fontsize=7)
    out = os.path.join(SHEETS, f"{take}_legibility.png")
    fig.savefig(out, dpi=80); plt.close(fig)
    return out

def report(res, take, flags, contact, faf):
    lines = [f"{take}: legibility, contact frames {contact[0]}-{contact[1]}, FAF {faf}"]
    by = {}
    for fl in flags:
        by.setdefault(fl["check"], []).append(fl)
    lines.append("VERDICT: " + ("PASS" if not flags else "FAIL (" + ", ".join(f"{k}: {len(v)}" for k, v in by.items()) + ")"))
    for k, lst in by.items():
        key = (lambda x: -x.get("steal_pct", 0)) if "rear arm" in k else (lambda x: x.get("fist_distinct_x", 0)) if "contact" in k else \
              (lambda x: -x.get("cm", 0)) if "elbow" in k else (lambda x: x.get("curl_deg", 0))
        lst = sorted(lst, key=key)
        if "rear arm" in k:
            lines.append(f"FAIL {k}: frames {sorted({x['frame'] for x in lst})}, views {sorted({x['view'] for x in lst})}")
        else:
            lines.append(f"FAIL {k}: {len(lst)}")
        for fl in lst[:4]:
            lines.append("   " + json.dumps(fl))
    for f in range(contact[0], contact[1] + 1):
        fr = res["frames"][str(f)]
        gv = game_arm_read(fr)
        for ring, name in (("f", "Follow el 25"), ("h", "rail el 40")):
            rv = {v[1:]: x for v, x in gv.items() if v.startswith(ring)}
            lines.append(f"{name}, frame {f}: attacking arm readable (% of its side view) by camera yaw {rv}; worst (front excluded) "
                         f"{min(((k, x) for k, x in rv.items() if k != '000'), key=lambda t: t[1])}")
        lines.append(f"PvP side, frame {f}: arm readable {[gv[v] for v in PVP_VIEWS]}%, reach out of the body "
                     f"{[fr['views'][v]['limb_out_cm']['l'] for v in PVP_VIEWS]} cm, elbow out {[fr['views'][v]['elbow_out_cm']['l'] for v in PVP_VIEWS]} cm; "
                     f"silhouette width/height Follow behind {fr['views']['f180'].get('sil_w_over_h')} (info)")
    g = res["frames"]["0"]
    lines.append(f"guard (frame 0): rear elbow {g['elbow_r_body_cm'][1]} cm out to the side of the chest centre, "
                 f"{g['views']['az315']['elbow_r_out_cm']} cm outside the torso outline at az315 (info: the guard pose itself)")
    # worst distinct per key part over the move window
    lines.append(f"strike: limb {STRIKE['limb']}, hiders {HIDERS or 'none (rule B off)'}, pose checks {sorted(POSE_CHECKS) or 'none'}")
    for p in ATTS + FORES + [q for q in HIDERS if q not in ATTS + FORES]:
        cells = [(res["frames"][f]["views"][v]["parts"].get(p, {}).get("distinct_x", 100), int(f), v)
                 for f in res["frames"] for v in VNAMES if contact[0] <= int(f) <= faf]
        d, f, v = min(cells)
        pairs = sorted(((k.split("|")[1], val[0], val[1]) for k, val in res["frames"][str(f)]["views"][v]["pairs"].items()
                        if k.startswith(p + "|") and k.split("|")[1] not in NEIGH[p]), key=lambda x: -x[1])[:3]
        lines.append(f"worst {p}: {d}% distinct at f{f} {v}; merges with " + ", ".join(f"{q} {o}% (covered {c}%)" for q, o, c in pairs))
    txt = "\n".join(lines)
    open(os.path.join(SHEETS, f"{take}_legibility.txt"), "w").write(txt)
    return txt


# ---------------------------------------------------------------- spot check against the mesh
def spot(take, frame, vname):
    import ue
    from PIL import Image
    d = json.load(open(os.path.join(RAW, f"{take}_legbones.json")))
    anim = d["anim"]; fr = d["frames"][str(frame)]
    view = VIEWS[VNAMES.index(vname)]
    if view["kind"] != "ortho":
        raise SystemExit("spot check uses the ortho depth capture; pick an az view")
    ue.py("setup")
    want = bone_tf(fr, "hand_r")[0]
    ue.py("set_frame", anim_path=anim, frame=(frame + 5) % 38, hide_arm=False)
    for _ in range(30):
        got = ue.py("set_frame", anim_path=anim, frame=frame, hide_arm=False)["hand_r"]
        if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
            ue.py("set_frame", anim_path=anim, frame=frame, hide_arm=False)
            break
        time.sleep(0.2)
    tag = f"leg_{take}_f{frame}_{vname}"
    ue.py("capture", kind="depth", az=view["az"], el=view["el"], tag=tag)
    mesh = np.fromfile(os.path.join(RAW, tag + ".f32"), dtype=np.float32).reshape(256, 256) < 10000
    depth = render(part_prims(fr), view)
    masks = np.isfinite(depth); U = masks.any(axis=0)
    iou = float((mesh & U).sum()) / max(1, float((mesh | U).sum()))
    # the attacking fist: share of its capsule mask that lies on mesh pixels, and where the rear arm sits
    hl = masks[PNAMES.index("hand_l")]
    on_mesh = float((hl & mesh).sum()) / max(1, float(hl.sum()))
    img = np.full((256, 256, 3), 255, np.uint8)
    img[mesh] = (170, 170, 170)
    img[boundary(U)] = (0, 0, 0)
    ra = masks[[PNAMES.index(q) for q in REAR_ARM]].any(axis=0)
    img[ra & ~hl] = (90, 120, 230)
    img[hl] = (230, 60, 40)
    img[hl & ra] = (150, 0, 150)
    out = os.path.join(SHEETS, f"{take}_legibility_spot_f{frame}_{vname}.png")
    Image.fromarray(img).resize((512, 512), Image.NEAREST).save(out)
    return {"iou_mesh_vs_capsules": round(iou, 3), "fist_mask_on_mesh": round(on_mesh, 3), "png": out}


if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "measure":
        anim, take = sys.argv[2], sys.argv[3]
        contact = tuple(int(x) for x in (sys.argv[4] if len(sys.argv) > 4 else "1-1").split("-"))
        contact = contact if len(contact) == 2 else (contact[0], contact[0])
        faf = int(sys.argv[5]) if len(sys.argv) > 5 else 17
        t0 = time.time()
        configure(take, anim)
        frames = sample(anim, take)
        fit_center(frames)
        jp = os.path.join(SHEETS, f"{take}_legibility.json")
        if os.path.exists(jp) and os.environ.get("LEG_REUSE"):
            res = json.load(open(jp))
        else:
            res = measure(frames); res["anim"] = anim
            json.dump(res, open(jp, "w"))
        res["take"] = take
        flags = judge(res, contact, faf)
        png = heatmap(res, take, contact, faf, flags)
        print(report(res, take, flags, contact, faf))
        print(json.dumps({"png": png, "sec": round(time.time() - t0, 1)}))
    elif mode == "spot":
        print(json.dumps(spot(sys.argv[2], int(sys.argv[3]), sys.argv[4])))
