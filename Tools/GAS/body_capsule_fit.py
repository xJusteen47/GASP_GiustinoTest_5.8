# AERIAL-CAPSULES (owner 2026-11-26, choice B): the movement capsule of an aerial fitted to its body, the bottom on the lowest foot. The pure part
# of setup_aerial_body_capsules.py (no editor): per-frame body samples in, Body Capsule runs out. The same rules as setup_dair_body_capsule.py
# (2026-11-15, measured on the dair v9 captures), written once here so every aerial gets them:
#   - per frame the capsule spans the lowest foot minus the sole to the head plus the skull, never shorter than 2 x the smallest half height;
#   - the bottom never goes under the mesh root (the standing capsule that comes back after the timeline has its bottom there: a lower bottom
#     on the ground drops the body by the difference on the last frame, -17 cm pop in the dair's first capture);
#   - once the feet reach their lowest frame the bottom stays there (the recovery tucks the legs: a rising bottom on the ground drops the body);
#   - frames are cut into runs where bottom, top and the torso's x, y stay within the tolerance; each run is one track (the run's envelope);
#   - no lateral offset by default (the dair's -10 cm sideways slid the attacker ~50 cm along the victim on the landing hit).
# Test: python Tools/GAS/tests/test_body_capsule_fit.py


def spans(samples, sole=8.0, skull=15.0, min_hh=30.0):
    """samples: per frame {'torso': (x, y, z), 'head_z': z, 'foot_z': (left z, right z)} in the capsule frame, cm, mesh root = 0.
    Returns per frame (torso x, torso y, bottom, top)."""
    if not samples:
        return []
    lows = [max(min(s['foot_z']) - sole, 0.0) for s in samples]
    deepest = lows.index(min(lows))
    out = []
    for i, s in enumerate(samples):
        bottom = lows[i] if i < deepest else lows[deepest]
        top = max(s['head_z'] + skull, bottom + 2.0 * min_hh)
        out.append((s['torso'][0], s['torso'][1], bottom, top))
    return out


def runs(frame_spans, tol=8.0, lateral=False):
    """Cuts per-frame spans into runs that stay within tol on every value. Returns (start, end, x, y, bottom, top), end exclusive:
    x, y the run's mean (y 0 unless lateral), bottom the run's lowest, top its highest."""
    out, start = [], 0
    n = len(frame_spans)
    for i in range(1, n + 1):
        if i < n:
            seg = frame_spans[start:i + 1]
            if all(max(v[k] for v in seg) - min(v[k] for v in seg) <= tol for k in range(4)):
                continue
        seg = frame_spans[start:i]
        x = sum(v[0] for v in seg) / len(seg)
        y = sum(v[1] for v in seg) / len(seg) if lateral else 0.0
        out.append((start, i, x, y, min(v[2] for v in seg), max(v[3] for v in seg)))
        start = i
    return out


def fit(samples, sole=8.0, skull=15.0, min_hh=30.0, tol=8.0, lateral=False):
    """The tracks of a clip: (start, end, half height, centre x, centre y, centre z) per run, Body anchor, capsule frame"""
    return [(a, b, (top - bottom) * 0.5, x, y, (top + bottom) * 0.5)
            for a, b, x, y, bottom, top in runs(spans(samples, sole, skull, min_hh), tol, lateral)]
