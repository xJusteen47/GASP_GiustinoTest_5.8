# Turnarounds 3 = variant 27 (2026-11-21). Owner's design (scope: turnaround starts 90/180 L/R): the first step is taken with the
# foot on the side you turn toward, placed already rotated, the body rotates on that step, then straight into the frantic run start.
# Gemini refinement endorsed by the owner: Phase 1 clean pivot (180: 6-9 f, 90: 4-6 f at 60 fps; here 30 fps clips: H = 4 / 3),
# head and shoulders lead, one planted pivot foot (the other side) + open step-out toward the new heading that never crosses,
# pelvis only -3/-4 cm, arms tucked; Phase 2 when the pelvis is within ~20-30 deg of the final heading: the w2 frantic rev (sprinter
# drop, rapid steps on parallel tracks, arms pumping along the run axis). One continuous clip, root yaw consistent with the body.
# Why v26 looked wrong (owner "parecchio strane"): it played the forward rev from frame 0 in the root frame while the root yawed
# slowly (stock 24 f), so the legs ran forward while the body was still turning.
# How: frames 0..H-1 are built in WORLD from the stock idle entry pose (stock start f0, the pose idle hands over) and the rev's first
# frame: upper body = blend idle -> rev f0 relative to the pelvis, turned by segment yaws (pelvis = root yaw, chest and head lead);
# feet explicit (pivot foot planted, turning on its ball, toe-off at TO; step foot lifts, arcs and plants already turned at PL,
# then pivots on its ball to the rev's stance); knees by two-bone IK, arms tucked then rebuilt to rig lengths. From H the clip is
# the w2 rev (runstart_generic_compose.py, C0 = stance of the step foot, no idle entry) placed so its stance foot sits on the plant,
# with the step foot pinned in world while planted and the pivot foot's first swing replaced by an explicit low drive swing that
# lands on the rev's own first plant. Root yaw = pelvis yaw (frac curve); written to info.yaw_ue_deg like the w2t clips.
# usage: python runstart_pivot_compose.py <180L|180R|090L|090R> [TAG]   (needs generic_<REV>_pose.npz: runstart_pivot_all.py)
# Writes generic_<TAG>_{pose.npz,curves.json}; prints <= 5 lines (gates in runstart_pivot_gates.py).
import json, math, os, sys
import numpy as np

ROOT = os.path.dirname(os.path.abspath(__file__)).split(os.sep + 'Tools' + os.sep)[0]
RS = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart')
TURN = sys.argv[1]
TAG = sys.argv[2] if len(sys.argv) > 2 else 'p27_' + TURN
DEG, SIDE = int(TURN[:3]), TURN[3]
SG = 1.0 if SIDE == 'L' else -1.0               # Cascadeur yaw a: L turns go a0 = -deg -> 0 (stock reface L: UE yaw +deg -> 0)
E = lambda k, d: float(os.environ.get(k, d))
BIG = DEG == 180
# FRAC = fraction of the turn per clip frame (root yaw = pelvis yaw). Round 1 used Gemini's 8 game frames (0,.05,.27,.58,.85,...);
# in game the body could not follow it (review v27 r1 item 1: the visible turn followed the capsule, ~17 frames), so the default is
# round 2 capture (hips facing logged): the body DOES follow the clip's root yaw (Offset Root Bone Accumulate) once the clip owns
# the pose; round 1 lost its yaw in the 0.2 s blend-in (clip f0-2, Unreal Assistant: docs/notes/research/unreal-assistant-turn-yaw.md)
# and steering caught up at ~capsule pace. So: little yaw in f0-2 (blend), the turn at full weight f3-6, 88 % at the hand-off.
FRAC = [float(x) for x in os.environ.get('FRAC', '0,0.05,0.15,0.33,0.55,0.75,0.88,0.95,0.99,1' if BIG else
                                         '0,0.06,0.2,0.45,0.7,0.88,0.96,1').split(',')]
