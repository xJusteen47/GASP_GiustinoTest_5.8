"""Measured check (owner 2026-11-19, scope: realistic dust particles - landing skirt/wisps and every move that reuses the dust look):
a dust particle never shrinks. Its drawn size must be non-decreasing over its whole life; it vanishes only through opacity.

Models what M_GSTN_VFX_Puff draws for one MI preset of setup_presentation_vfx_puff.PRESETS: the flipbook frame at life t
(t * FlipRate over 16 frames, two frames blended) times the quad scale of V3._life_size (lerp(StartScale, 1, easeOut(t / GrowEnd)) *
lerp(1, EndScale, ramp(t, ShrinkStart, 1))). Two extents per life step, in cell units (equivalent radius of the covered area):
  geo = pixels with flipbook alpha > 0.06 (the silhouette): must be non-decreasing (hard check, PASS/FAIL)
  vis = pixels with alpha * fade(t) * OpacityMax > 0.03 (what the eye still sees): reported up to 95 % of life (REVIEW-A2r5: the dark shrink was at 75-95 %), where the fade
        is still readable; a drop there reads as shrinking even when geo grows
Niagara side: the dust emitters size the sprite only at spawn (InitializeParticle + ScaleSpriteSize in ParticleSpawn), no update-stage
size module: checked by the setup log, not here.
Usage: python Tools/GASNPPTest/vfx_size_monotonic.py [Preset ...] [--out file]   (default SmokeReal SmokeHaze PuffClump; <= 5 lines)
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "GAS"))
TEX = os.path.join(ROOT, "Tools", "GAS", "vfx")
FRAMES, GRID, CELL = 16, 8, 128


def _preset(name):
    """The MI parameters, read from the setup module's source (it imports unreal, so it cannot be imported here)"""
    src = open(os.path.join(ROOT, "Tools", "GAS", "setup_presentation_vfx_puff.py"), encoding="utf-8").read()
    i = src.index(f'"{name}": dict(')
    j, depth = src.index("(", i), 0
    for k in range(j, len(src)):
        depth += {"(": 1, ")": -1}.get(src[k], 0)
        if depth == 0:
            break
    return eval("dict" + src[j:k + 1])  # noqa: S307 - our own source


def _frames(tex):
    a = np.asarray(Image.open(os.path.join(TEX, tex + ".png")).convert("RGBA"), dtype=np.float32)[..., 3] / 255.0
    return [[a[(k // GRID) * CELL:(k // GRID + 1) * CELL, (k % GRID) * CELL:(k % GRID + 1) * CELL]
             for k in (v * FRAMES + f for f in range(FRAMES))] for v in range(4)]


def _ramp(t, a):
    return 0.0 if a >= 1 else min(max((t - a) / (1 - a), 0.0), 1.0)


def _radius(alpha, thr, scale):
    return np.sqrt((alpha > thr).sum() / np.pi) / CELL * scale


def check(name):
    p = dict(StartScale=1.0, GrowEnd=0.05, ShrinkStart=0.0, EndScale=0.0, FadeStart=1.0, FadeCurve=1.0, OpacityMax=1.0, FlipRate=1.0,
             tex="T_GSTN_VFX_PuffFB")
    p.update(_preset(name))
    variants = _frames(p["tex"])
    worst_geo, worst_vis, rows = 0.0, 0.0, []
    for v, frames in enumerate(variants):
        prev_g = prev_v = None
        for s in range(61):
            t = s / 60
            fpos = min(t * p["FlipRate"], 1.0) * (FRAMES - 1)
            f0, w = int(fpos), fpos - int(fpos)
            alpha = frames[f0] * (1 - w) + frames[min(f0 + 1, FRAMES - 1)] * w
            g = min(t / max(p["GrowEnd"], 1e-4), 1.0)
            scale = (p["StartScale"] + (1 - p["StartScale"]) * (1 - (1 - g) ** 2)) * (1 + (p["EndScale"] - 1) * _ramp(t, p["ShrinkStart"]))
            fade = 1 - _ramp(t, p["FadeStart"]) ** p["FadeCurve"]
            geo = _radius(alpha, 0.06, scale)
            vis = _radius(alpha * fade * p["OpacityMax"], 0.03, scale)
            if prev_g is not None:
                worst_geo = max(worst_geo, (prev_g - geo) / max(prev_g, 1e-6))
                if t <= 0.95:
                    worst_vis = max(worst_vis, (prev_v - vis) / max(prev_v, 1e-6))
            prev_g, prev_v = geo, vis
            rows.append(f"{name} v{v} t={t:.3f} scale={scale:.3f} fade={fade:.3f} geo={geo:.4f} vis={vis:.4f}")
    # 0.5 % tolerance: pixel counting noise between two blended frames
    return worst_geo <= 0.005, worst_geo, worst_vis, rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else os.path.join(ROOT, "Saved", "VfxV3", "size_monotonic.txt")
    if "--out" in sys.argv:
        args.remove(out)
    names = args or ["SmokeReal", "SmokeHaze", "PuffClump"]
    os.makedirs(os.path.dirname(out), exist_ok=True)
    lines, ok_all = [], True
    for n in names:
        ok, wg, wv, rows = check(n)
        ok_all &= ok
        lines += rows
        print(f"{n}: {'PASS' if ok else 'FAIL'} silhouette max drop {wg * 100:.1f}% per step, visible (to 95% life, info) max drop {wv * 100:.1f}%")
    open(out, "w", encoding="utf-8").write("\n".join(lines))
    print(f"ESITO: {'PASS' if ok_all else 'FAIL'} -> {out}")
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
