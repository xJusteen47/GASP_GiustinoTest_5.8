# Run start v4r4 (offline, plain Python + numpy): elbow rotation targets for runstart_elbows_casc_v4r4.py.
# Inputs: rig-point dumps (JSON {frame: {point: [x,y,z]}}) of RunStart_v4r2.casc and RunStart_v4r3.casc, dumped from Cascadeur.
# Usage: python runstart_elbow_targets_v4r4.py <v4r2_pts.json> <v4r3_pts.json> <out_dpsi.json>
# Elbow height = (elbow - shoulder) . torso up (neck_01 - spine_02). Target per v4r3 frame = mean v4r2 elbow height of the 3 v4r2
# samples (both sides, f12-57) nearest in hand phase (hand angle about the shoulder in the torso sagittal plane, same swing direction
# preferred), never higher than v4r3. The elbow then turns about the shoulder->hand axis (hand fixed) toward "down" only, to the angle
# whose height is nearest the target; the angle curve is smoothed (1-2-1 twice), zero at f<=11 (stomp) and f>=59 (stock tail).
import json, math, sys
import numpy as np
def load(p):
    d = json.load(open(p)); return {int(k): {n: np.array(v) for n, v in d[k].items()} for k in d}
def u(v): return v / np.linalg.norm(v)
def m(P, s):
    up = u(P['neck_01_MainPoint'] - P['spine_02_MainPoint']); xh = u(np.array([1., 0, 0]) - up * up[0]); fw = np.cross(xh, up)
    S, E, H = P['upperarm_MainPoint_' + s], P['lowerarm_MainPoint_' + s], P['hand_MainPoint_' + s]
    return np.dot(E - S, up), math.degrees(math.atan2(np.dot(H - S, fw), -np.dot(H - S, up)))
A, B = load(sys.argv[1]), load(sys.argv[2])
samp = []
for s in 'lr':
    for f in range(12, 58):
        he, th = m(A[f], s); samp.append((th, np.sign(m(A[f + 1], s)[1] - m(A[f - 1], s)[1]), he))
def target_he(th, dirn):
    return np.mean([b[2] for b in sorted(samp, key=lambda x: abs(x[0] - th) + (0 if x[1] == dirn else 15))[:3]])
out = {}; rep = []
for s in 'lr':
    raw = []
    for f in range(69):
        P = B[f]; he, th = m(P, s)
        if not 12 <= f <= 58: raw.append(0.0); continue
        tgt = min(he, target_he(th, np.sign(m(B[min(68, f + 1)], s)[1] - m(B[f - 1], s)[1])))
        up = u(P['neck_01_MainPoint'] - P['spine_02_MainPoint'])
        S, E, H = P['upperarm_MainPoint_' + s], P['lowerarm_MainPoint_' + s], P['hand_MainPoint_' + s]
        ax = u(H - S); c = np.dot(E - S, ax); bend = E - S - ax * c; r = np.linalg.norm(bend)
        dn = u(-up - ax * np.dot(-up, ax)); lat = np.cross(ax, dn)
        psi = math.atan2(np.dot(bend, lat), np.dot(bend, dn)); best = (1e9, 0.0)
        for k in range(-90, 91):
            p = psi + math.radians(k)
            if abs(math.atan2(math.sin(p), math.cos(p))) > abs(psi) + 1e-6: continue
            h = np.dot(ax * c + r * (math.cos(p) * dn + math.sin(p) * lat), up)
            cost = abs(h - tgt) + 0.02 * abs(k)
            if cost < best[0]: best = (cost, math.radians(k))
        raw.append(best[1])
    sm = raw[:]
    for _ in range(2):
        sm = [0.25 * sm[max(0, i - 1)] + 0.5 * sm[i] + 0.25 * sm[min(68, i + 1)] for i in range(69)]
    out[s] = [0.0 if (f <= 11 or f >= 59) else v for f, v in enumerate(sm)]
    rep.append('%s max %.0f deg' % (s, max(abs(math.degrees(v)) for v in out[s])))
json.dump(out, open(sys.argv[3], 'w'))
print('ESITO: OK', ', '.join(rep))
