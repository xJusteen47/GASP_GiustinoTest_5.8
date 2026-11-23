"""Procedural follow-through for gstn-anim-mcp: deterministic math, the model only picks INTENT.

Pure Python (no `unreal`): importable and unit-testable offline. The UE side is follow_through_ue.py, which reads local
bone rotations, calls solve_chain() and writes the result back.

Idea. A bone chain (shoulder>elbow>wrist>fingers, spine>neck>head...) keeps moving after the move's anchor frame (the
hit or the FAF): every link is pulled through a damped second-order response (the same dynamics as a spring), so it
drags, overshoots the pose it had at the anchor and settles. The model never writes quaternions: it picks
  chain, mass, overshoot_pct, delay_frames, settle_frames, anchor (hit | faf | frame) and gets the rest from the math.

Per link k of a chain, with q_k[f] the authored local rotation and A the anchor frame:
  r[f]  = log(q_k[f] * inv(q_k[A]))                    rotation vector relative to the pose AT the anchor
  rd[f] = r[f - delay_k]                                delayed input (delay_k = delay_frames * k, fractional ok)
  x     = damped spring following rd                    omega = 2*pi/period_k, zeta from overshoot_pct (below)
  out   = r + w(f) * gain_k * (x - r)                   w = 0 at A and at the window end, smooth in between
  q_new = exp(out) * q_k[A]
Because w(A) == 0 the anchor pose (the hit) is bit-identical, and because w(end) == 0 nothing after the window moves.
Only the chosen bones change: root, pelvis and legs are not allowed in a chain, so the feet stay planted by construction
(conservation_report() also verifies it on the data). Amplitude is capped by max_extra_deg per link.

mass: light | normal | heavy | number (1 = normal). period_k = 9 frames * sqrt(mass) * 0.9**k (distal links are lighter and
swing faster), default delay_frames = 1.0 * sqrt(mass). overshoot% maps to the damping ratio EXACTLY
(zeta = -ln(os) / sqrt(pi^2 + ln(os)^2), the step-response overshoot of a 2nd order system); damping= overrides it.
"""
from __future__ import annotations

import math
import re
from typing import Any, Sequence

Q = tuple  # (x, y, z, w)
IDENT: Q = (0.0, 0.0, 0.0, 1.0)

MASS_PRESETS = {"light": 0.45, "normal": 1.0, "heavy": 2.4}
BASE_PERIOD_FRAMES = 9.0
DISTAL_PERIOD_FACTOR = 0.9
MIN_PERIOD_FRAMES = 3.0
SUBSTEPS = 20

# Only upper body chains. root/pelvis/legs/IK/feet are NOT allowed: that is what keeps the feet planted.
ALLOWED_BONE = re.compile(
    r"^(spine_0[1-5]|neck_0[1-3]|head|clavicle_[lr]|upperarm_[lr]|lowerarm_[lr]|hand_[lr]"
    r"|(thumb|index|middle|ring|pinky)_(metacarpal|0[1-3])_[lr])$"
)
FINGER_ROOTS = ("thumb_01", "index_01", "middle_01", "ring_01", "pinky_01")

CHAIN_PRESETS: dict[str, tuple[str, str]] = {
    # name -> (chain text, side)
    "arm_l": ("upperarm>lowerarm>hand", "l"),
    "arm_r": ("upperarm>lowerarm>hand", "r"),
    "arms": ("upperarm>lowerarm>hand", "both"),
    "arm_fingers_l": ("upperarm>lowerarm>hand>fingers", "l"),
    "arm_fingers_r": ("upperarm>lowerarm>hand>fingers", "r"),
    "arms_fingers": ("upperarm>lowerarm>hand>fingers", "both"),
    "shoulder_arm_l": ("clavicle>upperarm>lowerarm>hand", "l"),
    "shoulder_arm_r": ("clavicle>upperarm>lowerarm>hand", "r"),
    "shoulder_arms": ("clavicle>upperarm>lowerarm>hand", "both"),
    "spine_head": ("spine_02>spine_03>spine_04>spine_05>neck_01>head", "none"),
    "neck_head": ("neck_01>head", "none"),
}
ALIASES = {
    "shoulder": "clavicle", "clavicle": "clavicle", "upperarm": "upperarm", "elbow": "lowerarm",
    "lowerarm": "lowerarm", "forearm": "lowerarm", "wrist": "hand", "hand": "hand",
}
SIDED = {"clavicle", "upperarm", "lowerarm", "hand"}

