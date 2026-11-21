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


HOLD = os.environ.get('HOLD')                   # g3: full push until HOLD, release HOLD..N_REV (review g2: rev died by f12)
HOLD = int(HOLD) if HOLD else None
RATE_SAG = float(os.environ.get('RATE_SAG', 0.3))  # g3: fraction of the extra rate lost during the push (rest after HOLD)


PRE = float(os.environ.get('PRE', 0))             # sprinter: rate ramps RATE_PRE -> R0 over the first PRE frames (the set/load)
RATE_PRE = float(os.environ.get('RATE_PRE', 1.0))


def rate(u):
    if PRE and u < PRE:
        return RATE_PRE + (R0 - RATE_PRE) * ss(u / PRE)
    if HOLD is None:
        return R0 + (1.0 - R0) * min(u, N_REV) / N_REV
    if u <= HOLD:
        e = 1.0 - RATE_SAG * u / HOLD
    else:
        e = (1.0 - RATE_SAG) * (1.0 - ss(min((u - HOLD) / float(N_REV - HOLD), 1.0)))
    return 1.0 + (R0 - 1.0) * e


cs = [c0]
for u in range(1, 400):
    cs.append(cs[-1] + 0.5 * (rate(u - 1) + rate(u)))
c_end = cs[N_REV]
N_NEW = N_REV + int(round(TAIL_CYCLES * 22)) + 1
# root: stock ramp until stock speed >= loop speed, then loop speed
sd = [float(np.dot(sroot[u] - sroot[0], FWD)) for u in range(sroot.shape[0])]
# Turnarounds (runstart-turnaround-desktop, 2026-11-21): SRC = meta json of a stock REFACE start (runstart_turn_export_ue.py: root
# per frame [x, y, z, yaw] in UE mesh space). The root path (UE +Y = Cascadeur +Z) and the root YAW (180/90 -> 0) come from it.
# The clip is composed in the ROOT frame (body facing the run direction, the w1/w2 recipe unchanged) and the UE import writes the
# stock root yaw on the root bone (runstart_generic_g_ue.py, RSGG_SRC): body in world = root yaw x root-frame pose, like stock.
# Contacts are pinned in WORLD (PIN_U), the lean tips the body toward the run direction in world (LEAN_WORLD=1).
SRC = os.environ.get('SRC')
YAW = None
if SRC:
    _rr = json.load(open(SRC))['root']
    sd = [float(r[1] - _rr[0][1]) for r in _rr]
    YAW = [math.radians(-float(r[3])) for r in _rr]          # Cascadeur yaw about +Y (X = UE X, Z = UE Y: handedness flips)
LEAN_WORLD = os.environ.get('LEAN_WORLD', '1') == '1'
LEAN_TURN = float(os.environ.get('LEAN_TURN', 17.0))
# Review t180 r1 (Fable FAIL trait 1, v88-96): with the lean fixed to the final heading while the body still faced back, the body
# tipped BACKWARD (head thrown back, a slip, not a sprinter). TURN_LEAN=chest: the chest LEADS the turn (CHEST_LEAD deg ahead of the
# root yaw, head HEAD_LEAD more, ramped in over LEAD_IN frames: anticipation) and the lean tips along the CHEST facing, faded in only
# once the chest yaw is below LEAN_YAW0 deg (full by LEAN_YAW0 - LEAN_YAW_RAMP): sink upright -> turn -> fall forward.
TURN_LEAN = os.environ.get('TURN_LEAN', 'world')
CHEST_LEAD, HEAD_LEAD = math.radians(float(os.environ.get('CHEST_LEAD', 45))), math.radians(float(os.environ.get('HEAD_LEAD', 20)))
LEAD_IN = float(os.environ.get('LEAD_IN', 2))
LEAN_YAW0, LEAN_YAW_RAMP = float(os.environ.get('LEAN_YAW0', 120)), float(os.environ.get('LEAN_YAW_RAMP', 60))


def chest_lead(u):                                       # local rotation (rad) of the chest about the pelvis: toward the run direction
    t = yaw_at(u)
    if YAW is None or TURN_LEAN != 'chest' or abs(t) < 1e-6:
        return 0.0
    return -math.copysign(min(abs(t), CHEST_LEAD), t) * ss(u / LEAD_IN)


