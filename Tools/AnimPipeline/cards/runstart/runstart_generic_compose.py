# Generic run start g1 (runstart-generic-desktop, 2026-11-21). Owner: average Fortnite-style character, stock GASP gait; frantic
# limb rev-up ("engine revving", Mario / Astro Bot) from frame 1, no planted pause, ending EXACTLY on the stock loop poses
# (M_Relaxed_Run_Loop_F, the loop the game plays). Care about the animation, not the speed (DA untouched).
# Offline composer (plain Python, numpy) on two Cascadeur rig dumps (Y up, forward +Z):
#   loop  = Saved/AnimPipeline/runloop/loop_relaxed_dump.npz  (M_Relaxed_Run_Loop_F, 88 f, cycle 22 f, L plants f0/22/44/66)
#   start = Saved/AnimPipeline/runloop/startL_stock_dump.npz  (M_Relaxed_Run_Start_F_Lfoot, 91 f): f0 = idle entry pose, root ramp
# Output frame u:
#   limbs/torso = loop pose (root-relative) at warped loop time c(u); play rate decays R0 -> 1 over N_REV frames (revs highest at
#     frame 1, no cadence jump into the loop); after N_REV, rate 1: the tail IS the loop, frame for frame.
#   entry: first ENTRY frames crossfade from the stock start f0 pose (what idle hands over) into the warped loop pose.
#   root = stock start root ramp (stock GASP root motion) until its speed reaches the loop speed, then loop speed.
#   stride scale s = root speed / (rate * loop speed), clamped S_MIN..1: leg points' horizontal offset from the hip is scaled, vertical
#     kept => short churning steps with high knee pumps while the root is slow (wheel spin), full strides once rate 1 and speed match.
# Writes Saved/AnimPipeline/runstart/generic_<TAG>_pose.npz (points world, boxes local), generic_<TAG>_curves.json (loop curves sampled
# at c(u)), prints <= 5 lines.
import json, math, os, sys
import numpy as np

ROOT = os.path.dirname(os.path.abspath(__file__)).split(os.sep + 'Tools' + os.sep)[0]
TAG = os.environ.get('TAG', 'g1')
R0 = float(os.environ.get('R0', 3.2))
N_REV = int(os.environ.get('N_REV', 16))
ENTRY = float(os.environ.get('ENTRY', 3.0))
S_MIN = float(os.environ.get('S_MIN', 0.15))
TAIL_CYCLES = float(os.environ.get('TAIL_CYCLES', 2.0))
C0 = os.environ.get('C0')                       # loop start frame; default = best match to the stock f0 pose (legs)
LEG = ('calf_', 'foot_', 'ball_', 'ik_foot_l', 'ik_foot_r')
UP = 1
LP = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runloop/loop_relaxed_dump.npz'))
SP = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runloop/startL_stock_dump.npz'))
meta = json.load(open(os.path.join(ROOT, 'Saved/RunLoop/export/Relaxed_RunLoop_F.meta.json')))
NL = LP['P|root'].shape[0]
hz = lambda v: v - v[..., UP:UP + 1] * np.eye(3)[UP]
pnames = [k[2:] for k in LP.files if k.startswith('P|')]
bnames = [k[2:] for k in LP.files if k.startswith('R|')]
lroot = LP['P|root']
VL = float(np.linalg.norm(hz(lroot[NL - 1] - lroot[0]))) / (NL - 1)      # loop cm per frame
FWD = hz(lroot[NL - 1] - lroot[0]); FWD /= np.linalg.norm(FWD)


def ss(x):
    x = min(max(x, 0.0), 1.0); return x * x * (3 - 2 * x)


def lrel(f):                                         # loop root-relative pose at integer frame (cyclic, root advances per cycle)
    f = f % NL
    return {n: LP['P|' + n][f] - hz(lroot[f]) for n in pnames}


def nlerp(a, b, w):
    b = b if np.dot(a, b) >= 0 else -b
    q = a * (1 - w) + b * w; return q / np.linalg.norm(q)


