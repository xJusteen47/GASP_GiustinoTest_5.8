# Automatic gates for run start / turnaround clips (coordinator 2026-11-21, after the owner asked why crossing feet, bent toes and
# hand jitter reached him): FAIL before any video goes out on
#   ankles  lateral ankle gap (along the hips' axis, L side positive) < GAP_MIN cm in the authored window (frames < WIN); the tail is
#           the stock loop and is only checked against the loop's own minimum
#   toe     foot->ball vs ball->toe-direction angle outside the stock loop range +-TOE_TOL deg
#   bones   any limb segment length off the skeleton (loop f0) by more than 5 %
#   jitter  elbow / knee bend-offset zigzag (cm) and hand-vs-shoulder zigzag (cm) above max(JIT_DEG / JIT_MIN, JIT_K x the stock loop's)
# Input: a composer npz (generic_<TAG>_pose.npz + curves.json, root yaw applied) or, with --json, a joints json written from the
# imported UE clip ({"fps":30, "joints": {name: [[x,y,z]...]}} in the rig's names, Y up). usage:
#   python runstart_pivot_gates.py <TAG> [WIN]      |  python runstart_pivot_gates.py --json <file> [WIN]
# Writes Saved/AnimPipeline/runstart/gates_<TAG>.json; prints <= 6 lines, the last is "GATES: OK" or "GATES: FAIL <which>".
import json, math, os, sys
import numpy as np

ROOT = os.path.dirname(os.path.abspath(__file__)).split(os.sep + 'Tools' + os.sep)[0]
RS = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart')
GAP_MIN, TOE_TOL, BONE_TOL, JIT_MIN, JIT_DEG, JIT_K = 10.0, 5.0, 0.05, 8.0, 12.0, 2.0
CLR_HIPS = 21.0  # hand to mid-hips (old v27 r2 UE clip: 19 cm at f1-2, the owner's 'hands through the leg')
ROLL_MAX = 25.0   # foot lateral axis tilt, deg (UE clip)
TWIST_MIN = 35.0  # foot vs knee direction, deg (stock loop max + 10 if larger)
CLR_MIN = 14.0   # hand / forearm to thigh axis: thigh ~9 cm + hand ~4 cm radius + 1
UP = 1
SEG = [('thigh_MainPoint_%s', 'calf_MainPoint_%s'), ('calf_MainPoint_%s', 'foot_MainPoint_%s'), ('foot_MainPoint_%s', 'ball_MainPoint_%s'),
       ('upperarm_MainPoint_%s', 'lowerarm_MainPoint_%s'), ('lowerarm_MainPoint_%s', 'hand_MainPoint_%s')]
JIT = ['lowerarm_MainPoint_%s', 'hand_MainPoint_%s', 'calf_MainPoint_%s', 'foot_MainPoint_%s']

HEAD_STEP_MAX = 12   # deg/frame: head pitch relative to the neck must not snap


def ry(v, a):
    c, s_ = np.cos(a)[:, 0], np.sin(a)[:, 0]
    return np.stack([v[..., 0] * c + v[..., 2] * s_, v[..., 1], -v[..., 0] * s_ + v[..., 2] * c], axis=-1)


