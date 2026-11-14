"""`animpipe verify <move> --req=file.json`: measurable requirements on the built clip, PASS/FAIL per line.

The reviewer (or the animation thread) writes what the owner asked as numbers; the pipeline measures them on the bone dump of the
built clip (sheets/raw/<take>_legbones.json: component space, X left, Y forward, Z up, cm) and never judges by eye.

    {"video": true, "hitstop": 8, "take": "<optional, default the built anim>",
     "reqs": [
      {"id": "torso_f17", "type": "angle", "from": "pelvis", "to": "spine_05", "plane": "side", "frame": 17, "min": 10, "max": 25},
      {"id": "wrist_lag", "type": "delay", "a": "hand_l", "b": "lowerarm_l", "coord": "z", "peak": "min", "window": [17, 42], "min": 1, "max": 3},
      {"id": "reach",     "type": "amplitude", "bone": "hand_l", "coord": "y", "window": [17, 42], "min": 40}]}

angle      degrees of the segment from->to against straight up, in the `side` (Y,Z; + = leaning forward, default) or `front`
           (X,Z; + = toward the character's left) plane, at `frame`.
delay      frames the `b` bone's `peak` (max|min of `coord` x|y|z, default max) comes AFTER the `a` bone's, inside `window`
           (negative = b first). Counted in anim frames (the hitstop freeze adds none).
amplitude  max - min of `coord` of `bone` inside `window`, cm.
Frames (frame, window) are VIDEO frames when "video" is true (default: the owner's count, see animpipe/vframes.py), else anim frames.
`min` / `max` are both optional; a line with neither only reports the value. A missing bone or frame is a FAIL with the reason.
"""
import json, math, os

from animpipe import vframes as V

TYPES = ("angle", "delay", "amplitude")


def load_frames(path):
    return json.load(open(path, encoding="utf-8"))["frames"]


def _pt(F, f, bone):
    try:
        return F[str(f)][bone]
    except KeyError:
        raise KeyError(f"no {bone} at anim frame {f} in the bone dump")


def _coord(F, f, bone, coord):
    return _pt(F, f, bone)["xyz".index(coord)]


def _peak(F, bone, coord, kind, lo, hi):
    vals = [(_coord(F, f, bone, coord), f) for f in range(lo, hi + 1)]
    return (max(vals, key=lambda t: (t[0], -t[1])) if kind == "max" else min(vals, key=lambda t: (t[0], t[1])))[1]


def measure(F, r, conv):
    """-> (value, unit). conv maps a requested frame to an anim frame."""
    t = r["type"]
    if t == "angle":
        f = conv(r["frame"])
        a, b = _pt(F, f, r["from"]), _pt(F, f, r["to"])
        i = 0 if r.get("plane") == "front" else 1
        return round(math.degrees(math.atan2(b[i] - a[i], b[2] - a[2])), 2), "deg"
    lo, hi = (conv(x) for x in r["window"])
    if t == "amplitude":
        v = [_coord(F, f, r["bone"], r["coord"]) for f in range(lo, hi + 1)]
        return round(max(v) - min(v), 2), "cm"
    if t == "delay":
        k = r.get("peak", "max"); c = r.get("coord", "z")
        return _peak(F, r["b"], c, k, lo, hi) - _peak(F, r["a"], c, k, lo, hi), "frames"
    raise ValueError(f"type {t!r} not in {TYPES}")


def check(F, spec, a0=None, hit=V.HITSTOP):
    """-> [{id, ok, value, unit, line}] one per requirement."""
    video = spec.get("video", True)
    conv = (lambda v: V.video_to_anim(int(v), a0, hit)) if video else (lambda v: int(v))
    out = []
    for i, r in enumerate(spec.get("reqs", [])):
        rid = r.get("id") or f"req{i}"
        try:
            val, unit = measure(F, r, conv)
            ok = (r.get("min") is None or val >= r["min"]) and (r.get("max") is None or val <= r["max"])
            lim = f" (want {r.get('min', '-')}..{r.get('max', '-')})"
            line = f"{'PASS' if ok else 'FAIL'} {rid}: {val} {unit}{lim if 'min' in r or 'max' in r else ''}"
        except (KeyError, ValueError, IndexError, TypeError) as e:
            ok, val, unit, line = False, None, "", f"FAIL {rid}: {e}"
        out.append({"id": rid, "ok": ok, "value": val, "unit": unit, "line": line})
    return out


def run(c, spec, raw_dir):
    """Load the dump of the clip and check. Returns (rows, take, path)."""
    take = spec.get("take") or c["anim"].rsplit("/", 1)[1][3:]
    path = os.path.join(raw_dir, f"{take}_legbones.json")
    if not os.path.exists(path):
        raise FileNotFoundError(f"bone dump {path} missing: run build + check first (they write it)")
    a0, hit = V.params(c, {"hitstop": spec.get("hitstop")} if spec.get("hitstop") else {})
    return check(load_frames(path), spec, a0, hit), take, path
