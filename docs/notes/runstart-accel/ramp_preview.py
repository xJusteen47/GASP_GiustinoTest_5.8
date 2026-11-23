"""Paper preview of the capsule speed: current profile vs the Toad ramp (setup_run_start_ramp.py values: stamp off, stage 1 ramp 62 f from 0.15). DA values (desktop dump): 470/2000 (ramp 6 f / 0.3), 610/1400.
Simplified model of GSTN_RunStageLogic (stamp, linear accel ramp, stage hold), 62.5 Hz. Stage tops = measured shelf 450 and
steady 585 (v4r3 capture), accels = code defaults 1800/1400 (asset values not read). Prints max 5 lines.
Usage: python ramp_preview.py metrics_v4r3.json out.png"""
import json, sys
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
DT = 1 / 62.5
def sim(ramps, n=200, stamp=21):
    tops, acc, hold = (470, 610), (2000, 1400), 40   # DA values; measured steady 585 at the capture
    v, st, held, rf, out = 0.0, 0, 0, 0, []
    for f in range(n):
        if f < stamp: a, cap = acc[0] * .05, 60
        else:
            N, s = ramps[st]; k = 1 if N <= 0 else min(1, s + (1 - s) * rf / N)
            a, cap = acc[st] * k, tops[st]; rf += 1
        v = min(cap, v + a * DT) if v < cap else v; out.append(v)
        if st == 0 and f >= stamp:
            held = held + 1 if v >= .9 * tops[0] else 0
            if held >= hold: st, rf = 1, 0
    return [i * DT for i in range(n)], out
t0, v0 = sim({0: (6, .3), 1: (0, 1)}); t1, v1 = sim({0: (62, .15), 1: (30, .25)}, stamp=0)
d = json.load(open(sys.argv[1])); x = {r["f"]: r["actor_x"] for r in d}
tm = [(f - 82) / 60 for f in range(82, 250)]; vm = [(x[f + 6] - x[f - 6]) / 12 * 60 for f in range(82, 250)]
plt.figure(figsize=(7, 3.2)); plt.plot(tm, vm, c="0.7", label="v4r3 measured"); plt.plot(t0, v0, "--", label="model, current")
plt.plot(t1, v1, label="model, ramp from frame 1, no stamp"); plt.xlabel("s from stick"); plt.ylabel("cm/s"); plt.legend(); plt.tight_layout()
plt.savefig(sys.argv[2], dpi=110)
for name, t, v in (("current", t0, v0), ("ramp", t1, v1)):
    print(name, "t50 %.2f t77 %.2f t95 %.2f s" % tuple(next(ti for ti, vi in zip(t, v) if vi >= p * 585) for p in (.5, .77, .95)))
