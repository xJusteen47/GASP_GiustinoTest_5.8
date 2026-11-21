# Stick-figure sheet of a composer clip in world (root yaw applied): one row per view (behind = looking along the run direction,
# side = from the character's final right, top), one column per frame. usage: python runstart_pivot_sheet.py <TAG> [f0 f1] [out.png]
# Writes Saved/AnimPipeline/runstart/sheet_<TAG>.png (or out.png); prints 1 line.
import json, math, os, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT = os.path.dirname(os.path.abspath(__file__)).split(os.sep + 'Tools' + os.sep)[0]
RS = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart')
TAG = sys.argv[1]; a_, b_ = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (0, 11)
out = sys.argv[4] if len(sys.argv) > 4 else os.path.join(RS, 'sheet_%s.png' % TAG)
D = np.load(os.path.join(RS, 'generic_%s_pose.npz' % TAG)); I = json.load(open(os.path.join(RS, 'generic_%s_curves.json' % TAG)))['info']
ry = lambda v, a: np.array([v[0] * math.cos(a) + v[2] * math.sin(a), v[1], -v[0] * math.sin(a) + v[2] * math.cos(a)])
BON = [('pelvis_MainPoint', 'spine_02_MainPoint'), ('spine_02_MainPoint', 'spine_04_MainPoint'), ('spine_04_MainPoint', 'neck_01_MainPoint'),
       ('neck_01_MainPoint', 'head_MainPoint')]
for s in 'lr':
    BON += [('pelvis_MainPoint', 'thigh_MainPoint_' + s), ('thigh_MainPoint_' + s, 'calf_MainPoint_' + s), ('calf_MainPoint_' + s, 'foot_MainPoint_' + s),
            ('foot_MainPoint_' + s, 'ball_MainPoint_' + s), ('spine_04_MainPoint', 'clavicle_MainPoint_' + s), ('clavicle_MainPoint_' + s, 'upperarm_MainPoint_' + s),
            ('upperarm_MainPoint_' + s, 'lowerarm_MainPoint_' + s), ('lowerarm_MainPoint_' + s, 'hand_MainPoint_' + s)]
frames = list(range(a_, b_ + 1))
views = [('behind', lambda p: (p[0] * -1, p[1])), ('side', lambda p: (p[2], p[1])), ('top', lambda p: (-p[0], p[2]))]
fig, axs = plt.subplots(3, len(frames), figsize=(1.6 * len(frames), 7.5))
for j, u in enumerate(frames):
    y = math.radians(-I['yaw_ue_deg'][u]); r = D['P|root'][u] * [1, 0, 1]
    w = lambda n: r + ry(D['P|' + n][u] - r, y)
    for i, (vn, pr) in enumerate(views):
        ax = axs[i][j]
        for a, b in BON:
            P, Q = pr(w(a)), pr(w(b))
            col = 'tab:blue' if a.endswith('_l') or b.endswith('_l') else ('tab:red' if a.endswith('_r') or b.endswith('_r') else 'k')
            ax.plot([P[0], Q[0]], [P[1], Q[1]], color=col, lw=1.6)
        hp = pr(w('head_MainPoint')); ax.plot(*hp, 'ko', ms=4)
        cx = pr(w('pelvis_MainPoint'))[0]
        if vn == 'top':
            cy = pr(w('pelvis_MainPoint'))[1]; ax.set_xlim(cx - 60, cx + 60); ax.set_ylim(cy - 60, cy + 60)
        else:
            ax.set_xlim(cx - 60, cx + 60); ax.set_ylim(-2, 180)
            ax.axhline(0, color='0.6', lw=0.5)
        ax.set_aspect('equal'); ax.set_xticks([]); ax.set_yticks([])
        if i == 0:
            ax.set_title('f%d y%.0f' % (u, I['yaw_ue_deg'][u]), fontsize=8)
        if j == 0:
            ax.set_ylabel(vn, fontsize=8)
plt.tight_layout(); plt.savefig(out, dpi=80)
print('sheet %s (blue = L, red = R)' % out)
