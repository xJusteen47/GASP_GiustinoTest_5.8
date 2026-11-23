"""Unified UE 5.9 headless geometric engine for gstn-anim-mcp (anim_apply_intent).

Pure-Python math primitives (Vec3/Quat, axes, 2-bone IK, envelopes, v1->v2 intent
mapping, tuck_in_quat, signed_axis_quat_directed) stay importable and unit-testable
outside Unreal Engine; inside UE, `_vec`/`_quat` return native `unreal.Vector`/
`unreal.Quat` and `main()` applies character-anchored edits to AnimSequences.
"""
from __future__ import annotations

import json
import math
import os
import sys

try:
    import unreal
except ImportError:
    unreal = None

FOLDER = "/Game/GSTN/Animation/RunStart"


class Vec3:
    """Plain 3-vector, attribute-compatible with unreal.Vector (x/y/z)."""

    __slots__ = ("x", "y", "z")

    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None:
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)

    def normal(self) -> "Vec3":
        l = math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
        if l < 1e-12:
            return Vec3(0.0, 0.0, 0.0)
        return Vec3(self.x / l, self.y / l, self.z / l)

    def __repr__(self) -> str:
        return f"Vec3({self.x!r}, {self.y!r}, {self.z!r})"


class Quat:
    """Unit quaternion (x, y, z, w) with Rodrigues rotate_vector and Hamilton product."""

    __slots__ = ("x", "y", "z", "w")

    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0, w: float = 1.0) -> None:
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
        self.w = float(w)

    def normal(self) -> "Quat":
        l = math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z + self.w * self.w)
        if l < 1e-12:
            return Quat(0.0, 0.0, 0.0, 1.0)
        return Quat(self.x / l, self.y / l, self.z / l, self.w / l)

    def rotate_vector(self, v) -> Vec3:
        u = Vec3(self.x, self.y, self.z)
        s = self.w
        uxv = Vec3(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x)
        uxuxv = Vec3(u.y * uxv.z - u.z * uxv.y, u.z * uxv.x - u.x * uxv.z, u.x * uxv.y - u.y * uxv.x)
        return Vec3(
            v.x + 2.0 * s * uxv.x + 2.0 * uxuxv.x,
            v.y + 2.0 * s * uxv.y + 2.0 * uxuxv.y,
            v.z + 2.0 * s * uxv.z + 2.0 * uxuxv.z,
        )

    def __mul__(self, other):
        if isinstance(other, Quat):
            return Quat(
                self.w * other.x + self.x * other.w + self.y * other.z - self.z * other.y,
                self.w * other.y - self.x * other.z + self.y * other.w + self.z * other.x,
                self.w * other.z + self.x * other.y - self.y * other.x + self.z * other.w,
                self.w * other.w - self.x * other.x - self.y * other.y - self.z * other.z,
            )
        s = float(other)
        return Quat(self.x * s, self.y * s, self.z * s, self.w * s)

    def __repr__(self) -> str:
        return f"Quat({self.x!r}, {self.y!r}, {self.z!r}, {self.w!r})"


def _vec(x: float, y: float, z: float):
    if unreal is not None:
        return unreal.Vector(float(x), float(y), float(z))
    return Vec3(x, y, z)


def _quat(x: float, y: float, z: float, w: float):
    if unreal is not None:
        return unreal.Quat(float(x), float(y), float(z), float(w))
    return Quat(x, y, z, w)


def ss(a: float, b: float, x: float) -> float:
    if x <= a:
        return 0.0
    if x >= b:
        return 1.0
    t = (x - a) / float(b - a)
    return t * t * (3.0 - 2.0 * t)


def _window_weight(f: int, f0: int, f1: int, smooth=None) -> float:
    if f < f0 or f > f1:
        return 0.0
    if not smooth:
        return 1.0
    s0, s1 = int(smooth[0]), int(smooth[1])
    w = 1.0
    if s0 > f0 and f <= s0:
        w = min(w, ss(float(f0), float(s0), float(f)))
    if s1 < f1 and f >= s1:
        w = min(w, 1.0 - ss(float(s1), float(f1), float(f)))
    return w


def vsub(a, b):
    return _vec(a.x - b.x, a.y - b.y, a.z - b.z)


def vadd(a, b):
    return _vec(a.x + b.x, a.y + b.y, a.z + b.z)


def vmul(a, s: float):
    return _vec(a.x * s, a.y * s, a.z * s)


def vlen(a) -> float:
    return math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z)


def vnorm(a):
    l = vlen(a)
    if l <= 1e-6:
        return _vec(0.0, 1.0, 0.0)
    return _vec(a.x / l, a.y / l, a.z / l)


def vdot(a, b) -> float:
    return a.x * b.x + a.y * b.y + a.z * b.z


def vcross(a, b):
    return _vec(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    )


