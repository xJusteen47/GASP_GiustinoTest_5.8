"""Conservation guard for automatic clip fixes (project rule 10, 2026-11-20; audit_setup_agentico.md §2.2).

An automatic fix (filters, splice, floor clamp, any future generic fixer) optimises its own metric: a pop, a seam, a hand-off.
Without a counterweight it also flattens the motion: the audit found fixed clips that passed every check and had lost their
life. So every fix step is measured BEFORE and AFTER, and a step that takes away more than MAX_LOSS (15 %) of any of these is
a FAIL and is not written:

  excursion   every bone of the chains (arm_l, arm_r, leg_l, leg_r, spine = spine..head): how far it swings over the clip, the
              largest angle between two of its poses (deg). One-frame flicks are ignored (median of 3 frames), so a deflick that
              removes a 120 deg glitch is not counted as lost amplitude; a pose that is HELD (2+ frames) is.
  peak speed  every bone of the chains: the fastest it turns (deg/s), over two frames (f-1 -> f+1) of the same flick-free track,
              so a one-frame flick does not count, but a snap or a whip that a fix eases out does.
              Judged PER BONE (a fix that kills the forearm's whip while the upper arm is untouched is caught); the report names the
              chain and the bone (excursion.arm_r:lowerarm_r).
  pelvis      how far the pelvis moves relative to the root (cm): the body's bob, sway and lunge.
  root travel the length of the root bone's path on the ground (cm): the root motion the capsule follows.

Metrics smaller than FLOOR (a chain that barely moves) are reported, not judged: a 15 % change of 2 deg is noise.
Gains are never a fail (a fix that adds motion is clip_check.py's business: pops, seams).

Pure Python (no numpy, no bpy, no unreal): the same code runs in the editor (fix_clips_ue.py) and offline
(clip_check.py --conserve <before.fbx> <after.fbx>). Quaternions are (x, y, z, w), LOCAL bone rotation, one per frame;
translations (x, y, z) in cm, Z up.

Designer view: if a fix fails here, the fix was wrong for this clip, not the clip. Fix the clip at its source (pose, keys) or
change the generic fixer; never lower MAX_LOSS for one clip.
"""
import math

MAX_LOSS = 0.15
CHAINS = {
    'arm_l': ('clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l'),
    'arm_r': ('clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r'),
    'leg_l': ('thigh_l', 'calf_l', 'foot_l', 'ball_l'),
    'leg_r': ('thigh_r', 'calf_r', 'foot_r', 'ball_r'),
    'spine': ('spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01', 'neck_02', 'head'),
}
FLOOR = {'excursion': 10.0, 'speed': 90.0, 'pelvis': 2.0, 'root': 5.0}   # deg, deg/s, cm, cm: below = reported, not judged
GUARD_BONES = tuple(b for c in CHAINS.values() for b in c) + ('pelvis', 'root')
PAIRWISE_MAX_FRAMES = 200   # longer clips are subsampled for the excursion (a held extreme lasts more than 2 frames)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]


def _norm(a):
    n = math.sqrt(_dot(a, a)) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n, a[3] / n)


def _angle(a, b):
    return math.degrees(2.0 * math.acos(min(1.0, abs(_dot(a, b)))))


def _mid(a, b):
    if _dot(a, b) < 0:
        b = (-b[0], -b[1], -b[2], -b[3])
    return _norm(tuple(a[k] + b[k] for k in range(4)))


def _median3_q(qs):
    """A frame that is not between its neighbours (of previous, this, next it is not the one closest to the other two) is replaced by
    their midway, as clip_filters.deflick does: a one-frame flick disappears from both the before and the after clip, a hold stays."""
    qs = [_norm(q) for q in qs]
    if len(qs) < 3:
        return qs
    out = [qs[0]]
    for i in range(1, len(qs) - 1):
        trio = (qs[i - 1], qs[i], qs[i + 1])
        cost = [sum(_angle(c, o) for o in trio) for c in trio]
        out.append(qs[i] if cost[1] <= min(cost) + 1e-9 else _mid(qs[i - 1], qs[i + 1]))
    out.append(qs[-1])
    return out


