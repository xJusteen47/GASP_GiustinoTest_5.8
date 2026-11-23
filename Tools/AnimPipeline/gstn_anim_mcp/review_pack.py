"""Compact review pack: dense frames only around the anchor and the FAF, at most 8 images per view, ONE grid per view.

The reviewer gets nothing else (project rule 4). Each grid has one row per run (base on top, variant below) on the SAME
frames, so the A/B is homogeneous. Frames are capture frames (the f<NNN>.png of pie_capture_move.py).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

_GASNPP = str(Path(__file__).resolve().parents[2] / "GASNPPTest")
if _GASNPP not in sys.path:
    sys.path.insert(0, _GASNPP)
import capture_overlay  # noqa: E402

MAX_IMAGES = 8


def load_rows(cap_dir: Path, seq: str) -> dict[int, dict[str, Any]]:
    """frames.jsonl of a run's sequence by capture frame ({} when missing)."""
    p = Path(cap_dir) / seq / "frames.jsonl"
    if not p.exists():
        return {}
    rows = {}
    for line in p.read_text(encoding="utf-8").splitlines():
        if line.strip():
            r = json.loads(line)
            rows[int(r["f"])] = r
    return rows


def montage_start(rows: dict[int, dict[str, Any]], clip: str | None = None) -> int | None:
    """First capture frame whose montage is playing (the named clip when given): the A/B alignment point."""
    for f in sorted(rows):
        m = rows[f].get("montage")
        if m and m[0] != "error" and (clip is None or m[0] == clip):
            return f
    return None


def run_offsets(runs: dict[str, Path], seq: str, clips: dict[str, str] | None = None) -> dict[str, Any]:
    """Frame offset of every run against the first (base) one so their montages start on the same grid column.

    Why (dropkick K7 review, 2026-11-23): an airborne press fires on a height threshold, so a variant can start its move a frame
    later than the base (whiff T v138 vs v137) and the A/B compared different animation frames. Offset = start(run) - start(base);
    0 when a start is unknown (reported, never guessed)."""
    starts = {n: montage_start(load_rows(d, seq), (clips or {}).get(n)) for n, d in runs.items()}
    names = list(runs)
    b = starts[names[0]] if names else None
    offs = {n: (starts[n] - b) if (b is not None and starts[n] is not None) else 0 for n in names}
    unknown = [n for n in names if starts[n] is None]
    return {"starts": starts, "offsets": offs, "unknown": unknown}


def select_frames(window: tuple[int, int], anchor: int | None = None, faf: int | None = None, radius: int = 4,
                  step: int = 1, max_images: int = MAX_IMAGES) -> list[int]:
    """Frames of the pack, sorted, unique, inside `window`, at most `max_images`.

    With centers (anchor, faf): the budget is split between them, each takes offsets 0, -step, +step, -2*step... within
    `radius`. Without centers: evenly spaced over the window. The anchor and the FAF are always in the pack."""
    a, b = int(window[0]), int(window[1])
    if b < a:
        raise ValueError(f"inverted window {window!r}")
    max_images = max(1, int(max_images))
    centers = [c for c in (anchor, faf) if c is not None and a <= int(c) <= b]
    if not centers:
        n = min(max_images, b - a + 1)
        if n == 1:
            return [a]
        return sorted({a + round(i * (b - a) / (n - 1)) for i in range(n)})
    centers = sorted(set(int(c) for c in centers))
    budget = max(1, max_images // len(centers))
    out: set[int] = set()
    for c in centers:
        offs = [0]
        k = 1
        while k * step <= radius:
            offs += [-k * step, k * step]
            k += 1
        picked = [c + o for o in offs if a <= c + o <= b][:budget]
        out.update(picked)
    res = sorted(out)
    if len(res) > max_images:
        res = sorted(sorted(res, key=lambda f: min(abs(f - c) for c in centers))[:max_images])
    return res


def frame_path(cap_dir: Path, seq: str, view: str, frame: int) -> Path:
    return Path(cap_dir) / seq / view / f"f{frame:03d}.png"


def build_grid(runs: dict[str, Path], seq: str, view: str, frames: list[int], out_png: Path, cell: int = 256,
               offsets: dict[str, int] | None = None, hitboxes: bool = False) -> dict[str, Any]:
    """One PNG: a row per run (dict order, base first), a column per frame, labelled cells. Missing frames stay grey.

    `frames` are base frames; a run with offset k shows frame f+k in the same column (montage-aligned A/B), labelled with both.
    hitboxes=True draws the logged hitboxes (rows with 'hb' and 'cams') into the cell, label hb<N>. Off by default: the animation
    review stays clean (owner 2026-11-23); capture_move draws them only in the separate hitbox videos."""
    from PIL import Image, ImageDraw

    offsets = offsets or {}
    cols, rows = len(frames), len(runs)
    sheet = Image.new("RGB", (cols * cell, rows * cell), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    missing: list[str] = []
    hb_cells = 0
    for r, (name, cap) in enumerate(runs.items()):
        k = int(offsets.get(name, 0))
        jrows = load_rows(cap, seq) if hitboxes else {}
        for c, f0 in enumerate(frames):
            f = f0 + k
            p = frame_path(cap, seq, view, f)
            x, y = c * cell, r * cell
            n_hb = 0
            if p.exists():
                im = Image.open(p).convert("RGB")
                if f in jrows:
                    n_hb = capture_overlay.draw(im, jrows[f], view)
                im.thumbnail((cell, cell))
                sheet.paste(im, (x + (cell - im.width) // 2, y + (cell - im.height) // 2))
            else:
                missing.append(str(p))
            hb_cells += 1 if n_hb else 0
            text = f"{name} f{f}" + (f" (base f{f0})" if k else "") + (f" hb{n_hb}" if n_hb else "")
            draw.rectangle([x, y, x + 8 + len(text) * 6, y + 12], fill=(0, 0, 0))
            draw.text((x + 2, y + 1), text, fill=(255, 255, 255))
    out_png = Path(out_png)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out_png)
    return {"png": str(out_png), "frames": frames, "rows": list(runs), "missing": missing, "offsets": dict(offsets),
            "hitbox_cells": hb_cells}


def build_pack(runs: dict[str, Path], sequences: list[str], views: list[str], out_dir: Path, window: tuple[int, int],
               anchor: int | None = None, faf: int | None = None, radius: int = 4, step: int = 1,
               max_images: int = MAX_IMAGES, cell: int = 256, align: bool = True,
               clips: dict[str, str] | None = None) -> dict[str, Any]:
    """All grids of a review: <out_dir>/grid_<seq>_<view>.png, one per sequence and view, montage-aligned per sequence
    (align=False keeps raw capture frames). clips = {run: montage name} when the runs play different clips."""
    frames = select_frames(window, anchor, faf, radius, step, max_images)
    grids, alignment = [], {}
    for seq in sequences:
        al = run_offsets(runs, seq, clips) if align else {"starts": {}, "offsets": {}, "unknown": []}
        alignment[seq] = al
        for v in views:
            grids.append(build_grid(runs, seq, v, frames, Path(out_dir) / f"grid_{seq}_{v}.png", cell, al["offsets"]))
    return {"frames": frames, "grids": grids, "images_per_view": len(frames), "alignment": alignment}