H = int(E('H', next(k for k, f in enumerate(FRAC) if f >= 0.85)))     # hand-off (rev frame 0): pelvis within ~25 deg (Gemini)
PL = E('PL', next(k for k, f in enumerate(FRAC) if f >= 0.5))          # step foot plant: half the turn, planted turned
LO = E('LO', max(0.4, PL - 3.0))                                        # step foot lift-off
TO = E('TO', H - 1)                                                     # pivot foot toe-off (lands on the rev's first plant)
STEP = math.radians(E('STEP_DEG', 150 if BIG else 75)) * SG     # step-out: step foot ankle rotated about the pivot ankle
LAG = math.radians(E('PLANT_LAG', 35 if BIG else 20)) * SG      # step foot lands this much less turned, pivots on the ball
PIV = math.radians(E('PIV_DEG', 60 if BIG else 35)) * SG        # pivot foot heel swing about its ball until toe-off
HEEL = math.radians(E('HEEL_DEG', 25))           # pivot foot heel lift at toe-off
DROP = E('DROP', 3.5)                            # pelvis dip during the pivot (cm)
LEAD_C, LEAD_H = math.radians(E('LEAD_C', 18)) * SG, math.radians(E('LEAD_H', 35)) * SG
TUCK = E('TUCK', 0.5)          # review r1 item 4: arms stay tucked (elbows at the ribs, hands at the chest) through H + 2
ARC_STEP, ARC_SWING = E('ARC_STEP', 8), E('ARC_SWING', 14)     # r1 item 3: low drive swing, shin never above horizontal
REV = os.environ.get('REV', 'p27rev' + ('L' if SIDE == 'L' else 'R'))
UP = 1
st, pv = ('l', 'r') if SIDE == 'L' else ('r', 'l')     # step foot, pivot foot
FOOT = ('foot_MainPoint_', 'foot_Self0Point_', 'ball_MainPoint_', 'ball_DirectionPoint_', 'ball_AdditionalPoint_')

LP = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runloop/loop_relaxed_dump.npz'))
SP = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runloop/startL_stock_dump.npz'))
RV = np.load(os.path.join(RS, 'generic_%s_pose.npz' % REV))
RC = json.load(open(os.path.join(RS, 'generic_%s_curves.json' % REV)))
pnames = [k[2:] for k in RV.files if k.startswith('P|')]
bnames = [k[2:] for k in RV.files if k.startswith('R|')]
NR = RV['P|root'].shape[0]
hz = lambda v: v - v[..., UP:UP + 1] * np.eye(3)[UP]
upv = np.eye(3)[UP]


def ss(x):
    x = min(max(x, 0.0), 1.0); return x * x * (3 - 2 * x)


def ry(v, a):
    c, s_ = math.cos(a), math.sin(a)
    return np.array([v[0] * c + v[2] * s_, v[1], -v[0] * s_ + v[2] * c])


def rot_axis(v, ax, t):                          # Rodrigues
    ax = ax / np.linalg.norm(ax); c, s_ = math.cos(t), math.sin(t)
    return v * c + np.cross(ax, v) * s_ + ax * float(np.dot(ax, v)) * (1 - c)


def nlerp(a, b, w):
    b = b if np.dot(a, b) >= 0 else -b
    q = a * (1 - w) + b * w; return q / np.linalg.norm(q)


a0 = -SG * math.radians(DEG)
fr = lambda u: FRAC[min(int(u), len(FRAC) - 1)] if u == int(u) else FRAC[int(u)] + (FRAC[min(int(u) + 1, len(FRAC) - 1)] - FRAC[int(u)]) * (u - int(u))
NT = H + NR
yaw = [a0 * (1.0 - fr(u)) for u in range(NT)]
LT = {s: float(np.linalg.norm(LP['P|calf_MainPoint_' + s][0] - LP['P|thigh_MainPoint_' + s][0])) for s in 'lr'}
LC = {s: float(np.linalg.norm(LP['P|foot_MainPoint_' + s][0] - LP['P|calf_MainPoint_' + s][0])) for s in 'lr'}
LU = {s: float(np.linalg.norm(LP['P|lowerarm_MainPoint_' + s][0] - LP['P|upperarm_MainPoint_' + s][0])) for s in 'lr'}
LF = {s: float(np.linalg.norm(LP['P|hand_MainPoint_' + s][0] - LP['P|lowerarm_MainPoint_' + s][0])) for s in 'lr'}