def measures(J):
    n = len(next(iter(J.values())))
    m = {'gap': [], 'toe': {'l': [], 'r': []}, 'len': {}, 'jit': {}}
    for u in range(n):
        lat = J['thigh_MainPoint_l'][u] - J['thigh_MainPoint_r'][u]; lat[UP] = 0; lat /= np.linalg.norm(lat)
        m['gap'].append(float(np.dot(J['foot_MainPoint_l'][u] - J['foot_MainPoint_r'][u], lat)))
        for s in 'lr':
            a = J['ball_MainPoint_' + s][u] - J['foot_MainPoint_' + s][u]
            b = J['ball_DirectionPoint_' + s][u] - J['ball_MainPoint_' + s][u] if 'ball_DirectionPoint_' + s in J else None
            if b is not None:
                m['toe'][s].append(math.degrees(math.acos(max(-1, min(1, float(np.dot(a, b) / np.linalg.norm(a) / np.linalg.norm(b)))))))
    for s in 'lr':
        for a_, b_ in SEG:
            A, B = J[a_ % s], J[b_ % s]
            m['len'][(a_ % s)[:-12] + '_' + s] = np.linalg.norm(B - A, axis=1)
        # zigzag, not effort: the frantic rev has big but smooth accelerations and the feet stop hard at every plant. Jitter is
        # a change that reverses on the very next frame (v26: elbow alternating sides f3-6). Measured on
        #   elbow / knee: the middle joint's offset from the root->end line of its chain
        #   hand relative to the shoulder (cm): the arm's own motion, without the body travel
        # z(u) = the smaller of two consecutive second differences when they point against each other
        def zig(A):
            A = A.reshape(len(A), -1)
            na = np.linalg.norm(A, axis=1); dot = np.sum(A[1:] * A[:-1], axis=1)
            return np.concatenate([np.where(dot < -0.5 * na[1:] * na[:-1], np.minimum(na[1:], na[:-1]), 0.0), [0.0]])
        for nm, a_, b_, c_ in (('elbow', 'upperarm_MainPoint_', 'lowerarm_MainPoint_', 'hand_MainPoint_'),
                               ('knee', 'thigh_MainPoint_', 'calf_MainPoint_', 'foot_MainPoint_')):
            A, B, C = J[a_ + s], J[b_ + s], J[c_ + s]
            ax = (C - A) / np.linalg.norm(C - A, axis=1)[:, None]
            off = (B - A) - ax * np.sum((B - A) * ax, axis=1)[:, None]
            # the bend offset in cm (not an angle: the swivel angle is undefined on a straight limb and flips there harmlessly)
            m['jit'][nm + '_' + s] = zig(off[2:] - 2 * off[1:-1] + off[:-2])
        P = J['hand_MainPoint_' + s] - J['upperarm_MainPoint_' + s]
        m['jit']['hand_' + s] = zig(P[2:] - 2 * P[1:-1] + P[:-2])
    # clearance (owner 2026-11-21: tucked hands passed through the leg and no gate caught it): hand and wrist-side forearm point to
    # each thigh's axis (hip -> knee), per frame, cm between the axes (subtract ~9 cm thigh + ~4 cm hand radius for the gap)
    def seg_dist(P, A, B):
        d = B - A; t = np.clip(np.sum((P - A) * d, axis=1) / np.maximum(np.sum(d * d, axis=1), 1e-6), 0, 1)
        return np.linalg.norm(P - (A + d * t[:, None]), axis=1)
    # foot twist (owner 2026-11-21 "il piede destro è rotto": foot turned out and rolled vs the shin): horizontal angle between the
    # foot (ankle -> ball) and the knee's bend direction, on frames where the knee is bent (offset > 4 cm)
    m['twist'] = {}
    for s in 'lr':
        T_, K_, F_, B_ = (J[n + s] for n in ('thigh_MainPoint_', 'calf_MainPoint_', 'foot_MainPoint_', 'ball_MainPoint_'))
        kn = K_ - 0.5 * (T_ + F_); kn[:, UP] = 0; fd = B_ - F_; fd[:, UP] = 0
        nk, nf = np.linalg.norm(kn, axis=1), np.linalg.norm(fd, axis=1)
        cosv = np.sum(kn * fd, axis=1) / np.maximum(nk * nf, 1e-6)
        ang = np.degrees(np.arccos(np.clip(cosv, -1, 1)))
        ang = np.minimum(ang, 180.0 - ang)   # the rig's knee point can sit on either side of the hip-ankle line: deviation from colinear
        m['twist'][s] = np.where((nk > 4.0) & (nf > 4.0), ang, 0.0)
    m['clear'] = {}
    for s in 'lr':
        H_ = J['hand_MainPoint_' + s]; F_ = 0.5 * (H_ + J['lowerarm_MainPoint_' + s])
        for t_ in 'lr':
            A, B = J['thigh_MainPoint_' + t_], J['calf_MainPoint_' + t_]
            m['clear']['hand_%s-thigh_%s' % (s, t_)] = np.minimum(seg_dist(H_, A, B), seg_dist(F_, A, B))
        # hand to the middle of the hips (the owner's case: fists crossed in front of the hip read as inside the leg from behind)
        m['clear']['hand_%s-hips' % s] = np.linalg.norm(H_ - 0.5 * (J['thigh_MainPoint_l'] + J['thigh_MainPoint_r']), axis=1)
    return m


