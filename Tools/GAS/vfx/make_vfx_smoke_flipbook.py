"""Smoke redo A1b (owner 2026-11-19 on the A1 video: "size ok, too stylised; DK Bananza's dust is realistic"): a soft, billowy
semi-realistic smoke puff flipbook, no external assets. Same layout and channels as T_GSTN_VFX_PuffFB (make_vfx_puff_flipbook.py), so
M_GSTN_VFX_Puff draws it unchanged with its form lighting:
T_GSTN_VFX_SmokeFB.png 1024x1024, 8x8 frames of 128 px, 4 variants x 16 frames (variant k on rows 2k, 2k+1).
  R, G = normal (x right, y up) of the smoothed density seen as a height field: billows get a lit top and a shaded underside.
  B    = occlusion: dense cores and the underside of the mass are darker (cheap self-shadowing).
  A    = density: soft translucent edges and wisps (not a crisp silhouette), cauliflower billows from domain-warped noise.
Over the 16 frames: the mass expands and rises a little, its noise flows up and out, and it erodes from the thin parts inward
(threshold rising over life): it thins into wisps and holes instead of fading as a sheet.
Run: python Tools/GAS/vfx/make_vfx_smoke_flipbook.py [preview.png]     (prints one line; needs numpy + Pillow)
"""
import os
import sys

import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
CELL, GRID, FRAMES, VARIANTS = 128, 8, 16, 4
N = 256  # noise tile size


