"""One-frame pose pops in a capture (runstart-entry-desktop, 2026-11-21; plain Python). For each frame in [from, to]: the jump of
head / hands / feet (actor-relative, cm) against the mean of the frames before and after (second difference), and the start clip
time step. A pop with an even clip-time step and no blend running = the clip's own data. Writes <out>, prints the 3 worst frames.
  python runstart_pop_check.py <frames.jsonl> <clip> <out.txt> <from> <to>"""
import json
import math
import sys

src, clip, out, lo, hi = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
R = {r["f"]: r for r in (json.loads(l) for l in open(src, encoding="utf-8") if l.strip())}
BONES = ("head", "hand_l", "hand_r", "foot_l", "foot_r", "pelvis")


def ctime(r):
    t = [float(a.split()[2]) for a in (r.get("combat") or {}).get("anim") or [] if a.startswith(clip + " ")]
    return t[0] if t else -1.0


res = []
for f in range(lo, hi + 1):
    if f - 1 not in R or f + 1 not in R or f not in R:
        continue
    d = {}
    for b in BONES:
        p, c, n = R[f - 1]["body"][b], R[f]["body"][b], R[f + 1]["body"][b]
        d[b] = math.sqrt(sum((c[i] - (p[i] + n[i]) / 2) ** 2 for i in range(3)))
    worst = max(d, key=d.get)
    res.append((d[worst], f, worst, ctime(R[f]) - ctime(R[f - 1]), ctime(R[f])))
open(out, "w", encoding="utf-8").write("\n".join("f%d %s %.1f cm clip t %.3f dt %.3f" % (f, b, v, t, dt) for v, f, b, dt, t in res) + "\n")
for v, f, b, dt, t in sorted(res, reverse=True)[:3]:
    print("f%d %s %.1f cm (clip t %.3f, dt %.3f)" % (f, b, v, t, dt))