def yaw_at(x):
    if YAW is None:
        return 0.0
    if x >= len(YAW) - 1:
        return YAW[-1]
    a = int(math.floor(x)); w = x - a
    return YAW[a] * (1 - w) + YAW[a + 1] * w


def ry(v, a):                                            # rotate a vector about +Y by a (rad): +Z goes to (sin a, 0, cos a)
    c, s_ = math.cos(a), math.sin(a)
    return np.array([v[0] * c + v[2] * s_, v[1], -v[0] * s_ + v[2] * c])
dist = [0.0]
for u in range(1, N_NEW):
    v = (sd[u] - sd[u - 1]) if u < len(sd) else VL
    if u > 1 and dist[-1] - dist[-2] >= VL - 0.05:       # once at loop speed, stay there
        v = VL
    dist.append(dist[-1] + min(max(v, 0.0), VL))
spd = [0.0] + [dist[u] - dist[u - 1] for u in range(1, N_NEW)]
s_raw = [min(1.0, max(S_MIN, spd[u] / (rate(u) * VL))) for u in range(N_NEW)]
U = list(range(N_NEW)); C = [cs[u] for u in range(N_NEW)]
TMAP = os.environ.get('TMAP')
# w2 (owner 2026-11-21 "rifinisci lo scivolamento"): in game the capsule (Mover, DA ramp) runs 2-3x ahead of the stock start root ramp
# during the drive and the dynamic play rate (capsule speed / movedata_speed) speeds the clip up (w1: 1.0 -> 1.27x), so the planted
# drive feet slide (w1 capture: R 15 cm v94-100, L 22 cm v104-108). TMAP = json {"uf": authored frame of the source variant per 1/30 s
# of game time, "dist": capsule distance} measured in that variant's capture: the clip is resampled on the timing the game showed and
# its root follows the capsule, so movedata_speed = capsule speed (play rate ~1, same timing as seen) and strides match the capsule.
if TMAP:
    tm = json.load(open(TMAP)); UF, DS = list(tm['uf']), list(tm['dist'])
    cint = lambda x: cs[int(x)] + (cs[int(x) + 1] - cs[int(x)]) * (x - int(x))
    # w2 capture: following the capsule past the rev made the hand-off contacts slide MORE (capsule 600 cm/s > loop stride at the
    # measured clip rate; L 25.6 -> 37.3, R 28.8 -> 35.0 cm). From the first frame at authored frame >= TMAP_CUT (default N_REV) the
    # clip is the source variant again (1 authored frame per frame, root at loop speed) and the game's dynamic play rate does the rest.
    cut = next(k for k, x in enumerate(UF) if x >= float(os.environ.get('TMAP_CUT', N_REV)))
    UF, DS = UF[:cut + 1], DS[:cut + 1]
    while UF[-1] < N_NEW - 1:
        UF.append(min(UF[-1] + 1.0, N_NEW - 1.0)); DS.append(DS[-1] + VL * (UF[-1] - UF[-2]))
    N_NEW = len(UF); U = UF; C = [cint(x) for x in UF]; dist = DS
    spd = [0.0] + [dist[k] - dist[k - 1] for k in range(1, N_NEW)]
    rk = [(C[min(k + 1, N_NEW - 1)] - C[max(k - 1, 0)]) / (min(k + 1, N_NEW - 1) - max(k - 1, 0)) for k in range(N_NEW)]
    s_raw = [min(1.0, max(S_MIN, spd[k] / (rk[k] * VL))) for k in range(N_NEW)]
