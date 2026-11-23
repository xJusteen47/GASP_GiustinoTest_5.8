"""Quick pose preview (owner 2026-11-24, 1C): base vs variant from the CARDS, offline, in ~1 s, before any editor is opened.

It exists to SCRAP a wrong round early (arms on the wrong side, a key that reads as a ball from behind, a change too small to see), not
to approve one: no Mover root motion, capsule, IK, blend from the jump, hitstop or dummy. Project rule 10 is unchanged: a review starts
only from the in-game capture with ASSERT: OK. A preview image is never shown to the owner as a result (rule 6: it is a PROTO).

    python Tools/AnimPipeline/quick_preview.py <variant_card.json> [--base=<base_card.json>] [--frames=12,15,18 | --frames=keys]
                                               [--azs=90,180,135] [--out=<png>] [--cell=260]

One column per frame, one row per view (side az90, behind az180, quarter az135 by default; azimuth as in pie_capture_move.py: 0 = in
front of the pawn, 90 = on its right). The base is drawn grey under the variant (blue = left limbs, red = right). Frames are the
card's authored frames (author.frames: the poses build bakes, smoothstep between keys). Prints at most 5 lines: the image, the frames,
and the peak hand/foot/head change against the base (the capture gate's (d) "too small to read" threshold, 2 cm, is flagged here first).
Needs the pose rig files of animpipe (Tools/AnimPipeline/sheets/raw, present on the PCs, not in git) and Pillow + numpy.
"""
from __future__ import annotations

import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

END_JOINTS = ("hand_l", "hand_r", "foot_l", "foot_r", "head")
MIN_MOVE_CM = 2.0          # same as capture_move.precapture_gate min_move_cm
LEFT, RIGHT, MID, BASE = (40, 80, 230), (220, 50, 40), (60, 60, 60), (185, 185, 185)


def view_axes(az_deg: float) -> tuple[np.ndarray, np.ndarray]:
    """(screen right, view direction) in the mesh's component space (forward +Y, the pawn's right -X, up +Z) for a camera at azimuth az."""
    a = math.radians(az_deg)
    fwd, right, up = np.array([0.0, 1.0, 0.0]), np.array([-1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0])
    to_cam = math.cos(a) * fwd + math.sin(a) * right
    look = -to_cam
    return np.cross(up, look), look


def project(points: dict[str, np.ndarray], az: float, centre: np.ndarray) -> dict[str, tuple[float, float, float]]:
    """joint -> (screen x cm, screen y cm up, depth cm away from the camera)."""
    sx, look = view_axes(az)
    return {b: (float(np.dot(p - centre, sx)), float(p[2] - centre[2]), float(np.dot(p - centre, look))) for b, p in points.items()}


def colour(bone: str) -> tuple[int, int, int]:
    return LEFT if bone.endswith("_l") else RIGHT if bone.endswith("_r") else MID


def draw_cell(draw, ox: int, oy: int, cell: int, scale: float, figures: list, parent: dict[str, str | None]) -> None:
    """figures: [(projected joints, colour or None for per-side colours, width)], drawn far to near per segment."""
    segs = []
    for pts, col, width in figures:
        for b, p in parent.items():
            if p and b in pts and p in pts:
                depth = (pts[b][2] + pts[p][2]) / 2.0
                segs.append((-depth if col is None else 1e9, pts[p], pts[b], col or colour(b), width))
    for _, a, b, col, width in sorted(segs, key=lambda s: s[0]):
        draw.line([(ox + cell / 2 + a[0] * scale, oy + cell * 0.55 - a[1] * scale),
                   (ox + cell / 2 + b[0] * scale, oy + cell * 0.55 - b[1] * scale)], fill=col, width=width)


