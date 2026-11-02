"""Jab v6 motion model (pure numpy, no Cascadeur): re-times the v5 recovery as a parent-relative chain.

Each body group g has a main point. Its position is parent_new(f) + offset_g(f), where offset_g is the v5 offset to the
parent sampled at a warped time (lead > 0: the part gets there earlier; lead < 0: later), plus authored accents
(arrival overshoot, post-move relaxation), then passed through a damped spring so a part drags behind its parent and
settles on its own instead of being carried as cargo.
Frames 0..3 are kept (contact). 3..17 follows the v5 path re-timed. 17..37 targets the guard (v5 frame 0) plus the
accents, so the arms keep living after the move. BLEND0..37 blends exactly back to the guard (frame 37 == frame 0).

Used twice: by the offline prototype on Unreal bone data, and by the Cascadeur build on rig points."""
import math
import numpy as np

# group -> parent (None = world). Parents come before children.
CHAIN = {
    "pelvis": None, "spine2": "pelvis", "chest": "spine2", "neck": "chest", "head": "neck",
    "sho_l": "chest", "elbow_l": "sho_l", "hand_l": "elbow_l",
    "sho_r": "chest", "elbow_r": "sho_r", "hand_r": "elbow_r",
}
ORDER = list(CHAIN)

# lead (frames; + earlier, - later; acts on the offset to the parent), omega (rad/frame), zeta
DEFAULT = {
    "pelvis":  (-1.2, 1.10, 0.80),
    "spine2":  (-0.5, 1.30, 0.85),
    "chest":   (-0.8, 1.20, 0.75),
    "neck":    (+0.4, 1.40, 0.85),
    "head":    (+0.8, 1.50, 0.90),
    "sho_l":   (+1.2, 1.20, 0.65),
    "elbow_l": (+2.0, 1.30, 0.55),
    "hand_l":  (+2.6, 1.40, 0.50),
    "sho_r":   (+0.2, 1.10, 0.65),
    "elbow_r": (+0.6, 1.10, 0.55),
    "hand_r":  (+0.9, 1.10, 0.50),
}

# accents: group -> list of (amp_cm, start_frame, duration_frames, direction, rebound_ratio)
# direction: "arrive" = direction the offset travels into the guard (overshoot past it), "down" = gravity
DEFAULT_ACCENTS = {
    # recovery arrival: fist overshoots past the guard and settles, elbow a frame later, off-hand later still
    "hand_l":  [(3.0, 11.0, 6.0, "arrive", 0.35), (0.7, 17.0, 10.0, "down", 0.0)],
    "elbow_l": [(2.0, 12.0, 6.0, "arrive", 0.35), (0.5, 16.5, 10.0, "down", 0.0)],
    "sho_l":   [(1.0, 12.5, 6.0, "arrive", 0.30), (0.4, 16.0, 10.0, "down", 0.0)],
    "hand_r":  [(2.4, 13.0, 6.0, "arrive", 0.35), (0.6, 18.5, 10.0, "down", 0.0)],
    "elbow_r": [(1.5, 13.5, 6.0, "arrive", 0.35), (0.5, 18.0, 10.0, "down", 0.0)],
    "sho_r":   [(0.8, 14.0, 6.0, "arrive", 0.30), (0.35, 17.5, 10.0, "down", 0.0)],
    # weight settle after the move, carried up the chain; the head counters it to keep the eyes level
    "pelvis":  [(1.2, 16.0, 11.0, "down", 0.25)],
    "chest":   [(0.5, 17.0, 11.0, "down", 0.0)],
    "head":    [(-0.9, 16.5, 11.0, "down", 0.0)],
}
REC0, REC1, TAIL1 = 3, 17, 37      # recovery start (last contact frame), guard frame, last frame
BLEND0 = 31                        # from here the result blends exactly back to the guard by TAIL1

