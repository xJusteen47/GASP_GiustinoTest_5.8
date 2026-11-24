"""Dense in-betweens for the card build (curve "spline") and the stop-and-go gate that checks them. Pure Python: ue_rig.py
imports it inside Unreal (no numpy there), the checks and the tests import it outside.

Why (diagnosis dropkick-k8-3, 2026-11-24): the tangent build (curve "tangents") gives every key that borders a weighted
spacing segment a FLAT tangent, so with 8-10 keys in 16 frames the body stops on every key (K7 pelvis 3 2 0 0 5 3 3 0 1
2 3 1 4 9 12 cm/frame) and the move reads stop-and-go. The spline build writes a key on EVERY frame, sampled from one
cubic per channel that goes THROUGH the card keys without stopping: the tangent at a key is the non-uniform
three-point slope (Catmull-Rom for uneven key spacing), limited Fritsch-Carlson style so the curve never overshoots
between two keys (no bone travels past a pose the owner set). Only keys declared as held or impact poses get a flat
tangent. A channel that reverses at a key has slope 0 there on that channel only: the other channels keep the body moving.
"""

STOP_CM = 3.5        # cm per anim frame: under this on every tracked point the whole body is still
JUMP_RATIO = 4.0     # the largest per-frame travel may be at most this many times the median one
# tracked points: the pelvis in component space, the others relative to the pelvis (what the eye reads as the limb moving)
GATE_POINTS = ("hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "head", "foot_l", "foot_r")


def _unwrap(vs):
    """Euler degrees: each value moved by whole turns to within 180 of the previous one (the short way round)."""
    out = [float(vs[0])]
    for v in vs[1:]:
        v = float(v)
        out.append(v + round((out[-1] - v) / 360.0) * 360.0)
    return out


def slopes(ts, vs, flat=(), mode="monotone"):
    """Per-key slope (value per frame). ts strictly ascending frames, vs values, flat = frames whose key stops.
    mode "monotone": three-point slope limited so each segment stays between its two key values (no overshoot);
    "catmull_rom": the three-point slope unlimited (rounder arcs, may overshoot a little)."""
    n = len(ts)
    if n < 2:
        return [0.0] * n
    d = [(vs[i + 1] - vs[i]) / (ts[i + 1] - ts[i]) for i in range(n - 1)]
    m = [0.0] * n
    m[0], m[-1] = d[0], d[-1]
    for i in range(1, n - 1):
        h0, h1 = ts[i] - ts[i - 1], ts[i + 1] - ts[i]
        m[i] = (d[i - 1] * h1 + d[i] * h0) / (h0 + h1)
    if mode == "monotone":
        for i in range(n):
            left = d[i - 1] if i > 0 else None
            right = d[i] if i < n - 1 else None
            if left is not None and right is not None and left * right <= 0:
                m[i] = 0.0                          # the channel turns back (or is level) at this key
                continue
            # Fritsch-Carlson: |m| <= 3 |secant| on both sides keeps the hermite inside the key values
            for s in (left, right):
                if s is not None and abs(m[i]) > 3.0 * abs(s):
                    m[i] = 3.0 * s
    flat = set(int(f) for f in flat)
    return [0.0 if int(t) in flat else mi for t, mi in zip(ts, m)]


def sample(ts, vs, frames, flat=(), mode="monotone", angle=False):
    """Values at `frames` (ints) from the cubic hermite through (ts, vs) with `slopes`. Outside the keys: the end value."""
    ts = [int(t) for t in ts]
    vs = _unwrap(vs) if angle else [float(v) for v in vs]
    if len(ts) == 1:
        return [vs[0] for _ in frames]
    m = slopes(ts, vs, flat, mode)
    out, j = [], 0
    for f in frames:
        if f <= ts[0]:
            out.append(vs[0]); continue
        if f >= ts[-1]:
            out.append(vs[-1]); continue
        while ts[j + 1] < f:
            j += 1
        h = ts[j + 1] - ts[j]
        u = (f - ts[j]) / h
        h00, h10, h01, h11 = 2 * u ** 3 - 3 * u ** 2 + 1, u ** 3 - 2 * u ** 2 + u, -2 * u ** 3 + 3 * u ** 2, u ** 3 - u ** 2
        out.append(h00 * vs[j] + h10 * h * m[j] + h01 * vs[j + 1] + h11 * h * m[j + 1])
    return out


def held_frames(hold_keys, pose_frames):
    """Frames whose step f -> f+1 may be still on purpose: the step into and out of every held key, and every step
    between two consecutive held keys (a held pose kept over several keys)."""
    hold = sorted(set(int(f) for f in hold_keys))
    keys = sorted(set(int(f) for f in pose_frames) | set(hold))
    out = set()
    for k in hold:
        out.update((k - 1, k))
    for a, b in zip(keys, keys[1:]):
        if a in hold and b in hold:
            out.update(range(a, b))
    return out


def _dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def body_steps(F, points=GATE_POINTS):
    """Per step f -> f+1: (largest travel in cm among the pelvis and the points relative to the pelvis, which point).
    F: list of {bone: (x, y, z)} per frame, component space."""
    out = []
    for f in range(len(F) - 1):
        a, b = F[f], F[f + 1]
        best = (_dist(a["pelvis"], b["pelvis"]), "pelvis")
        for p in points:
            if p in a and p in b:
                ra = [x - y for x, y in zip(a[p], a["pelvis"])]
                rb = [x - y for x, y in zip(b[p], b["pelvis"])]
                d = _dist(ra, rb)
                if d > best[0]:
                    best = (d, p)
        out.append(best)
    return out


def stop_and_go(F, window, held=(), min_cm=STOP_CM, max_ratio=JUMP_RATIO, points=GATE_POINTS):
    """Gate: in window [a, b] (steps a -> a+1 ... b-1 -> b), outside the held steps, no step with the whole body under
    `min_cm`, and the largest step <= `max_ratio` x the median step. Returns (ok, line, detail)."""
    a, b = int(window[0]), min(int(window[1]), len(F) - 1)
    steps = body_steps(F, points)
    held = set(int(f) for f in held)
    idx = [f for f in range(a, b) if f not in held]
    if len(idx) < 2:
        return True, f"n/a: {len(idx)} free steps in {a}-{b}", {}
    v = [steps[f][0] for f in idx]
    srt = sorted(v)
    med = srt[len(srt) // 2] if len(srt) % 2 else 0.5 * (srt[len(srt) // 2 - 1] + srt[len(srt) // 2])
    stops = [f for f in idx if steps[f][0] < min_cm]
    fmax = max(idx, key=lambda f: steps[f][0])
    ratio = steps[fmax][0] / med if med > 1e-6 else float("inf")
    ok = not stops and ratio <= max_ratio
    line = (f"steps {a}-{b} cm/frame " + " ".join(f"{steps[f][0]:.0f}" for f in range(a, b)) +
            f"; still (<{min_cm}) {stops or 'none'}; jump {steps[fmax][0]:.1f} cm at {fmax}->{fmax + 1} ({steps[fmax][1]})"
            f" = {ratio:.1f}x median {med:.1f} (max {max_ratio}x); held {sorted(held & set(range(a, b))) or 'none'}")
    return ok, line, {"steps": [round(s[0], 2) for s in steps], "stops": stops, "ratio": round(ratio, 2), "median": round(med, 2)}


STILL_RUN = 3        # consecutive still steps allowed anywhere in the clip (take-off, landing) outside the holds
PART_POINTS = {"arms": ("hand_l", "hand_r"), "head": ("head",), "legs": ("foot_l", "foot_r")}


def long_stills(F, held=(), min_cm=STOP_CM, run=STILL_RUN, points=GATE_POINTS):
    """Whole clip: runs of more than `run` consecutive steps with the whole body under `min_cm`, outside `held`.
    Covers take-off and landing, which the startup window of stop_and_go does not see. [(first step, last step)]."""
    steps = body_steps(F, points)
    held = set(int(f) for f in held)
    out, cur = [], []
    for f in range(len(steps) + 1):
        if f < len(steps) and f not in held and steps[f][0] < min_cm:
            cur.append(f); continue
        if len(cur) > run:
            out.append((cur[0], cur[-1]))
        cur = []
    return out


def near_equal_keys(F, keys, hold_keys=(), min_cm=STOP_CM, points=GATE_POINTS):
    """Consecutive card keys a, b (not both declared holds) whose poses differ by less than `min_cm` on every tracked
    point: between near-identical keys the body stays still even with continuous tangents. [(a, b, cm)]."""
    hold = set(int(f) for f in hold_keys)
    ks = [int(k) for k in sorted(keys) if int(k) < len(F)]
    out = []
    for a, b in zip(ks, ks[1:]):
        if a in hold and b in hold:
            continue
        d = body_steps([F[a], F[b]], points)[0][0]
        if d < min_cm:
            out.append((a, b, round(d, 2)))
    return out


def _part_speed(F, pts):
    """cm/frame of a part (largest of its points, relative to the pelvis) per step."""
    out = []
    for f in range(len(F) - 1):
        best = 0.0
        for p in pts:
            if p in F[f] and p in F[f + 1]:
                ra = [x - y for x, y in zip(F[f][p], F[f]["pelvis"])]
                rb = [x - y for x, y in zip(F[f + 1][p], F[f + 1]["pelvis"])]
                best = max(best, _dist(ra, rb))
        out.append(best)
    return out


def arrivals(F, window=None, parts=PART_POINTS, min_peak=STOP_CM):
    """{part: [frames]}: frames where the part ARRIVES (its speed falls under `min_peak` cm/frame after a step at or
    above it). Frame f = the end of step f-1, the start of step f."""
    a, b = (0, len(F) - 1) if window is None else (int(window[0]), min(int(window[1]), len(F) - 1))
    out = {}
    for name, pts in parts.items():
        s = _part_speed(F, pts)
        fr = []
        for f in range(max(a, 1), min(b, len(s))):
            if s[f] < min_peak <= s[f - 1]:
                fr.append(f)
        out[name] = fr
    return out


def same_frame_arrivals(F, window=None, tol=0, parts=PART_POINTS):
    """Warning (no overlap): frames where arms, head and legs all arrive within `tol` frames of each other. A body that
    settles everywhere on the same frame reads as a pose snap, not as follow-through. [frame]."""
    arr = arrivals(F, window, parts)
    names = list(parts)
    if any(not arr[n] for n in names):
        return []
    hits = []
    for f in arr[names[0]]:
        if all(any(abs(g - f) <= tol for g in arr[n]) for n in names[1:]):
            hits.append(f)
    return hits