def peak_change(var: dict[int, dict[str, np.ndarray]], base: dict[int, dict[str, np.ndarray]]) -> tuple[float, str, int]:
    best = (0.0, "", -1)
    for f, pv in var.items():
        pb = base.get(f)
        if not pb:
            continue
        for j in END_JOINTS:
            if j in pv and j in pb:
                d = float(np.linalg.norm((pv[j] - pv["pelvis"]) - (pb[j] - pb["pelvis"])))
                if d > best[0]:
                    best = (d, j, f)
    return best


def render(var: dict[int, dict[str, np.ndarray]], base: dict[int, dict[str, np.ndarray]] | None, frames: list[int], azs: list[float],
           parent: dict[str, str | None], out: str, cell: int = 260, title: str = "") -> str:
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (cell * len(frames), cell * len(azs) + 18), "white")
    d = ImageDraw.Draw(img)
    d.text((4, 3), f"PROTO preview (not a review): {title}  grey = base, blue = left, red = right", fill=(0, 0, 0))
    scale = cell / 230.0     # ~2.3 m across a cell
    for c, f in enumerate(frames):
        pv = var[f]
        centre = pv["pelvis"].copy()
        for r, az in enumerate(azs):
            ox, oy = c * cell, 18 + r * cell
            figs = []
            if base and f in base:
                figs.append((project(base[f], az, base[f]["pelvis"]), BASE, 5))
            figs.append((project(pv, az, centre), None, 3))
            draw_cell(d, ox, oy, cell, scale, figs, parent)
            d.rectangle([ox, oy, ox + cell - 1, oy + cell - 1], outline=(210, 210, 210))
            d.text((ox + 4, oy + 3), f"f{f} az{az:g}", fill=(0, 0, 0))
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    img.save(out)
    return out


def card_joints(card_path: str, frames: list[int] | None = None) -> tuple[dict[int, dict[str, np.ndarray]], list[int], dict]:
    """Component-space joint positions of the card's authored frames (animpipe author.Poser), and the card's key frames."""
    from animpipe import author as A
    card = json.load(open(card_path, encoding="utf-8"))
    au = card["author"]
    poser = A.Poser(au.get("thumb", "along"))
    per_frame = A.frames(au)
    keys = sorted(int(k) for k in au["poses"])
    want = frames if frames is not None else sorted(per_frame)
    out = {}
    for f in want:
        if f in per_frame:
            P, _R = poser.solve(per_frame[f])
            out[f] = {b: np.asarray(p, float) for b, p in P.items()}
    return out, keys, card


def main(argv: list[str]) -> int:
    opts = dict((a[2:].split("=", 1) + [""])[:2] for a in argv if a.startswith("--"))
    args = [a for a in argv if not a.startswith("--")]
    if not args:
        print(__doc__.strip().splitlines()[3].strip())
        return 2
    from animpipe import author as A
    var_all, keys, card = card_joints(args[0])
    fr = opts.get("frames", "keys")
    frames = keys if fr in ("", "keys") else [int(x) for x in fr.split(",") if x.strip()]
    frames = [f for f in frames if f in var_all][:10]
    base = card_joints(opts["base"], frames)[0] if opts.get("base") else None
    azs = [float(x) for x in opts.get("azs", "90,180,135").split(",")]
    move = card.get("move", os.path.splitext(os.path.basename(args[0]))[0])
    out = opts.get("out") or os.path.join(HERE, "..", "..", "Saved", "AnimPipeline", "preview", f"{move}_preview.png")
    render(var_all, base, frames, azs, A.PARENT, os.path.abspath(out), int(opts.get("cell", 260)), move)
    print(f"preview: {os.path.abspath(out)}")
    print(f"frames {frames}, views az{','.join(f'{a:g}' for a in azs)}")
    if base:
        cm, joint, f = peak_change({f: var_all[f] for f in frames}, base)
        flag = "  TOO SMALL TO READ: scrap or enlarge before capturing" if cm < MIN_MOVE_CM else ""
        print(f"peak change vs base (pelvis relative): {cm:.1f} cm, {joint} on f{f}{flag}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