# --- sources -------------------------------------------------------------------------------------------------------------------
root0 = hz(SP['P|root'][0])
S0 = {n: SP['P|' + n][0] - root0 for n in pnames}                     # idle entry, root-relative, facing +Z
SB0 = {n: (SP['R|' + n][0], SP['T|' + n][0]) for n in bnames}
rr = lambda k: hz(RV['P|root'][k])
RVk = lambda k: {n: RV['P|' + n][k] - rr(k) for n in pnames}           # rev frame k root-relative, facing +Z
R0 = RVk(0)
W0 = {n: root0 + ry(S0[n], a0) for n in pnames}                         # idle in world at f0
# step foot plant: its ankle rotated about the pivot ankle by STEP (open, toward the new heading)
WL = W0['foot_MainPoint_' + pv] + ry(W0['foot_MainPoint_' + st] - W0['foot_MainPoint_' + pv], STEP)
WL[UP] = W0['foot_MainPoint_' + st][UP]
# rev placement: rev f0 (yaw[H]) with its stance (step) ankle on WL
rootH = WL - ry(R0['foot_MainPoint_' + st], yaw[H]); rootH[UP] = 0.0
root = []
for u in range(NT):
    if u < H:
        root.append(root0 + (rootH - root0) * ss(u / H))
    else:
        root.append(rootH + rr(u - H) - rr(0))
world_rev = lambda u: {n: root[u] + ry(RVk(u - H)[n], yaw[u]) for n in pnames}

# --- groups -------------------------------------------------------------------------------------------------------------------
LEGS = ('calf_', 'foot_', 'ball_')
def seg_yaw(n, u, lead_w):
    a = yaw[u]
    if n.startswith(('neck', 'head')):
        return a + (LEAD_C + LEAD_H) * lead_w
    if n.startswith(('spine_04', 'clavicle', 'upperarm', 'lowerarm', 'hand')):
        return a + LEAD_C * lead_w
    if n.startswith('spine_02'):
        return a + 0.5 * LEAD_C * lead_w
    return a


def foot_group(W, s):
    return {n + s: W[n + s].copy() for n in FOOT}


def turn_group(G, pivot, t):                     # yaw a foot group about a point
    return {n: pivot + ry(v - pivot, t) for n, v in G.items()}


def place_group(G, anchor_name, p):
    d = p - G[anchor_name]; return {n: v + d for n, v in G.items()}


def lerp_group(A, B, w, anchor):                  # translate anchor linearly, blend shape (rel vectors) with the yaw difference
    fa = hz(A['ball_MainPoint_' + anchor[-1]] - A[anchor]); fb = hz(B['ball_MainPoint_' + anchor[-1]] - B[anchor])
    dy = math.atan2(fb[0], fb[2]) - math.atan2(fa[0], fa[2])            # yaw A -> B in the ry convention (ry(+Z, a) = (sin a, 0, cos a))
    dy = (dy + math.pi) % (2 * math.pi) - math.pi
    pa = A[anchor] * (1 - w) + B[anchor] * w
    out = {}
    for n in A:
        ra, rb = ry(A[n] - A[anchor], dy * w), B[n] - B[anchor]
        v = ra * (1 - w) + rb * w; ln = float(np.linalg.norm(v))       # keep segment lengths (lerp of vectors shortens)
        out[n] = pa + (v * (float(np.linalg.norm(ra)) * (1 - w) + float(np.linalg.norm(rb)) * w) / ln if ln > 1e-6 else v)
    return out


def leg_ik(W, s, fw):
    Hh, F, K_old = W['thigh_MainPoint_' + s], W['foot_MainPoint_' + s], W['calf_MainPoint_' + s]
    d = F - Hh; dl = float(np.linalg.norm(d)); ax = d / dl
    f = fw - ax * float(np.dot(fw, ax)); f /= np.linalg.norm(f)
    dc = min(dl, LT[s] + LC[s] - 0.01)
    x = (LT[s] ** 2 - LC[s] ** 2 + dc ** 2) / (2 * dc); h = math.sqrt(max(LT[s] ** 2 - x * x, 0.0))
    K = Hh + ax * x + f * h
    for n in [n for n in W if n.startswith('calf_') and n.endswith('_' + s) and n not in ('calf_MainPoint_' + s, 'calf_LimbDir_' + s)]:
        W[n] = W[n] - K_old + K
    W['calf_MainPoint_' + s] = K; W['calf_LimbDir_' + s] = K + f * 45.0


