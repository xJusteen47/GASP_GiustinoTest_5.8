# Turnaround composer check (runstart-turnaround-desktop): world lean toward the run direction, pelvis height, facing, knees, per frame
# of generic_<TAG>_pose.npz (root yaw applied). usage: python runstart_turn_check.py <TAG> [frames...]; prints <= 10 lines.
import json, math, os, sys
import numpy as np
ROOT = os.path.dirname(os.path.abspath(__file__)).split(os.sep + 'Tools' + os.sep)[0]
TAG = sys.argv[1]
D = np.load(os.path.join(ROOT, 'Saved/AnimPipeline/runstart/generic_%s_pose.npz' % TAG))
I = json.load(open(os.path.join(ROOT, 'Saved/AnimPipeline/runstart/generic_%s_curves.json' % TAG)))['info']
FR = [int(x) for x in sys.argv[2:]] or [0, 2, 4, 6, 9, 12, 16, 20, 26]
ry = lambda v, a: np.array([v[0] * math.cos(a) + v[2] * math.sin(a), v[1], -v[0] * math.sin(a) + v[2] * math.cos(a)])
rows, back = [], 0
for u in range(D['P|root'].shape[0]):
    a = math.radians(-I['yaw_ue_deg'][u]); r = D['P|root'][u]
    w = lambda n: r + ry(D['P|' + n][u] - r, a)
    for s in 'lr':                                          # knee ahead of the hip-foot line along the body facing (root frame)
        H, K, Fo = D['P|thigh_MainPoint_' + s][u], D['P|calf_MainPoint_' + s][u], D['P|foot_MainPoint_' + s][u]
        back += float(np.dot(K - (H + Fo) / 2, [0, 0, 1])) < 0
    if u in FR:
        ln = w('neck_01_MainPoint') - w('pelvis_MainPoint')
        tow = math.degrees(math.atan2(ln[2], ln[1]))       # tilt toward +Z (run direction) in world
        side = math.degrees(math.atan2(ln[0], ln[1]))
        rows.append('f%-3d yaw %4.0f  lean toward run %5.1f side %5.1f  pelvis %5.1f' % (u, I['yaw_ue_deg'][u], tow, side, w('pelvis_MainPoint')[1]))
print('\n'.join(rows[:9]))
print('%s: backward knee frames %d, slip world %s, pinned %s' % (TAG, back, I['slip_cm'], I['pinned']))