s_sm = [s_raw[0]] + [(s_raw[i - 1] + s_raw[i] + s_raw[i + 1]) / 3 for i in range(1, N_NEW - 1)] + [s_raw[-1]]
BOUNCE = float(os.environ.get('BOUNCE', 1.0))         # pelvis vertical bounce scale at frame 0 (g2: 0.3)
LEAN_DEG = float(os.environ.get('LEAN_DEG', 0.0))      # extra forward lean at frame 0, upper body about the pelvis (g2: 22)
ARM = float(os.environ.get('ARM', 1.0))                # arm swing amplitude at frame 0 about the loop mean (g2: 1.7)
LEAN_IN = float(os.environ.get('LEAN_IN', 5.0))
UPPER = ('spine', 'neck', 'head')
ARMP = ('lowerarm', 'hand')
PEL_MEAN = float(LP['P|pelvis_MainPoint'][:, UP].mean())
LEAN_OUT = float(os.environ.get('LEAN_OUT', N_REV))     # lean/arm envelope end (default: end of the rev)
DROP_CM = float(os.environ.get('DROP_CM', 0.0))          # sprinter: pelvis drop (cm) during the drive steps
DROP_IN, DROP_HOLD, DROP_OUT = (float(os.environ.get(k, d)) for k, d in (('DROP_IN', 4), ('DROP_HOLD', 10), ('DROP_OUT', 16)))
ARM_BACK = float(os.environ.get('ARM_BACK', 0.0))        # sprinter: elbows / hands drawn back (cm, -Z) with the lean envelope
FOOT_RIGID = os.environ.get('FOOT_RIGID', '1') == '1'     # 0 = bit-exact rebuild of clips made before 2026-11-21 20:00Z (w1, w2, w2t*)
FOOT_GROUP = ('foot_', 'ball_', 'ik_foot_l', 'ik_foot_r')
LEG_IK = os.environ.get('LEG_IK', '1') == '1'           # rebuild knees + poles (fix for backward knees, see below)
POLE_CM = float(os.environ.get('POLE_CM', 45.0))
LT = {sd_: float(np.linalg.norm(LP['P|calf_MainPoint_' + sd_][0] - LP['P|thigh_MainPoint_' + sd_][0])) for sd_ in 'lr'}
LC = {sd_: float(np.linalg.norm(LP['P|foot_MainPoint_' + sd_][0] - LP['P|calf_MainPoint_' + sd_][0])) for sd_ in 'lr'}
ENTRY_LEAD = float(os.environ.get('ENTRY_LEAD', 0.0))   # g3: pelvis/legs finish the entry this many frames before the chest
KNEE_DEG = float(os.environ.get('KNEE_DEG', 0.0))       # g3: extra hip flexion of the swing leg (high knees), shin folds back
KNEE_A, KNEE_B = float(os.environ.get('KNEE_A', 4)), float(os.environ.get('KNEE_B', 13))   # frame window (steps 2-3)


def rot_x(v, c, t):                                      # about lateral X through c; positive t swings a hanging segment forward
    d = v - c
    return c + np.array([d[0], d[1] * math.cos(t) + d[2] * math.sin(t), -d[1] * math.sin(t) + d[2] * math.cos(t)])


ARM_MEAN = {n: (LP['P|' + n] - LP['P|upperarm_MainPoint_' + n[-1]]).mean(axis=0) for n in pnames if n.startswith(ARMP)}
def leg_ik(W):
    # Owner 2026-11-21 "le gambe sono rotte" (g3 and s1): stride scaling and the pelvis drop are not rigid, so the knee point and
    # the pole (calf_LimbDir) ended near the hip-foot line and Cascadeur's leg IK bent the knee BACKWARD (-16..-37 cm, s1 f1-6).
    # Rebuild each knee by two-bone IK with the rig lengths, bending toward the character's forward, and put the pole in front.
    for sd_ in 'lr':
        H, F = W['thigh_MainPoint_' + sd_], W['foot_MainPoint_' + sd_]
        K_old = W['calf_MainPoint_' + sd_]
        d = F - H; dl = float(np.linalg.norm(d)); ax = d / dl
        fw = FWD - ax * float(np.dot(FWD, ax)); fw /= np.linalg.norm(fw)          # forward, perpendicular to hip-foot line
        dc = min(dl, LT[sd_] + LC[sd_] - 0.01)
        x = (LT[sd_] ** 2 - LC[sd_] ** 2 + dc ** 2) / (2 * dc)
        h = math.sqrt(max(LT[sd_] ** 2 - x * x, 0.0))
        K = H + ax * x + fw * h
        for n in [n for n in W if n.startswith('calf_') and n.endswith('_' + sd_) and n not in ('calf_MainPoint_' + sd_, 'calf_LimbDir_' + sd_)]:
            W[n] = W[n] - K_old + K
        W['calf_MainPoint_' + sd_] = K
        W['calf_LimbDir_' + sd_] = K + fw * POLE_CM