def arm_ik(W, s, soft=0.85):
    S, E0, H0 = W['upperarm_MainPoint_' + s], W['lowerarm_MainPoint_' + s], W['hand_MainPoint_' + s]
    d = H0 - S; dl = float(np.linalg.norm(d)); ax = d / dl
    reach = LU[s] + LF[s]; r0 = soft * reach
    dsoft = dl if dl <= r0 else r0 + (reach - r0) * (1.0 - math.exp(-(dl - r0) / (reach - r0)))
    Hn = S + ax * dsoft; dc = float(np.linalg.norm(Hn - S))
    b = W['lowerarm_LimbDir_' + s] - S; b = b - ax * float(np.dot(b, ax))
    if np.linalg.norm(b) < 1e-3:
        b = (E0 - S) - ax * float(np.dot(E0 - S, ax))
    b /= np.linalg.norm(b)
    x = (LU[s] ** 2 - LF[s] ** 2 + dc ** 2) / (2 * dc); hh = math.sqrt(max(LU[s] ** 2 - x * x, 0.0))
    En = S + ax * x + b * hh
    for n in [n for n in W if n.startswith('hand_') and n.endswith('_' + s)]:
        W[n] = W[n] - H0 + Hn
    for n in [n for n in W if n.startswith('lowerarm_') and n.endswith('_' + s) and n != 'lowerarm_LimbDir_' + s]:
        W[n] = W[n] - E0 + En


# key foot configurations in world
GH_st = foot_group(world_rev(H), st)                                    # step foot in the rev's stance at H
ball_st = GH_st['ball_MainPoint_' + st]
G_plant = turn_group(GH_st, ball_st, -LAG)                              # lands less turned, pivots on the ball to GH_st
G0_st, G0_pv = foot_group(W0, st), foot_group(W0, pv)
ball_pv = G0_pv['ball_MainPoint_' + pv]
k_rp = next(p[1] for p in RC['info']['pinned'] if p[0] == pv)          # rev frame of the pivot foot's first plant
U_RP = H + k_rp


def pivot_foot(u):
    if u <= TO:
        G = turn_group(G0_pv, ball_pv, PIV * ss(u / TO))
        # heel lift about the FOOT's own lateral axis (owner 2026-11-21 "il piede destro è rotto": the body's lateral axis, while the
        # foot is yawed differently on its ball, rolled the foot on its side and bent the ankle)
        fd = hz(G['ball_MainPoint_' + pv] - G['foot_MainPoint_' + pv])
        lat = np.cross(upv, fd / max(float(np.linalg.norm(fd)), 1e-6))
        return {n: ball_pv + rot_axis(v - ball_pv, lat, HEEL * ss((u - (TO - 1.5)) / 1.5)) for n, v in G.items()}
    return None


G_to = pivot_foot(TO)
# heel lift direction check: the ankle must rise; flip the axis sign if it went down
if G_to['foot_MainPoint_' + pv][UP] < G0_pv['foot_MainPoint_' + pv][UP]:
    HEEL = -HEEL; G_to = pivot_foot(TO)
G_rp = foot_group(world_rev(U_RP), pv)

