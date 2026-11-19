"""Smoke redo A1 (docs/notes/reviews/vfx-v3/hd1/REVIEW-SMOKE-LOOK.md, option A): a lumpy stylised puff flipbook, no external assets.

T_GSTN_VFX_PuffFB.png 1024x1024, 8x8 frames of 128 px. Rows 0-3 = variant A (frames 0-31), rows 4-7 = variant B (frames 0-31).
  R, G = the puff's surface normal in the frame (x right, y UP), 0.5 + 0.5 n. The material rebuilds z and shades it against a light
         direction given in the same frame space, so the sprite must not rotate (it does not: the motion lives in the flipbook).
  B    = crease / ambient occlusion (1 = open, low = between lobes and on low parts).
  A    = density: a crisp lumpy silhouette (hard union of spheres, ~2 px feather), not a blurred blob.
The puff is a hard union of 6-8 main spheres plus 10-14 small knobs on their surface (cauliflower billows). Over the 16 frames it pops
out from a compact shape (frames 0-3), its lobes drift out and up, then shrink one after another at staggered times: the puff splits
into smaller blobs with holes and is gone by the last frame. Erosion is the animation, not a threshold.
Run: python Tools/GAS/vfx/make_vfx_puff_flipbook.py      (prints one line; needs numpy + Pillow)
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
CELL, GRID, FRAMES = 128, 8, 16
VARIANTS = 4  # A1 review: 2 variants read as clones; 4 x 16 frames, 2 atlas rows each
SC = 1.5  # lobe layout scale: the puff fills ~85 % of its frame at its biggest


def _smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def _lobes(rng):
    """(cx, cy, r, die_start, die_end) in cell space -1..1, y up. Knobs sit on a parent's surface and die with or before it"""
    main = []
    for i in range(rng.integers(4, 6)):  # A1 review: one dominant mass with a few bumps, not a bunch of grapes
        a = rng.uniform(0, 2 * np.pi)
        d = 0.0 if i == 0 else rng.uniform(0.24, 0.38)
        r = 0.38 if i == 0 else rng.uniform(0.19, 0.28)  # a core of 0.44 swallowed the lobes: a plain ball
        cy = d * np.sin(a) * 0.75 + 0.02  # a little wider than tall, sits low in the cell
        t0 = rng.uniform(0.40, 0.66)
        main.append([d * np.cos(a), cy, r, t0, min(t0 + rng.uniform(0.28, 0.42), 0.97)])
    main[0][3], main[0][4] = 0.45, 0.88  # the core goes before the last outer blobs: the puff splits into pieces
    knobs = []
    for _ in range(rng.integers(2, 4)):  # few knobs: more read as soap foam in game (a1 light2, A1 review)
        p = main[rng.integers(0, len(main))]
        a = rng.uniform(-0.2 * np.pi, 1.2 * np.pi)  # mostly on top and sides: billows grow upward
        r = rng.uniform(0.09, 0.15)
        k = p[2] * 0.92
        t0 = rng.uniform(0.15, p[3] + 0.1)
        knobs.append([p[0] + k * np.cos(a), p[1] + k * np.sin(a), r, t0, min(t0 + rng.uniform(0.2, 0.35), p[4])])
    return np.array(main + knobs)


