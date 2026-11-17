"""Pure math of GSTN Motion Tools: no `unreal` import, so it runs and is tested outside the editor.

Every function works on one sampled channel (a list of floats, one value per display frame) and returns either a
new signal or the DELTA to add on top of the base. The editor side (sequencer_io.py) samples the visible motion,
calls these, and writes only the delta on an additive layer: the base keys are never touched.

Units: frames are display frames of the Level Sequence (60 fps for the pilot clips); rotations are Euler degrees.
"""
import math

# ---------------------------------------------------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------------------------------------------------

def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


def smoothstep(x):
    x = clamp(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def unwrap_degrees(values):
    """Removes +-360 jumps so a rotation channel is continuous (an Euler read can flip from 179 to -179)."""
    if not values:
        return []
    out = [float(values[0])]
    for v in values[1:]:
        prev = out[-1]
        d = float(v) - prev
        d -= 360.0 * round(d / 360.0)
        out.append(prev + d)
    return out


def sample_shifted(values, shift):
    """values(t - shift) with linear interpolation; before the start it holds the first value (the move starts at rest)."""
    n = len(values)
    out = []
    for t in range(n):
        x = t - shift
        if x <= 0.0:
            out.append(values[0])
        elif x >= n - 1:
            out.append(values[-1])
        else:
            i = int(math.floor(x))
            a = x - i
            out.append(values[i] * (1.0 - a) + values[i + 1] * a)
    return out


def zero_phase_smooth(values, softness):
    """Rounds sharp corners without adding lag: a one-pole low-pass run forward then backward.
    softness 0..1 -> time constant 0..3 frames. 0 returns the input unchanged."""
    tau = 3.0 * clamp(softness, 0.0, 1.0)
    if tau <= 1e-6 or len(values) < 3:
        return list(values)
    a = 1.0 - math.exp(-1.0 / tau)

    def one_pass(seq):
        out = []
        y = seq[0]
        for v in seq:
            y += a * (v - y)
            out.append(y)
        return out

    fwd = one_pass(list(values))
    back = one_pass(fwd[::-1])[::-1]
    return back


def spring_follow(target, omega, zeta, substeps=8):
    """Damped spring that chases `target` (one value per frame). omega in rad/frame, zeta = damping ratio
    (1 = no overshoot, lower = bounce). Starts at rest on target[0]. Semi-implicit Euler with substeps: stable for
    omega up to ~4 rad/frame."""
    if not target:
        return []
    dt = 1.0 / substeps
    x = float(target[0])
    v = 0.0
    out = [x]
    for t in range(1, len(target)):
        x0, x1 = target[t - 1], target[t]
        for s in range(substeps):
            goal = x0 + (x1 - x0) * (s + 1) / substeps
            acc = omega * omega * (goal - x) - 2.0 * zeta * omega * v
            v += acc * dt
            x += v * dt
        out.append(x)
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Overlap / follow-through
# ---------------------------------------------------------------------------------------------------------------------

MODE_SPRING = "spring"      # "Molla": each link chases its own motion through a damped spring (soft, organic)
MODE_OFFSET = "offset"      # "Sfasamento": each link replays its motion a few frames later (whip, wave down the chain)

# Fixed spring of the offset mode, only there to add the bounce; its own lag is subtracted from the shift.
_OFFSET_OMEGA = 1.2


def bounce_to_zeta(bounce):
    """Rimbalzo 0..1 -> damping ratio 1.0 (settles, no overshoot) .. 0.2 (several visible wobbles)."""
    return 1.0 - 0.8 * clamp(bounce, 0.0, 1.0)


def link_delay(delay, link_index, link_count):
    """The chain tip lags `delay` frames; links in between lag proportionally (root link least)."""
    if link_count <= 0:
        return 0.0
    return max(0.0, delay) * (link_index + 1) / link_count


def overlap_signal(values, delay, softness, bounce, mode=MODE_SPRING):
    """The follow-through version of one link's channel. `delay` is this link's own lag in frames (see link_delay)."""
    if len(values) < 2:
        return list(values)
    zeta = bounce_to_zeta(bounce)
    src = zero_phase_smooth(values, softness)
    if mode == MODE_OFFSET:
        spring_lag = 2.0 * zeta / _OFFSET_OMEGA
        shifted = sample_shifted(src, max(0.0, delay - spring_lag))
        if bounce <= 1e-6 and delay < spring_lag:
            return shifted
        return spring_follow(shifted, _OFFSET_OMEGA, zeta)
    # Spring: for a constant velocity a damped spring lags 2*zeta/omega frames, so omega is solved from the delay.
    if delay <= 1e-6:
        return src
    omega = clamp(2.0 * zeta / delay, 0.15, 4.0)
    return spring_follow(src, omega, zeta)


def overlap_delta(values, delay, softness, bounce, mode=MODE_SPRING, is_rotation=True):
    """Additive delta (follow-through minus base) for one channel."""
    base = unwrap_degrees(values) if is_rotation else [float(v) for v in values]
    follow = overlap_signal(base, delay, softness, bounce, mode)
    return [f - b for f, b in zip(follow, base)]


# ---------------------------------------------------------------------------------------------------------------------
# Key protection: the animator's key poses stay exact
# ---------------------------------------------------------------------------------------------------------------------

PROTECT_KEYS = "keys"   # weight 0 on every base key frame of the chain, plus the first and last frame
PROTECT_ENDS = "ends"   # weight 0 only on the first and last frame (the FAF pose must be arrived)
PROTECT_NONE = "none"


def key_weight_mask(n_frames, key_frames, falloff=2.0, protect=PROTECT_KEYS):
    """Per-frame weight 0..1. 0 exactly on protected frames, back to 1 over `falloff` frames (smoothstep), so the
    layer never moves a key pose and never pops next to it. key_frames are indices 0..n_frames-1 of the range."""
    if n_frames <= 0:
        return []
    pins = set()
    if protect in (PROTECT_KEYS, PROTECT_ENDS):
        pins.update((0, n_frames - 1))
    if protect == PROTECT_KEYS:
        pins.update(k for k in key_frames if 0 <= k < n_frames)
    if not pins:
        return [1.0] * n_frames
    pins = sorted(pins)
    out = []
    for t in range(n_frames):
        d = min(abs(t - p) for p in pins)
        if falloff <= 0.0:
            out.append(0.0 if d == 0 else 1.0)
        else:
            out.append(smoothstep(d / falloff))
    return out


def apply_mask(delta, mask, weight=1.0):
    return [d * m * weight for d, m in zip(delta, mask)]


# ---------------------------------------------------------------------------------------------------------------------
# Easing presets: replace the in-between shape of a key interval, keep the two key values
# ---------------------------------------------------------------------------------------------------------------------

def _ease_linear(u, k):
    return u


def _ease_smooth(u, k):
    return smoothstep(u)


def _ease_snap(u, k):
    """Scatto (curve sheet 'strike'): long build to ~30% of the path, then the rest in the last quarter, speed peak on
    arrival. k (0.5..2) moves the build/snap split."""
    split = clamp(0.75 + 0.1 * (k - 1.0), 0.5, 0.9)
    share = 0.3
    if u <= split:
        return share * (u / split) ** 2
    x = (u - split) / (1.0 - split)
    return share + (1.0 - share) * (x ** 1.5)


def _ease_anticipation(u, k):
    """Anticipo: goes back ~6% (k=1) before travelling (back-in)."""
    s = 1.0 * k
    return u * u * ((s + 1.0) * u - s)


def _ease_overshoot(u, k):
    """Overshoot: passes the target by ~6% (k=1) and comes back (back-out)."""
    s = 1.0 * k
    x = u - 1.0
    return 1.0 + (s + 1.0) * x ** 3 + s * x ** 2


def _ease_settle(u, k):
    """Assestamento: arrives fast, then a small decaying wobble around the target (elastic-out, mild)."""
    decay = 6.0 / max(0.25, k)
    cycles = 1.5
    f = lambda x: 1.0 - math.exp(-decay * x) * math.cos(2.0 * math.pi * cycles * x)
    end = f(1.0)
    return f(u) / end if abs(end) > 1e-9 else u


EASING_PRESETS = {
    "lineare": _ease_linear,
    "morbido": _ease_smooth,
    "scatto": _ease_snap,
    "anticipo": _ease_anticipation,
    "overshoot": _ease_overshoot,
    "assestamento": _ease_settle,
}

EASING_ALIASES = {"linear": "lineare", "smooth": "morbido", "snap": "scatto", "strike": "scatto",
                  "anticipation": "anticipo", "settle": "assestamento", "back": "overshoot"}


def easing_name(name):
    key = (name or "").strip().lower()
    key = EASING_ALIASES.get(key, key)
    if key not in EASING_PRESETS:
        raise ValueError("Unknown easing preset '%s'. Known: %s" % (name, ", ".join(sorted(EASING_PRESETS))))
    return key


def ease(name, u, intensity=1.0):
    return EASING_PRESETS[easing_name(name)](clamp(u, 0.0, 1.0), intensity)


def easing_delta(values, start, end, preset, intensity=1.0, is_rotation=True):
    """Delta that turns the frames start..end of `values` into preset(value[start] -> value[end]). Zero at both ends
    (the key values stay) and outside the interval."""
    base = unwrap_degrees(values) if is_rotation else [float(v) for v in values]
    n = len(base)
    if not (0 <= start < end < n):
        raise ValueError("Easing interval %d..%d is outside 0..%d" % (start, end, n - 1))
    a, b = base[start], base[end]
    out = [0.0] * n
    for t in range(start + 1, end):
        u = (t - start) / float(end - start)
        out[t] = a + (b - a) * ease(preset, u, intensity) - base[t]
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Floor guard: the layer may not push a body part through a solid plane
# ---------------------------------------------------------------------------------------------------------------------

def floor_guard_scale(z_base, z_layer, floor_z, radius):
    """Per-frame scale 0..1 for the layer delta so the point's sphere stays above floor_z.
    Only frames where the layer made it worse are reduced; if the base itself is under the floor the layer is
    removed there (a base problem is not this layer's to fix: that is the phase 2 'Solidi' correction).
    Linear estimate: z(s) = z_base + s * (z_layer - z_base)."""
    out = []
    limit = floor_z + radius
    for zb, zl in zip(z_base, z_layer):
        if zl >= limit or zl >= zb:
            out.append(1.0)
        elif zb <= limit:
            out.append(0.0)
        else:
            out.append(clamp((zb - limit) / (zb - zl), 0.0, 1.0))
    return out


def soften_scale(scale, spread=2):
    """Keeps a reduced frame's scale and eases the neighbours toward it (min over a window, then smoothed), so the
    guard does not pop."""
    n = len(scale)
    if n == 0:
        return []
    widened = [min(scale[max(0, t - spread):t + spread + 1]) for t in range(n)]
    sm = zero_phase_smooth(widened, 0.4)
    # Never above the frame's own limit: the smoothing only softens the approach, it never gives back depth.
    return [clamp(min(s, c), 0.0, 1.0) for s, c in zip(sm, scale)]