def loop_at(c):
    a = int(math.floor(c)); w = c - a
    A, B = lrel(a), lrel(a + 1)
    P = {n: A[n] * (1 - w) + B[n] * w for n in pnames}
    ia, ib = a % NL, (a + 1) % NL
    Bx = {n: (nlerp(LP['R|' + n][ia], LP['R|' + n][ib], w), LP['T|' + n][ia] * (1 - w) + LP['T|' + n][ib] * w) for n in bnames}
    return P, Bx


sroot = SP['P|root']
S0 = {n: SP['P|' + n][0] - hz(sroot[0]) for n in pnames}
SB0 = {n: (SP['R|' + n][0], SP['T|' + n][0]) for n in bnames}
legk = [n for n in pnames if n.startswith(LEG) or 'thigh' in n]
if C0 is None:
    cost = [sum(float(np.linalg.norm(lrel(f)[n] - S0[n])) for n in legk) for f in range(22)]
    c0 = float(int(np.argmin(cost)))
else:
    c0 = float(C0)


def rate(u):
    return R0 + (1.0 - R0) * min(u, N_REV) / N_REV


cs = [c0]
for u in range(1, 400):
    cs.append(cs[-1] + 0.5 * (rate(u - 1) + rate(u)))
c_end = cs[N_REV]
N_NEW = N_REV + int(round(TAIL_CYCLES * 22)) + 1
# root: stock ramp until stock speed >= loop speed, then loop speed
sd = [float(np.dot(sroot[u] - sroot[0], FWD)) for u in range(sroot.shape[0])]
dist = [0.0]
for u in range(1, N_NEW):
    v = (sd[u] - sd[u - 1]) if u < len(sd) else VL
    if u > 1 and dist[-1] - dist[-2] >= VL - 0.05:       # once at loop speed, stay there
        v = VL
    dist.append(dist[-1] + min(max(v, 0.0), VL))
spd = [0.0] + [dist[u] - dist[u - 1] for u in range(1, N_NEW)]
s_raw = [min(1.0, max(S_MIN, spd[u] / (rate(u) * VL))) for u in range(N_NEW)]
s_sm = [s_raw[0]] + [(s_raw[i - 1] + s_raw[i] + s_raw[i + 1]) / 3 for i in range(1, N_NEW - 1)] + [s_raw[-1]]
BOUNCE = float(os.environ.get('BOUNCE', 1.0))         # pelvis vertical bounce scale at frame 0 (g2: 0.3)
LEAN_DEG = float(os.environ.get('LEAN_DEG', 0.0))      # extra forward lean at frame 0, upper body about the pelvis (g2: 22)
ARM = float(os.environ.get('ARM', 1.0))                # arm swing amplitude at frame 0 about the loop mean (g2: 1.7)
LEAN_IN = float(os.environ.get('LEAN_IN', 5.0))
UPPER = ('spine', 'neck', 'head')
ARMP = ('lowerarm', 'hand')
PEL_MEAN = float(LP['P|pelvis_MainPoint'][:, UP].mean())
ARM_MEAN = {n: (LP['P|' + n] - LP['P|upperarm_MainPoint_' + n[-1]]).mean(axis=0) for n in pnames if n.startswith(ARMP)}
out = {}
for u in range(N_NEW):
    P, Bx = loop_at(cs[u])
    w = ss(u / ENTRY) if ENTRY > 0 else 1.0
    if w < 1.0:
        P = {n: S0[n] * (1 - w) + P[n] * w for n in pnames}
        Bx = {n: (nlerp(SB0[n][0], Bx[n][0], w), SB0[n][1] * (1 - w) + Bx[n][1] * w) for n in bnames}
    # g2 (review g1 FAIL posa): during the rev, damp the vertical bounce (hop), add a forward drive lean, pump the arms harder.
    # 0 at frame 0 (= stock idle entry pose, no clash), full by LEAN_IN frames, back to 0 at the end of the rev
    g = (ss(u / LEAN_IN) * ss(1.0 - u / N_REV)) if u < N_REV else 0.0
    if g > 0 and (BOUNCE != 1.0 or LEAN_DEG or ARM != 1.0):
        dy = (BOUNCE - 1.0) * g * (P['pelvis_MainPoint'][UP] - PEL_MEAN)
        th = math.radians(LEAN_DEG) * g; cth, sth = math.cos(th), math.sin(th)
        pel = P['pelvis_MainPoint'] + np.eye(3)[UP] * dy
        A = 1.0 + (ARM - 1.0) * g
        Q = {}
        for n, v in P.items():
            if n.startswith(LEG) or n == 'ik_foot_root' or n == 'root':
                Q[n] = v; continue
            v = v + np.eye(3)[UP] * dy
            if n.startswith(ARMP):
                sh = P['upperarm_MainPoint_' + n[-1]] + np.eye(3)[UP] * dy
                v = sh + ARM_MEAN[n] + (v - sh - ARM_MEAN[n]) * A
            if n.startswith(UPPER) or n.startswith(ARMP) or n.startswith('upperarm') or n.startswith('clavicle'):
                d = v - pel                                    # forward lean about the lateral (X) axis, top goes +Z
                v = pel + np.array([d[0], d[1] * cth - d[2] * sth, d[1] * sth + d[2] * cth])
            Q[n] = v
        P = Q
    s = s_sm[u]
    rnew = hz(sroot[0]) + FWD * dist[u]
    W = {}
    for n, v in P.items():
        if s < 1.0 and n.startswith(LEG) and n != 'ik_foot_root':
            hip = P.get('thigh_MainPoint_' + n[-1])
            if hip is not None:
                d = v - hip; v = hip + hz(d) * s + (d - hz(d))
        W[n] = v + rnew
    out[u] = (W, Bx)