def sample(traj, t):
    """Linear sample of an (N,3) trajectory at fractional frame t (clamped)."""
    n = len(traj)
    t = min(max(t, 0.0), n - 1.0)
    i = int(math.floor(t)); a = t - i
    if i >= n - 1:
        return traj[n - 1].copy()
    return traj[i] * (1 - a) + traj[i + 1] * a

def warp(f, lead):
    """Frame remap on [REC0, REC1], identity at both ends; the shift peaks early (a third of the way), so a leading
    part gets going right after the hold and a lagging part is held back at the start."""
    if f <= REC0 or f >= REC1:
        return float(f)
    x = (f - REC0) / (REC1 - REC0)
    return f + lead * x * (1 - x) ** 2 / (4.0 / 27.0)

def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)

def bell(tau, dur):
    """Smooth bump: 0 with zero slope at both ends, 1 in the middle."""
    if tau <= 0 or tau >= dur:
        return 0.0
    return math.sin(math.pi * tau / dur) ** 2

def accent_curve(f, amp, start, dur, rebound):
    tau = f - start
    return amp * (bell(tau, dur) - rebound * bell(tau - dur * 0.85, dur * 0.8))

def spring(target, omega, zeta, start, x0, v0=None, sub=8):
    """Damped spring follower of `target` (dict frame->vec) from frame `start` (state x0, velocity v0)."""
    out = {start: x0.copy()}
    x = x0.copy(); v = np.zeros(3) if v0 is None else v0.copy()
    frames = sorted(k for k in target if k > start)
    h = 1.0 / sub
    for f in frames:
        tgt0, tgt1 = target[f - 1] if (f - 1) in target else x, target[f]
        for s in range(sub):
            a = (s + 1) / sub
            tg = tgt0 * (1 - a) + tgt1 * a
            acc = omega * omega * (tg - x) - 2 * zeta * omega * v
            v = v + acc * h
            x = x + v * h
        out[f] = x.copy()
    return out

def offset_fn(main, g, par):
    if par is None:
        return lambda t: sample(main[g], t)
    return lambda t: sample(main[g], t) - sample(main[par], t)

def build(main, params=None, accents=None, up=np.array([0.0, 1.0, 0.0])):
    """main: dict group -> (N,3) v5 trajectory of the group's main point (N >= 38).
    up: unit up vector of the space (Cascadeur Y up; Unreal Z up).
    Returns (new trajectories dict, debug dict with offsets/warps)."""
    P = params or DEFAULT
    A = DEFAULT_ACCENTS if accents is None else accents
    N = len(next(iter(main.values())))
    guard = {g: main[g][0].copy() for g in main}
    new, dbg = {}, {}
    for g in ORDER:
        if g not in main:
            continue
        par = CHAIN[g]
        lead, om, ze = P[g]
        off = offset_fn(main, g, par)
        goff = off(0.0)
        arrive = off(REC1) - off(REC1 - 3)
        arrive = arrive / (np.linalg.norm(arrive) + 1e-9)
        tgt = {}
        for f in range(REC0, N):
            base = off(warp(f, lead)) if f <= REC1 else goff.copy()
            for amp, start, dur, kind, reb in A.get(g, []):
                d = arrive if kind == "arrive" else -up
                base = base + d * accent_curve(f, amp, start, dur, reb)
            tgt[f] = base
        x0 = off(REC0)
        sp = spring(tgt, om, ze, REC0, x0, np.zeros(3))
        traj = np.array(main[g], dtype=float).copy()
        for f in range(REC0, N):
            traj[f] = sp[f] if par is None else new[par][f] + sp[f]
        new[g] = traj
        dbg[g] = {"arrive_dir": arrive.tolist()}
    for g in new:
        for f in range(BLEND0, N):
            s = smooth((f - BLEND0) / (TAIL1 - BLEND0))
            new[g][f] = new[g][f] * (1 - s) + guard[g] * s
    return new, dbg
