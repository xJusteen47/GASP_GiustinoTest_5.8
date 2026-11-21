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
UP = 1
SEG = [('thigh_MainPoint_%s', 'calf_MainPoint_%s'), ('calf_MainPoint_%s', 'foot_MainPoint_%s'), ('foot_MainPoint_%s', 'ball_MainPoint_%s'),
       ('upperarm_MainPoint_%s', 'lowerarm_MainPoint_%s'), ('lowerarm_MainPoint_%s', 'hand_MainPoint_%s')]
JIT = ['lowerarm_MainPoint_%s', 'hand_MainPoint_%s', 'calf_MainPoint_%s', 'foot_MainPoint_%s']


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
    return m


def loop_ref():
    L = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runloop/loop_relaxed_dump.npz'))
    return measures({k[2:]: L[k] for k in L.files if k.startswith('P|')})


def main():
    args = sys.argv[1:]
    if args[0] == '--json':
        src = args[1]; d = json.load(open(src)); J = {k: np.array(v, float) for k, v in d['joints'].items()}
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
    for s in 'lr':
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
    rep['fails'] = fails
    json.dump(rep, open(os.path.join(RS, 'gates_%s.json' % tag), 'w'), indent=1)
    print('%s: gap min f0-%d %.1f cm (f%d), tail %s (loop %s); toe L %s R %s (loop %s)' % (
        tag, win - 1, gw.min(), int(gw.argmin()), rep['gap_min_tail'], rep['gap_loop_min'], rep['toe_l'], rep['toe_r'], rep['toe_range_loop']))
    print('bone worst %s%% %s; jitter max %s' % (rep['bone_worst'][0], rep['bone_worst'][1],
          max(((v[0], k) for k, v in rep.items() if k.startswith('jit_')), key=lambda x: x[0])))
    print('GATES: OK' if not fails else 'GATES: FAIL ' + '; '.join(fails[:4]))


main()
