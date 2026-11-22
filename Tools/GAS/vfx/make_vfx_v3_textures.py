"""VFX v3 (docs/notes/stile/vfx-stile-v3-design-sheet.md): procedural textures, no external assets. Writes PNGs next to this file.

T_GSTN_VFX_Star.png  256x256  hit star-burst (astro_1): 8 sharp tapered rays (4 long, 4 short) + a small round core.
                              R = white core (inner part of the shape), G = coloured fringe (outer rim of the shape), A = shape.
T_GSTN_VFX_Streak.png 64x256  short streak / laser line: a thin tapered line along V, bright head at the top. R = core, G = fringe, A = shape.
T_GSTN_VFX_Smoke.png 512x512  2x2 atlas of soft puffs (astro_3): R = density (opacity), G = self-shading (lit from the top, 1 = lit),
                              B = erosion noise (the material erodes the puff with age: threshold rises over B).
Run: python -I Tools/GAS/vfx/make_vfx_v3_textures.py   (prints one line)
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
RNG = np.random.default_rng(7)


def _smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def _rgba(r, g, b, a):
    img = np.stack([r, g, b, a], -1)
    return Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")


def star(size=256):
    """Ray k at angle k*45 deg: half-width tapers linearly from w0 at the centre to 0 at length L (long or short). Edges are sharp (1.5 px AA)"""
    y, x = (np.mgrid[0:size, 0:size] + 0.5 - size / 2) / (size / 2)  # -1..1
    r = np.hypot(x, y)
    ang = np.arctan2(y, x)
    shape = np.zeros_like(r)
    aa = 1.5 / (size / 2)
    for k in range(8):
        a = k * np.pi / 4 + np.pi / 8  # rays off the axes: never reads as a "+"
        length = 0.96 if k % 2 == 0 else 0.62
        w0 = 0.15 if k % 2 == 0 else 0.12
        d = np.angle(np.exp(1j * (ang - a)))
        along = r * np.cos(d)
        across = np.abs(r * np.sin(d))
        half = w0 * np.clip(1 - along / length, 0, 1)
        ray = (along > 0) * (along < length) * _smooth(half - across, -aa, aa)
        shape = np.maximum(shape, ray)
    core_disc = _smooth(0.26 - r, -aa, aa)
    shape = np.maximum(shape, core_disc)
    # Inner white: the same star shrunk toward the centre (rays at 55% width, 80% length) -> the fringe is the outer rim of every ray
    inner = np.zeros_like(r)
    for k in range(8):
        a = k * np.pi / 4 + np.pi / 8
        length = (0.96 if k % 2 == 0 else 0.62) * 0.9
        w0 = (0.15 if k % 2 == 0 else 0.12) * 0.72
        d = np.angle(np.exp(1j * (ang - a)))
        along = r * np.cos(d)
        across = np.abs(r * np.sin(d))
        half = w0 * np.clip(1 - along / length, 0, 1)
        inner = np.maximum(inner, (along > 0) * (along < length) * _smooth(half - across, -aa, aa))
    inner = np.maximum(inner, _smooth(0.23 - r, -aa, aa))
    fringe = np.clip(shape - inner, 0, 1)
    return _rgba(inner, fringe, np.zeros_like(r), shape)


def streak(w=256, h=64):
    y, x = np.mgrid[0:h, 0:w] + 0.5
    u = x / w  # 0 tail .. 1 head
    v = np.abs(y - h / 2) / (h / 2)
    half = 0.9 * np.minimum(np.clip(u / 0.8, 0, 1) ** 0.7, np.clip((1.0 - u) / 0.2, 0, 1))  # long thin tail, short pointed head
    aa = 1.5 / (h / 2)
    shape = _smooth(half - v, -aa, aa)
    inner = _smooth(half * 0.6 - v, -aa, aa) * _smooth(u, 0.1, 0.45)
    fringe = np.clip(shape - inner, 0, 1)
    return _rgba(inner, fringe, np.zeros_like(u), shape)


def _noise(size, octaves=5, seed=0):
    """Tileable-enough value noise by summed upsampled random grids (bicubic through PIL)"""
    rng = np.random.default_rng(seed)
    acc = np.zeros((size, size))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        n = 4 * 2 ** o
        grid = rng.random((n, n)).astype(np.float32)
        up = np.asarray(Image.fromarray(grid, "F").resize((size, size), Image.BICUBIC))
        acc += amp * up
        total += amp
        amp *= 0.5
    acc /= total
    return (acc - acc.min()) / (acc.max() - acc.min())


def puff(size=256, seed=0):
    """A soft cloud: 5-7 overlapping gaussian lobes, density edge broken by noise; G = top-lit shading of a sphere-ish volume"""
    rng = np.random.default_rng(100 + seed)
    y, x = (np.mgrid[0:size, 0:size] + 0.5 - size / 2) / (size / 2)
    dens = np.zeros((size, size))
    for _ in range(rng.integers(5, 8)):
        cx, cy = rng.normal(0, 0.22, 2)
        s = rng.uniform(0.22, 0.36)
        dens += np.exp(-((x - cx) ** 2 + (y - cy) ** 2) / (2 * s * s))
    dens /= dens.max()
    n = _noise(size, seed=seed)
    edge = _smooth(dens + 0.35 * (n - 0.5), 0.18, 0.75)
    edge *= _smooth(1.0 - np.hypot(x, y), 0.0, 0.25)  # never touches the cell border
    # Fake volume: normal of a sphere of radius 1 from (x, y), light from top-left-front
    z = np.sqrt(np.clip(1 - x * x - y * y, 0, 1))
    L = np.array([-0.35, -0.6, 0.72])
    L /= np.linalg.norm(L)
    lit = np.clip(x * L[0] + y * L[1] + z * L[2], 0, 1)
    shade = 0.45 + 0.55 * (0.6 * lit + 0.4 * (0.5 + 0.5 * (n - 0.5) * 2) * lit)
    erosion = 0.65 * _noise(size, 4, seed + 50) + 0.35 * (1 - dens)
    return edge, np.clip(shade, 0, 1), np.clip(erosion, 0, 1)


def smoke(cell=256):
    R = np.zeros((cell * 2, cell * 2))
    G = np.zeros_like(R)
    B = np.zeros_like(R)
    for i in range(4):
        r, g, b = puff(cell, seed=i)
        oy, ox = (i // 2) * cell, (i % 2) * cell
        R[oy:oy + cell, ox:ox + cell] = r
        G[oy:oy + cell, ox:ox + cell] = g
        B[oy:oy + cell, ox:ox + cell] = b
    return _rgba(R, G, B, np.ones_like(R))


if __name__ == "__main__":
    star().save(os.path.join(OUT, "T_GSTN_VFX_Star.png"))
    # Niagara velocity-aligned sprites run along V: the head (along the motion) at the top of the image, 64 wide x 256 high
    streak().transpose(Image.ROTATE_90).save(os.path.join(OUT, "T_GSTN_VFX_Streak.png"))
    smoke().save(os.path.join(OUT, "T_GSTN_VFX_Smoke.png"))
    print("ok: Star 256, Streak 256x64, Smoke 512 (2x2)")
