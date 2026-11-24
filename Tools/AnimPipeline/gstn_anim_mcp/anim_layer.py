"""Pose layers for gstn-anim-mcp (`anim_layer`): the agent writes INTENT, this module does the quaternions.

Pure Python (no `unreal`): importable and unit-testable offline. The UE side is anim_layer_ue.py, which dumps the clip's
local transforms, calls apply_stack() and writes back only the rotation tracks of the edited bones.

Why. The w3 run start was polished in 8 small steps, each one "a part, an axis in the body, a value, the peak frame, how it
comes in and goes out" over a DENSE clip (docs/notes/reviews/runstart-w3-c*/passo-*). Every step was hand-written
quaternion code in runstart_w3_ue.py. A layer is that step as data, so a cheap model only picks the intent:

  {"part": "elbow_r", "motion": "out", "deg": 10, "start": 1, "peak": [5, 13], "end": 21}

  = the right elbow goes 10 deg further out (the upper arm rotates in the body's frontal plane), coming in smoothly over
    f1..f5, full value f5..f13, going out over f13..f21. Frames are CLIP-LOCAL (0..n-1).

Rules that make a layer safe by construction (not by a later check):
- Envelope = smoothstep(start..p0) * (1 - smoothstep(p1..end)), the exact curve of the w3 steps. It is 0 AT start and AT end,
  so those frames and everything outside are bit-identical (anchors stay anchored), and it is C1: no step key, ever.
  A ramp shorter than 2 frames is refused (that IS a step), except at the clip's first/last frame.
- A layer is a world-space rotation of the bone about ITS joint (FK): children follow, bone lengths cannot change, and only
  the local rotation track of the edited bone is rewritten (one value per frame of the dense clip; no keys are added).
- Upper body only: root, pelvis and legs are refused (rotating them would slide the feet; use anim_run_intent's IK params).
- Axes come from the BODY at each frame (left = hip line thigh_l - thigh_r, up = component Z, forward = up x left), not from
  pelvis travel, so they are right on in-place moves too. frame="chest" takes the left axis from the shoulders instead;
  frame="clip" uses the asset axes (X left, Y forward, Z up), which is what runstart_w3_ue.py hard-coded.
- The direction of a motion ("out", "forward", "chin_up") is resolved ONCE per layer at its first peak frame, by which of
  +deg / -deg moves the reference vector toward the target direction, then kept for the whole window: a sign cannot flip
  mid-layer and make a pop. flex/extend are the exception (hinge axis per frame, always toward/away from the shoulder).
- Layers stack in order; each is applied to the pose produced by the ones before, so undo = rebuild without the last one.

Quaternions are (x, y, z, w) tuples, Hamilton product (a*b applies b first), the same convention as unreal.Quat.
World child rotation = parent_world * local; a world delta D on a bone gives local' = inv(parent_world) * D * world.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
from typing import Any, Sequence

from follow_through import IDENT, qexp, qinv, qmul, ss  # noqa: F401  (shared quaternion helpers, one implementation)

Q = tuple
V = tuple

# Mannequin hierarchy needed for FK of the editable bones and of the reference joints. Bones absent from a clip are skipped
# and their children re-parented to the nearest present ancestor (resolve_parents); the UE side verifies FK against the
# engine's own component-space pose, so a wrong row here is an error, never a silent bad edit.
PARENTS: dict[str, str] = {
    "pelvis": "root",
    "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03", "spine_05": "spine_04",
    "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02",
    "thigh_l": "pelvis", "calf_l": "thigh_l", "foot_l": "calf_l", "ball_l": "foot_l",
    "thigh_r": "pelvis", "calf_r": "thigh_r", "foot_r": "calf_r", "ball_r": "foot_r",
}
for _s in ("l", "r"):
    PARENTS.update({
        f"clavicle_{_s}": "spine_05", f"upperarm_{_s}": f"clavicle_{_s}", f"lowerarm_{_s}": f"upperarm_{_s}",
        f"hand_{_s}": f"lowerarm_{_s}", f"middle_metacarpal_{_s}": f"hand_{_s}",
    })
FK_BONES: tuple[str, ...] = ("root",) + tuple(PARENTS)

EDITABLE = re.compile(r"^(spine_0[1-5]|neck_0[12]|head|clavicle_[lr]|upperarm_[lr]|lowerarm_[lr]|hand_[lr])$")
REFUSED = re.compile(r"^(root|pelvis|thigh_[lr]|calf_[lr]|foot_[lr]|ball_[lr]|ik_.*)$")
# reference child that gives a bone its direction (the joint it moves); head has none (uses the body up axis)
DIR_CHILD = {
    "spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "spine_04", "spine_04": "spine_05", "spine_05": "neck_01",
    "neck_01": "neck_02", "neck_02": "head",
}
for _s in ("l", "r"):
    DIR_CHILD.update({f"clavicle_{_s}": f"upperarm_{_s}", f"upperarm_{_s}": f"lowerarm_{_s}",
                      f"lowerarm_{_s}": f"hand_{_s}", f"hand_{_s}": f"middle_metacarpal_{_s}"})

# part -> [(bone, weight)]. "{s}" is the side. Weighted parts spread the degrees over a chain (spine like geom_intent_ue).
PARTS: dict[str, list[tuple[str, float]]] = {
    "spine": [("spine_01", 0.40), ("spine_02", 0.35), ("spine_03", 0.25)],
    "chest": [("spine_04", 0.5), ("spine_05", 0.5)],
    "torso": [("spine_01", 0.2), ("spine_02", 0.2), ("spine_03", 0.2), ("spine_04", 0.2), ("spine_05", 0.2)],
    "neck": [("neck_01", 1.0)],
    "head": [("head", 1.0)],
    "neck_head": [("neck_01", 0.5), ("head", 0.5)],
    "shoulder_{s}": [("clavicle_{s}", 1.0)],
    "arm_{s}": [("upperarm_{s}", 1.0)],
    "elbow_{s}": [("upperarm_{s}", 1.0)],          # moving the elbow = rotating the upper arm (flex/extend -> lowerarm)
    "forearm_{s}": [("lowerarm_{s}", 1.0)],
    "wrist_{s}": [("hand_{s}", 1.0)],
    "hand_{s}": [("hand_{s}", 1.0)],
}
PART_ALIASES = {   # Italian words the owner's notes use; flash models copy them
    "schiena": "spine", "busto": "torso", "petto": "chest", "torace": "chest", "collo": "neck", "testa": "head",
    "spalla": "shoulder", "braccio": "arm", "gomito": "elbow", "avambraccio": "forearm", "polso": "wrist", "mano": "hand",
    "clavicle": "shoulder", "upperarm": "arm", "lowerarm": "forearm",
}

# motion -> (axis, toward, ref). axis is a body axis; the bone rotates about it so that `ref` moves toward `toward`.
MOTIONS: dict[str, tuple[str, str, str]] = {
    "out": ("fwd", "out", "bone"),            # frontal plane, away from the midline (abduction for arms)
    "in": ("fwd", "in", "bone"),
    "forward": ("lat", "fwd", "bone"),         # sagittal swing; for spine/neck = lean forward
    "back": ("lat", "back", "bone"),
    "turn_out": ("up", "out", "bone"),         # about the vertical axis (yaw), tip toward outside
    "turn_in": ("up", "in", "bone"),
    "turn_left": ("up", "left", "face"),       # face turns to the character's left (spine, neck, head)
    "turn_right": ("up", "right", "face"),
    "tilt_left": ("fwd", "left", "bone"),      # side bend; for head the reference is the body up axis
    "tilt_right": ("fwd", "right", "bone"),
    "chin_up": ("lat", "up", "face"),          # face looks up (head/neck pitch)
    "chin_down": ("lat", "down", "face"),
    "flex": ("hinge", "flex", "bone"),         # lowerarm only: elbow bends more
    "extend": ("hinge", "extend", "bone"),
}
MOTION_ALIASES = {
    "fuori": "out", "dentro": "in", "avanti": "forward", "indietro": "back", "lean_forward": "forward",
    "lean_back": "back", "piega": "flex", "stendi": "extend", "mento_su": "chin_up", "mento_giu": "chin_down",
    "abduct": "out", "adduct": "in",
}
AXES = ("lat", "fwd", "up")
TOWARDS = ("out", "in", "fwd", "back", "up", "down", "left", "right")
FRAMES = ("body", "chest", "clip")

LAYER_KEYS = {"part", "bones", "motion", "axis", "toward", "ref", "deg", "start", "peak", "end", "frame", "note", "id"}
MAX_DEG = 45.0
MIN_RAMP = 2              # frames: shorter is a step key
RATE_WARN_DEG = 6.0       # added deg per frame: a MEASURE shown to the reviewer (owner 2A), not a block
EXAMPLE = '{"part": "elbow_r", "motion": "out", "deg": 10, "start": 1, "peak": [5, 13], "end": 21}'


# ---------------------------------------------------------------- vector / quaternion helpers
def vsub(a: V, b: V) -> V:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def vadd(a: V, b: V) -> V:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def vmul(a: V, s: float) -> V:
    return (a[0] * s, a[1] * s, a[2] * s)


def vdot(a: V, b: V) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vcross(a: V, b: V) -> V:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def vlen(a: V) -> float:
    return math.sqrt(vdot(a, a))


def vnorm(a: V) -> V:
    n = vlen(a)
    return (0.0, 0.0, 0.0) if n < 1e-12 else (a[0] / n, a[1] / n, a[2] / n)


def qrot(q: Q, v: V) -> V:
    """Rotate v by unit quaternion q (same as unreal.Quat.rotate_vector)."""
    u = (q[0], q[1], q[2])
    s = q[3]
    t = vmul(vcross(u, v), 2.0)
    return vadd(vadd(v, vmul(t, s)), vcross(u, t))


def qnorm(q: Q) -> Q:
    n = math.sqrt(sum(c * c for c in q))
    return IDENT if n < 1e-12 else (q[0] / n, q[1] / n, q[2] / n, q[3] / n)


def axis_quat(axis: V, deg: float) -> Q:
    a = vnorm(axis)
    return qexp(vmul(a, math.radians(deg)))


def q_angle(q: Q) -> float:
    """Rotation angle of q in degrees (atan2 form: exact near 0, where acos loses ~1e-6 deg)."""
    return math.degrees(2.0 * math.atan2(math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]), abs(q[3])))


def qdiff_deg(a: Q, b: Q) -> float:
    return q_angle(qmul(a, qinv(b)))


# ---------------------------------------------------------------- validation
def _side(name: str) -> str | None:
    m = re.search(r"_([lr])$", name)
    return m.group(1) if m else None


def resolve_part(part: str, motion: str | None) -> list[tuple[str, float]]:
    """part name (English, Italian or a bone) -> [(bone, weight)]."""
    p = str(part).strip().lower().replace(" ", "_").replace("-", "_")
    side = _side(p)
    stem = p[:-2] if side else p
    stem = PART_ALIASES.get(stem, stem)
    if side is None and stem in ("shoulder", "arm", "elbow", "forearm", "wrist", "hand"):
        raise ValueError(f"part {part!r} needs a side: {stem}_l or {stem}_r")
    if side and stem == "elbow" and motion in ("flex", "extend"):
        return [(f"lowerarm_{side}", 1.0)]
    key = f"{stem}_{{s}}" if side else stem
    if key in PARTS:
        return [(b.replace("{s}", side or ""), w) for b, w in PARTS[key]]
    bone = f"{stem}_{side}" if side else stem
    if REFUSED.match(bone):
        raise ValueError(f"part {part!r} ({bone}) is not editable with layers: root, pelvis and legs would slide the feet. "
                         "Use anim_run_intent (knee_drop, catch_fwd_cm, pelvis_*: 2-bone IK keeps the feet planted)")
    if EDITABLE.match(bone):
        return [(bone, 1.0)]
    raise ValueError(f"unknown part {part!r}. Parts: spine, chest, torso, neck, head, neck_head, shoulder_l/r, arm_l/r, "
                     "elbow_l/r, forearm_l/r, wrist_l/r (or an upper-body bone name)")


def _int(layer: dict, key: str) -> int:
    v = layer.get(key)
    if isinstance(v, bool) or not isinstance(v, (int, float)) or float(v) != int(v):
        raise ValueError(f"{key} must be a whole clip-local frame number, got {v!r}. Example: {EXAMPLE}")
    return int(v)


def validate_layer(layer: Any, n: int | None = None) -> dict[str, Any]:
    """Normalize one layer. Raises ValueError with a message a flash model can act on."""
    if not isinstance(layer, dict):
        raise ValueError(f"a layer is a JSON object. Example: {EXAMPLE}")
    unknown = sorted(set(layer) - LAYER_KEYS)
    if unknown:
        raise ValueError(f"unknown layer keys {unknown}; allowed: {sorted(LAYER_KEYS)}")
    motion = layer.get("motion")
    if motion is not None:
        motion = MOTION_ALIASES.get(str(motion).strip().lower(), str(motion).strip().lower())
        if motion not in MOTIONS:
            raise ValueError(f"unknown motion {layer['motion']!r}; motions: {sorted(MOTIONS)}")
        axis, toward, ref = MOTIONS[motion]
        if any(k in layer for k in ("axis", "toward")):
            raise ValueError("give either motion, or axis + toward (+ ref), not both")
    else:
        axis, toward, ref = layer.get("axis"), layer.get("toward"), layer.get("ref", "bone")
        if axis not in AXES or toward not in TOWARDS or ref not in ("bone", "face"):
            raise ValueError(f"without motion give axis in {AXES}, toward in {TOWARDS}, ref bone|face; or a motion: {sorted(MOTIONS)}")
    if ("part" in layer) == ("bones" in layer):
        raise ValueError(f"give exactly one of part (e.g. 'elbow_r') or bones ({{'upperarm_r': 1.0}}). Example: {EXAMPLE}")
    if "part" in layer:
        bones = resolve_part(layer["part"], motion)
    else:
        if not isinstance(layer["bones"], dict) or not layer["bones"]:
            raise ValueError("bones must be an object {bone: weight}")
        bones = []
        for b, w in layer["bones"].items():
            if not EDITABLE.match(b):
                raise ValueError(f"bone {b!r} is not editable with layers (upper body only: spine, neck, head, clavicle, "
                                 "upperarm, lowerarm, hand)")
            if isinstance(w, bool) or not isinstance(w, (int, float)) or not (0.0 < float(w) <= 1.0):
                raise ValueError(f"weight of {b} must be in (0, 1]")
            bones.append((b, float(w)))
    for b, _ in bones:
        if toward in ("out", "in") and _side(b) is None:
            raise ValueError(f"{b} is on the midline: 'out'/'in' have no meaning there, use left/right (tilt_left, turn_right...)")
        if axis == "hinge" and not b.startswith("lowerarm_"):
            raise ValueError(f"flex/extend bend the elbow: use part elbow_l/elbow_r or forearm_*, not {b}")
    deg = layer.get("deg")
    if isinstance(deg, bool) or not isinstance(deg, (int, float)) or not math.isfinite(float(deg)):
        raise ValueError(f"deg must be a number (degrees, signed: negative = the opposite motion). Example: {EXAMPLE}")
    deg = float(deg)
    if abs(deg) > MAX_DEG:
        raise ValueError(f"deg={deg} is more than {MAX_DEG}: a layer is a correction, not a new pose (split it, or author a new card)")
    for k in ("start", "peak", "end"):
        if k not in layer:
            raise ValueError(f"missing {k}. A layer needs start, peak (frame or [first, last]) and end. Example: {EXAMPLE}")
    start, end = _int(layer, "start"), _int(layer, "end")
    pk = layer["peak"]
    if isinstance(pk, (list, tuple)):
        if len(pk) != 2:
            raise ValueError("peak is a frame or [first, last] of the full-value plateau")
        p0, p1 = _int({"peak": pk[0]}, "peak"), _int({"peak": pk[1]}, "peak")
    else:
        p0 = p1 = _int(layer, "peak")
    if not (0 <= start <= p0 <= p1 <= end):
        raise ValueError(f"need 0 <= start <= peak <= end, got start={start} peak=[{p0}, {p1}] end={end}")
    if n is not None and end > n - 1:
        raise ValueError(f"end={end} is outside the clip: frames are clip-local 0..{n - 1} (n={n}), not capture v-frames")
    clip_first = start == 0
    clip_last = n is not None and end == n - 1
    if p0 - start < MIN_RAMP and not (clip_first and p0 == start):
        raise ValueError(f"ramp in start={start}..peak={p0} is {p0 - start} frame(s): at least {MIN_RAMP}, or it is a step key")
    if end - p1 < MIN_RAMP and not (clip_last and p1 == end):
        raise ValueError(f"ramp out peak={p1}..end={end} is {end - p1} frame(s): at least {MIN_RAMP}, or it is a step key")
    frame = layer.get("frame", "body")
    if frame not in FRAMES:
        raise ValueError(f"frame must be one of {FRAMES}")
    out = {"bones": bones, "motion": motion, "axis": axis, "toward": toward, "ref": ref, "deg": deg,
           "start": start, "peak": [p0, p1], "end": end, "frame": frame}
    if "part" in layer:
        out["part"] = str(layer["part"])
    if "note" in layer:
        out["note"] = str(layer["note"])[:200]
    return out


def layer_hash(norm: dict[str, Any]) -> str:
    return hashlib.sha1(json.dumps({k: norm[k] for k in sorted(norm) if k not in ("note", "id")}, sort_keys=True).encode()).hexdigest()[:10]


# ---------------------------------------------------------------- envelope
def envelope(f: float, start: int, p0: int, p1: int, end: int) -> float:
    """w3 curve: smoothstep in over start..p0, 1 on p0..p1, smoothstep out over p1..end; 0 at and outside start/end
    (a zero-length ramp, allowed only at the clip's first/last frame, means full value there)."""
    if f < start or f > end:
        return 0.0
    up = 1.0 if p0 == start else ss(float(start), float(p0), float(f))
    dn = 0.0 if p1 == end else ss(float(p1), float(end), float(f))
    return up * (1.0 - dn)


def envelope_series(norm: dict[str, Any], n: int) -> list[float]:
    p0, p1 = norm["peak"]
    return [envelope(f, norm["start"], p0, p1, norm["end"]) for f in range(n)]


def max_rate(norm: dict[str, Any], n: int) -> tuple[float, int]:
    """Largest added degrees per frame (and where) for the strongest bone of the layer."""
    e = envelope_series(norm, n)
    w = max(wt for _, wt in norm["bones"])
    best, at = 0.0, norm["start"]
    for f in range(1, n):
        r = abs(norm["deg"]) * w * abs(e[f] - e[f - 1])
        if r > best:
            best, at = r, f
    return round(best, 2), at


# ---------------------------------------------------------------- skeleton / FK / body frame
def resolve_parents(present: Sequence[str]) -> dict[str, str]:
    have = set(present)
    if "root" not in have:
        raise ValueError("the clip pose has no root bone")
    out: dict[str, str] = {}
    for b, p in PARENTS.items():
        if b not in have:
            continue
        while p not in have and p != "root":
            p = PARENTS.get(p, "root")
        out[b] = p
    return out


def fk(local: dict[str, tuple[V, Q]], parents: dict[str, str]) -> dict[str, tuple[V, Q]]:
    """Component-space (pos, rot) of every bone in `parents` (+ root). Scale assumed 1 (mannequin)."""
    world: dict[str, tuple[V, Q]] = {"root": local["root"]}

    def get(b: str) -> tuple[V, Q]:
        if b in world:
            return world[b]
        pp, pq = get(parents[b])
        t, q = local[b]
        world[b] = (vadd(pp, qrot(pq, t)), qnorm(qmul(pq, q)))
        return world[b]

    for b in parents:
        get(b)
    return world


def body_axes(world: dict[str, tuple[V, Q]], kind: str = "body") -> tuple[V, V, V]:
    """(left, forward, up) unit vectors. body: hip line; chest: shoulder line; clip: asset axes X left, Y fwd, Z up."""
    up = (0.0, 0.0, 1.0)
    if kind == "clip":
        return (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), up
    a, b = ("upperarm_l", "upperarm_r") if kind == "chest" else ("thigh_l", "thigh_r")
    if a not in world or b not in world:
        raise ValueError(f"frame {kind!r} needs {a} and {b} in the clip")
    d = vsub(world[a][0], world[b][0])
    lat = vnorm((d[0], d[1], 0.0))
    if vlen(lat) < 0.5:
        raise ValueError(f"frame {kind!r}: {a}-{b} line is vertical, no left axis; use frame 'clip'")
    return lat, vcross(up, lat), up


def _dir(world: dict[str, tuple[V, Q]], bone: str, up: V) -> V:
    child = DIR_CHILD.get(bone)
    if child and child in world:
        return vsub(world[child][0], world[bone][0])
    if bone.startswith("hand_"):   # no metacarpal in the clip: forearm direction
        return vsub(world[bone][0], world[PARENTS[bone]][0])
    return vmul(up, 10.0)          # head


def _toward_vec(toward: str, bone: str, lat: V, fwd: V, up: V) -> V:
    s = 1.0 if _side(bone) == "l" else -1.0
    return {
        "out": vmul(lat, s), "in": vmul(lat, -s), "fwd": fwd, "back": vmul(fwd, -1.0), "up": up,
        "down": vmul(up, -1.0), "left": lat, "right": vmul(lat, -1.0),
    }[toward]


def _pick_sign(axis: V, deg: float, ref: V, target: V) -> float:
    """+1/-1: which rotation of |deg| about axis moves ref more toward target (signed_axis_quat in geom_intent_ue)."""
    a = abs(deg) if abs(deg) > 1e-6 else 1.0
    dp = vdot(vsub(qrot(axis_quat(axis, a), ref), ref), target)
    dn = vdot(vsub(qrot(axis_quat(axis, -a), ref), ref), target)
    return 1.0 if dp >= dn else -1.0


def _layer_axis(norm: dict[str, Any], bone: str, world: dict, lat: V, fwd: V, up: V) -> tuple[V, V, V]:
    """(axis, ref vector, target direction) of a layer on one bone at one frame."""
    ref = _dir(world, bone, up) if norm["ref"] == "bone" else fwd
    if norm["axis"] == "hinge":
        upper = vsub(world[bone][0], world[PARENTS[bone]][0])      # shoulder -> elbow
        fore = ref                                                 # elbow -> wrist
        hinge = vcross(upper, fore)
        if vlen(hinge) < 0.05 * vlen(upper) * vlen(fore):
            hinge = lat                                            # straight arm: bend in the sagittal plane
        target = vmul(vnorm(upper), -1.0 if norm["toward"] == "flex" else 1.0)
        return hinge, fore, target
    axis = {"lat": lat, "fwd": fwd, "up": up}[norm["axis"]]
    return axis, ref, _toward_vec(norm["toward"], bone, lat, fwd, up)


# ---------------------------------------------------------------- apply
def clip_from_dump(dump: dict[str, Any]) -> tuple[int, dict[str, list[tuple[V, Q]]]]:
    """Dump format (anim_layer_ue.py --dump): {"n": N, "local": {bone: [[tx,ty,tz,qx,qy,qz,qw], ...]}}."""
    local = {b: [((r[0], r[1], r[2]), qnorm((r[3], r[4], r[5], r[6]))) for r in rows] for b, rows in dump["local"].items()}
    n = int(dump.get("n") or len(next(iter(local.values()))))
    for b, rows in local.items():
        if len(rows) != n:
            raise ValueError(f"bone {b} has {len(rows)} frames, clip has {n}")
    return n, local


def apply_stack(n: int, local: dict[str, list[tuple[V, Q]]], layers: Sequence[dict[str, Any]]) -> tuple[dict[str, list[Q]], dict[str, Any]]:
    """Apply normalized layers in order. Returns (new local rotations of the edited bones, report).

    Report per layer and bone: degrees reached at the first peak frame, active frames, the moved joint's displacement in
    cm along the body axes at that frame (what the instruction asks: "elbow 5 cm further out"), max added deg/frame."""
    parents = resolve_parents(list(local))
    for nl in layers:
        for b, _ in nl["bones"]:
            if b not in parents:
                raise ValueError(f"bone {b} is not in this clip")
    cur = {b: list(rows) for b, rows in local.items()}          # (t, q) per frame, updated layer after layer
    edited: set[str] = set()
    rep_layers: list[dict[str, Any]] = []
    for li, nl in enumerate(layers):
        env = envelope_series(nl, n)
        p0 = nl["peak"][0]
        w_pk = fk({b: cur[b][p0] for b in cur}, parents)
        lat, fwd, up = body_axes(w_pk, nl["frame"])
        signs = {}
        for b, wt in nl["bones"]:
            ax, ref, tgt = _layer_axis(nl, b, w_pk, lat, fwd, up)
            signs[b] = _pick_sign(ax, nl["deg"] * wt, ref, tgt) * (1.0 if nl["deg"] >= 0.0 else -1.0)
        moved_joint = {b: DIR_CHILD.get(b) for b, _ in nl["bones"]}
        pre = {b: w_pk[moved_joint[b]][0] if moved_joint[b] in w_pk else None for b, _ in nl["bones"]}
        flips = 0
        for f in range(n):
            if env[f] <= 0.0:
                continue
            # bones of one layer are applied root-to-tip so a child's axis sees its parent's new pose
            for b, wt in sorted(nl["bones"], key=lambda x: FK_BONES.index(x[0])):
                w = fk({k: cur[k][f] for k in cur}, parents)
                fl, ff, fu = body_axes(w, nl["frame"])
                ax, ref, tgt = _layer_axis(nl, b, w, fl, ff, fu)
                if nl["axis"] == "hinge":
                    sgn = _pick_sign(ax, nl["deg"] * wt, ref, tgt) * (1.0 if nl["deg"] >= 0.0 else -1.0)
                    if f == p0 and sgn != signs[b]:
                        flips += 1
                else:
                    sgn = signs[b]
                d = axis_quat(ax, sgn * abs(nl["deg"]) * wt * env[f])
                pq = w[parents[b]][1]
                t, _ = cur[b][f]
                newq = qnorm(qmul(qinv(pq), qmul(d, w[b][1])))
                cur[b][f] = (t, newq)
                edited.add(b)
        w_after = fk({b: cur[b][p0] for b in cur}, parents)
        bones_rep = []
        for b, wt in nl["bones"]:
            j = moved_joint[b]
            disp = None
            if j and pre[b] is not None:
                dv = vsub(w_after[j][0], pre[b])
                disp = {"joint": j, "left_cm": round(vdot(dv, lat), 2), "fwd_cm": round(vdot(dv, fwd), 2),
                        "up_cm": round(vdot(dv, up), 2)}
                s = _side(b)
                if s:
                    disp["out_cm"] = round(vdot(dv, lat) * (1.0 if s == "l" else -1.0), 2)
            bones_rep.append({"bone": b, "deg_at_peak": round(abs(nl["deg"]) * wt * env[p0], 2), "sign": int(signs[b]),
                              "moved_joint_at_peak": disp})
        rate, rate_f = max_rate(nl, n)
        active = [f for f in range(n) if env[f] > 0.0]
        rl = {"layer": li + 1, "id": nl.get("id"), "motion": nl.get("motion") or f"{nl['axis']}->{nl['toward']}",
              "frames": [active[0], active[-1]] if active else None, "peak": nl["peak"], "bones": bones_rep,
              "max_added_deg_per_frame": rate, "max_rate_frame": rate_f, "warnings": []}
        if rate > RATE_WARN_DEG:
            rl["warnings"].append(f"adds {rate} deg/frame at f{rate_f} (> {RATE_WARN_DEG}): may read as a pop, lengthen the ramp")
        if flips:
            rl["warnings"].append("elbow almost straight at the peak: flex/extend direction is ambiguous there")
        rep_layers.append(rl)
    new = {b: [cur[b][f][1] for f in range(n)] for b in sorted(edited)}
    return new, {"layers": rep_layers, "edited_bones": sorted(edited)}


def conservation(n: int, local: dict[str, list[tuple[V, Q]]], new: dict[str, list[Q]], layers: Sequence[dict[str, Any]],
                 tol_deg: float = 1e-3) -> dict[str, Any]:
    """On the DATA: frames outside every layer window identical, no refused bone touched, root/pelvis/legs positions
    identical on every frame (they are not descendants of an editable bone, so FK must leave them bit-equal)."""
    windows = [(nl["start"], nl["end"]) for nl in layers]
    outside = 0.0
    for b, qs in new.items():
        for f in range(n):
            if any(a < f < e for a, e in windows):
                continue
            outside = max(outside, qdiff_deg(qs[f], local[b][f][1]))
    refused = sorted(b for b in new if not EDITABLE.match(b))
    parents = resolve_parents(list(local))
    legs_moved = 0.0
    watch = [b for b in ("pelvis", "foot_l", "foot_r", "ball_l", "ball_r") if b in parents]
    for f in range(n):
        w0 = fk({b: local[b][f] for b in local}, parents)
        w1 = fk({b: ((local[b][f][0], new[b][f]) if b in new else local[b][f]) for b in local}, parents)
        for b in watch:
            legs_moved = max(legs_moved, vlen(vsub(w0[b][0], w1[b][0])))
    ok = outside <= tol_deg and not refused and legs_moved <= 1e-6
    return {"ok": ok, "outside_windows_max_diff_deg": round(outside, 5), "refused_bones": refused,
            "pelvis_feet_max_move_cm": round(legs_moved, 6)}


def vdot4(a: Q, b: Q) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]


