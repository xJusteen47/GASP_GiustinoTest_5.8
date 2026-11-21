# Diagnostic: per frame world positions of a composer clip (root yaw applied): yaw, pelvis, ankles, knees, lateral ankle gap,
# segment length errors. usage: python runstart_pivot_dump.py <TAG> [f0 f1]; prints one line per frame (keep the range short).
import json, math, os, sys
import numpy as np
ROOT = os.path.dirname(os.path.abspath(__file__)).split(os.sep + 'Tools' + os.sep)[0]
RS = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart')
TAG = sys.argv[1]; a_, b_ = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (0, 12)
D = np.load(os.path.join(RS, 'generic_%s_pose.npz' % TAG)); I = json.load(open(os.path.join(RS, 'generic_%s_curves.json' % TAG)))['info']
ry = lambda v, a: np.array([v[0] * math.cos(a) + v[2] * math.sin(a), v[1], -v[0] * math.sin(a) + v[2] * math.cos(a)])
for u in range(a_, b_ + 1):
    y = math.radians(-I['yaw_ue_deg'][u]); r = D['P|root'][u] * [1, 0, 1]
    w = lambda n: r + ry(D['P|' + n][u] - r, y)
    lat = w('thigh_MainPoint_l') - w('thigh_MainPoint_r'); lat[1] = 0; lat /= np.linalg.norm(lat)
    gap = float(np.dot(w('foot_MainPoint_l') - w('foot_MainPoint_r'), lat))
    f = lambda v: '(%4.0f %3.0f %4.0f)' % tuple(v)
    print('f%-2d yaw %4.0f pel %s L %s kL %s R %s kR %s gap %5.1f' % (u, I['yaw_ue_deg'][u], f(w('pelvis_MainPoint')), f(w('foot_MainPoint_l')),
          f(w('calf_MainPoint_l')), f(w('foot_MainPoint_r')), f(w('calf_MainPoint_r')), gap))