out = {}
roots = {}
for k in range(N_NEW):
    u = U[k]
    P, Bx = loop_at(C[k])
    w = ss(u / ENTRY) if ENTRY > 0 else 1.0
    wl = ss(u / max(ENTRY - ENTRY_LEAD, 1.0)) if ENTRY > 0 else 1.0     # g3: pelvis and legs lead the entry, upper body follows
    if w < 1.0:
        lead = lambda n: n.startswith(LEG) or 'thigh' in n or n.startswith('pelvis')
        P = {n: S0[n] * (1 - (wl if lead(n) else w)) + P[n] * (wl if lead(n) else w) for n in pnames}
        Bx = {n: (nlerp(SB0[n][0], Bx[n][0], w), SB0[n][1] * (1 - w) + Bx[n][1] * w) for n in bnames}
    # g2 (review g1 FAIL posa): during the rev, damp the vertical bounce (hop), add a forward drive lean, pump the arms harder.
    # 0 at frame 0 (= stock idle entry pose, no clash), full by LEAN_IN frames, back to 0 at the end of the rev
    if HOLD is None:
        g = (ss(u / LEAN_IN) * ss(1.0 - u / N_REV)) if u < N_REV else 0.0
    else:                                                    # g3: plateau, full from LEAN_IN to HOLD, ease out to N_REV
        g = (ss(u / LEAN_IN) * ((1.0 - ss((u - HOLD) / float(LEAN_OUT - HOLD))) if u > HOLD else 1.0)) if u < LEAN_OUT else 0.0
    # sprinter: low centre of mass (pelvis and everything above it down, feet stay, knees bend by IK) and elbows drawn back
    gd = (ss(u / DROP_IN) * ((1.0 - ss((u - DROP_HOLD) / float(DROP_OUT - DROP_HOLD))) if u > DROP_HOLD else 1.0)) if DROP_CM and u < DROP_OUT else 0.0
    if gd > 0:
        P = {n: (v if (n.startswith(LEG) or n == 'ik_foot_root' or n == 'root') else v - np.eye(3)[UP] * DROP_CM * gd) for n, v in P.items()}
    if ARM_BACK and g > 0:
        P = {n: (v - np.array([0.0, 0.0, ARM_BACK * g * (1.0 if n.startswith('lowerarm') else 0.6)]) if n.startswith(ARMP) else v)
             for n, v in P.items()}
    if g > 0 and (BOUNCE != 1.0 or LEAN_DEG or ARM != 1.0):
        dy = (BOUNCE - 1.0) * g * (P['pelvis_MainPoint'][UP] - PEL_MEAN)
        # turnaround: the loop's own forward lean (~LEAN_TURN deg, along the body) points away from the run direction while the body
        # still faces back; the extra (1 - cos yaw) x LEAN_TURN keeps the fall toward the new heading at ~w1's 40 deg (trait 1)
        if TURN_LEAN == 'chest' and YAW is not None:
            phi = chest_lead(u)
            cy = math.degrees(abs(yaw_at(u) + phi))           # chest yaw left to turn (world)
            th = math.radians(LEAN_DEG) * g * ss((LEAN_YAW0 - cy) / LEAN_YAW_RAMP)
        else:
            th = math.radians(LEAN_DEG + (LEAN_TURN * (1.0 - math.cos(yaw_at(u))) if LEAN_WORLD else 0.0)) * g
        cth, sth = math.cos(th), math.sin(th)
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
                ya = yaw_at(u) if LEAN_WORLD else 0.0           # turnaround: tip toward the RUN direction (root frame: ry(+Z, -yaw))
                if TURN_LEAN == 'chest' and YAW is not None:
                    ya = 0.0     # chest-led: tip along +Z, the lead rotation below carries it to the chest facing
                d = ry(d, ya)
                d = np.array([d[0], d[1] * cth - d[2] * sth, d[1] * sth + d[2] * cth])
                v = pel + ry(d, -ya)
            Q[n] = v
        P = Q
    phi = chest_lead(u)
    if phi:
        pel = P['pelvis_MainPoint']
        hl = math.copysign(min(HEAD_LEAD, max(abs(yaw_at(u)) - abs(phi), 0.0)), phi) * ss(u / LEAD_IN)
        for n in list(P):
            if n.startswith(LEG) or n in ('ik_foot_root', 'root') or n.startswith('pelvis') or 'thigh' in n:
                continue
            k_ = 0.5 if n.startswith('spine_02') else (0.8 if n.startswith('spine_04') else 1.0)
            a_ = phi * k_ + (hl if n.startswith(('head', 'neck')) else 0.0)
            P[n] = pel + ry(P[n] - pel, a_)
    if KNEE_DEG and KNEE_A - 2 < u < KNEE_B + 2:
        h = ss((u - KNEE_A + 2) / 2.0) * ss((KNEE_B + 2 - u) / 2.0)
        for sd_, ot in (('l', 'r'), ('r', 'l')):
            sw = ss((P['foot_MainPoint_' + sd_][UP] - P['foot_MainPoint_' + ot][UP] - 2.0) / 8.0)   # swing side only
            a = math.radians(KNEE_DEG) * h * sw
            if a <= 0:
                continue
            H = P['thigh_MainPoint_' + sd_]
            K1 = rot_x(P['calf_MainPoint_' + sd_], H, a)
            for n in [n for n in P if n.endswith('_' + sd_) and n.startswith(LEG)]:
                v = rot_x(P[n], H, a)
                if not n.startswith('calf_'):
                    v = rot_x(v, K1, -0.75 * a)                 # shin folds back under the raised knee
                P[n] = v
    s = s_sm[k]
    rnew = hz(sroot[0]) + FWD * dist[k]
    roots[k] = rnew
    W = {}
    for n, v in P.items():
        if s < 1.0 and n.startswith(LEG) and n != 'ik_foot_root':
            hip = P.get('thigh_MainPoint_' + n[-1])
            if hip is not None:
                # FOOT_RIGID (owner 2026-11-21 "la punta si piega"): scaling every foot point's offset from the hip squeezed the
                # foot -> ball distance (15 -> 2.6 cm) and bent the toe 5-119 deg in the rev (stock loop 25-44). Only foot_MainPoint
                # is scaled; the rest of the foot (ball, toe, ik_foot) follows it rigidly, so the foot keeps the loop's shape.
                src = 'foot_MainPoint_' + n[-1] if (FOOT_RIGID and n.startswith(FOOT_GROUP)) else n
                d = P[src] - hip; v = v - P[src] + hip + hz(d) * s + (d - hz(d))
        W[n] = v + rnew
    if LEG_IK:
        leg_ik(W)
    out[k] = (W, Bx)
