"""Speed ramp of the Overlord run start from a capture's per-frame actor_x.
Usage: python speed_curve.py metrics_v4r3.json out.png  (prints max 5 lines)"""
import json, sys
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
d = json.load(open(sys.argv[1])); x = {r["f"]: r["actor_x"] for r in d}
F0, FPS, W = 82, 60, 12         # f82 = first frame the anim reacts to the stick
v = lambda f: (x[f + W // 2] - x[f - W // 2]) / W * FPS
VSS = 585.0                     # steady run 9.75 cm/frame (f168-250); 680 peaks = double sim tick
ts = [(f - F0) / FPS for f in range(F0, 250)]; vs = [v(f) for f in range(F0, 250)]
plt.figure(figsize=(7, 3)); plt.plot(ts, vs, label="Overlord v4r3 (capsule)")
plt.axhline(VSS, ls=":", c="k"); plt.xlabel("s from stick"); plt.ylabel("cm/s"); plt.legend(); plt.tight_layout()
plt.savefig(sys.argv[2], dpi=110)
for p in (.1, .5, .9):
    print(p, round((next(f for f in range(F0 + 3, 250) if v(f) >= p * VSS) - F0) / FPS, 2), "s")