W_all = {}
for u in range(NT):
    if u < H:
        w = (u / H) ** 2          # ease-in only: the body is moving at the rev's speed when the rev takes over (no stop at H)
        # review r1 item 2: the torso stays upright (<= ~10 deg) until the hand-off; the rev's lean comes after H
        w_up = (u / H) ** 3
        UPB = ('spine', 'neck', 'head', 'clavicle', 'upperarm', 'lowerarm', 'hand')
        lead_w = ss(u / 1.0) * (1.0 - ss((u - (H - 2)) / 2.0))
        pel_rel = {n: (S0[n] - S0['pelvis_MainPoint']) * (1 - (w_up if n.startswith(UPB) else w))
                   + (R0[n] - R0['pelvis_MainPoint']) * (w_up if n.startswith(UPB) else w) for n in pnames}
        pw0 = W0['pelvis_MainPoint']; pwH = root[H] + ry(R0['pelvis_MainPoint'], yaw[H])
        Pw = pw0 + (pwH - pw0) * ss(u / H) - upv * DROP * math.sin(math.pi * min(u / H, 1.0))
        W = {n: Pw + ry(pel_rel[n], seg_yaw(n, u, lead_w)) for n in pnames}
        W['root'] = root[u].copy()
        for n in ('ik_foot_root', 'ik_hand_root'):
            W[n] = root[u] + ry(R0[n] * w + S0[n] * (1 - w), yaw[u])
    else:
        W = world_rev(u)
    # arms tucked (review r1 item 4): elbows down at the ribs, fists in front of the chest, from f1, held through H, released into
    # the rev's pump by H + 2
    # Arms lead the turn (owner 2026-11-21, all four turns, replaces "arms tucked"): the hands travel back and outward like a
    # person who turns and starts to run, never stuck to the body and never still: the target moves every frame (swing grows
    # through the pivot, the turn-side arm a frame ahead), then hands over to the rev's pump by H + 2. Rejected before: tucked
    # elbows + fists at the hip ("T-rex", hands through the leg).
    for s in 'lr':
        lead = 1.0 if s == st else 0.0                        # turn-side arm leads by one frame
        p = min(max((u + lead) / H, 0.0), 1.2)
        tk = TUCK * ss((u + lead) / 1.5) * (1.0 - ss((u - H) / 3.0))
        if tk <= 0:
            continue
        fwd = ry(np.array([0.0, 0.0, 1.0]), yaw[u] + (LEAD_C * ss(u / 1.0) * (1.0 - ss((u - (H - 2)) / 2.0)) if u < H else 0.0))
        sh = W['upperarm_MainPoint_' + s]
        inward = hz(W['spine_04_MainPoint'] - sh)
        out_s = -inward / max(float(np.linalg.norm(inward)), 1e-6)
        sw = ss(p / 0.8) * (1.15 if lead else 0.9)            # swing back/out grows through the pivot (continuous motion)
        # owner 22:47: each elbow moves out to its own side (right elbow to the right, left to the left), the lead arm also back
        e_tgt = sh - upv * (21.0 - 5.0 * sw) - fwd * ((6.0 + 10.0 * sw) if lead else 3.0) + out_s * (7.0 + 13.0 * sw)
        h_tgt = e_tgt - upv * (16.0 - 6.0 * sw) - fwd * (6.0 + 10.0 * sw) + out_s * (5.0 + 9.0 * sw)
        if True:
            dh, de = (h_tgt - W['hand_MainPoint_' + s]) * tk, (e_tgt - W['lowerarm_MainPoint_' + s]) * tk
            for n in [n for n in W if n.startswith('hand_') and n.endswith('_' + s)]:
                W[n] = W[n] + dh
            for n in [n for n in W if n.startswith('lowerarm_') and n.endswith('_' + s)]:
                W[n] = W[n] + de
    # step foot
    if u < H:
        if u <= LO:
            G = G0_st
        elif u < PL:
            s_ = ss((u - LO) / (PL - LO))
            G = lerp_group(G0_st, G_plant, s_, 'foot_MainPoint_' + st)
            G = {n: v + upv * ARC_STEP * math.sin(math.pi * s_) for n, v in G.items()}
        else:
            G = turn_group(G_plant, ball_st, LAG * ss((u - PL) / (H - PL)))
        W.update(G)
    # pivot foot
    if u <= U_RP:
        if u <= TO:
            G = pivot_foot(u)
        else:
            s_ = ss((u - TO) / (U_RP - TO))
            G = lerp_group(G_to, G_rp, s_, 'foot_MainPoint_' + pv)
            G = {n: v + upv * ARC_SWING * math.sin(math.pi * s_) for n, v in G.items()}
        W.update(G)
    W_all[u] = W

# step foot pinned in world while the rev keeps it planted (rev pin of the stance foot at rev frame 0)
pin = next((p for p in RC['info']['pinned'] if p[0] == st and p[1] == 0), None)
if pin:
    for u in range(H, H + pin[2] + 1):
        W_all[u].update(GH_st)
# knees and arms
# knees only where this script moved the legs (pivot frames, the pinned stance, the explicit pivot-foot swing): bend toward
# the pelvis facing, plus the foot direction when the foot is near flat (a kicked-up foot has no usable direction). The bend
# direction is smoothed in time (the facing turns 40-55 deg per frame in the 180: unsmoothed, the knee zigzagged at f1-2).
SM = lambda seq: [seq[0]] + [0.25 * seq[i - 1] + 0.5 * seq[i] + 0.25 * seq[i + 1] for i in range(1, len(seq) - 1)] + [seq[-1]]
for s in 'lr':
    us = [u for u in range(NT) if u < H or (s == st and u <= H + (pin[2] if pin else 0)) or (s == pv and u <= U_RP)]
    fws = []
    for u in us:
        W = W_all[u]
        fdir = hz(W['ball_MainPoint_' + s] - W['foot_MainPoint_' + s]); nf = float(np.linalg.norm(fdir))
        fw = ry(np.array([0.0, 0.0, 1.0]), yaw[u]) + (2.0 * fdir / nf * min(nf / 10.0, 1.0) if nf > 1e-3 else 0.0)   # knee over the toes (owner: twisted R foot)
        fws.append(fw / np.linalg.norm(fw))
    fws = SM(SM(SM(fws)))
    for u, fw in zip(us, fws):
        leg_ik(W_all[u], s, fw / np.linalg.norm(fw))