PIN_U = float(os.environ.get('PIN_U', 0))
# w2: drive contacts (authored frame < PIN_U) pinned in the world: each planted run (foot below 14 cm) holds the run's mean horizontal
# position (foot, ball, ik_foot of that side move together), knees rebuilt by leg_ik. Removes the residual slide of the stride scaling.
pinned = []
if PIN_U:
    for sd_ in 'lr':
        fk = 'foot_MainPoint_' + sd_
        run = []
        for k in list(range(N_NEW)) + [None]:
            if k is not None and U[k] < PIN_U and out[k][0][fk][UP] < 14:
                run.append(k); continue
            if len(run) > 1:
                wf = lambda q: roots[q] + ry(out[q][0][fk] - roots[q], yaw_at(U[q]))     # foot in world (root yaw applied)
                tgt = sum(hz(wf(q)) for q in run) / len(run)
                for q in run:
                    W = out[q][0]; dlt = ry(tgt - hz(wf(q)), -yaw_at(U[q]))
                    for n in [n for n in W if n.endswith('_' + sd_) and n.startswith(('foot_', 'ball_'))] + ['ik_foot_' + sd_]:
                        if n in W:
                            W[n] = W[n] + dlt
                    if LEG_IK:
                        leg_ik(W)
                pinned.append((sd_, run[0], run[-1]))
            run = []