FT_BOUNDS = {
    "overshoot_pct": (0.0, 80.0),
    "damping": (0.05, 1.5),
    "delay_frames": (0.0, 6.0),
    "settle_frames": (3, 60),
    "blend_in": (1, 6),
    "link_gain": (0.3, 2.0),
    "max_extra_deg": (1.0, 90.0),
    "assume_arrival_deg": (1.0, 120.0),
}
FT_KEYS = {
    "chain", "side", "mass", "overshoot_pct", "damping", "delay_frames", "settle_frames", "anchor", "hit_frame",
    "faf_frame", "frame_range", "blend_in", "ease", "link_gain", "max_extra_deg", "assume_arrival_deg",
}


# ---------------------------------------------------------------- quaternion helpers (x, y, z, w tuples)
def qmul(a: Q, b: Q) -> Q:
    return (
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    )


def qinv(q: Q) -> Q:
    return (-q[0], -q[1], -q[2], q[3])


def qlog(q: Q) -> tuple[float, float, float]:
    """Rotation vector (axis * angle, radians), shortest path."""
    x, y, z, w = q
    if w < 0.0:
        x, y, z, w = -x, -y, -z, -w
    s = math.sqrt(x * x + y * y + z * z)
    if s < 1e-12:
        return (0.0, 0.0, 0.0)
    ang = 2.0 * math.atan2(s, w)
    k = ang / s
    return (x * k, y * k, z * k)


def qexp(r: Sequence[float]) -> Q:
    ang = math.sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2])
    if ang < 1e-12:
        return IDENT
    s = math.sin(ang * 0.5) / ang
    return (r[0] * s, r[1] * s, r[2] * s, math.cos(ang * 0.5))


def q_angle_deg(a: Q, b: Q) -> float:
    d = qmul(a, qinv(b))
    return math.degrees(math.sqrt(sum(c * c for c in qlog(d))))


def ss(a: float, b: float, x: float) -> float:
    if x <= a:
        return 0.0
    if x >= b:
        return 1.0
    t = (x - a) / (b - a)
    return t * t * (3.0 - 2.0 * t)


# ---------------------------------------------------------------- parameters
def mass_value(mass: Any) -> float:
    if isinstance(mass, str):
        key = mass.strip().lower()
        if key not in MASS_PRESETS:
            raise ValueError(f"mass must be light|normal|heavy or a number 0.2..5, got {mass!r}")
        return MASS_PRESETS[key]
    if isinstance(mass, bool) or not isinstance(mass, (int, float)):
        raise ValueError("mass must be light|normal|heavy or a number 0.2..5")
    if not (0.2 <= float(mass) <= 5.0):
        raise ValueError(f"mass={mass} out of range [0.2, 5]")
    return float(mass)


def zeta_from_overshoot(pct: float) -> float:
    """Damping ratio whose unit-step response overshoots by exactly pct percent (0 -> critically damped)."""
    if pct <= 0.0:
        return 1.0
    ln = math.log(min(0.95, max(1e-3, pct / 100.0)))
    return -ln / math.sqrt(math.pi * math.pi + ln * ln)


def overshoot_from_zeta(z: float) -> float:
    if z >= 1.0:
        return 0.0
    return 100.0 * math.exp(-math.pi * z / math.sqrt(1.0 - z * z))


def _num(ft: dict, key: str, lo: float, hi: float, integer: bool = False) -> float:
    v = ft[key]
    if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(float(v)):
        raise ValueError(f"follow_through.{key} must be a finite number")
    if integer and float(v) != int(v):
        raise ValueError(f"follow_through.{key} must be an integer")
    if not (lo <= float(v) <= hi):
        raise ValueError(f"follow_through.{key}={v} out of safe bounds [{lo}, {hi}]")
    return int(v) if integer else float(v)


