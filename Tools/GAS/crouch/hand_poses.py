"""Reusable hand poses for the UE mannequin skeleton (thread hit-arms-1, 2026-11-18). Pure math + one helper that reads the reference pose.

A hand pose is a set of finger deltas from the skeleton's reference pose, in degrees, authored about axes measured on the reference pose
(curl = toward the palm, spread = away from the middle finger in the palm plane), so it does not depend on the bones' local axes and works
for both hands. finger_locals() turns a pose into local rotations for every finger bone (absolute: reference local * delta); a clip that
wants the pose just overwrites its finger tracks with them (fingers have no other animation in our clips).

Per finger: (curl_01, curl_02, curl_03, spread). Negative curl = hyperextension (bent back). Thumb: (curl_01, curl_02, curl_03, out), where
out = swing away from the index finger (thumb opened). Values are deltas from the reference pose, which already has a slight curl.
"""
import math

FINGERS = ('index', 'middle', 'ring', 'pinky')

POSES = {
    # Shock hand (Smash damage poses, Sakurai's grid): fingers wide and bent back, thumb out. Reads at thumbnail size.
    # (owner's grid frame 2:21: fingers wide apart, knuckles near straight, tips a little hooked, thumb well out)
    'Splayed': {'index': (2, 14, 10, 16), 'middle': (4, 16, 12, 3), 'ring': (6, 18, 12, 10), 'pinky': (8, 18, 12, 22),
                'thumb': (-5, 5, 10, 32)},
    # Strong-hit variant: more hyperextension, pinky and index further out; held during hitstop on big hits.
    'SplayedTense': {'index': (-20, -10, -4, 18), 'middle': (-18, -10, -4, 4), 'ring': (-18, -10, -4, 12), 'pinky': (-15, -8, -2, 26),
                     'thumb': (-10, -15, -15, 38)},
    # Rest hand: graded curl, more toward the pinky (natural cascade), fingers drawn together, thumb in.
    'Relaxed': {'index': (12, 18, 10, -2), 'middle': (16, 24, 14, 0), 'ring': (22, 30, 16, 1), 'pinky': (28, 34, 18, 2),
                'thumb': (8, 10, 12, -5)},
    # Fist: full curl, thumb across the middle phalanges.
    'Fist': {'index': (82, 100, 55, -4), 'middle': (85, 100, 55, 0), 'ring': (88, 100, 55, 2), 'pinky': (90, 100, 55, 5),
             'thumb': (35, 40, 35, -10)},
    # Claw: knuckles almost straight, tips hooked, fingers apart (tension, grab, strong pain).
    'Claw': {'index': (15, 65, 50, 10), 'middle': (15, 65, 50, 2), 'ring': (18, 65, 50, 8), 'pinky': (20, 60, 45, 15),
             'thumb': (10, 30, 30, 20)},
    # Flat blade: straight and together (chops, guards, slaps).
    'Flat': {'index': (-2, -2, -2, -4), 'middle': (-2, -2, -2, 0), 'ring': (-2, -2, -2, 3), 'pinky': (-2, -2, -2, 6),
             'thumb': (0, 0, 0, -8)},
}
ORDER = ['Splayed', 'SplayedTense', 'Relaxed', 'Fist', 'Claw', 'Flat']


# Quaternions (x, y, z, w), FQuat semantics: qmul(a, b) applies b first, then a.
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return r[:3]


def qaxis(axis, deg):
    a = math.radians(deg) * 0.5
    s = math.sin(a)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(a))


def qslerp(a, b, t):
    d = sum(a[i] * b[i] for i in range(4))
    if d < 0:
        b, d = tuple(-x for x in b), -d
    if d > 0.9995:
        r = tuple(a[i] + (b[i] - a[i]) * t for i in range(4))
    else:
        th = math.acos(d)
        s = math.sin(th)
        wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
        r = tuple(a[i] * wa + b[i] * wb for i in range(4))
    n = math.sqrt(sum(x * x for x in r))
    return tuple(x / n for x in r)


def sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def norm(a):
    n = math.sqrt(dot(a, a)) or 1.0
    return tuple(x / n for x in a)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def chain(f):
    """Finger bones from the root of the finger to the tip (thumb has no metacarpal bone on the mannequin)."""
    return [f'{f}_01', f'{f}_02', f'{f}_03']


def palm_frame(cs, s):
    """Palm normal (pointing out of the palm, the side fingers curl to) and the index->pinky direction, from component-space positions
    cs[bone] = (rot, pos). The sign comes from where the finger tips sit: the reference pose has a slight curl."""
    hand = cs[f'hand_{s}'][1]
    i1, p1 = cs[f'index_01_{s}'][1], cs[f'pinky_01_{s}'][1]
    n = norm(cross(sub(i1, hand), sub(p1, hand)))
    curl = 0.0
    for f in FINGERS:
        curl += dot(sub(cs[f'{f}_03_{s}'][1], cs[f'{f}_01_{s}'][1]), n)
    if curl < 0:
        n = tuple(-x for x in n)
    return n, norm(sub(p1, i1)), curl


def finger_locals(ref_cs, ref_local, s, pose, log=None):
    """{bone: local quat} for every finger bone of side s ('l'/'r') in `pose` (a POSES key or a dict). ref_cs: bone -> (rot, pos) in
    component space on the reference pose; ref_local: bone -> local quat on the reference pose."""
    P = POSES[pose] if isinstance(pose, str) else pose
    n, across, curl = palm_frame(ref_cs, s)
    if log is not None:
        log(f'palm {s}: normal {tuple(round(x, 2) for x in n)} tip offset {curl:.2f} cm (>0 expected)')
    out = {}
    for f in FINGERS + ('thumb',):
        bones = [f'{b}_{s}' for b in chain(f)]
        if any(b not in ref_cs for b in bones):
            continue
        c1, c2, c3, sp = P[f]
        for j, b in enumerate(bones):
            pos = ref_cs[b][1]
            nxt = ref_cs[bones[j + 1]][1] if j < 2 else None
            d = norm(sub(nxt, pos)) if nxt else norm(sub(pos, ref_cs[bones[j - 1]][1]))
            # curl: rotating d about cross(d, n) by + moves it toward n (into the palm)
            w = qaxis(norm(cross(d, n)), (c1, c2, c3)[j])
            if j == 0 and sp:
                if f == 'thumb':
                    away = norm(sub(pos, ref_cs[f'index_01_{s}'][1]))
                else:
                    away = across if f in ('ring', 'pinky') else tuple(-x for x in across)
                    if f == 'middle':
                        away = across
                ax = n
                sgn = 1.0 if dot(cross(ax, d), away) > 0 else -1.0
                w = qmul(qaxis(ax, sgn * sp), w)
            pb = _parent(b, s)
            parent = ref_cs[pb if pb in ref_cs else f'hand_{s}'][0]
            delta = qmul(qinv(parent), qmul(w, parent))
            out[b] = qmul(delta, ref_local[b])
    return out


def _parent(b, s):
    base, idx = b[:-2].rsplit('_', 1)
    if idx != '01':
        return f'{base}_{int(idx) - 1:02d}_{s}'
    if base == 'thumb':
        return f'hand_{s}'
    return f'{base}_metacarpal_{s}'
