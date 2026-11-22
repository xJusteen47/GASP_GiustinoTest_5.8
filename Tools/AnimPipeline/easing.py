"""Shared easing profiles for the dense-key motion models (v8 onward import from here; v6/v7 keep their own frozen curves).

Default for strikes, from the owner's note on Sakurai's Terry forward tilt: a long interpolation can be good or bad;
many frames of ease-in while the limb extends, then arriving in only 1-2 frames, hits much harder than a contiguous move.
So a strike's end effector (fist, foot, weapon tip) builds slowly from the anticipation key and covers most of its path in
the last 1-2 frames, with the speed peak on the frame into contact. motion_check.py measures the same thing (impact spacing).
"""
import math

def strike(n_frames, snap_frames=2, snap_share=0.7, build_power=2.0):
    """Cumulative progress (0..1) at frames 0..n_frames, anticipation key -> contact.
    The first n_frames - snap_frames frames ease in (progress ~ t^build_power) up to 1 - snap_share; the last snap_frames
    cover snap_share, the last frame the larger part, so the speed peaks on the frame into contact."""
    snap_frames = max(1, min(snap_frames, n_frames))
    build = n_frames - snap_frames
    out = []
    for f in range(n_frames + 1):
        if f <= build:
            x = f / build if build else 1.0
            out.append((1 - snap_share) * x ** build_power if build else 0.0)
        else:
            k = f - build                                            # 1..snap_frames
            weights = [i + 1 for i in range(snap_frames)]             # 1:2 for two frames, so the last one is the fastest
            done = sum(weights[:k]) / sum(weights)
            out.append((1 - snap_share) + snap_share * done)
    out[-1] = 1.0
    return out

def even(n_frames):
    """A contiguous move for comparison: the same distance every frame."""
    return [f / n_frames for f in range(n_frames + 1)]

def strong_strike(build=8, snap=2, hold=6, recovery=12, recoil_amp=0.06, recoil_cycles=1.5, limb_lead=2, snap_share=0.7):
    """Strong strikes (smash, heavy tilt), from the owner's notes on Sakurai and Ryu's forward smash: a long ease-in build,
    a 1-2 frame snap to contact, a hold on the extended pose with a small decaying recoil (back-and-forth of a few % of the
    reach, >= 2 reversals), then a recovery led by the limb, the torso following limb_lead frames later.
    Returns {"limb": [...], "torso": [...]}: extension fraction per frame (1 = full extension at contact)."""
    a = strike(build + snap, snap_frames=snap, snap_share=snap_share)
    h = []
    for k in range(1, hold + 1):
        x = k / hold
        h.append(1.0 - recoil_amp * math.exp(-3.0 * x) * math.sin(2 * math.pi * recoil_cycles * x))
    def rec(lag):
        out = []
        for k in range(1, recovery + 1):
            x = max(0.0, (k - lag) / max(1, recovery - lag))
            out.append(1.0 - smoothstep(x))
        return out
    limb = a + h + rec(0)
    torso = a + [1.0] * hold + rec(limb_lead)
    return {"limb": limb, "torso": torso}

PROFILES = {"strike": strike, "even": even, "strong_strike": strong_strike}
DEFAULT = {"strike": strike, "strong_strike": strong_strike}

def smoothstep(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3 - 2 * x)

if __name__ == "__main__":
    s = strike(8)
    print([round(v, 3) for v in s], "last two frames:", round(s[-1] - s[-3], 3))
