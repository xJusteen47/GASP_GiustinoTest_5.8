"""Run start ENTRY metrics from a capture (runstart-entry-desktop, 2026-11-21; plain Python, outside the editor).
Per frame from the first moving frame: start clip weight/time, ABP Gait/StateName (if the capture had PIE_CAPTURE_ABPVARS),
and the world slide (cm, xy) of each foot that is down (z < FOOT_DOWN_Z over the capsule bottom). Writes <out>; prints max 5 lines:
the walk-start frames, the start's first frame, its weight on the first 6 frames, and the max per-frame / total stance slide in the
entry window (first moving frame .. +WIN).
  python runstart_entry_metrics.py <frames.jsonl> <clip name> <out.txt> [WIN=20]"""
import json
import math
import sys

FOOT_DOWN_Z = 12.0   # cm above the capsule bottom (body z is relative to the actor; actor = capsule centre)
src, clip, out = sys.argv[1], sys.argv[2], sys.argv[3]
WIN = int(sys.argv[4]) if len(sys.argv) > 4 else 20
rows = [json.loads(l) for l in open(src, encoding="utf-8") if l.strip()]
start = next(r["f"] for r in rows if (r.get("combat") or {}).get("run", {}).get("sinceMove", -1) >= 0)
lines, prev, slides, walk, first, weights = [], None, [], [], None, []
for r in rows:
    f = r["f"]
    if f < start - 1 or f > start + WIN:
        continue
    anim = (r.get("combat") or {}).get("anim") or []
    w = max([float(a.split()[1]) for a in anim if a.startswith(clip + " ")] or [0.0])
    t = max([float(a.split()[2]) for a in anim if a.startswith(clip + " ")] or [-1.0])
    if any(a.startswith("M_Relaxed_Walk_Start") for a in anim):
        walk.append(f)
    if w > 0 and first is None:
        first = f
    if first is not None and len(weights) < 6:
        weights.append(w)
    b, a = r["body"], r["body"]["actor"]
    hh = b["cap_hh"]
    feet = {k: (a[0] + b[k][0], a[1] + b[k][1], b[k][2] + hh) for k in ("foot_l", "foot_r")}
    s = {}
    if prev:
        for k, p in feet.items():
            if p[2] < FOOT_DOWN_Z and prev[k][2] < FOOT_DOWN_Z:
                s[k] = math.hypot(p[0] - prev[k][0], p[1] - prev[k][1])
    slides.append(s)
    prev = feet
    v = r.get("abpv") or {}
    lines.append("%d w%.2f t%.3f gait=%s state=%s | %s" % (f, w, t, v.get("Gait", "?").replace("NewEnumerator", ""), v.get("StateName", "?"),
                 " ".join("%s %.1f (z%.1f)" % (k[-1].upper(), x, feet[k][2]) for k, x in s.items())))
per = [x for s in slides for x in s.values()]
tot = {k: sum(s.get(k, 0.0) for s in slides) for k in ("foot_l", "foot_r")}
open(out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("first moving frame %d; walk start frames %s; %s first frame %s" % (start, walk or "none", clip, first))
print("start weight first 6 frames: " + " ".join("%.2f" % x for x in weights))
print("stance slide in f%d..f%d: max/frame %.1f cm, total L %.1f R %.1f cm" % (start - 1, start + WIN, max(per or [0]), tot["foot_l"], tot["foot_r"]))