def loop_ref():
    L = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runloop/loop_relaxed_dump.npz'))
    return measures({k[2:]: L[k] for k in L.files if k.startswith('P|')})


AXES = {}


def main():
    args = sys.argv[1:]
    if args[0] == '--json':
        src = args[1]; d = json.load(open(src)); J = {k: np.array(v, float) for k, v in d['joints'].items()}
        AXES.update(d.get('axes') or {})
        tag = os.path.splitext(os.path.basename(src))[0]; rest = args[2:]
    else:
        tag = args[0]; rest = args[1:]
        D = np.load(os.path.join(RS, 'generic_%s_pose.npz' % tag))
        I = json.load(open(os.path.join(RS, 'generic_%s_curves.json' % tag)))['info']
        yaw = np.radians(-np.array(I['yaw_ue_deg']))[:, None]
        r = D['P|root'].copy(); r[:, UP] = 0
        J = {k[2:]: r + ry(D[k] - r, yaw) for k in D.files if k.startswith('P|')}
    n = len(J['root']) if 'root' in J else len(J['foot_MainPoint_l'])
    win = int(rest[0]) if rest else min(n, 18)
    m, ref = measures(J), loop_ref()
    fails, rep = [], {}
    g = np.array(m['gap']); gw = g[:win]
    rep['gap_min_win'] = [round(float(gw.min()), 1), int(gw.argmin())]
    rep['gap_min_tail'] = round(float(g[win:].min()), 1) if n > win else None
    rep['gap_loop_min'] = round(float(min(ref['gap'])), 1)
    if gw.min() < GAP_MIN:
        fails.append('ankles f%d %.1f cm' % (int(gw.argmin()), gw.min()))
    if n > win and g[win:].min() < min(ref['gap']) - 1.0:
        fails.append('ankles tail %.1f < loop %.1f' % (g[win:].min(), min(ref['gap'])))
    lo, hi = min(min(ref['toe'][s]) for s in 'lr') - TOE_TOL, max(max(ref['toe'][s]) for s in 'lr') + TOE_TOL
    rep['toe_range_loop'] = [round(lo + TOE_TOL, 1), round(hi - TOE_TOL, 1)]
    for s in [s for s in 'lr' if m['toe'][s]]:          # UE joints have no toe-tip point: toe checked on the composer clip
        t = np.array(m['toe'][s]); bad = np.where((t < lo) | (t > hi))[0]
        rep['toe_' + s] = [round(float(t.min()), 1), round(float(t.max()), 1)]
        if len(bad):
            fails.append('toe %s f%d %.0f deg' % (s, int(bad[0]), t[bad[0]]))
    worst = (0.0, '')
    for k, v in m['len'].items():
        r0 = float(ref['len'][k][0]); e = float(np.abs(v / r0 - 1).max())
        if e > worst[0]:
            worst = (e, '%s f%d' % (k, int(np.abs(v / r0 - 1).argmax())))
    rep['bone_worst'] = [round(worst[0] * 100, 1), worst[1]]
    if worst[0] > BONE_TOL:
        fails.append('bone %s %.0f%%' % (worst[1], worst[0] * 100))
    for k, v in m['jit'].items():
        # knees 10 cm: the 2.3x rev's own knee drive reaches 9.2 (rev f12, R swing); elbows/hands 8 (v26 clip elbow 8.8 / hand 10.3)
        lim = max(10.0 if k.startswith('knee') else JIT_MIN, JIT_K * float(ref['jit'][k].max()))
        rep['jit_' + k] = [round(float(v.max()), 1), int(v.argmax()) + 1, round(lim, 1)]
        if v.max() > lim:
            fails.append('jitter %s f%d %.1f>%.1f' % (k, int(v.argmax()) + 1, v.max(), lim))
    tw_lim = max(TWIST_MIN, max(float(ref['twist'][s].max()) for s in 'lr') + 10.0)
    for s in 'lr':
        v = m['twist'][s][:win]
        rep['twist_' + s] = [round(float(v.max()), 1), int(v.argmax()), round(tw_lim, 1)]
        if v.max() > tw_lim:
            fails.append('foot twist %s f%d %.0f>%.0f deg' % (s, int(v.argmax()), v.max(), tw_lim))
    for k, v in m['clear'].items():
        lim = min(CLR_HIPS if k.endswith('hips') else CLR_MIN, 0.9 * float(ref['clear'][k].min()))   # never stricter than the stock loop's own closest pass
        rep['clear_' + k] = [round(float(v.min()), 1), int(v.argmin()), round(lim, 1)]
        if v.min() < lim:
            fails.append('clearance %s f%d %.1f<%.1f cm' % (k, int(v.argmin()), v.min(), lim))
    # foot roll (UE clip only: needs the foot bone axes): the foot's lateral axis tilting out of the horizontal = foot on its side
    # (owner 2026-11-21, R foot on the toe "flattened sideways"). Lateral axis = the bone axis most perpendicular to up and to the
    # foot direction at frame 0 (flat foot). Heel lift does not tilt it; a roll does.
    if AXES:
        for s in 'lr':
            A = np.array(AXES['foot_axes_' + s], float)            # frames x 3 axes x 3
            fd0 = J['ball_MainPoint_' + s][0] - J['foot_MainPoint_' + s][0]; fd0 /= np.linalg.norm(fd0)
            i = int(np.argmin([abs(A[0, k, UP]) + abs(float(np.dot(A[0, k], fd0))) for k in range(3)]))
            roll = np.degrees(np.arcsin(np.clip(np.abs(A[:win, i, UP]), 0, 1)))
            rep['roll_' + s] = [round(float(roll.max()), 1), int(roll.argmax()), ROLL_MAX]
            if roll.max() > ROLL_MAX:
                fails.append('foot roll %s f%d %.0f>%.0f deg' % (s, int(roll.argmax()), roll.max(), ROLL_MAX))
    rep['fails'] = fails
    json.dump(rep, open(os.path.join(RS, 'gates_%s.json' % tag), 'w'), indent=1)
    print('%s: gap min f0-%d %.1f cm (f%d), tail %s (loop %s); toe L %s R %s (loop %s)' % (
        tag, win - 1, gw.min(), int(gw.argmin()), rep['gap_min_tail'], rep['gap_loop_min'], rep.get('toe_l'), rep.get('toe_r'), rep['toe_range_loop']))
    print('bone worst %s%% %s; jitter max %s' % (rep['bone_worst'][0], rep['bone_worst'][1],
          max(((v[0], k) for k, v in rep.items() if k.startswith('jit_')), key=lambda x: x[0])))
    if all(k in J for k in ('spine_05', 'neck_01', 'head', 'thigh_MainPoint_l', 'thigh_MainPoint_r')):   # head/neck gate (anim-mcp v2)
        fw = []
        for u in range(n):
            lat = J['thigh_MainPoint_l'][u] - J['thigh_MainPoint_r'][u]; lat[UP] = 0
            fw.append(np.array([lat[2], 0.0, -lat[0]]) / max(np.linalg.norm(lat), 1e-9))
        # character frame sign is arbitrary: use the head-fwd vs spine direction to orient, then measure |pitch| and its frame-to-frame step
        pit = []
        for u in range(n):
            d1 = J['neck_01'][u] - J['spine_05'][u]; d2 = J['head'][u] - J['neck_01'][u]
            pit.append(np.degrees(np.arctan2(np.dot(d2, fw[u]), d2[UP]) - np.arctan2(np.dot(d1, fw[u]), d1[UP])))
        pit = np.array(pit)
        step = np.abs(np.diff(pit))
        step = np.minimum(step, 360 - step)
        rep['head_pitch_rel_neck'] = [round(float(np.abs(pit).min()), 1), round(float(np.abs(pit).max()), 1)]
        if len(step) and step.max() > HEAD_STEP_MAX:
            fails.append('head pitch step f%d %.0f>%d deg/frame' % (int(step.argmax()) + 1, step.max(), HEAD_STEP_MAX))
    print('GATES: OK' if not fails else 'GATES: FAIL ' + '; '.join(fails[:4]))


main()
