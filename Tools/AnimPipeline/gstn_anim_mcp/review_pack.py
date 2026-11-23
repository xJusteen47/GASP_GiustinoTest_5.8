"""Compact review pack: dense frames only around the anchor and the FAF, at most 8 images per view, ONE grid per view.

The reviewer gets nothing else (project rule 4). Each grid has one row per run (base on top, variant below) on the SAME
frames, so the A/B is homogeneous. Frames are capture frames (the f<NNN>.png of pie_capture_move.py).
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

MAX_IMAGES = 8


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


def build_grid(runs: dict[str, Path], seq: str, view: str, frames: list[int], out_png: Path, cell: int = 256) -> dict[str, Any]:
    """One PNG: a row per run (dict order, base first), a column per frame, labelled cells. Missing frames stay grey."""
    from PIL import Image, ImageDraw

    cols, rows = len(frames), len(runs)
    sheet = Image.new("RGB", (cols * cell, rows * cell), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    missing: list[str] = []
    for r, (name, cap) in enumerate(runs.items()):
        for c, f in enumerate(frames):
            p = frame_path(cap, seq, view, f)
            x, y = c * cell, r * cell
            if p.exists():
                im = Image.open(p).convert("RGB")
                im.thumbnail((cell, cell))
                sheet.paste(im, (x + (cell - im.width) // 2, y + (cell - im.height) // 2))
            else:
                missing.append(str(p))
            draw.rectangle([x, y, x + 8 * 8 + len(name) * 6, y + 12], fill=(0, 0, 0))
            draw.text((x + 2, y + 1), f"{name} f{f}", fill=(255, 255, 255))
    out_png = Path(out_png)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out_png)
    return {"png": str(out_png), "frames": frames, "rows": list(runs), "missing": missing}


def build_pack(runs: dict[str, Path], sequences: list[str], views: list[str], out_dir: Path, window: tuple[int, int],
               anchor: int | None = None, faf: int | None = None, radius: int = 4, step: int = 1,
               max_images: int = MAX_IMAGES, cell: int = 256) -> dict[str, Any]:
    """All grids of a review: <out_dir>/grid_<seq>_<view>.png, one per sequence and view."""
    frames = select_frames(window, anchor, faf, radius, step, max_images)
    grids = []
    for seq in sequences:
        for v in views:
            grids.append(build_grid(runs, seq, v, frames, Path(out_dir) / f"grid_{seq}_{v}.png", cell))
    return {"frames": frames, "grids": grids, "images_per_view": len(frames)}