def qinv(q):
    return _quat(-q.x, -q.y, -q.z, q.w)


def qnlerp(a, b, t: float):
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    s = -1.0 if dot < 0.0 else 1.0
    x = a.x * (1.0 - t) + s * b.x * t
    y = a.y * (1.0 - t) + s * b.y * t
    z = a.z * (1.0 - t) + s * b.z * t
    w = a.w * (1.0 - t) + s * b.w * t
    l = math.sqrt(x * x + y * y + z * z + w * w)
    if l < 1e-12:
        return _quat(0.0, 0.0, 0.0, 1.0)
    return _quat(x / l, y / l, z / l, w / l)


def axis_quat(ax, deg: float):
    if abs(deg) < 1e-5:
        return _quat(0.0, 0.0, 0.0, 1.0)
    axn = vnorm(ax)
    r = math.radians(float(deg)) * 0.5
    s, c = math.sin(r), math.cos(r)
    return _quat(axn.x * s, axn.y * s, axn.z * s, c)


def signed_axis_quat(ax, deg: float, ref_vec, target_dir):
    if abs(deg) < 1e-5:
        return _quat(0.0, 0.0, 0.0, 1.0)
    qp = axis_quat(ax, deg)
    qn = qinv(qp)
    dp = vdot(vsub(qp.rotate_vector(ref_vec), ref_vec), target_dir)
    dn = vdot(vsub(qn.rotate_vector(ref_vec), ref_vec), target_dir)
    return qp if dp >= dn else qn


def signed_axis_quat_directed(ax, deg: float, ref_vec, target_dir):
    if abs(deg) < 1e-5:
        return _quat(0.0, 0.0, 0.0, 1.0)
    if deg >= 0.0:
        return signed_axis_quat(ax, deg, ref_vec, target_dir)
    return signed_axis_quat(ax, -deg, ref_vec, vmul(target_dir, -1.0))


def tuck_in_quat(fwd_vec, out_dir, up_vec, la_dir0, req_deg: float, theta_min_deg: float = -5.0):
    """Tuck upperarm toward body without outward winging in any quadrant (L or R)."""
    out_c = vdot(la_dir0, out_dir)
    up_c = vdot(la_dir0, up_vec)
    if math.hypot(out_c, up_c) < 1e-4 or abs(req_deg) < 1e-5:
        return _quat(0.0, 0.0, 0.0, 1.0)

    theta_deg = math.degrees(math.atan2(out_c, -up_c))
    t_in = vadd(vmul(out_dir, up_c), vmul(up_vec, -out_c))

    if req_deg >= 0.0:
        delta = min(179.9, float(req_deg), max(0.0, theta_deg - float(theta_min_deg)))
        if delta < 1e-4:
            return _quat(0.0, 0.0, 0.0, 1.0)
        return signed_axis_quat(fwd_vec, delta, la_dir0, t_in)

    delta = min(179.9, -float(req_deg), max(0.0, 135.0 - theta_deg))
    if delta < 1e-4:
        return _quat(0.0, 0.0, 0.0, 1.0)
    return signed_axis_quat(fwd_vec, delta, la_dir0, vmul(t_in, -1.0))