D = {}
for n in pnames:
    D['P|' + n] = np.stack([out[u][0][n] for u in range(N_NEW)])
for n in bnames:
    D['R|' + n] = np.stack([out[u][1][n][0] for u in range(N_NEW)]); D['T|' + n] = np.stack([out[u][1][n][1] for u in range(N_NEW)])
odir = os.path.join(ROOT, 'Saved/AnimPipeline/runstart'); os.makedirs(odir, exist_ok=True)
np.savez(os.path.join(odir, 'generic_%s_pose.npz' % TAG), **D)
# curves: loop curves sampled at c(u) (cyclic); stock phase triangle and contacts come with the loop
lc = meta['curves'] if isinstance(meta['curves'], dict) else eval(meta['curves'])
cur = {}
for k, arr in lc.items():
    arr = list(arr); n = len(arr)
    vals = []
    for u in range(N_NEW):
        c = cs[u]; a = int(math.floor(c)); w = c - a
        vals.append(round(arr[a % n] * (1 - w) + arr[(a + 1) % n] * w, 5))
    cur[k] = vals
# foot slip in world while planted (foot MainPoint y < 14 cm), per side
slip = {}
for sd_ in 'lr':
    fp = D['P|foot_MainPoint_' + sd_]
    sl = [float(np.linalg.norm(hz(fp[u] - fp[u - 1]))) for u in range(1, N_NEW) if fp[u, UP] < 14 and fp[u - 1, UP] < 14]
    slip[sd_] = round(sum(sl), 1)
info = {'TAG': TAG, 'c0': c0, 'c_end': round(c_end, 3), 'tail_loop_frame': round(c_end % NL, 3), 'N_REV': N_REV, 'N_NEW': N_NEW,
        'R0': R0, 'VL_cm_f': round(VL, 3), 'dist_end': round(dist[-1], 1), 's_min': round(min(s_sm), 3), 'slip_cm': slip,
        'steps_in_rev': round((c_end - c0) / 11.0, 2), 'cs': [round(x, 3) for x in cs[:N_NEW]], 'spd': [round(x, 2) for x in spd]}
json.dump({'info': info, 'curves': cur}, open(os.path.join(odir, 'generic_%s_curves.json' % TAG), 'w'))
print('ESITO: OK %s N_NEW %d c0 %.0f c_end %.2f (loop f %.2f) steps_in_rev %.2f' % (TAG, N_NEW, c0, c_end, c_end % NL, info['steps_in_rev']))
print('root end %.1f cm  s_min %.2f  slip L/R %s' % (dist[-1], min(s_sm), slip))
