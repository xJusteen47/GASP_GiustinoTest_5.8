"""Shared helpers of the offline analysis tools (anim_silhouette_check, anim_motion_overlay): joints loading, view
projection, convex hull and small PIL drawing utilities. numpy-free, deterministic.

Joints JSON (runstart_clip_joints_ue.py / clip_joints_dump_ue.py): {"fps", "joints": {name: [[x, y_up, z] per frame]}} in
component space, cm, the character facing +x (UE X forward, z = UE Y = the character's RIGHT). Views are azimuths from the
pawn's forward as in pie_capture_move.py: az0 in front, az90 on its right, az180 behind, az315 front-left."""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any

_HERE = str(Path(__file__).resolve().parent)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import anim_ops  # noqa: E402

DEFAULT_VIEWS = "az90,az180,az315"
TORSO_KEYS = ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "clavicle_l", "clavicle_r",
              "thigh_MainPoint_l", "thigh_MainPoint_r"]
SHOULDER_FALLBACK = {"clavicle_l": "upperarm_MainPoint_l", "clavicle_r": "upperarm_MainPoint_r"}
LIMB_ENDS = {"hand_l": "hand_MainPoint_l", "hand_r": "hand_MainPoint_r", "elbow_l": "lowerarm_MainPoint_l",
             "elbow_r": "lowerarm_MainPoint_r", "foot_l": "foot_MainPoint_l", "foot_r": "foot_MainPoint_r",
             "knee_l": "calf_MainPoint_l", "knee_r": "calf_MainPoint_r", "head": "head"}
BONES = [("pelvis", "spine_01"), ("spine_01", "spine_02"), ("spine_02", "spine_03"), ("spine_03", "spine_04"),
         ("spine_04", "spine_05"), ("spine_05", "neck_01"), ("neck_01", "head")]
for _s in "lr":
    BONES += [("thigh_MainPoint_" + _s, "calf_MainPoint_" + _s), ("calf_MainPoint_" + _s, "foot_MainPoint_" + _s),
              ("foot_MainPoint_" + _s, "ball_MainPoint_" + _s), ("upperarm_MainPoint_" + _s, "lowerarm_MainPoint_" + _s),
              ("lowerarm_MainPoint_" + _s, "hand_MainPoint_" + _s), ("pelvis", "thigh_MainPoint_" + _s),
              ("spine_05", "upperarm_MainPoint_" + _s)]


def load_joints(root: Path, asset: str) -> dict[str, Any]:
    p = anim_ops._joints_path(root, asset)
    if not p.exists():
        raise FileNotFoundError(f"joints file not found: {p} (run runstart_clip_joints_ue.py or clip_joints_dump_ue.py)")
    d = json.loads(p.read_text(encoding="utf-8"))
    if not d.get("joints"):
        raise ValueError(f"empty joints in {p}")
    return d


def parse_views(views: Any) -> list[str]:
    vs = [v.strip() for v in (views.split(",") if isinstance(views, str) else list(views or [])) if str(v).strip()]
    return vs or DEFAULT_VIEWS.split(",")


def view_angles(view: str, game_yaw: float = 0.0, game_pitch: float = 10.0) -> tuple[float, float]:
    """(azimuth deg, camera pitch deg looking down) of a view name; 'game' uses the given yaw/pitch."""
    if view == "game":
        return game_yaw, game_pitch
    if view.startswith("az"):
        return float(view[2:]), 0.0
    raise ValueError(f"unknown view {view!r}: use azNNN or game")


def project(p: list[float], az: float, pitch: float = 0.0) -> tuple[float, float]:
    """3D joint [x, y_up, z] -> screen (u right, v up), cm. Camera on the azimuth side looking at the pawn; pitch looks down."""
    a, pr = math.radians(az), math.radians(pitch)
    u = p[0] * math.sin(a) - p[2] * math.cos(a)
    depth = p[0] * math.cos(a) + p[2] * math.sin(a)   # toward the camera
    return u, p[1] * math.cos(pr) - depth * math.sin(pr)


def point(joints: dict[str, list], key: str, f: int) -> list[float] | None:
    if key in joints and f < len(joints[key]):
        return joints[key][f]
    if key in SHOULDER_FALLBACK and SHOULDER_FALLBACK[key] in joints and f < len(joints[SHOULDER_FALLBACK[key]]):
        return joints[SHOULDER_FALLBACK[key]][f]
    return None


def hull(pts: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """Convex hull (Andrew monotone chain), counter-clockwise; degenerate inputs return the unique points."""
    P = sorted(set(pts))
    if len(P) <= 2:
        return P

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lo: list = []
    for p in P:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    up: list = []
    for p in reversed(P):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]


def area(poly: list[tuple[float, float]]) -> float:
    if len(poly) < 3:
        return 0.0
    return abs(sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                   for i in range(len(poly)))) / 2.0


def _seg_dist2(p, a, b) -> float:
    dx, dy = b[0] - a[0], b[1] - a[1]
    dd = dx * dx + dy * dy
    t = 0.0 if dd < 1e-12 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / dd))
    return math.hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy))


def signed_dist(p: tuple[float, float], poly: list[tuple[float, float]]) -> float:
    """Distance from p to the polygon outline, negative inside (degenerate polygon: distance to the point/segment)."""
    if not poly:
        return float("inf")
    if len(poly) == 1:
        return math.hypot(p[0] - poly[0][0], p[1] - poly[0][1])
    edges = [(poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly))] if len(poly) > 2 else [(poly[0], poly[1])]
    d = min(_seg_dist2(p, a, b) for a, b in edges)
    if len(poly) > 2:
        inside = all((b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]) >= 0 for a, b in edges)
        if inside:
            return -d
    return d


def font(size: int = 12):
    from PIL import ImageFont
    try:
        return ImageFont.load_default(size=size)
    except TypeError:   # Pillow < 10.1
        return ImageFont.load_default()


def out_dir(root: Path, asset: str) -> Path:
    d = Path(root) / "Saved" / "AnimPipeline" / "analysis" / asset.rsplit("/", 1)[-1]
    d.mkdir(parents=True, exist_ok=True)
    return d
