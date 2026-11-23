# Game timing map for runstart_generic_compose.py TMAP (runstart-turnaround-desktop; w2 made it by hand from metrics_w1_h1.json).
# From a capture's metrics_<cap>.json: every 2nd game frame (30 Hz) from the first frame the clip shows, uf = clip time x 30
# (authored frame), dist = capsule ('root' row) distance from that frame along the run direction (first -> last kept frame).
# usage: python runstart_game_tmap.py <metrics json> <clip name> <out json> [frames=40]   prints 1 line
import json, math, sys
M, clip, out = json.load(open(sys.argv[1])), sys.argv[2], sys.argv[3]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 40
rows = []
for d in M:
    t = next((a for a in d['anim'] if a.startswith(clip + ' ')), None)
    if t is not None or rows:
        rows.append((d['f'], float(t.split()[2]) * 30.0 if t else None, d['root'][:2]))
rows = [r for r in rows if r[1] is not None]
v0 = rows[0][0]
rows = [r for r in rows if (r[0] - v0) % 2 == 0][:N]
p0, p1 = rows[0][2], rows[-1][2]
dv = [p1[0] - p0[0], p1[1] - p0[1]]; L = math.hypot(*dv); dv = [dv[0] / L, dv[1] / L]
uf = [round(r[1], 2) for r in rows]
dist = [round((r[2][0] - p0[0]) * dv[0] + (r[2][1] - p0[1]) * dv[1], 1) for r in rows]
if uf[0] > 0.01:                                         # the clip was entered at uf[0] (pose match): start the map at 0
    uf, dist = [0.0] + uf, [0.0] + dist
json.dump({'uf': uf, 'dist': dist, 'src': '%s, game v=%d+2k' % (sys.argv[1].replace(chr(92), '/'), v0)}, open(out, 'w'))
print('tmap %s: %d points, uf %.2f -> %.2f, dist %.1f -> %.1f cm' % (out, len(uf), uf[0], uf[-1], dist[0], dist[-1]))
