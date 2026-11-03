"""Offline: composite layout (size, ghosts) on a synthetic take; no editor."""
import os, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
from animpipe import composite as C

bones = sorted({b for ch in C.CHAINS.values() for b in ch} | {"head", "neck_01"})
F = {f: {b: np.array([0.0, f * 0.5 + i, 10.0 * i + 1]) for i, b in enumerate(bones)} for f in range(23)}
ghost = {5: {b: p + 3 for b, p in F[5].items()}}
assert C.grid_shape(23) == (3, 11)
out = os.path.join(tempfile.mkdtemp(), "g.png")
w, h = C.render(F, ghost, [0, 5, 10], [8, 9], 8, "t", out)
assert w == C.WIDTH and w >= h, (w, h)
assert abs(C.shift_to_pelvis(ghost[5], F[5])["pelvis"] - F[5]["pelvis"]).max() == 0
assert os.listdir(os.path.dirname(out)) == ["g.png"]
print("test_composite ok")