# ---------------------------------------------------------------- dry run without clip data
def plan(layers: Sequence[dict[str, Any]], n: int | None) -> dict[str, Any]:
    """No clip data: bones, active frames, degrees at the peak and the max added deg/frame of each layer."""
    nn = n if n is not None else max(nl["end"] for nl in layers) + 1
    out = []
    for i, nl in enumerate(layers):
        rate, at = max_rate(nl, nn)
        e = envelope_series(nl, nn)
        active = [f for f in range(nn) if e[f] > 0.0]
        out.append({
            "layer": i + 1, "motion": nl.get("motion") or f"{nl['axis']}->{nl['toward']}", "axis": nl["axis"],
            "toward": nl["toward"], "frame": nl["frame"], "frames": [active[0], active[-1]] if active else None,
            "peak": nl["peak"], "bones": [{"bone": b, "deg_at_peak": round(abs(nl["deg"]) * w, 2)} for b, w in nl["bones"]],
            "max_added_deg_per_frame": rate, "max_rate_frame": at,
            "warnings": [f"adds {rate} deg/frame at f{at}: may read as a pop"] if rate > RATE_WARN_DEG else [],
        })
    return {"n_frames": nn, "n_frames_assumed": n is None, "layers": out,
            "note": "no clip dump: joint displacements in cm need the dump (anim_layer action=dump, once per clip, in UE)"}