# arms: hand and elbow relative to the shoulder smoothed over the hand-off (template + tuck -> rev pump), then rig lengths
AW = range(1, H + 6)
for s in 'lr':
    grp = [n for n in pnames if n.startswith(('hand_', 'lowerarm_')) and n.endswith('_' + s)]
    rel0 = {u: {n: W_all[u][n] - W_all[u]['upperarm_MainPoint_' + s] for n in grp} for u in range(0, H + 7)}
    rel = rel0
    for _ in range(2):
        rel = {u: ({n: 0.25 * rel[u - 1][n] + 0.5 * rel[u][n] + 0.25 * rel[u + 1][n] for n in grp} if u in AW else rel[u]) for u in rel}
    for u in AW:
        tw = 1.0 - ss((u - (H + 2)) / 3.0)     # smoothing fades out into the rev's own pump (no step at the window's edge)
        for n in grp:
            W_all[u][n] = W_all[u]['upperarm_MainPoint_' + s] + rel[u][n] * tw + rel0[u][n] * (1.0 - tw)
for u in range(H + 6):
    for s in 'lr':
        arm_ik(W_all[u], s, 0.85 if u < H else 0.999)     # rev frames are already soft-reached once

# world -> clip (root frame, root yaw written in UE)
D = {}
out = []
for u in range(NT):
    W = W_all[u]
    out.append({n: root[u] + ry(W[n] - root[u], -yaw[u]) for n in pnames})
for n in pnames:
    D['P|' + n] = np.stack([out[u][n] for u in range(NT)])
for n in bnames:
    qs, ts = [], []
    for u in range(NT):
        if u < H:
            # fingers: the rev's loose fists from f1 (review r1 item 4: no open hanging hands), the rest blends over the pivot
            w = ss(u / 1.0) if n.startswith(('thumb', 'index', 'middle', 'ring', 'pinky')) else ss(u / H)
            qs.append(nlerp(SB0[n][0], RV['R|' + n][0], w)); ts.append(SB0[n][1] * (1 - w) + RV['T|' + n][0] * w)
        else:
            qs.append(RV['R|' + n][u - H]); ts.append(RV['T|' + n][u - H])
    D['R|' + n] = np.stack(qs); D['T|' + n] = np.stack(ts)
np.savez(os.path.join(RS, 'generic_%s_pose.npz' % TAG), **D)
cur = {k: [v[0]] * H + list(v) for k, v in RC['curves'].items()}
# Stock reface curves re-timed to this turn (round 2 capture: the stock-copied enable_warping switched orientation warping on at
# clip f5 and there was no steeringtargettime, so in game the body stopped at the capsule's facing for the last ~40 deg).
# Stock 180_L_Lfoot: enable_warping 0 until its turn is nearly done (f5 -> 1 at f15), steeringtargettime 1.0 through the turn,
# down to 0 over 13 frames after it. Here: warping 0 until H + WARP_IN, ramp WARP_RAMP; steering time 1.0 until H + 2, ramp 6.
WARP_IN, WARP_RAMP = E('WARP_IN', 2), E('WARP_RAMP', 4)
cur['own_enable_warping'] = [round(ss((u - H - WARP_IN) / WARP_RAMP), 4) for u in range(NT)]
cur['own_steeringtargettime'] = [round(1.0 - ss((u - H - 2) / 6.0), 4) for u in range(NT)]
slip = {}
for s in 'lr':
    fp = [W_all[u]['foot_MainPoint_' + s] for u in range(NT)]
    slip[s] = round(sum(float(np.linalg.norm(hz(fp[u] - fp[u - 1]))) for u in range(1, NT) if fp[u][UP] < 14 and fp[u - 1][UP] < 14), 1)
info = {'TAG': TAG, 'turn': TURN, 'H': H, 'N_NEW': NT, 'rev': REV, 'yaw_ue_deg': [round(-math.degrees(y), 3) for y in yaw],
        'pinned': [[st, int(PL), H + (pin[2] if pin else 0)], [pv, 0, int(TO)]], 'slip_cm': slip, 'U_RP': U_RP,
        'root_H': [round(float(x), 1) for x in rootH], 'src': 'pivot27'}
json.dump({'info': info, 'curves': cur}, open(os.path.join(RS, 'generic_%s_curves.json' % TAG), 'w'))
print('ESITO: OK %s N %d H %d pivot-foot plant f%d root_H %s' % (TAG, NT, H, U_RP, info['root_H']))
print('yaw ue %s' % [round(-math.degrees(y)) for y in yaw[:H + 4]])
print('slip world L/R %s (planted < 14 cm)' % slip)