def resolve_chain(chain: Any, side: str | None) -> list[dict[str, Any]]:
    """-> list of side-chains: {"side", "links": [[bone, ...], ...]} (a link may hold several bones, e.g. fingers)."""
    if isinstance(chain, str) and chain.strip().lower() in CHAIN_PRESETS:
        text, preset_side = CHAIN_PRESETS[chain.strip().lower()]
        side = side or preset_side
        parts = text.split(">")
    elif isinstance(chain, str):
        parts = [p.strip().lower() for p in chain.split(">") if p.strip()]
    elif isinstance(chain, (list, tuple)):
        parts = [str(p).strip().lower() for p in chain if str(p).strip()]
    else:
        raise ValueError(f"chain must be a preset ({', '.join(sorted(CHAIN_PRESETS))}), 'a>b>c' or a list of bones")
    if not parts:
        raise ValueError("chain is empty")
    side = (side or "").strip().lower() or None
    if side not in (None, "l", "r", "both", "none"):
        raise ValueError(f"side must be l, r or both, got {side!r}")

    needs_side = any(ALIASES.get(p, p) in SIDED or p == "fingers" for p in parts)
    if needs_side and side in (None, "none"):
        side = "both"
    sides = ["l", "r"] if side == "both" else [side] if side in ("l", "r") else [None]

    out = []
    for s in sides:
        links: list[list[str]] = []
        for p in parts:
            if p == "fingers":
                if s is None:
                    raise ValueError("'fingers' needs a side")
                links.append([f"{f}_{s}" for f in FINGER_ROOTS])
                continue
            base = ALIASES.get(p, p)
            if base in SIDED:
                if s is None:
                    raise ValueError(f"{p!r} needs a side")
                name = f"{base}_{s}"
            else:
                name = p
            if not ALLOWED_BONE.match(name):
                raise ValueError(
                    f"bone {name!r} is not allowed in a follow-through chain (upper body only: spine, neck, head, "
                    "clavicle, upperarm, lowerarm, hand, fingers; root, pelvis, legs and IK stay planted)"
                )
            links.append([name])
        out.append({"side": s, "links": links})
    return out


def validate_follow_through(ft: Any) -> dict[str, Any]:
    """Normalize the follow_through intent. Raises ValueError with a message the caller can act on."""
    if not isinstance(ft, dict):
        raise ValueError("follow_through must be an object")
    unknown = set(ft) - FT_KEYS
    if unknown:
        raise ValueError(f"Unknown follow_through parameter: {sorted(unknown)[0]!r}")
    if "chain" not in ft:
        raise ValueError("follow_through.chain is required")
    c: dict[str, Any] = {"chain": ft["chain"], "side": ft.get("side")}
    c["chains"] = resolve_chain(ft["chain"], ft.get("side"))
    c["mass"] = ft.get("mass", "normal")
    c["mass_value"] = mass_value(c["mass"])
    if "overshoot_pct" in ft and "damping" in ft:
        raise ValueError("give overshoot_pct OR damping, not both")
    if "damping" in ft:
        c["zeta"] = _num(ft, "damping", *FT_BOUNDS["damping"])
    else:
        c["zeta"] = zeta_from_overshoot(_num(ft, "overshoot_pct", *FT_BOUNDS["overshoot_pct"]) if "overshoot_pct" in ft else 25.0)
    c["overshoot_pct"] = round(overshoot_from_zeta(c["zeta"]), 2)
    dl = ft.get("delay_frames")
    if isinstance(dl, (list, tuple)):
        if not dl or len(dl) > 8 or any(isinstance(x, bool) or not isinstance(x, (int, float)) or not (0 <= x <= 6) for x in dl):
            raise ValueError("delay_frames list: 1..8 numbers in [0, 6]")
        c["delay_frames"] = [float(x) for x in dl]
    elif dl is None:
        c["delay_frames"] = math.sqrt(c["mass_value"])
    else:
        c["delay_frames"] = _num(ft, "delay_frames", *FT_BOUNDS["delay_frames"])
    c["blend_in"] = _num(ft, "blend_in", *FT_BOUNDS["blend_in"], integer=True) if "blend_in" in ft else 2
    c["link_gain"] = _num(ft, "link_gain", *FT_BOUNDS["link_gain"]) if "link_gain" in ft else 1.0
    c["max_extra_deg"] = _num(ft, "max_extra_deg", *FT_BOUNDS["max_extra_deg"]) if "max_extra_deg" in ft else 40.0
    c["assume_arrival_deg"] = _num(ft, "assume_arrival_deg", *FT_BOUNDS["assume_arrival_deg"]) if "assume_arrival_deg" in ft else 40.0
    ease = ft.get("ease", "smooth")
    if ease not in ("smooth", "linear"):
        raise ValueError("ease must be 'smooth' or 'linear'")
    c["ease"] = ease

    # anchor / window (clip-local frames)
    if "frame_range" in ft and "anchor" in ft:
        raise ValueError("give anchor OR frame_range (frame_range[0] is then the anchor), not both")
    if "frame_range" in ft:
        fr = ft["frame_range"]
        if not (isinstance(fr, (list, tuple)) and len(fr) == 2 and all(isinstance(x, int) and not isinstance(x, bool) for x in fr)):
            raise ValueError("follow_through.frame_range must be [anchor_frame, end_frame] (ints)")
        if fr[0] < 0 or fr[1] - fr[0] < 3:
            raise ValueError(f"follow_through.frame_range {list(fr)} invalid: need 0 <= a and at least 3 frames")
        c["anchor_kind"] = "frame"
        c["anchor_frame"] = int(fr[0])
        c["settle_frames"] = int(fr[1] - fr[0])
        c["settle_explicit"] = True
    else:
        a = ft.get("anchor")
        if a in ("hit", "faf"):
            key = f"{a}_frame"
            if key not in ft:
                raise ValueError(f"anchor={a!r} needs {key} (clip-local frame from the move card)")
            c["anchor_kind"] = a
            c["anchor_frame"] = int(_num(ft, key, 0, 600, integer=True))
        elif isinstance(a, int) and not isinstance(a, bool) and 0 <= a <= 600:
            c["anchor_kind"], c["anchor_frame"] = "frame", a
        else:
            raise ValueError("follow_through needs anchor ('hit' + hit_frame, 'faf' + faf_frame, or a frame number) or frame_range")
        if "settle_frames" in ft:
            c["settle_frames"] = _num(ft, "settle_frames", *FT_BOUNDS["settle_frames"], integer=True)
            c["settle_explicit"] = True
        else:
            c["settle_explicit"] = False
            c["settle_frames"] = default_settle_frames(c)
    return c