def _smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def _tile_noise(rng, octaves=5):
    acc = np.zeros((N, N))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        n = 4 * 2 ** o
        grid = rng.random((n, n))
        # periodic bicubic-ish upsample: pad by wrap, resize, crop
        pad = np.pad(grid, 2, mode="wrap").astype(np.float32)
        big = np.asarray(Image.fromarray(pad, "F").resize(((n + 4) * N // n,) * 2, Image.BICUBIC))
        off = 2 * N // n
        acc += amp * big[off:off + N, off:off + N]
        total += amp
        amp *= 0.55
    acc /= total
    return (acc - acc.min()) / (acc.max() - acc.min())


def _sample(tex, u, v):
    """Bilinear, wrapping; u, v in tile units (1 = one tile)"""
    x = (u % 1.0) * N
    y = (v % 1.0) * N
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0
    x0 %= N
    y0 %= N
    x1, y1 = (x0 + 1) % N, (y0 + 1) % N
    a = tex[y0, x0] * (1 - fx) + tex[y0, x1] * fx
    b = tex[y1, x0] * (1 - fx) + tex[y1, x1] * fx
    return a * (1 - fy) + b * fy


def _blur(img, r):
    """Separable box blur x3 (~gaussian), radius r px"""
    out = img
    for _ in range(3):
        for ax in (0, 1):
            c = np.cumsum(np.pad(out, [(r + 1, r) if a == ax else (0, 0) for a in (0, 1)], mode="edge"), axis=ax)
            out = ((np.take(c, range(2 * r + 1, c.shape[ax]), axis=ax) - np.take(c, range(0, c.shape[ax] - 2 * r - 1), axis=ax))
                   / (2 * r + 1))
    return out


def _frame(lobes, n1, n2, t, x, y, seed_off):
    grow = 0.90 + 0.15 * (1 - (1 - t) ** 2)  # A2 (owner: the early extent is the max): slight growth only
    lift = 0.10 * t
    mass = np.zeros(x.shape)
    for cx, cy, s in lobes:
        dx, dy = x - cx * grow, y - (cy * grow + lift)
        mass = np.maximum(mass, np.exp(-(dx * dx + dy * dy) / (2 * (s * grow) ** 2)))
    # Domain-warped billow noise, flowing up and out over life
    u, v = x * 0.42 + seed_off, y * 0.42 + seed_off * 0.7  # A2 review A1b flaw 2: more billows (0.55 gave fur)
    wu = _sample(n2, u * 0.7 + 0.13 * t, v * 0.7) - 0.5
    wv = _sample(n2, u * 0.7 + 0.37, v * 0.7 - 0.1 * t) - 0.5
    flow_u, flow_v = u + 0.12 * wu - 0.03 * t * x, v + 0.12 * wv - 0.08 * t  # light warp: strong warp drew hair
    billow = _sample(n1, flow_u, flow_v)
    billow = _smooth(billow, 0.2, 0.85)  # rounded lumps with clear gaps
    dens = mass * (0.55 + 0.75 * billow)  # A2: the core cannot open holes (variant 3 square hole)
    # Erosion from the thin parts inward; soft edge so wisps stay translucent
    # A1b review: the core stayed opaque and the puff died by erosion (shrank): low threshold, wide soft edge, never fully opaque,
    # so the material's fade-while-growing (owner) is what thins it out
    # A2 r5 (owner 2026-11-19, scope: realistic dust: "the particles visibly shrink before they vanish"): the threshold no longer rises
    # over life. Its rise (0.18 -> 0.40 from 40 % of life) ate the silhouette from the edge inward, -17 % radius by the end
    # (Tools/GASNPPTest/vfx_size_monotonic.py). The puff only grows; it vanishes through the material's opacity fade
    # r5b (ref IMG_9543: round lumps, not hair; r5 dark read fibrous): a little firmer, still constant over life
    th = 0.24
    alpha = 0.7 * _smooth(dens, th, th + 0.35)  # r5c REVIEW-A2r5: firmer soft edge (0.55)
    alpha *= _smooth(0.97 - np.maximum(np.abs(x), np.abs(y)), 0.0, 0.08)
    # Height field = smoothed density -> normal; occlusion = dense core and underside darker
    h = _blur(dens, 2)  # A2: sharper billow relief
    gy, gx = np.gradient(h)
    k = 10.0  # r5c REVIEW-A2r5 (dark creases): 14 -> 10. A2: stronger form shading (A1b read flat)
    nx, ny = -gx * k, gy * k  # image rows go down, y goes up
    nz = np.ones_like(h)
    nrm = np.sqrt(nx * nx + ny * ny + nz * nz)
    big = _blur(dens, 10)
    occ = np.clip(1.0 - 0.2 * _smooth(big, 0.5, 1.1) - 0.45 * np.clip(-y, 0, 1) * mass, 0, 1)  # A2 critic: core was darker than the rim (hollow shell); shade the underside instead
    return nx / nrm, ny / nrm, occ, alpha


def flipbook():
    size = CELL * GRID
    img = np.zeros((size, size, 4))
    yy, xx = np.mgrid[0:CELL, 0:CELL] + 0.5
    x = xx / (CELL / 2) - 1
    y = 1 - yy / (CELL / 2)
    for vi in range(VARIANTS):
        rng = np.random.default_rng(907 + 31 * vi)
        n1, n2 = _tile_noise(rng), _tile_noise(rng, 3)
        lobes = [(0.0, -0.08, 0.40)]
        for _ in range(rng.integers(3, 5)):
            a = rng.uniform(-0.15 * np.pi, 1.15 * np.pi)
            d = rng.uniform(0.22, 0.36)
            lobes.append((d * np.cos(a) * 0.9, d * np.sin(a) * 0.7 - 0.05, rng.uniform(0.20, 0.28)))
        for f in range(FRAMES):
            nx, ny, occ, a = _frame(lobes, n1, n2, f / (FRAMES - 1), x, y, vi * 0.31)
            k = vi * FRAMES + f
            oy, ox = (k // GRID) * CELL, (k % GRID) * CELL
            img[oy:oy + CELL, ox:ox + CELL] = np.stack([0.5 + 0.5 * nx, 0.5 + 0.5 * ny, occ, a], -1)
    return Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")


if __name__ == "__main__":
    fb = flipbook()
    fb.save(os.path.join(OUT, "T_GSTN_VFX_SmokeFB.png"))
    if len(sys.argv) > 1:
        sys.path.insert(0, OUT)
        import make_vfx_puff_flipbook as P
        P.preview(fb, sys.argv[1])
    print("ok: SmokeFB 1024 (8x8, 4 variants x 16 frames)")