def _frame(lobes, t, x, y, rng_noise, turn=0.0):
    pop = 1 - (1 - min(t / 0.12, 1.0)) ** 2  # 0..1 over frames 0-3
    spread = SC * (0.62 + 0.38 * pop + 0.14 * t)  # drifts out
    lift = 0.08 * t - 0.04  # and up
    sdf1 = np.full(x.shape, -9.0)
    sdf2 = np.full(x.shape, -9.0)
    nx = np.zeros(x.shape)
    ny = np.zeros(x.shape)
    nz = np.ones(x.shape)
    hgt = np.zeros(x.shape)
    for cx, cy, r0, t0, t1 in lobes:
        life = 1.0 - _smooth(np.array(t), t0, t1)
        r = SC * r0 * (0.7 + 0.3 * pop) * float(life)
        if r < 0.01:
            continue
        ang = turn * min(t / 0.6, 1.0)
        rx, ry = cx * np.cos(ang) - cy * np.sin(ang), cx * np.sin(ang) + cy * np.cos(ang)
        px, py = rx * spread * 0.85, ry * spread + lift  # 0.85: wide layouts stayed inside the frame
        dx, dy = x - px, y - py
        d = np.hypot(dx, dy)
        s = r - d
        win = s > sdf1
        sdf2 = np.where(win, sdf1, np.maximum(sdf2, s))
        sdf1 = np.where(win, s, sdf1)
        h = np.sqrt(np.clip(r * r - d * d, 0, None)) / r
        nx = np.where(win, dx / r, nx)
        ny = np.where(win, dy / r, ny)
        nz = np.where(win, h, nz)
        hgt = np.where(win, h * r, hgt)
    edge = sdf1 + 0.008 * rng_noise
    px1 = 2.0 / CELL
    alpha = _smooth(edge, -1.0 * px1, 1.2 * px1)
    alpha *= _smooth(0.97 - np.maximum(np.abs(x), np.abs(y)), 0.0, 0.03)  # never touch the cell border (atlas bleed)
    n = np.stack([nx, ny, nz + 0.15], -1)  # +0.15: the rim tilts toward the viewer a little (no razor-dark outline)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    crease = 1.0 - 0.15 * np.exp(-np.clip(sdf1 - sdf2, 0, None) / 0.08) * (sdf2 > -0.04)
    ambient = 0.72 + 0.28 * np.clip(hgt / 0.3, 0, 1)
    occ = np.clip(crease * ambient, 0, 1)
    return n, occ, alpha


def flipbook():
    size = CELL * GRID
    img = np.zeros((size, size, 4))
    yy, xx = np.mgrid[0:CELL, 0:CELL] + 0.5
    x = xx / (CELL / 2) - 1
    y = 1 - yy / (CELL / 2)  # y up
    for v in range(VARIANTS):
        rng = np.random.default_rng(311 + 17 * v)
        lobes = _lobes(rng)
        noise = rng.standard_normal((CELL, CELL))
        noise = (noise + np.roll(noise, 1, 0) + np.roll(noise, 1, 1) + np.roll(noise, -1, 0) + np.roll(noise, -1, 1)) / 2.2
        for f in range(FRAMES):
            n, occ, a = _frame(lobes, f / (FRAMES - 1), x, y, noise, turn=(0.35 if v % 2 else -0.35))  # the layout turns ~20 deg
            k = v * FRAMES + f
            oy, ox = (k // GRID) * CELL, (k % GRID) * CELL
            img[oy:oy + CELL, ox:ox + CELL] = np.stack([0.5 + 0.5 * n[..., 0], 0.5 + 0.5 * n[..., 1], occ, a], -1)
    return Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")


def preview(fb, path):
    """Shaded preview (light from top-left-front) on grey, for a quick look outside the engine"""
    a = np.asarray(fb).astype(np.float32) / 255
    nx, ny = a[..., 0] * 2 - 1, a[..., 1] * 2 - 1
    nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
    L = np.array([-0.45, 0.7, 0.55]) / np.linalg.norm([-0.45, 0.7, 0.55])
    ndl = np.clip((nx * L[0] + ny * L[1] + nz * L[2] + 0.3) / 1.3, 0, 1)
    ramp = _smooth(ndl, 0.3, 0.7)
    lit, shade = np.array([0.95, 0.96, 1.0]), np.array([0.55, 0.62, 0.8])
    col = (shade + (lit - shade) * ramp[..., None]) * (0.3 + 0.7 * a[..., 2:3])
    bg = np.full_like(col, 0.35)
    out = bg * (1 - a[..., 3:4]) + col * a[..., 3:4]
    Image.fromarray((out * 255).astype(np.uint8), "RGB").save(path)


if __name__ == "__main__":
    fb = flipbook()
    fb.save(os.path.join(OUT, "T_GSTN_VFX_PuffFB.png"))
    import sys
    if len(sys.argv) > 1:
        preview(fb, sys.argv[1])
    print("ok: PuffFB 1024 (8x8, 4 variants x 16 frames)")