def solve_2bone_ik(hip_new, foot_new, hip_old, knee_old, foot_old):
    """Exact law-of-cosines 2-bone IK preserving segment lengths and old-knee pole."""
    l1 = vlen(vsub(knee_old, hip_old))
    l2 = vlen(vsub(foot_old, knee_old))
    d_vec = vsub(foot_new, hip_new)
    d = vlen(d_vec)
    d_clamped = max(abs(l1 - l2) + 0.5, min(l1 + l2 - 0.5, d))
    dir_hf = vnorm(d_vec)
    old_hf = vnorm(vsub(foot_old, hip_old))
    off = vsub(knee_old, hip_old)
    pole = vsub(off, vmul(old_hf, vdot(off, old_hf)))
    if vlen(pole) < 1e-3:
        fb = _vec(0.0, 1.0, 0.0) if abs(vdot(dir_hf, _vec(0.0, 1.0, 0.0))) < 0.95 else _vec(1.0, 0.0, 0.0)
        pole = vnorm(vsub(fb, vmul(dir_hf, vdot(fb, dir_hf))))
    else:
        pole = vsub(pole, vmul(dir_hf, vdot(pole, dir_hf)))
        if vlen(pole) > 1e-3:
            pole = vnorm(pole)
        else:
            fb = _vec(0.0, 1.0, 0.0) if abs(vdot(dir_hf, _vec(0.0, 1.0, 0.0))) < 0.95 else _vec(1.0, 0.0, 0.0)
            pole = vnorm(vsub(fb, vmul(dir_hf, vdot(fb, dir_hf))))
    cos_a = max(-1.0, min(1.0, (l1 * l1 + d_clamped * d_clamped - l2 * l2) / max(1e-6, 2.0 * l1 * d_clamped)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    knee_new = vadd(hip_new, vadd(vmul(dir_hf, l1 * cos_a), vmul(pole, l1 * sin_a)))
    foot_out = vadd(hip_new, vmul(dir_hf, d_clamped))
    return knee_new, foot_out


def compute_character_axes(pelvis_pos, thigh_l_pos, thigh_r_pos):
    """Compute continuous per-frame (lat_vecs, fwd_vecs) in UE Z-up space."""
    n = len(pelvis_pos)
    if n == 0:
        return [], []
    up_vec = _vec(0.0, 0.0, 1.0)
    lat_vecs, fwd_vecs = [], []
    for f in range(n):
        raw = vsub(thigh_l_pos[f], thigh_r_pos[f])
        lat_raw = _vec(raw.x, raw.y, 0.0)
        lat_vec = vnorm(lat_raw) if vlen(lat_raw) > 1e-4 else _vec(1.0, 0.0, 0.0)
        lat_vecs.append(lat_vec)
        fwd_vecs.append(vnorm(vcross(lat_vec, up_vec)))

    i_end = min(24, n - 1)
    i_start = min(14, max(0, i_end - 4))
    rd = vsub(pelvis_pos[i_end], pelvis_pos[i_start])
    rd_flat = _vec(rd.x, rd.y, 0.0)
    if vlen(rd_flat) < 1e-3 and n > 1:
        rd = vsub(pelvis_pos[-1], pelvis_pos[0])
        rd_flat = _vec(rd.x, rd.y, 0.0)
    ref_fwd = vnorm(rd_flat) if vlen(rd_flat) >= 1e-3 else fwd_vecs[-1]

    for f in range(n - 1, -1, -1):
        if vdot(fwd_vecs[f], ref_fwd) < 0.0:
            fwd_vecs[f] = vmul(fwd_vecs[f], -1.0)
            lat_vecs[f] = vmul(lat_vecs[f], -1.0)
        ref_fwd = fwd_vecs[f]

    return lat_vecs, fwd_vecs


ENVELOPE_KEYS = (
    "w3", "piv_lean", "run_lean", "bank", "head", "sh",
    "arm_st_back", "arm_pv_fwd", "arm_clr", "lag", "catch", "piv_foot", "widen", "win",
)


def check_frame_range(frame_range, n: int):
    """frame_range is in CLIP-LOCAL frames (0..n-1). Raise ValueError if it is not inside the clip."""
    if frame_range is None:
        return None
    a, b = int(frame_range[0]), int(frame_range[1])
    if a < 0 or b < a or b > n - 1:
        raise ValueError(
            f"frame_range [{a}, {b}] is outside the clip: valid clip-local frames are 0..{n - 1} (n={n} frames)"
        )
    return a, b


def range_envelope(f: int, f0: int, f1: int) -> float:
    """Trapezoid over [f0, f1]: smoothstep ramp in/out (<= 3 frames each), plateau 1.0 in the middle."""
    ramp = float(max(1, min(3, (f1 - f0) // 3)))
    return ss(f0 - 1.0, f0 - 1.0 + ramp, float(f)) * (1.0 - ss(f1 - ramp, f1 + 1.0, float(f)))


def compute_envelopes(
    f: int,
    n: int,
    turn_deg: float = 180.0,
    turn_side: str = "L",
    start_from_frame=None,
    frame_range=None,
    handoff_smooth_range=None,
    mode: str = "turnaround",
) -> dict[str, float]:
    """Compute per-frame smoothstep envelopes; strictly 0.0 before start_from_frame.

    mode "turnaround" (legacy): channel timings are FIXED on the turnaround pivot (see tool description);
    frame_range only clips them. mode "range": every channel is the same trapezoid over frame_range
    (ramp in/out <= 3 frames, plateau 1.0), so the requested degrees are reached inside the range.
    """
    if frame_range:
        f0 = max(0, int(frame_range[0]))
        f1 = min(max(0, n - 1), int(frame_range[1]))
    else:
        f0 = 0
        f1 = max(0, n - 1)

    sff = 2 if (start_from_frame is None and turn_side in ("L", "R")) else (0 if start_from_frame is None else int(start_from_frame))
    f_eff0 = max(f0, sff)

    keys = ENVELOPE_KEYS
    if f < f_eff0 or f > f1:
        return {k: 0.0 for k in keys}

    smooth = tuple(handoff_smooth_range) if handoff_smooth_range else None
    win = _window_weight(f, f_eff0, f1, smooth)
    if mode == "range":
        e = range_envelope(f, f_eff0, f1) * win
        return {k: e for k in keys}
    piv_scale = 0.8 if 45.0 <= float(turn_deg) <= 135.0 else 1.0
    atk = float(f_eff0) - 1.0
    ff = float(f)
    return {
        "w3": ss(atk, atk + 3.0, ff) * (1.0 - ss(22.0, 40.0, ff)) * win,
        "piv_lean": ss(atk, atk + 2.8, ff) * (1.0 - ss(7.0 * piv_scale, 13.0 * piv_scale, ff)) * win,
        "run_lean": ss(6.0 * piv_scale, 11.0 * piv_scale, ff) * ((1.0 - ss(22.0, 42.0, ff)) ** 1.2) * win,
        "bank": ss(atk, atk + 3.0, ff) * (1.0 - ss(10.0 * piv_scale, 22.0, ff)) * win,
        "head": ss(max(atk + 1.5, 3.5), max(atk + 5.5, 7.5), ff) * (1.0 - ss(16.0, 25.0, ff)) * win,
        "sh": ss(atk, atk + 2.8, ff) * (1.0 - ss(20.0, 31.0, ff)) * win,
        "arm_st_back": ss(atk, atk + 2.8, ff) * (1.0 - ss(6.0 * piv_scale, 9.5 * piv_scale, ff)) * win,
        "arm_pv_fwd": ss(atk, atk + 2.8, ff) * (1.0 - ss(7.5 * piv_scale, 13.0 * piv_scale, ff)) * win,
        "arm_clr": ss(6.5, 9.5, ff) * (1.0 - ss(12.0, 16.0, ff)) * win,
        "lag": ss(max(atk + 0.5, 1.5), max(atk + 3.5, 4.5), ff) * (1.0 - ss(8.0, 12.0, ff)) * win,
        "catch": ss(6.0, 10.5, ff) * (1.0 - ss(18.0, 25.0, ff)) * win,
        "piv_foot": ss(atk, atk + 1.8, ff) * (1.0 - ss(5.5 * piv_scale, 9.5 * piv_scale, ff)) * win,
        "widen": ss(atk, atk + 3.0, ff) * (1.0 - ss(11.0, 16.0, ff)) * win,
        "win": win,
    }


def map_v1_to_v2_intent(intent: dict) -> dict:
    """Translate legacy v1 parameters into normalized v2 parameters."""
    bank = float(intent.get("torso_bank_deg", 0.0))
    raw_side = intent.get("turn_side")
    if isinstance(raw_side, str):
        rs = raw_side.strip().lower()
        turn_side = {"l": "L", "r": "R", "none": "none"}.get(rs, "L")
        bank_val = bank
    else:
        has_turn = any(
            k in intent for k in ("turn_deg", "arm_st_tuck_in_deg", "pivot_heel_deg", "torso_bank_deg", "step_narrow_cm")
        )
        if has_turn:
            turn_side = "R" if bank < 0.0 else "L"
            bank_val = abs(bank)
        else:
            turn_side = "none"
            bank_val = bank

    sh_comp = float(intent.get("shoulder_level_compensate", 0.0))
    if "clav_st_elev_deg" in intent:
        clav_st = float(intent["clav_st_elev_deg"])
    elif "shoulder_level_compensate" in intent:
        clav_st = abs(bank) * 0.5 * sh_comp
    else:
        clav_st = 0.0

    if "clav_pv_elev_deg" in intent:
        clav_pv = float(intent["clav_pv_elev_deg"])
    elif "shoulder_level_compensate" in intent:
        clav_pv = -abs(bank) * 0.5 * sh_comp
    else:
        clav_pv = 0.0

    if "knee_drop" in intent:
        knee_drop = float(intent["knee_drop"])
    elif "w3_knee_drop" in intent:
        knee_drop = min(0.85, max(0.0, abs(float(intent["w3_knee_drop"])) / 25.0))
    else:
        knee_drop = 0.0

    if "arm_pv_fwd_deg" in intent:
        arm_pv_fwd = float(intent["arm_pv_fwd_deg"])
    elif "arm_back_arc" in intent:
        arm_pv_fwd = 0.5 * float(intent["arm_back_arc"])
    else:
        arm_pv_fwd = 0.0

    return {
        "turn_side": turn_side,
        "turn_deg": float(intent.get("turn_deg", 180.0)),
        "start_from_frame": intent.get("start_from_frame"),
        "frame_range": intent.get("frame_range"),
        "handoff_smooth_range": intent.get("handoff_smooth_range"),
        "smooth_pelvis_z": bool(intent.get("smooth_pelvis_z", True)),
        "smooth_upper_passes": int(intent.get("smooth_upper_passes", 4)),
        "smooth_leg_passes": int(intent.get("smooth_leg_passes", 0)),
        "pelvis_fwd_cm": float(intent.get("pelvis_fwd_cm", intent.get("com_shift_fwd_cm", 0.0))),
        "pelvis_drop_cm": float(intent.get("pelvis_drop_cm", 0.0)),
        "spine_lean_pivot_deg": float(intent.get("spine_lean_pivot_deg", intent.get("torso_lean_pivot_deg", 0.0))),
        "spine_lean_run_deg": float(intent.get("spine_lean_run_deg", 0.0)),
        "torso_bank_deg": bank_val,
        "head_lift_deg": float(intent.get("head_lift_deg", 0.0)),
        "knee_drop": knee_drop,
        "swing_lag_cm": float(intent.get("swing_lag_cm", 0.0)),
        "catch_fwd_cm": float(intent.get("catch_fwd_cm", 0.0)),
        "step_narrow_cm": float(intent.get("step_narrow_cm", 0.0)),
        "lat_widen_cm": float(intent.get("lat_widen_cm", 0.0)),
        "pivot_heel_deg": float(intent.get("pivot_heel_deg", intent.get("foot_pushoff_deg", 0.0))),
        "clav_st_elev_deg": clav_st,
        "clav_pv_elev_deg": clav_pv,
        "clav_open_deg": float(intent.get("clav_open_deg", 0.0)),
        "arm_st_tuck_in_deg": float(intent.get("arm_st_tuck_in_deg", 0.0)),
        "arm_pv_out_deg": float(intent.get("arm_pv_out_deg", 0.0)),
        "arm_st_back_deg": float(intent.get("arm_st_back_deg", intent.get("arm_back_arc", 0.0))),
        "arm_st_elbow_deg": float(intent.get("arm_st_elbow_deg", 0.0)),
        "arm_pv_fwd_deg": arm_pv_fwd,
        "arm_pv_elbow_deg": float(intent.get("arm_pv_elbow_deg", 0.0)),
        "arm_clr_deg": float(intent.get("arm_clr_deg", 0.0)),
        "envelope": intent.get("envelope") or ("range" if intent.get("frame_range") else "turnaround"),
    }


def _resolve_asset(name_or_path: str) -> str:
    if name_or_path.startswith("/Game/"):
        return name_or_path
    return f"{FOLDER}/{name_or_path}"


def _mk_tf(pos, quat, scl):
    return unreal.Transform(location=pos, rotation=quat.rotator(), scale=scl)


def main() -> int:
    if unreal is None:
        print("GEOM_INTENT ERROR: requires Unreal Engine environment")
        return 2

    al = unreal.AnimationLibrary
    eal = unreal.EditorAssetLibrary

    intent_path = os.environ.get("GSTN_INTENT_JSON", "")
    if not intent_path or not os.path.exists(intent_path):
        print(f"GEOM_INTENT ERROR: missing GSTN_INTENT_JSON={intent_path!r}")
        return 2

    payload = json.loads(open(intent_path, encoding="utf-8").read())
    src_path = _resolve_asset(payload["src"])
    dst_path = _resolve_asset(payload["dst"])
    v = map_v1_to_v2_intent(payload.get("intent", {}))
    save = os.environ.get("GSTN_SAVE", "0") == "1"

    if not eal.does_asset_exist(src_path):
        print(f"GEOM_INTENT ERROR: src not found {src_path}")
        return 2

    if save and src_path != dst_path:
        if eal.does_asset_exist(dst_path):
            eal.delete_asset(dst_path)
        if not eal.duplicate_asset(src_path, dst_path):
            print(f"GEOM_INTENT ERROR: failed to duplicate {src_path} -> {dst_path}")
            return 2

    seq = unreal.load_asset(dst_path if save else src_path)
    n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else al.get_num_frames(seq) + 1
    fps = 30.0
    if hasattr(seq, "get_sampling_frame_rate"):
        try:
            fr_rate = seq.get_sampling_frame_rate()
            if fr_rate and getattr(fr_rate, "denominator", 1):
                fps = float(fr_rate.numerator) / float(fr_rate.denominator)
        except Exception:
            fps = 30.0
    opts = unreal.AnimPoseEvaluationOptions()
    if hasattr(opts, "should_retarget"):
        opts.should_retarget = False

    all_bones = [
        "root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "head",
        "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
        "thigh_l", "calf_l", "foot_l", "ball_l", "ik_foot_root", "ik_foot_l",
        "thigh_r", "calf_r", "foot_r", "ball_r", "ik_foot_r",
    ]
    world_frames = []
    for f in range(n):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / fps, opts)
        world_frames.append({b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD) for b in all_bones})

    foot_air_w = {"l": [], "r": []}
    for s in ("l", "r"):
        fz = [world_frames[f]["foot_" + s].translation.z for f in range(n)]
        fmin = min(fz[:min(n, 28)])
        for f in range(n):
            foot_air_w[s].append(ss(fmin + 1.5, fmin + 6.0, fz[f]))

    lat_vecs, fwd_vecs = compute_character_axes(
        [w["pelvis"].translation for w in world_frames],
        [w["thigh_l"].translation for w in world_frames],
        [w["thigh_r"].translation for w in world_frames],
    )

    turn_side = v["turn_side"]
    turn_deg = v["turn_deg"]
    st, pv, bank_sign = ("r", "l", -1.0) if turn_side == "R" else ("l", "r", 1.0)

    try:
        check_frame_range(v["frame_range"], n)
    except ValueError as exc:
        print(f"GEOM_INTENT ERROR: {exc}")
        return 2
    fr = v["frame_range"] or [0, n - 1]
    f0, f1 = max(0, int(fr[0])), min(n - 1, int(fr[1]))
    sff = 2 if (v["start_from_frame"] is None and turn_side in ("L", "R")) else (0 if v["start_from_frame"] is None else int(v["start_from_frame"]))
    f_eff0 = max(f0, sff)

    pel_z_sm = [world_frames[f]["pelvis"].translation.z for f in range(n)]
    if v["smooth_pelvis_z"]:
        for _ in range(4):
            nz = list(pel_z_sm)
            for f in range(max(1, f_eff0), min(27, f1)):
                w_sm = 0.55 * ss(float(max(0, f_eff0 - 1)), float(f_eff0 + 2), float(f)) * (1.0 - ss(22.0, 27.0, float(f)))
                nz[f] = pel_z_sm[f] * (1.0 - w_sm) + 0.5 * (pel_z_sm[f - 1] + pel_z_sm[f + 1]) * w_sm
            pel_z_sm = nz

    tracked = [
        "pelvis", "spine_01", "spine_02", "spine_03", "neck_01",
        "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
        "thigh_l", "calf_l", "foot_l", "ik_foot_l",
        "thigh_r", "calf_r", "foot_r", "ik_foot_r",
    ]
    tracks_out = {b: ([], [], []) for b in tracked}

    parents = {
        "pelvis": "root",
        "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "neck_01": "spine_05",
        "clavicle_l": "spine_05", "upperarm_l": "clavicle_l", "lowerarm_l": "upperarm_l", "hand_l": "lowerarm_l",
        "clavicle_r": "spine_05", "upperarm_r": "clavicle_r", "lowerarm_r": "upperarm_r", "hand_r": "lowerarm_r",
        "thigh_l": "pelvis", "calf_l": "thigh_l", "foot_l": "calf_l", "ik_foot_l": "ik_foot_root",
        "thigh_r": "pelvis", "calf_r": "thigh_r", "foot_r": "calf_r", "ik_foot_r": "ik_foot_root",
    }

    up_vec = _vec(0.0, 0.0, 1.0)
    for f in range(n):
        w = world_frames[f]
        nw = {b: w[b] for b in all_bones}
        env = compute_envelopes(
            f, n, turn_deg=turn_deg, turn_side=turn_side,
            start_from_frame=sff, frame_range=[f0, f1],
            handoff_smooth_range=v["handoff_smooth_range"], mode=v["envelope"],
        )

        if f >= f_eff0 and f <= f1:
            lat_vec = lat_vecs[f]
            fwd_vec = fwd_vecs[f]
            inward_dir = vmul(lat_vec, bank_sign)

            pel_pos = vadd(w["pelvis"].translation, vmul(fwd_vec, v["pelvis_fwd_cm"] * env["w3"]))
            pel_pos.z = pel_z_sm[f] + v["pelvis_drop_cm"] * env["w3"]
            nw["pelvis"] = _mk_tf(pel_pos, w["pelvis"].rotation, w["pelvis"].scale3d)

            q_sp_cum = _quat(0.0, 0.0, 0.0, 1.0)
            sp_weights = (("spine_01", 0.40), ("spine_02", 0.35), ("spine_03", 0.25))
            sp_par = {"spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02"}
            lean_deg = v["spine_lean_pivot_deg"] * env["piv_lean"] + v["spine_lean_run_deg"] * env["run_lean"]
            for sp, wt in sp_weights:
                par = sp_par[sp]
                pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
                q_bank = signed_axis_quat_directed(fwd_vec, v["torso_bank_deg"] * wt * env["bank"], up_vec, inward_dir)
                q_lean = signed_axis_quat_directed(lat_vec, lean_deg * wt, up_vec, fwd_vec)
                q_sp_cum = (q_lean * q_bank) * q_sp_cum
                nw[sp] = _mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

            for sp, par in (("spine_04", "spine_03"), ("spine_05", "spine_04")):
                pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
                nw[sp] = _mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

            nk_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w["neck_01"].translation, w["spine_05"].translation)))
            q_nk = signed_axis_quat_directed(lat_vec, v["head_lift_deg"] * env["head"], fwd_vec, up_vec)
            nw["neck_01"] = _mk_tf(nk_pos, q_nk * q_sp_cum * w["neck_01"].rotation, w["neck_01"].scale3d)

            for s, lsign in (("l", 1.0), ("r", -1.0)):
                th, ca, fo = "thigh_" + s, "calf_" + s, "foot_" + s
                hip_old, knee_old, foot_old = w[th].translation, w[ca].translation, w[fo].translation
                hip_new = vadd(nw["pelvis"].translation, vsub(hip_old, w["pelvis"].translation))
                w_air = foot_air_w[s][f]
                foot_tgt = _vec(foot_old.x, foot_old.y, foot_old.z)
                if env["widen"] > 1e-4:
                    foot_tgt = vadd(foot_tgt, vmul(lat_vec, (v["lat_widen_cm"] * lsign) * env["widen"]))
                    if turn_side != "none" and s == st and abs(v["step_narrow_cm"]) > 1e-4:
                        foot_tgt = vadd(foot_tgt, vmul(lat_vec, (-lsign * v["step_narrow_cm"]) * env["widen"]))
                if w_air > 1e-4:
                    excess_z = max(0.0, foot_old.z - 14.0)
                    foot_tgt.z = foot_old.z - v["knee_drop"] * excess_z * env["w3"] * w_air
                    foot_tgt = vadd(foot_tgt, vmul(fwd_vec, (-v["swing_lag_cm"] * env["lag"] + v["catch_fwd_cm"] * env["catch"]) * w_air))
                knee_new, foot_new = solve_2bone_ik(hip_new, foot_tgt, hip_old, knee_old, foot_old)
                cr_th = vcross(vsub(knee_old, hip_old), vsub(knee_new, hip_new))
                q_th = axis_quat(cr_th, math.degrees(math.acos(max(-1.0, min(1.0, vdot(vnorm(vsub(knee_old, hip_old)), vnorm(vsub(knee_new, hip_new)))))))) if vlen(cr_th) > 1e-4 else _quat(0.0, 0.0, 0.0, 1.0)
                cr_ca = vcross(vsub(foot_old, knee_old), vsub(foot_new, knee_new))
                q_ca = axis_quat(cr_ca, math.degrees(math.acos(max(-1.0, min(1.0, vdot(vnorm(vsub(foot_old, knee_old)), vnorm(vsub(foot_new, knee_new)))))))) if vlen(cr_ca) > 1e-4 else _quat(0.0, 0.0, 0.0, 1.0)
                q_fo_push = _quat(0.0, 0.0, 0.0, 1.0)
                if s == pv and env["piv_foot"] > 1e-4 and abs(v["pivot_heel_deg"]) > 1e-4:
                    ball_dir = vsub(w["ball_" + pv].translation, foot_old)
                    q_fo_push = signed_axis_quat_directed(lat_vec, v["pivot_heel_deg"] * env["piv_foot"], ball_dir, vmul(up_vec, -1.0))
                nw[th] = _mk_tf(hip_new, q_th * w[th].rotation, w[th].scale3d)
                nw[ca] = _mk_tf(knee_new, q_ca * w[ca].rotation, w[ca].scale3d)
                nw[fo] = _mk_tf(foot_new, q_fo_push * w[fo].rotation, w[fo].scale3d)

            for s, sign in (("l", 1.0), ("r", -1.0)):
                cl, ua, la, ha = "clavicle_" + s, "upperarm_" + s, "lowerarm_" + s, "hand_" + s
                out_dir = vmul(lat_vec, sign)
                clav_elev = v["clav_st_elev_deg"] if s == st else v["clav_pv_elev_deg"]

                cl_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w[cl].translation, w["spine_05"].translation)))
                ua_dir0 = q_sp_cum.rotate_vector(vsub(w[ua].translation, w[cl].translation))
                q_cl_elev = signed_axis_quat_directed(fwd_vec, clav_elev * env["sh"], ua_dir0, up_vec)
                q_cl_open = signed_axis_quat_directed(up_vec, v["clav_open_deg"] * env["sh"], ua_dir0, vmul(fwd_vec, -1.0))
                q_cl = q_cl_open * q_cl_elev
                nw[cl] = _mk_tf(cl_pos, q_cl * q_sp_cum * w[cl].rotation, w[cl].scale3d)

                ua_pos = vadd(cl_pos, q_cl.rotate_vector(ua_dir0))
                la_dir0 = (q_cl * q_sp_cum).rotate_vector(vsub(w[la].translation, w[ua].translation))
                q_ua_lat = _quat(0.0, 0.0, 0.0, 1.0)
                q_ua_swing = _quat(0.0, 0.0, 0.0, 1.0)
                if s == st:
                    if env["arm_st_back"] > 1e-4:
                        q_ua_lat = tuck_in_quat(fwd_vec, out_dir, up_vec, la_dir0, v["arm_st_tuck_in_deg"] * env["arm_st_back"])
                        la_dir1 = q_ua_lat.rotate_vector(la_dir0)
                        q_ua_swing = signed_axis_quat_directed(lat_vec, v["arm_st_back_deg"] * env["arm_st_back"], la_dir1, vmul(fwd_vec, -1.0))
                    if env["arm_clr"] > 1e-4 and abs(v["arm_clr_deg"]) > 1e-4:
                        q_ua_lat = signed_axis_quat_directed(fwd_vec, v["arm_clr_deg"] * env["arm_clr"], la_dir0, out_dir) * q_ua_lat
                else:
                    q_ua_lat = signed_axis_quat_directed(fwd_vec, v["arm_pv_out_deg"] * env["sh"], la_dir0, out_dir)
                    if env["arm_pv_fwd"] > 1e-4:
                        la_dir1 = q_ua_lat.rotate_vector(la_dir0)
                        q_ua_swing = signed_axis_quat_directed(lat_vec, v["arm_pv_fwd_deg"] * env["arm_pv_fwd"], la_dir1, fwd_vec)
                q_ua = q_ua_swing * q_ua_lat
                nw[ua] = _mk_tf(ua_pos, q_ua * q_cl * q_sp_cum * w[ua].rotation, w[ua].scale3d)

                la_pos = vadd(ua_pos, q_ua.rotate_vector(la_dir0))
                la_dir_rot = q_ua.rotate_vector(la_dir0)
                ha_dir0 = (q_ua * q_cl * q_sp_cum).rotate_vector(vsub(w[ha].translation, w[la].translation))
                q_la = _quat(0.0, 0.0, 0.0, 1.0)
                el_deg = (v["arm_st_elbow_deg"] * env["arm_st_back"]) if s == st else (v["arm_pv_elbow_deg"] * env["arm_pv_fwd"])
                if abs(el_deg) > 1e-3:
                    hinge = vcross(la_dir_rot, ha_dir0)
                    ax = vnorm(hinge) if vlen(hinge) > 1e-3 else lat_vec
                    qp = axis_quat(ax, abs(el_deg))
                    qn = qinv(qp)
                    q_flex = qp if vdot(vmul(la_dir_rot, -1.0), qp.rotate_vector(ha_dir0)) >= vdot(vmul(la_dir_rot, -1.0), qn.rotate_vector(ha_dir0)) else qn
                    q_la = q_flex if el_deg >= 0.0 else qinv(q_flex)
                nw[la] = _mk_tf(la_pos, q_la * q_ua * q_cl * q_sp_cum * w[la].rotation, w[la].scale3d)

                ha_pos = vadd(la_pos, q_la.rotate_vector(ha_dir0))
                nw[ha] = _mk_tf(ha_pos, q_la * q_ua * q_cl * q_sp_cum * w[ha].rotation, w[ha].scale3d)

        for b in tracked:
            loc = nw["foot_" + b[-1]].make_relative(nw["ik_foot_root"]) if b.startswith("ik_foot_") else nw[b].make_relative(nw[parents[b]])
            tracks_out[b][0].append(loc.translation)
            tracks_out[b][1].append(loc.rotation)
            tracks_out[b][2].append(loc.scale3d)

    for b in ("spine_01", "spine_02", "spine_03", "neck_01", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
        pos, rot, scl = tracks_out[b]
        for _ in range(max(0, v["smooth_upper_passes"])):
            nr = list(rot)
            for f in range(max(2, f_eff0), min(25, f1)):
                w_sm = 0.5 * ss(float(max(1, f_eff0 - 1)), float(f_eff0 + 1), float(f)) * (1.0 - ss(17.0, 25.0, float(f)))
                nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_sm)
            rot = nr
        tracks_out[b] = (pos, rot, scl)

    for b in ("thigh_l", "calf_l", "thigh_r", "calf_r"):
        pos, rot, scl = tracks_out[b]
        for _ in range(max(0, v["smooth_leg_passes"])):
            nr = list(rot)
            for f in range(max(8, f_eff0), min(16, f1)):
                w_sm = 0.5 * ss(7.0, 9.5, float(f)) * (1.0 - ss(13.0, 16.0, float(f)))
                nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_sm)
            rot = nr
        tracks_out[b] = (pos, rot, scl)

    if save:
        ctrl = seq.controller if hasattr(seq, "controller") else seq.get_editor_property("controller")
        ctrl.open_bracket(unreal.Text("gstn_anim_intent"), False)
        try:
            for b, (pos, rot, scl) in tracks_out.items():
                ok_track = ctrl.set_bone_track_keys(b, pos, rot, scl, False)
                if ok_track is False:
                    raise RuntimeError(f"set_bone_track_keys returned False for {b}")
        finally:
            ctrl.close_bracket(False)
        eal.save_asset(dst_path, only_if_is_dirty=False)

    out_target = dst_path if save else src_path
    print(f"GEOM_INTENT: OK -> {out_target} (save={save}, frames={f_eff0}..{f1})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
