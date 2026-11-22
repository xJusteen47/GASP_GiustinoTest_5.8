"""Generic clip clean-up filters: they remove exactly what clip_check.py fails as a pop, nothing else.

Pure Python (no bpy, no unreal, no numpy), so the same math runs in the UE editor (fix_clips_ue.py, on the asset) and in Blender
(any generator can call it before its FBX export). Quaternions are (x, y, z, w) tuples, one per frame, LOCAL bone rotation.

Designer view (one line each):
  deflick     a bone that jumps away for ONE frame and comes straight back (> FLIP_DEG off the midway of its neighbours)
              gets the midway pose. No motion needs this; it is a solver/key glitch (run start r3 fingers: 120 deg for one frame).
  ease_snaps  a one-frame step much bigger than the steps around it (> SNAP_DEG and > SNAP_REL x the neighbours) is spread over
              EASE_FRAMES frames on each side with an ease-in-out, so the same pose change happens without the pop
              (dash r3: toe bend 62 -> 0 deg in one frame).
Thresholds are clip_check.py's (60 fps values, scaled by 60/fps), so "fixed" means "passes the gate".
"""
import math

FLIP_DEG = 45.0
SNAP_DEG = 20.0
SNAP_REL = 3.0
EASE_FRAMES = 2          # at 30 fps; scaled with fps (4 at 60 fps): the step is spread over 2 * EASE_FRAMES + 1 frames


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]


def _norm(a):
    n = math.sqrt(_dot(a, a)) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n, a[3] / n)


def angle(a, b):
    """Rotation angle (deg) between two unit quaternions."""
    d = min(1.0, abs(_dot(a, b)))
    return math.degrees(2.0 * math.acos(d))


def slerp(a, b, t):
    d = _dot(a, b)
    if d < 0:
        b, d = (-b[0], -b[1], -b[2], -b[3]), -d
    if d > 0.9995:
        return _norm(tuple(a[i] + (b[i] - a[i]) * t for i in range(4)))
    th = math.acos(d)
    s = math.sin(th)
    wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
    return _norm(tuple(wa * a[i] + wb * b[i] for i in range(4)))


def continuous(qs):
    """Same rotations, signs chosen so neighbours sit in the same hemisphere (no -q / q jumps for interpolators)."""
    out = [tuple(qs[0])]
    for q in qs[1:]:
        q = tuple(q)
        out.append(q if _dot(out[-1], q) >= 0 else (-q[0], -q[1], -q[2], -q[3]))
    return out


def _medoid(qs):
    best, bi = None, 0
    for i, q in enumerate(qs):
        tot = sum(angle(q, r) for r in qs)
        if best is None or tot < best:
            best, bi = tot, i
    return qs[bi]


def _fill(out, bad):
    """Frames in `bad` get the slerp between the nearest good frames (held at the ends)."""
    n = len(out)
    good = [i for i in range(n) if i not in bad]
    if not good:
        return out
    res = list(out)
    for i in sorted(bad):
        a = max([g for g in good if g < i], default=None)
        b = min([g for g in good if g > i], default=None)
        if a is None:
            res[i] = out[b]
        elif b is None:
            res[i] = out[a]
        else:
            res[i] = slerp(out[a], out[b], (i - a) / float(b - a))
    return res


def deflick(qs, fps, flip_deg=FLIP_DEG):
    """Returns (new list, [frames fixed, counted from 1]).
    1. flicker: a bone that is almost still (most frames within flip_deg/2 of its medoid) but jumps > flip_deg away on some
       frames (run start r3 finger tips: 120 deg on and off). The jump frames get the slerp of the nearest still frames.
    2. spike: one frame > flip_deg off the midway of neighbours that agree with each other (out and straight back).
    3. peak: a spike whose neighbours do not agree (a bend that releases in one frame, dash r3 ball_l 0-40-72-0): the bigger
       step out of the peak is spread over the next/previous frames, so the peak pose stays and the release is eased."""
    qs = continuous(qs)
    n = len(qs)
    lim = flip_deg
    out, fixed = list(qs), set()
    if n >= 3:
        m = _medoid(qs)
        far = {i for i in range(n) if angle(qs[i], m) > lim}
        near = [i for i in range(n) if i not in far]
        if far and len(near) >= n / 2 and max(angle(qs[i], m) for i in near) < lim / 2:
            out = continuous(_fill(out, far)); fixed |= far
    w = max(1, int(round(EASE_FRAMES * fps / 30.0)))
    for _ in range(3):
        changed = False
        for i in range(1, n - 1):
            mid = slerp(out[i - 1], out[i + 1], 0.5)
            if angle(mid, out[i]) <= lim:
                continue
            if angle(out[i - 1], out[i + 1]) < lim / 2:
                out[i] = mid
            elif angle(out[i], out[i + 1]) >= angle(out[i - 1], out[i]):     # release after the peak: ease forward
                b = min(i + w + 1, n - 1)
                for j in range(i + 1, b):
                    t = (j - i) / float(b - i); t = t * t * (3 - 2 * t)
                    out[j] = slerp(out[i], out[b], t)
            else:                                                              # sudden arrival: ease backward
                a = max(i - w - 1, 0)
                for j in range(a + 1, i):
                    t = (j - a) / float(i - a); t = t * t * (3 - 2 * t)
                    out[j] = slerp(out[a], out[i], t)
            fixed.add(i); changed = True
        if not changed:
            break
    return out, sorted(f + 1 for f in fixed)


def ease_snaps(qs, fps, snap_deg=SNAP_DEG, snap_rel=SNAP_REL, ease_frames=EASE_FRAMES):
    """Returns (new list, [frames fixed, counted from 1]). Spreads each snap over the frames around it."""
    qs = continuous(qs)
    n = len(qs)
    k = 60.0 / fps
    w = max(1, int(round(ease_frames * fps / 30.0)))
    st = [0.0] + [angle(qs[i - 1], qs[i]) for i in range(1, n)]
    snaps = []
    for i in range(1, n):
        nb = max(st[i - 1] if i > 1 else 0.0, st[i + 1] if i + 1 < n else 0.0)
        if st[i] > snap_deg * k and st[i] > snap_rel * max(nb, 1.0):
            snaps.append(i)
    out = list(qs)
    for i in snaps:   # the step is between frame i-1 and i: re-time frames [i-w, i+w-1] from out[i-w-1] to out[i+w]
        a, b = max(i - w - 1, 0), min(i + w, n - 1)
        qa, qb = out[a], out[b]
        for j in range(a + 1, b):
            t = (j - a) / float(b - a)
            t = t * t * (3 - 2 * t)          # ease in-out
            out[j] = slerp(qa, qb, t)
    return out, [i + 1 for i in snaps]


def clean_track(qs, fps):
    """deflick then ease_snaps. Returns (new list, report dict or None when unchanged)."""
    a, f1 = deflick(qs, fps)
    b, f2 = ease_snaps(a, fps)
    if not f1 and not f2:
        return list(qs), None
    return b, {'deflick': f1, 'ease_snaps': f2}