def link_period(c: dict[str, Any], k: int) -> float:
    return max(MIN_PERIOD_FRAMES, BASE_PERIOD_FRAMES * math.sqrt(c["mass_value"]) * DISTAL_PERIOD_FACTOR ** k)


def link_delay(c: dict[str, Any], k: int) -> float:
    d = c["delay_frames"]
    if isinstance(d, list):
        return d[min(k, len(d) - 1)] if k > 0 else 0.0
    return d * k


def default_settle_frames(c: dict[str, Any]) -> int:
    """Frames for the response to decay to ~5% (3 time constants of the slowest link), clamped 6..40."""
    n_links = max(len(ch["links"]) for ch in c["chains"])
    worst = 0.0
    for k in range(n_links):
        omega = 2.0 * math.pi / link_period(c, k)
        worst = max(worst, 3.0 / (c["zeta"] * omega) + link_delay(c, k))
    return int(min(40, max(6, math.ceil(worst))))


# ---------------------------------------------------------------- the response
def _sample(series: list[tuple[float, float, float]], t: float) -> tuple[float, float, float]:
    if t <= 0.0:
        return series[0]
    if t >= len(series) - 1:
        return series[-1]
    i = int(t)
    u = t - i
    a, b = series[i], series[i + 1]
    return (a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u)


def spring_follow(rd: list[tuple[float, float, float]], s0: int, omega: float, zeta: float) -> list[tuple[float, float, float]]:
    """Damped spring (per-frame time) following the input rd from frame s0 on; frames < s0 return rd unchanged."""
    out = list(rd)
    x = list(rd[s0])
    nxt = rd[min(s0 + 1, len(rd) - 1)]
    v = [nxt[i] - rd[s0][i] for i in range(3)]
    dt = 1.0 / SUBSTEPS
    for f in range(s0, len(rd)):
        tgt = rd[f]
        for _ in range(SUBSTEPS):
            for i in range(3):
                acc = omega * omega * (tgt[i] - x[i]) - 2.0 * zeta * omega * v[i]
                v[i] += acc * dt
                x[i] += v[i] * dt
        out[f] = (x[0], x[1], x[2])
    return out