# ANKLE_MIN (turnarounds-2, owner 2026-11-21 "piedi che si incrociano", Gemini agile numbers: clamp 10-12 cm): in the turn the
# feet crossed (lateral L-R gap 0-1 cm at f8-13 in w2t180). Each frame the lateral gap (root frame X, L on +X) is kept >= ANKLE_MIN
# cm by moving the SWING foot (higher one; foot, ball, ik_foot together) outward, knees by leg_ik; planted feet and their pins stay.
# Full until ANKLE_HOLD (authored frame), smooth to 0 at ANKLE_OUT, so the tail is still the stock loop (its own passing gap is
# ~5 cm at f17-23 and f38-44; clamping there would pop at the hand-off). ANKLE_MIN=0 (default) = clip unchanged.
ANKLE_MIN = float(os.environ.get('ANKLE_MIN', 0.0))
ANKLE_HOLD, ANKLE_OUT = float(os.environ.get('ANKLE_HOLD', 14)), float(os.environ.get('ANKLE_OUT', 20))
ankle = []
if ANKLE_MIN:
    for k in range(N_NEW):
        w = 1.0 if U[k] <= ANKLE_HOLD else (0.0 if U[k] >= ANKLE_OUT else 1.0 - ss((U[k] - ANKLE_HOLD) / (ANKLE_OUT - ANKLE_HOLD)))
        W = out[k][0]
        gap = float(W['foot_MainPoint_l'][0] - W['foot_MainPoint_r'][0])
        push = (ANKLE_MIN - gap) * w
        if push <= 0.01:
            continue
        sw = 'l' if W['foot_MainPoint_l'][UP] > W['foot_MainPoint_r'][UP] else 'r'
        dx = np.array([push if sw == 'l' else -push, 0.0, 0.0])
        for n in [n for n in W if n.endswith('_' + sw) and n.startswith(('foot_', 'ball_'))] + ['ik_foot_' + sw]:
            if n in W:
                W[n] = W[n] + dx
        if LEG_IK:
            leg_ik(W)
        ankle.append((k, sw, round(push, 1)))
# ARM_IK (turnarounds-2, owner 2026-11-21 "le braccia tremano a 90/180"): the rev pumps the arms by scaling hand/elbow points about
# the loop mean (ARM, ARM_BACK, chest lead in the turns), which stretches the upper arm to 26-50 cm and the forearm to 17-37 cm (rig
# 26.65 / 22.35). Cascadeur's solve then misses the targets by up to 43 cm and the elbow zigzags frame to frame (w2t180 R elbow
# f3-6, measured on the solved scene). ARM_IK=1 rebuilds each arm with the rig lengths: shoulder kept, hand on the shoulder->hand
# target line (pulled in only when out of reach), elbow by two-bone IK bending toward the target elbow, hand/forearm helper points
# moved with their joint. The tail (stock loop, exact lengths) is unchanged. ARM_IK=0 (default) = clip unchanged.
ARM_IK = os.environ.get('ARM_IK', '0') == '1'
ARM_SOFT = float(os.environ.get('ARM_SOFT', 0.85))
LU = {sd_: float(np.linalg.norm(LP['P|lowerarm_MainPoint_' + sd_][0] - LP['P|upperarm_MainPoint_' + sd_][0])) for sd_ in 'lr'}
LF = {sd_: float(np.linalg.norm(LP['P|hand_MainPoint_' + sd_][0] - LP['P|lowerarm_MainPoint_' + sd_][0])) for sd_ in 'lr'}
arm_fix = []
if ARM_IK:
    for k in range(N_NEW):
        W = out[k][0]
        for sd_ in 'lr':
            S, E0, H0 = W['upperarm_MainPoint_' + sd_], W['lowerarm_MainPoint_' + sd_], W['hand_MainPoint_' + sd_]
            d = H0 - S; dl = float(np.linalg.norm(d)); ax = d / dl
            # soft reach (ARM_SOFT, fraction of full reach where the softening starts): near full extension the two-bone elbow
            # moves a lot for a tiny hand change (the elbow zigzag came back with a hard clamp); distances above the start are
            # compressed exponentially toward the full reach, never reaching it
            reach = LU[sd_] + LF[sd_]
            r0 = ARM_SOFT * reach
            dsoft = dl if dl <= r0 else r0 + (reach - r0) * (1.0 - math.exp(-(dl - r0) / (reach - r0)))
            H = S + ax * dsoft
            dc = float(np.linalg.norm(H - S))
            # bend side: toward the arm's pole point (lowerarm_LimbDir, smooth in time). The target elbow's own offset from the
            # shoulder-hand line is small and flips side frame to frame when the arm is near straight (zigzag at f3-12)
            b = W['lowerarm_LimbDir_' + sd_] - S; b = b - ax * float(np.dot(b, ax))
            if np.linalg.norm(b) < 1e-3:
                b = (E0 - S) - ax * float(np.dot(E0 - S, ax))
            b /= np.linalg.norm(b)
            x = (LU[sd_] ** 2 - LF[sd_] ** 2 + dc ** 2) / (2 * dc)
            hh = math.sqrt(max(LU[sd_] ** 2 - x * x, 0.0))
            E = S + ax * x + b * hh
            for n in [n for n in W if n.startswith('hand_') and n.endswith('_' + sd_)]:
                W[n] = W[n] - H0 + H
            for n in [n for n in W if n.startswith('lowerarm_') and n.endswith('_' + sd_)]:
                W[n] = W[n] - E0 + E
            arm_fix.append(float(max(np.linalg.norm(E - E0), np.linalg.norm(H - H0))))
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
        c = C[u]; a = int(math.floor(c)); w = c - a
        vals.append(round(arr[a % n] * (1 - w) + arr[(a + 1) % n] * w, 5))
    cur[k] = vals
