"""anim_motion_overlay: trails of wrists, ankles, head and pelvis with per-frame velocity arrows and arc tangents, per view.

Flags (computed in 3D, so they hold in every view; drawn on each view):
  arc break: the direction of travel turns by > break_deg between consecutive frames while both speeds are > min_speed;
  pop: the speed at a frame is > pop_ratio x BOTH neighbours (and > min_speed): a one-frame spike;
  STOP (skill audit 2026-11-23, option A): the whole body is still on that step (no_stop_gate / animpipe curves: every tracked
  point under stop_cm per frame, limbs relative to the pelvis), outside the declared holds. A stop is the defect the eye misses
  in a still grid (it reads as a held pose), so it gets a filled red dot on the head and pelvis trails and a red band in the
  SPEED STRIP under the trails: per-frame speed (cm/frame) of wrists, ankles, head, pelvis and the whole-body step (black),
  with the stop threshold dashed. Runs longer than still_run are what the capture gate blocks.
Images are for the reviewer and must read at small size: thick lines, big dots, big labels, 5 colours.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any

import no_stop_gate
import viewproj as vp

TRACKS = {"wrist_l": "hand_MainPoint_l", "wrist_r": "hand_MainPoint_r", "ankle_l": "foot_MainPoint_l",
          "ankle_r": "foot_MainPoint_r", "head": "head", "pelvis": "pelvis"}
COLORS = {"wrist_l": (0, 110, 220), "wrist_r": (230, 120, 0), "ankle_l": (0, 160, 90), "ankle_r": (160, 60, 200),
          "head": (200, 30, 30), "pelvis": (60, 60, 60)}


def _sub(a, b):
    return [a[i] - b[i] for i in range(3)]


def _norm(v):
    return math.sqrt(sum(x * x for x in v))


def _angle(u, v) -> float:
    nu, nv = _norm(u), _norm(v)
    if nu < 1e-9 or nv < 1e-9:
        return 0.0
    return math.degrees(math.acos(max(-1.0, min(1.0, sum(u[i] * v[i] for i in range(3)) / (nu * nv)))))


def track_flags(series: list[list[float]], f0: int, fps: float, min_speed_cm_s: float, break_deg: float,
                pop_ratio: float) -> dict[str, Any]:
    """series: positions per frame of f0..f1. Returns per-segment speeds (cm/s) and the flags (clip frames)."""
    vel = [_sub(series[i + 1], series[i]) for i in range(len(series) - 1)]
    spd = [_norm(v) * fps for v in vel]
    breaks, pops = [], []
    for i in range(1, len(vel)):
        if spd[i - 1] > min_speed_cm_s and spd[i] > min_speed_cm_s:
            ang = _angle(vel[i - 1], vel[i])
            if ang > break_deg:
                breaks.append({"f": f0 + i, "deg": round(ang, 1)})
    for i in range(1, len(spd) - 1):
        nb = max(spd[i - 1], spd[i + 1])
        if spd[i] > min_speed_cm_s and spd[i] > pop_ratio * nb:
            pops.append({"f": f0 + i, "speed": round(spd[i], 1), "neighbours": round(nb, 1)})
    return {"speed_cm_s": [round(s, 1) for s in spd], "arc_breaks": breaks, "pops": pops,
            "peak_speed_cm_s": round(max(spd, default=0.0), 1)}


def anim_motion_overlay(asset: str, frame_range: Any = None, views: Any = None, base_asset: str | None = None,
                        root: str | Path | None = None, game_yaw: float = 0.0, game_pitch: float = 10.0,
                        min_speed_cm_s: float = 30.0, break_deg: float = 60.0, pop_ratio: float = 3.0,
                        arrow_step: int = 1, out: str | Path | None = None, holds: Any = None,
                        stop_cm: float = no_stop_gate.DEFAULTS["stop_cm"],
                        still_run: int = no_stop_gate.DEFAULTS["still_run"], prefix: str = "motion") -> dict[str, Any]:
    repo = vp.anim_ops._repo_root(root)
    try:
        data = vp.load_joints(repo, asset)
        base = vp.load_joints(repo, base_asset) if base_asset else None
    except (FileNotFoundError, ValueError) as exc:
        return {"ok": False, "error": str(exc)}
    joints = data["joints"]
    fps = float(data.get("fps") or 30)
    total = len(next(iter(joints.values())))
    f0, f1 = vp.anim_ops._parse_frame_range(frame_range, total)
    if f1 - f0 < 2:
        return {"ok": False, "error": "frame range too short (need >= 3 frames)"}
    tracks = {t: joints[k][f0:f1 + 1] for t, k in TRACKS.items() if k in joints}
    if not tracks:
        return {"ok": False, "error": "joints dump has none of hand/foot/head/pelvis"}
    flags = {t: track_flags(s, f0, fps, min_speed_cm_s, break_deg, pop_ratio) for t, s in tracks.items()}
    btracks = {}
    if base:
        bj = base["joints"]
        bt = len(next(iter(bj.values())))
        btracks = {t: bj[k][min(f0, bt - 1):min(f1, bt - 1) + 1] for t, k in TRACKS.items() if k in bj}
    ns = no_stop_gate.check(joints, holds, None, stop_cm, no_stop_gate.DEFAULTS["max_ratio"], still_run)
    body = ns["steps"][f0:f1]                                  # whole-body cm per step f -> f+1, f in f0..f1-1
    stops = [f for f in ns["stills"] if f0 <= f < f1]
    long_runs = [[a, b + 1] for a, b in ns["still_runs"] if b >= f0 and a < f1]
    if out:
        out = Path(out) if Path(out).is_absolute() else repo / out
        out.mkdir(parents=True, exist_ok=True)
    else:
        out = vp.out_dir(repo, asset)
    result: dict[str, Any] = {"ok": True, "asset": asset, "base_asset": base_asset, "frame_range": [f0, f1], "fps": fps,
                              "flags": {t: {"arc_breaks": fl["arc_breaks"], "pops": fl["pops"], "peak_speed_cm_s": fl["peak_speed_cm_s"]}
                                        for t, fl in flags.items()}, "images": {},
                              "stops": stops, "still_runs_blocking": long_runs, "stop_cm": stop_cm}
    for v in vp.parse_views(views):
        png = out / f"{prefix}_{v}.png"
        draw_view(tracks, btracks, flags, f0, v, game_yaw, game_pitch, png, arrow_step, fps=fps, body=body, stops=stops,
                  stop_cm=stop_cm, title=asset.rsplit("/", 1)[-1])
        result["images"][v] = str(png)
    nb = sum(len(fl["arc_breaks"]) for fl in flags.values())
    npop = sum(len(fl["pops"]) for fl in flags.values())
    worst = [f"{t} f{b['f']} {b['deg']}deg" for t, fl in flags.items() for b in fl["arc_breaks"]][:3]
    worst_p = [f"{t} f{p['f']}" for t, fl in flags.items() for p in fl["pops"]][:3]
    jpath = out / f"{prefix}.json"
    jpath.write_text(json.dumps(dict(result, speeds={t: fl["speed_cm_s"] for t, fl in flags.items()}, body_cm=body), indent=1),
                     encoding="utf-8")
    result["details"] = str(jpath)
    result["summary"] = [f"{nb} arc breaks, {npop} pops, {len(stops)} stop steps over f{f0}-{f1}"
                         + (f" (BLOCKING still runs {long_runs})" if long_runs else ""),
                         "arc breaks: " + (", ".join(worst) or "none"), "pops: " + (", ".join(worst_p) or "none"),
                         "peak cm/s: " + " ".join(f"{t}={fl['peak_speed_cm_s']:g}" for t, fl in flags.items()),
                         f"details: {jpath}"]
    return result


def _speed_strip(d, tracks, body, stops, f0, n, fps, stop_cm, x0, y0, W, h, font) -> None:
    """Per-frame speed under the trails: one line per track (cm/frame, limbs in component space), the whole-body step in thick
    black (what the stop gate measures), the stop threshold dashed, a red band on every STOP step. Same x for the same frame."""
    pad_l, pad_r, top, bot = 50, 20, 26, 22
    pw, ph = W - pad_l - pad_r, h - top - bot
    sp = {t: [math.dist(s[i], s[i + 1]) for i in range(len(s) - 1)] for t, s in tracks.items()}
    vmax = max([stop_cm * 2] + [v for vs in sp.values() for v in vs] + list(body)) * 1.05
    steps = max(1, n - 1)

    def X(i):   # step i (frame f0+i -> f0+i+1) drawn at its middle
        return x0 + pad_l + (i + 0.5) * pw / steps

    def Y(v):
        return y0 + top + ph - min(v, vmax) / vmax * ph

    d.rectangle([x0 + pad_l, y0 + top, x0 + pad_l + pw, y0 + top + ph], outline=(160, 160, 160), width=1)
    for f in stops:
        i = f - f0
        if 0 <= i < steps:
            d.rectangle([x0 + pad_l + i * pw / steps, y0 + top, x0 + pad_l + (i + 1) * pw / steps, y0 + top + ph], fill=(255, 205, 205))
    ty = Y(stop_cm)
    for xx in range(int(x0 + pad_l), int(x0 + pad_l + pw), 12):
        d.line([(xx, ty), (xx + 6, ty)], fill=(200, 0, 0), width=2)
    for t, vs in sp.items():
        if len(vs) > 1:
            d.line([(X(i), Y(v)) for i, v in enumerate(vs)], fill=COLORS[t], width=2)
    if len(body) > 1:
        d.line([(X(i), Y(v)) for i, v in enumerate(body)], fill=(0, 0, 0), width=4)
    for i in range(0, steps + 1, 5 if steps > 12 else 1):
        d.text((x0 + pad_l + i * pw / steps - 6, y0 + top + ph + 3), f"{f0 + i}", fill=(0, 0, 0), font=font)
    d.text((x0 + 4, y0 + top - 4), f"{vmax:.0f}", fill=(0, 0, 0), font=font)
    d.text((x0 + 4, ty - 8), f"{stop_cm:g}", fill=(200, 0, 0), font=font)
    d.text((x0 + pad_l, y0 + 4), f"SPEED cm/frame (x{fps:g} = cm/s): black = whole body, dashed = stop, red band = STOP",
           fill=(0, 0, 0), font=font)


def _arrow(d, p0, p1, color, width=3):
    d.line([p0, p1], fill=color, width=width)
    ang = math.atan2(p1[1] - p0[1], p1[0] - p0[0])
    for s in (-0.5, 0.5):
        d.line([p1, (p1[0] - 8 * math.cos(ang + s), p1[1] - 8 * math.sin(ang + s))], fill=color, width=width)


def draw_view(tracks, btracks, flags, f0, view, game_yaw, game_pitch, png: Path, arrow_step: int = 1,
              W: int = 720, H: int = 560, fps: float = 30.0, body: list | None = None, stops: list | None = None,
              stop_cm: float = no_stop_gate.DEFAULTS["stop_cm"], title: str = "", strip_h: int = 190) -> None:
    from PIL import Image, ImageDraw
    az, pitch = vp.view_angles(view, game_yaw, game_pitch)
    P = {t: [vp.project(p, az, pitch) for p in s] for t, s in tracks.items()}
    B = {t: [vp.project(p, az, pitch) for p in s] for t, s in btracks.items()}
    def _extent(s):
        return max(max(q[0] for q in s) - min(q[0] for q in s), max(q[1] for q in s) - min(q[1] for q in s))

    moving = [s for s in list(P.values()) + list(B.values()) if _extent(s) > 4.0]   # frame on what moves: static joints may fall off the image
    allp = [q for s in (moving or list(P.values())) for q in s]
    umin, umax = min(q[0] for q in allp), max(q[0] for q in allp)
    vmin, vmax = min(q[1] for q in allp), max(q[1] for q in allp)
    m = 0.12 * max(umax - umin, vmax - vmin, 10.0)
    umin, umax, vmin, vmax = umin - m, umax + m, vmin - m, vmax + m
    pad = 50
    sc = min((W - 2 * pad) / max(umax - umin, 1.0), (H - 2 * pad) / max(vmax - vmin, 1.0))

    def xy(q):
        return (pad + (q[0] - umin) * sc + ((W - 2 * pad) - (umax - umin) * sc) / 2,
                H - pad - (q[1] - vmin) * sc - ((H - 2 * pad) - (vmax - vmin) * sc) / 2)

    stops = list(stops or [])
    img = Image.new("RGB", (W, H + (strip_h if body is not None else 0)), (252, 252, 252))
    d = ImageDraw.Draw(img)
    big, small = vp.font(20), vp.font(15)
    for t, s in B.items():
        d.line([xy(q) for q in s], fill=(185, 185, 185), width=2)
    for t, s in P.items():
        col = COLORS[t]
        pts = [xy(q) for q in s]
        d.line(pts, fill=col, width=3)
        for i, p in enumerate(pts):
            r = 5 if (f0 + i) % 5 == 0 else 2
            d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=col)
        for i in range(0, len(pts) - 1, max(1, arrow_step)):   # velocity arrow = the frame step, stretched x2.5 to read
            a, b = pts[i], pts[i + 1]
            _arrow(d, a, (a[0] + 2.5 * (b[0] - a[0]), a[1] + 2.5 * (b[1] - a[1])), col, 2)
        for i in range(1, len(pts) - 1, max(1, 4)):   # arc tangent at every 4th frame: thin line through the point
            tx, ty = pts[i + 1][0] - pts[i - 1][0], pts[i + 1][1] - pts[i - 1][1]
            n = math.hypot(tx, ty) or 1.0
            d.line([(pts[i][0] - 10 * tx / n, pts[i][1] - 10 * ty / n), (pts[i][0] + 10 * tx / n, pts[i][1] + 10 * ty / n)], fill=(0, 0, 0), width=1)
        d.text((pts[-1][0] + 6, pts[-1][1] - 8), t, fill=col, font=small)
        for br in flags[t]["arc_breaks"]:
            p = pts[br["f"] - f0]
            d.ellipse([p[0] - 11, p[1] - 11, p[0] + 11, p[1] + 11], outline=(230, 0, 0), width=4)
            d.text((p[0] + 12, p[1] - 22), f"f{br['f']} {br['deg']:.0f}d", fill=(230, 0, 0), font=small)
        for pp in flags[t]["pops"]:
            p = pts[pp["f"] - f0]
            d.rectangle([p[0] - 10, p[1] - 10, p[0] + 10, p[1] + 10], outline=(230, 0, 230), width=4)
            d.text((p[0] + 12, p[1] + 6), f"pop f{pp['f']}", fill=(230, 0, 230), font=small)
        if t in ("head", "pelvis"):   # STOP: whole body still on step f -> f+1, drawn on frame f
            for k, f in enumerate(stops):
                if 0 <= f - f0 < len(pts):
                    p = pts[f - f0]
                    d.ellipse([p[0] - 9, p[1] - 9, p[0] + 9, p[1] + 9], fill=(230, 0, 0))
                    if t == "head" and (k == 0 or stops[k - 1] != f - 1):
                        d.text((p[0] + 10, p[1] - 26), f"STOP f{f}", fill=(230, 0, 0), font=small)
    n = len(next(iter(P.values())))
    d.text((8, 6), f"{title + '  ' if title else ''}{view}  f{f0}-{f0 + n - 1}  red ring=arc break, magenta box=pop, "
           f"red dot=STOP" + ("  grey=base" if B else ""), fill=(0, 0, 0), font=big)
    if body is not None:
        _speed_strip(d, tracks, body, stops, f0, n, fps, stop_cm, 0, H, W, strip_h, small)
    Path(png).parent.mkdir(parents=True, exist_ok=True)
    img.save(png)