def window_weight(f: int, a: int, e: int, blend_in: int, ease: str) -> float:
    """0 at the anchor and at the end frame, ramp in over blend_in frames, ramp out over the last frames."""
    if f <= a or f >= e:
        return 0.0
    bo = max(1, min(3, (e - a) // 3))
    if ease == "linear":
        up = min(1.0, (f - a) / float(blend_in))
        dn = min(1.0, (e - f) / float(bo))
        return min(up, dn)
    return ss(float(a), float(a + blend_in), float(f)) * (1.0 - ss(float(e - bo), float(e), float(f)))


def resolve_window(c: dict[str, Any], n: int) -> dict[str, Any]:
    a = c["anchor_frame"]
    if a > n - 2:
        raise ValueError(f"anchor frame {a} leaves no frames to follow through: clip has frames 0..{n - 1} (clip-local)")
    e_req = a + c["settle_frames"]
    e = min(e_req, n - 1)
    warn = []
    if e < e_req:
        warn.append(f"window cut by the clip end: wanted {a}..{e_req}, clip ends at {n - 1}")
    if e - a < 3:
        raise ValueError(f"window {a}..{e} too short (< 3 frames) for a follow-through")
    return {"anchor": a, "end": e, "warnings": warn}


def solve_link(
    quats: list[Q], k: int, c: dict[str, Any], a: int, e: int
) -> tuple[list[Q], dict[str, Any]]:
    """Follow-through for ONE bone track (list of local quaternions, one per frame). Returns (new quats, stats)."""
    n = len(quats)
    rest = quats[a]
    inv_rest = qinv(rest)
    r = [qlog(qmul(q, inv_rest)) for q in quats]
    period = link_period(c, k)
    omega = 2.0 * math.pi / period
    d = link_delay(c, k)
    rd = [_sample(r, f - d) for f in range(n)]
    s0 = max(0, a - int(math.ceil(period)))
    x = spring_follow(rd, s0, omega, c["zeta"])
    gain = c["link_gain"] ** k
    cap = math.radians(c["max_extra_deg"])
    new_q = list(quats)
    peak_deg, peak_f, active = 0.0, None, []
    for f in range(a, e + 1):
        w = window_weight(f, a, e, c["blend_in"], c["ease"])
        if w <= 1e-9:
            continue
        dx = [(x[f][i] - r[f][i]) * w * gain for i in range(3)]
        mag = math.sqrt(dx[0] ** 2 + dx[1] ** 2 + dx[2] ** 2)
        if mag > cap:
            dx = [v * cap / mag for v in dx]
        out = (r[f][0] + dx[0], r[f][1] + dx[1], r[f][2] + dx[2])
        new_q[f] = qmul(qexp(out), rest)
        deg = q_angle_deg(new_q[f], quats[f])
        if deg > 0.05:
            active.append(f)
        if deg > peak_deg:
            peak_deg, peak_f = deg, f
    tail = [q_angle_deg(new_q[f], quats[f]) for f in range(max(a, e - 1), e + 1)]
    return new_q, {
        "delay_frames": round(d, 2), "period_frames": round(period, 2), "gain": round(gain, 3),
        "peak_deg": round(peak_deg, 2), "peak_frame": peak_f,
        "frames": [active[0], active[-1]] if active else None, "residual_deg": round(max(tail), 2),
    }


def solve_chain(rot_tracks: dict[str, list[Q]], c: dict[str, Any], n: int) -> tuple[dict[str, list[Q]], dict[str, Any]]:
    """Apply the follow-through to every chain in c. rot_tracks: bone -> local quats (all frames).
    Returns (new tracks for the chain bones only, report)."""
    win = resolve_window(c, n)
    a, e = win["anchor"], win["end"]
    new_tracks: dict[str, list[Q]] = {}
    links_report: list[dict[str, Any]] = []
    for ch in c["chains"]:
        for k, bones in enumerate(ch["links"]):
            for b in bones:
                if b not in rot_tracks:
                    raise ValueError(f"bone {b!r} has no rotation track in the clip")
                new_tracks[b], st = solve_link(rot_tracks[b], k, c, a, e)
                st.update({"bone": b, "link": k})
                links_report.append(st)
    warnings = list(win["warnings"])
    worst = max((s["residual_deg"] for s in links_report), default=0.0)
    if worst > 1.0:
        warnings.append(f"response not settled at the window end (residual {worst} deg): raise settle_frames or lower overshoot_pct")
    return new_tracks, {
        "anchor_kind": c["anchor_kind"], "anchor": a, "window": [a, e], "zeta": round(c["zeta"], 3),
        "overshoot_pct": c["overshoot_pct"], "mass": c["mass"], "links": links_report, "warnings": warnings,
    }


# ---------------------------------------------------------------- dry run (no clip data) and conservation
def plan(c: dict[str, Any], n: int | None) -> dict[str, Any]:
    """Side-effect-free plan: bones, frames, peak degrees. Without the clip rotations the driver is modelled as a limb
    that ARRIVES at the anchor pose from assume_arrival_deg away (3-frame smooth stop) and then holds: the response is
    linear, so peak degrees scale with the real arrival excursion (reported as peak_per_100deg)."""
    assumed = n is None
    nn = n if n is not None else c["anchor_frame"] + c["settle_frames"] + 4
    a = c["anchor_frame"]
    win = resolve_window(c, nn)
    a, e = win["anchor"], win["end"]
    arrive = c["assume_arrival_deg"]
    axis_rot = []
    for f in range(nn):
        ang = -math.radians(arrive) * (1.0 - ss(float(a - 3), float(a), float(f)))
        axis_rot.append(qexp((ang, 0.0, 0.0)))
    links: list[dict[str, Any]] = []
    for ch in c["chains"]:
        for k, bones in enumerate(ch["links"]):
            _, st = solve_link(axis_rot, k, c, a, e)
            for b in bones:
                links.append({
                    "bone": b, "link": k, "delay_frames": st["delay_frames"], "period_frames": st["period_frames"],
                    "frames": st["frames"], "peak_frame": st["peak_frame"], "peak_deg_if_arrival_%gdeg" % arrive: st["peak_deg"],
                    "peak_per_100deg": round(st["peak_deg"] * 100.0 / arrive, 1), "residual_deg": st["residual_deg"],
                })
    touched = [l["frames"] for l in links if l["frames"]]
    warnings = list(win["warnings"])
    if any(l["residual_deg"] > 1.0 for l in links):
        warnings.append("response not settled at the window end: raise settle_frames or lower overshoot_pct")
    return {
        "kind": "follow_through", "anchor_kind": c["anchor_kind"], "anchor": a, "window": [a, e],
        "n_frames": nn, "n_frames_assumed": assumed, "zeta": round(c["zeta"], 3), "overshoot_pct": c["overshoot_pct"],
        "mass": c["mass"], "frames_touched": [min(t[0] for t in touched), max(t[1] for t in touched)] if touched else None,
        "bones": sorted({l["bone"] for l in links}), "links": links, "max_extra_deg": c["max_extra_deg"],
        "note": "dry-run peaks assume the limb arrives at the anchor pose from the stated degrees; the real run reports the measured peaks",
        "warnings": warnings,
    }


def conservation_report(
    orig: dict[str, list[Q]], new: dict[str, list[Q]], chain_bones: Sequence[str], a: int, e: int, tol_deg: float = 0.01
) -> dict[str, Any]:
    """Checks on the DATA (local rotations of every bone in `orig`, `new` holding the edited ones):
    anchor pose identical for every bone, nothing moves outside (a, e), nothing but the chain changes, no forbidden bone."""
    forbidden = sorted(b for b in new if not ALLOWED_BONE.match(b))
    off_chain = sorted(b for b in new if b not in set(chain_bones))
    anchor_diff = max((q_angle_deg(new[b][a], orig[b][a]) for b in new), default=0.0)
    outside = 0.0
    for b in new:
        for f in range(len(orig[b])):
            if a < f < e:
                continue
            outside = max(outside, q_angle_deg(new[b][f], orig[b][f]))
    ok = (not forbidden) and (not off_chain) and anchor_diff <= tol_deg and outside <= tol_deg
    return {
        "ok": ok, "anchor_frame_unchanged": anchor_diff <= tol_deg, "anchor_max_diff_deg": round(anchor_diff, 4),
        "outside_window_max_diff_deg": round(outside, 4), "forbidden_bones": forbidden, "off_chain_bones": off_chain,
        "root_pelvis_legs_feet": "untouched (not in chain; not writable)" if not forbidden else "VIOLATED",
    }