# foot slip in world while planted (foot MainPoint y < 14 cm), per side
slip = {}
for sd_ in 'lr':
    fp = np.stack([roots[k] + ry(D['P|foot_MainPoint_' + sd_][k] - roots[k], yaw_at(U[k])) for k in range(N_NEW)])   # world
    sl = [float(np.linalg.norm(hz(fp[u] - fp[u - 1]))) for u in range(1, N_NEW) if fp[u, UP] < 14 and fp[u - 1, UP] < 14]
    slip[sd_] = round(sum(sl), 1)
info = {'TAG': TAG, 'c0': c0, 'c_end': round(c_end, 3), 'tail_loop_frame': round(c_end % NL, 3), 'N_REV': N_REV, 'N_NEW': N_NEW,
        'R0': R0, 'VL_cm_f': round(VL, 3), 'dist_end': round(dist[-1], 1), 's_min': round(min(s_sm), 3), 'slip_cm': slip,
        'steps_in_rev': round((c_end - c0) / 11.0, 2), 'cs': [round(x, 3) for x in C], 'spd': [round(x, 2) for x in spd], 'pinned': pinned,
        'ankle_push': ankle, 'arm_ik_max_cm': round(max(arm_fix), 1) if arm_fix else 0.0, 'ankle_min': ANKLE_MIN, 'yaw_ue_deg': [round(-math.degrees(yaw_at(U[k])), 3) for k in range(N_NEW)], 'src': SRC or ''}
json.dump({'info': info, 'curves': cur}, open(os.path.join(odir, 'generic_%s_curves.json' % TAG), 'w'))
print('ESITO: OK %s N_NEW %d c0 %.0f c_end %.2f (loop f %.2f) steps_in_rev %.2f' % (TAG, N_NEW, c0, c_end, c_end % NL, info['steps_in_rev']))
print('root end %.1f cm  s_min %.2f  slip L/R %s' % (dist[-1], min(s_sm), slip))
_g = D['P|foot_MainPoint_l'][:, 0] - D['P|foot_MainPoint_r'][:, 0]
_n = int(ANKLE_OUT) if ANKLE_MIN else N_REV
print('ankle gap min f0-%d %.1f cm (f%d), pushed %d frames' % (_n, float(_g[:_n].min()), int(_g[:_n].argmin()), len(ankle)))
