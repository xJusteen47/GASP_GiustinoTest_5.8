"""Why does PIE feel nauseating? Read one gstn.Camera.Trace csv from a real play session and split the candidates.

Usage: python Tools/GASNPPTest/nausea_check.py [Saved/Logs/gstn_camera_trace.csv]

camera_trace_check.py looks for vertical zig-zags (camera bounce). Motion sickness in a third person game more often comes from
horizontal and rotational judder: the view advancing by uneven amounts frame to frame while the body moves at a steady speed, or
frames arriving at uneven times. This script reports, per candidate, one number and a verdict:

1. Frame pacing: dt median / p95 / p99, coefficient of variation, hitches (> 1.5x median). Low fps (< 45) or many hitches = the
   engine, not the camera (3 PIE players in one process measured 19-24 fps, pie-perf-probe-2 / camera-jitter-2).
2. Sim beat: the actor root moves in 62.5 Hz steps; at 60 fps one frame in ~24 gets two steps. If the VIEW also shows that
   2x step pattern while the drawn body (stand_*) does not, the camera reads the sim position somewhere (camera-tick-order lesson).
3. View judder vs body: frame-to-frame change of horizontal speed of the final view (out) against the drawn body (stand), only while
   moving > 150 cm/s. Ratio > 2 = the camera adds judder the body does not have.
4. Rotation judder: yaw rate change frame to frame while turning (> 20 deg/s). Look stick smoothing, auto follow or lag springs.
5. View vs body screen offset: second difference of (out - stand), i.e. how much the character jumps on screen each frame.

Thresholds are first guesses to tune with the owner's own "this feels bad" moments, not proven limits.
"""
import csv, math, statistics, sys

path = sys.argv[1] if len(sys.argv) > 1 else "Saved/Logs/gstn_camera_trace.csv"
rows = list(csv.DictReader(open(path)))
if len(rows) < 30:
    sys.exit(f"{path}: only {len(rows)} frames, play at least 30 s with gstn.Camera.Trace 1")


def col(k):
    return [float(r[k]) for r in rows]


def pct(v, p):
    s = sorted(v)
    return s[min(len(s) - 1, int(p / 100 * len(s)))]


dt = col("dt")
t = col("time")
med = statistics.median(dt)
cv = statistics.pstdev(dt) / statistics.mean(dt)
hitches = sum(1 for d in dt if d > 1.5 * med)
off2ms = sum(1 for d in dt if abs(d - med) > 0.002) / len(dt)
print(f"[1 pacing] {len(rows)} frames over {t[-1] - t[0]:.0f} s  fps {1 / med:.0f}  dt med {med * 1000:.1f} p95 {pct(dt, 95) * 1000:.1f}"
      f" p99 {pct(dt, 99) * 1000:.1f} max {max(dt) * 1000:.1f} ms  cv {cv:.2f}  hitches {hitches} ({hitches / (t[-1] - t[0]) * 60:.1f}/min)"
      f"  frames >2 ms off median {off2ms:.0%}")
verdict = []
if 1 / med < 45:
    verdict.append("LOW FPS: fix the run setup first (players, viewport, editor in front), everything else is secondary")
if cv > 0.25 or hitches / max(1.0, t[-1] - t[0]) > 0.5:
    verdict.append("UNEVEN PACING: frames arrive at irregular times (vsync off, background load, GC, log I/O)")


def hstep(prefix):
    x, y = col(prefix + "_x"), col(prefix + "_y")
    return [math.hypot(x[i] - x[i - 1], y[i] - y[i - 1]) for i in range(1, len(rows))]


root, stand, view = hstep("root"), hstep("stand"), hstep("out")


def beat_ratio(steps):
    # frames where a moving thing advanced ~2x its neighbours' median step: the 62.5 vs render rate beat
    n = hits = 0
    for i in range(2, len(steps) - 2):
        ref = statistics.median(steps[i - 2:i] + steps[i + 1:i + 3])
        if ref < 1.0:  # not moving (< 1 cm per frame)
            continue
        n += 1
        if steps[i] > 1.7 * ref or steps[i] < 0.3 * ref:
            hits += 1
    return hits / n if n else 0.0


br, bs, bv = beat_ratio(root), beat_ratio(stand), beat_ratio(view)
print(f"[2 sim beat] frames with a 2x/0x step while moving: root {br:.1%}  drawn body {bs:.1%}  view {bv:.1%}")
if bv > 2 * max(bs, 0.005) and bv > 0.02:
    verdict.append("VIEW FOLLOWS SIM STEPS: some camera stage reads the 62.5 Hz root, not the smoothed body")


def speed_judder(steps):
    out = []
    for i in range(1, len(steps)):
        a, b = steps[i - 1] / dt[i], steps[i] / dt[i + 1] if i + 1 < len(dt) else steps[i] / dt[i]
        if min(a, b) > 150:
            out.append(abs(b - a) / max(a, b))
    return statistics.median(out) if out else 0.0, len(out)


jb, nb = speed_judder(stand)
jv, nv = speed_judder(view)
print(f"[3 view judder] median frame-to-frame speed change while moving: drawn body {jb:.1%}  view {jv:.1%}  ({nv} moving frames)")
if nv > 30 and jv > 2 * max(jb, 0.01) and jv > 0.04:
    verdict.append("VIEW JUDDERS MORE THAN THE BODY: camera lag/spring or a late write")

yaw = col("out_yaw")
rates = []
for i in range(1, len(rows)):
    d = (yaw[i] - yaw[i - 1] + 180) % 360 - 180
    rates.append(d / dt[i] if dt[i] > 0 else 0.0)
yj = [abs(rates[i] - rates[i - 1]) / max(abs(rates[i]), abs(rates[i - 1])) for i in range(1, len(rates))
      if min(abs(rates[i]), abs(rates[i - 1])) > 20 and rates[i] * rates[i - 1] > 0]
yjm = statistics.median(yj) if yj else 0.0
print(f"[4 rotation judder] median yaw rate change while turning: {yjm:.1%}  ({len(yj)} turning frames)")
if len(yj) > 30 and yjm > 0.08:
    verdict.append("TURNING JUDDERS: look input or yaw follow is uneven frame to frame")

ox = [(float(r["out_x"]) - float(r["stand_x"]), float(r["out_y"]) - float(r["stand_y"]), float(r["out_z"]) - float(r["stand_z"])) for r in rows]
jerk = [math.dist([2 * b - a - c for a, b, c in zip(ox[i - 1], ox[i], ox[i + 1])], (0, 0, 0)) for i in range(1, len(ox) - 1)]
print(f"[5 body on screen] view-minus-body 2nd difference: median {statistics.median(jerk):.2f} cm  p95 {pct(jerk, 95):.2f}"
      f"  frames > 2 cm {sum(1 for j in jerk if j > 2)}")
if pct(jerk, 95) > 2:
    verdict.append("CHARACTER JUMPS ON SCREEN: view and body offsets change unevenly")

print("VERDICT: " + (" | ".join(verdict) if verdict else "nothing in the camera numbers; look at FOV, distance, monitor Hz/vsync and the capture or stream chain"))
