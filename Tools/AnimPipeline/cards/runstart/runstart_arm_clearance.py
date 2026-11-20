# Run start v4r3 (runstart-v3-pose-2): arm vs body clearance on the CURRENT Cascadeur scene, from rig points (no mesh in the API).
# Volumes (cm, UEFN mannequin, approximate): chest capsule spine_02 -> neck_01 r 12 (torso depth), belly/pelvis capsule pelvis -> spine_02
# r 13, head sphere r 11, thigh capsules thigh -> calf r 8. Arm: forearm capsule elbow -> wrist r 4, fist capsule wrist -> wrist + 9 cm
# along the forearm, r 5. Upper arm not judged (it is attached to the chest at the shoulder). Clearance = distance - radii (< 0 = inside).
# Owner feedback v4r2: arms interpenetrate the body while pumping. Run with run_python: exec(open(path).read()); result = CLEAR
import math

CL_F0, CL_F1 = globals().get('CL_RANGE', (12, 48))


def _sub(a, b): return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _add(a, b): return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
def _mul(a, k): return [a[0] * k, a[1] * k, a[2] * k]


def seg_seg(p1, q1, p2, q2):
    d1, d2, r = _sub(q1, p1), _sub(q2, p2), _sub(p1, p2)
    a, e, f = _dot(d1, d1), _dot(d2, d2), _dot(d2, r)
    c, b = _dot(d1, r), _dot(d1, d2)
    den = a * e - b * b
    s = min(1.0, max(0.0, (b * f - c * e) / den)) if den > 1e-9 else 0.0
    t = (b * s + f) / e if e > 1e-9 else 0.0
    if t < 0: t, s = 0.0, min(1.0, max(0.0, -c / a))
    elif t > 1: t, s = 1.0, min(1.0, max(0.0, (b - c) / a))
    c1, c2 = _add(p1, _mul(d1, s)), _add(p2, _mul(d2, t))
    return math.sqrt(_dot(_sub(c1, c2), _sub(c1, c2)))


_mv = scene.model_viewer(); _bv = _mv.behaviour_viewer(); _dv = _mv.data_viewer()
_NAMES = ['pelvis_MainPoint', 'spine_02_MainPoint', 'neck_01_MainPoint', 'head_MainPoint', 'thigh_MainPoint_l', 'thigh_MainPoint_r',
          'calf_MainPoint_l', 'calf_MainPoint_r'] + [p + s for s in ('_l', '_r') for p in ('lowerarm_MainPoint', 'hand_MainPoint')]
_ids = {}
for _n in _NAMES:
    _o = _mv.get_objects(_n)[0]
    _ids[_n] = _bv.get_behaviour_data(_bv.get_behaviour_by_name(_o, 'Transform'), 'global_position')
CLEAR = {'min': (99.0, '', 0), 'per_frame': {}}
for _f in range(CL_F0, CL_F1 + 1):
    P = {n: [float(c) for c in _dv.get_data_value(i, _f)] for n, i in _ids.items()}
    body = [('chest', P['spine_02_MainPoint'], P['neck_01_MainPoint'], 12.0), ('belly', P['pelvis_MainPoint'], P['spine_02_MainPoint'], 13.0),
            ('head', P['head_MainPoint'], P['head_MainPoint'], 11.0),
            ('thigh_l', P['thigh_MainPoint_l'], P['calf_MainPoint_l'], 8.0), ('thigh_r', P['thigh_MainPoint_r'], P['calf_MainPoint_r'], 8.0)]
    worst = (99.0, '')
    for s in 'lr':
        E, W = P['lowerarm_MainPoint_' + s], P['hand_MainPoint_' + s]
        u = _sub(W, E); L = math.sqrt(_dot(u, u)) or 1.0
        F = _add(W, _mul(u, 9.0 / L))
        for part, a, b, rr in body:
            for lab, p, q, ra in (('forearm', E, W, 4.0), ('fist', W, F, 5.0)):
                c = seg_seg(p, q, a, b) - rr - ra
                if c < worst[0]: worst = (c, f'{lab}_{s}-{part}')
    CLEAR['per_frame'][_f] = (round(worst[0], 1), worst[1])
    if worst[0] < CLEAR['min'][0]: CLEAR['min'] = (round(worst[0], 1), worst[1], _f)
CLEAR['neg_frames'] = [f for f, v in CLEAR['per_frame'].items() if v[0] < 0]