def _median3_v(vs):
    if len(vs) < 3:
        return list(vs)
    out = [vs[0]]
    for i in range(1, len(vs) - 1):
        out.append(tuple(sorted((vs[i - 1][k], vs[i][k], vs[i + 1][k]))[1] for k in range(3)))
    out.append(vs[-1])
    return out


def _sub(seq):
    if len(seq) <= PAIRWISE_MAX_FRAMES:
        return seq
    step = int(math.ceil(len(seq) / float(PAIRWISE_MAX_FRAMES)))
    return seq[::step] + [seq[-1]]


def excursion_deg(qs):
    s = _sub(_median3_q(qs))
    best = 0.0
    for i in range(len(s)):
        for j in range(i + 1, len(s)):
            a = _angle(s[i], s[j])
            if a > best:
                best = a
    return best


def peak_speed_dps(qs, fps):
    qs = _median3_q(qs)
    if len(qs) < 3:
        return 0.0
    return max(_angle(qs[i - 1], qs[i + 1]) for i in range(1, len(qs) - 1)) * fps / 2.0


def amplitude_cm(ts):
    s = _sub(_median3_v([tuple(t) for t in ts]))
    best = 0.0
    for i in range(len(s)):
        for j in range(i + 1, len(s)):
            d = math.sqrt(sum((s[i][k] - s[j][k]) ** 2 for k in range(3)))
            if d > best:
                best = d
    return best


def path_xy_cm(ts):
    return sum(math.hypot(ts[i][0] - ts[i - 1][0], ts[i][1] - ts[i - 1][1]) for i in range(1, len(ts)))


def measure(tracks, fps):
    """tracks: {bone: {'q': [(x,y,z,w)...], 't': [(x,y,z)...]}}. Returns {metric_name: value} for the bones present."""
    m = {}
    for chain, bones in CHAINS.items():
        for b in bones:
            if b in tracks and tracks[b].get('q'):
                m[f'excursion.{chain}:{b}'] = excursion_deg(tracks[b]['q'])
                m[f'speed.{chain}:{b}'] = peak_speed_dps(tracks[b]['q'], fps)
    if 'pelvis' in tracks and tracks['pelvis'].get('t'):
        m['pelvis'] = amplitude_cm(tracks['pelvis']['t'])
    if 'root' in tracks and tracks['root'].get('t'):
        m['root'] = path_xy_cm(tracks['root']['t'])
    return m


def compare(before, after, fps, max_loss=MAX_LOSS):
    """before/after: tracks (see measure) of the same clip. Returns {'ok', 'fails': [str], 'metrics': {name: [before, after, %]}}."""
    mb, ma = measure(before, fps), measure(after, fps)
    out = {'ok': True, 'fails': [], 'metrics': {}, 'max_loss': max_loss}
    for name, vb in mb.items():
        va = ma.get(name, 0.0)
        change = (va - vb) / vb if vb > 1e-9 else 0.0
        out['metrics'][name] = [round(vb, 2), round(va, 2), round(change * 100.0, 1)]
        if vb < FLOOR[name.split('.')[0]] or name.split('.')[0] in ('excursion', 'speed') and va > vb:
            continue
        if change < -max_loss - 1e-9:
            out['ok'] = False
            unit = {'excursion': 'deg', 'speed': 'deg/s', 'pelvis': 'cm', 'root': 'cm'}[name.split('.')[0]]
            out['fails'].append(f'{name} {vb:.1f} -> {va:.1f} {unit} ({change * 100:.0f}%, max -{max_loss * 100:.0f}%)')
    return out


def overlay(before, edits):
    """The tracks after a fix: `before` with the edited bones' tracks replaced. edits: {bone: {'q': [...], 't': [...]}} (either key)."""
    after = {b: dict(v) for b, v in before.items()}
    for b, e in edits.items():
        if b not in after:
            continue
        for k in ('q', 't'):
            if e.get(k) is not None:
                after[b][k] = e[k]
    return after
