"""anim_silhouette_check: is the pose readable from this view? Projects joints to 2D per view and, per frame, measures the
silhouette hull area and how close each limb end (hands, elbows, feet, knees, head) is to the torso polygon.
A frame is FLAGGED ("unreadable from this view") when >= 2 limb ends are inside / within `near_cm` of the torso.
The torso is the hull of pelvis, spines, neck, clavicles (upperarms when the dump has no clavicles) and hips, grown by
`torso_pad_cm` because the body has depth (a hand in front of the chest is hidden from the side). Offline, from the joints dump.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

import viewproj as vp

NEAR_CM = 3.0
PAD_CM = 7.0


def analyze_view(joints: dict[str, list], f0: int, f1: int, view: str, game_yaw: float, game_pitch: float,
                 near_cm: float = NEAR_CM, pad_cm: float = PAD_CM) -> dict[str, Any]:
    az, pitch = vp.view_angles(view, game_yaw, game_pitch)
    frames = []
    for f in range(f0, f1 + 1):
        allp = [vp.project(p, az, pitch) for k in joints if (p := vp.point(joints, k, f)) is not None]
        torso = [vp.project(p, az, pitch) for k in vp.TORSO_KEYS if (p := vp.point(joints, k, f)) is not None]
        th = vp.hull(torso)
        ends = {}
        for name, key in vp.LIMB_ENDS.items():
            p = vp.point(joints, key, f)
            if p is not None:
                ends[name] = round(vp.signed_dist(vp.project(p, az, pitch), th) - pad_cm, 2)
        hidden = sorted(n for n, d in ends.items() if d <= near_cm)
        frames.append({"f": f, "hull_area_cm2": round(vp.area(vp.hull(allp)), 1), "limb_dist_cm": ends,
                       "hidden": hidden, "flag": len(hidden) >= 2})
    return {"view": view, "az": az, "pitch": pitch, "frames": frames}


def anim_silhouette_check(asset: str, frame_range: Any = None, views: Any = None, base_asset: str | None = None,
                          root: str | Path | None = None, game_yaw: float = 0.0, game_pitch: float = 10.0,
                          near_cm: float = NEAR_CM, torso_pad_cm: float = PAD_CM, max_cells: int = 12) -> dict[str, Any]:
    repo = anim_ops_root(root)
    try:
        data = vp.load_joints(repo, asset)
        base = vp.load_joints(repo, base_asset) if base_asset else None
    except (FileNotFoundError, ValueError) as exc:
        return {"ok": False, "error": str(exc)}
    joints = data["joints"]
    total = len(next(iter(joints.values())))
    f0, f1 = vp.anim_ops._parse_frame_range(frame_range, total)
    views_l = vp.parse_views(views)
    out = vp.out_dir(repo, asset)
    result: dict[str, Any] = {"ok": True, "asset": asset, "base_asset": base_asset, "frame_range": [f0, f1], "views": {}}
    summary: list[str] = []
    for v in views_l:
        a = analyze_view(joints, f0, f1, v, game_yaw, game_pitch, near_cm, torso_pad_cm)
        b = None
        if base is not None:
            bj = base["joints"]
            bt = len(next(iter(bj.values())))
            b = analyze_view(bj, min(f0, bt - 1), min(f1, bt - 1), v, game_yaw, game_pitch, near_cm, torso_pad_cm)
        flagged = [fr["f"] for fr in a["frames"] if fr["flag"]]
        worst = sorted(a["frames"], key=lambda fr: (-len(fr["hidden"]), min(fr["limb_dist_cm"].values(), default=0)))[:5]
        entry = {"flagged_frames": flagged, "worst": [{"f": w["f"], "hidden": w["hidden"]} for w in worst if w["hidden"]],
                 "hull_area_min": min(fr["hull_area_cm2"] for fr in a["frames"]),
                 "hull_area_mean": round(sum(fr["hull_area_cm2"] for fr in a["frames"]) / len(a["frames"]), 1), "frames": a["frames"]}
        line = f"{v}: {len(flagged)}/{len(a['frames'])} frames unreadable" + (f" (worst f{worst[0]['f']}: {','.join(worst[0]['hidden'])})" if flagged else "")
        if b is not None:
            bfl = [fr["f"] for fr in b["frames"] if fr["flag"]]
            ma, mb = entry["hull_area_mean"], sum(fr["hull_area_cm2"] for fr in b["frames"]) / len(b["frames"])
            entry["base"] = {"flagged_frames": bfl, "hull_area_mean": round(mb, 1)}
            entry["delta_vs_base"] = {"flagged": len(flagged) - len(bfl), "hull_area_mean_pct": round(100.0 * (ma - mb) / mb, 1) if mb else None}
            line += f"; vs base {len(flagged) - len(bfl):+d} frames, hull {entry['delta_vs_base']['hull_area_mean_pct']:+}%"
        png = out / f"silhouette_{v}.png"
        draw_sheet(joints, base["joints"] if base else None, a, png, max_cells)
        entry["png"] = str(png)
        result["views"][v] = entry
        summary.append(line)
    jpath = out / "silhouette.json"
    jpath.write_text(__import__("json").dumps(result, indent=1), encoding="utf-8")
    result["details"] = str(jpath)
    for e in result["views"].values():
        e.pop("frames", None)   # per-frame numbers stay in the JSON file, the reply stays small
    result["summary"] = summary[:4] + [f"details: {jpath}"]
    return result


def anim_ops_root(root: str | Path | None) -> Path:
    return vp.anim_ops._repo_root(root)


def draw_sheet(joints: dict, base_joints: dict | None, analysis: dict, png: Path, max_cells: int = 12, cell: int = 170) -> None:
    """Stick figure + silhouette hull per frame; flagged frames have a red frame and red limb ends; base in grey behind."""
    from PIL import Image, ImageDraw
    fr = analysis["frames"]
    flagged = [x["f"] for x in fr if x["flag"]]
    pick = set(flagged[:max_cells // 2])
    rest = [x["f"] for x in fr if x["f"] not in pick]
    n_more = max_cells - len(pick)
    if rest and n_more > 0:
        pick.update(rest[round(i * (len(rest) - 1) / max(1, n_more - 1))] for i in range(min(n_more, len(rest))))
    frames = sorted(pick)
    az, pitch = analysis["az"], analysis["pitch"]
    pts = [vp.project(p, az, pitch) for f in frames for k in joints if (p := vp.point(joints, k, f)) is not None]
    if base_joints:
        bt = len(next(iter(base_joints.values())))
        pts += [vp.project(p, az, pitch) for f in frames if f < bt for k in base_joints if (p := vp.point(base_joints, k, f)) is not None]
    umin, umax = min(p[0] for p in pts), max(p[0] for p in pts)
    vmin, vmax = min(p[1] for p in pts), max(p[1] for p in pts)
    scale = (cell - 28) / max(umax - umin, vmax - vmin, 1.0)
    cols = min(6, len(frames))
    rows = (len(frames) + cols - 1) // cols
    img = Image.new("RGB", (cols * cell, rows * cell), (250, 250, 250))
    d = ImageDraw.Draw(img)
    fnt = vp.font(14)
    by_f = {x["f"]: x for x in fr}

    def xy(p, ox, oy):
        u, v = vp.project(p, az, pitch)
        return (ox + 14 + (u - umin) * scale + ((cell - 28) - (umax - umin) * scale) / 2,
                oy + cell - 14 - (v - vmin) * scale)

    for i, f in enumerate(frames):
        ox, oy = (i % cols) * cell, (i // cols) * cell
        red = by_f[f]["flag"]
        if base_joints and f < len(next(iter(base_joints.values()))):
            for a, b in vp.BONES:
                pa, pb = vp.point(base_joints, a, f), vp.point(base_joints, b, f)
                if pa and pb:
                    d.line([xy(pa, ox, oy), xy(pb, ox, oy)], fill=(170, 170, 170), width=2)
        allp = [xy(p, ox, oy) for k in joints if (p := vp.point(joints, k, f)) is not None]
        h = vp.hull(allp)
        if len(h) >= 3:
            d.polygon(h, outline=(200, 40, 40) if red else (90, 120, 200))
        for a, b in vp.BONES:
            pa, pb = vp.point(joints, a, f), vp.point(joints, b, f)
            if pa and pb:
                d.line([xy(pa, ox, oy), xy(pb, ox, oy)], fill=(20, 20, 20), width=3)
        for name in by_f[f]["hidden"]:
            p = vp.point(joints, vp.LIMB_ENDS[name], f)
            if p:
                x, y = xy(p, ox, oy)
                d.ellipse([x - 6, y - 6, x + 6, y + 6], outline=(220, 0, 0), width=3)
        d.rectangle([ox, oy, ox + cell - 1, oy + cell - 1], outline=(220, 0, 0) if red else (200, 200, 200), width=3 if red else 1)
        d.text((ox + 5, oy + 3), f"f{f}", fill=(220, 0, 0) if red else (0, 0, 0), font=fnt)
    Path(png).parent.mkdir(parents=True, exist_ok=True)
    img.save(png)
