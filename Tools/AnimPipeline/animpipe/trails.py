"""animpipe trails <card>: motion trails as one composite image (brief §3.4): side, front and top views of the striking
hand, elbows, knees, feet, head and pelvis, built (solid, dots every frame, keyed frames ringed) over the source (thin
grey). Reads the bone caches legibility.py wrote (sheets/raw/<take>_legbones.json, component space X left, Y forward,
Z up); no editor needed. Output: sheets/<take>_trails.png."""
import json, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from animpipe.card import load, key_frames, windows
from animpipe.__main__ import TOOLS

PARTS = {"middle_01_l": "#d62728", "lowerarm_l": "#ff7f0e", "lowerarm_r": "#bcbd22", "calf_l": "#2ca02c", "calf_r": "#17becf",
         "ball_l": "#1f77b4", "ball_r": "#9467bd", "head": "#8c564b", "pelvis": "#e377c2"}
VIEWS = (("side (Y fwd, Z up)", 1, 2), ("front (X, Z up)", 0, 2), ("top (X, Y fwd)", 0, 1))


def _load(take):
    p = os.path.join(TOOLS, "sheets", "raw", f"{take}_legbones.json")
    if not os.path.exists(p):
        return None
    d = json.load(open(p))["frames"]
    return {b: np.array([d[str(i)][b][:3] for i in range(len(d))]) for b in PARTS}


def run(path):
    c = load(path)
    take = c["anim"].rsplit("/", 1)[1][3:]
    B = _load(take)
    S = _load(c["source_anim"].rsplit("/", 1)[1][3:]) if c.get("source_anim") else None
    keys = key_frames(c)
    a0, a1 = windows(c)["anim"]["active"]
    fig, ax = plt.subplots(1, 3, figsize=(18, 6.5))
    for (title, i, j), a in zip(VIEWS, ax):
        for b, col in PARTS.items():
            if S is not None:
                a.plot(S[b][:, i], S[b][:, j], color="0.75", lw=1)
            p = B[b]
            a.plot(p[:, i], p[:, j], color=col, lw=1.6, label=b)
            a.scatter(p[:, i], p[:, j], color=col, s=6)
            k = [f for f in keys if f < len(p)]
            a.scatter(p[k, i], p[k, j], facecolors="none", edgecolors=col, s=40, lw=1)
            a.scatter(p[a0:a1 + 1, i], p[a0:a1 + 1, j], color="k", marker="x", s=30, zorder=5)
        a.set_title(title); a.set_aspect("equal"); a.grid(alpha=0.3)
    ax[0].legend(fontsize=7, loc="lower left")
    fig.suptitle(f"{take}: trails (build solid, source grey; rings = keys, x = active {a0}-{a1})")
    fig.tight_layout()
    out = os.path.join(TOOLS, "sheets", f"{take}_trails.png")
    fig.savefig(out, dpi=80)
    print(f"trails {take}: {len(PARTS)} parts, 3 views, source overlay {S is not None}")
    print(out)
