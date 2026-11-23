"""Synthetic tests of the spline build and the stop-and-go gate (animpipe/curves.py). No editor, no numpy.
Run: python tests/test_curves.py   (or python -m pytest tests)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import curves as CV

# K7 startup key frames (diagnosis dropkick-k8-3) and a pelvis path that keeps going forward and up through them
KEYS = [0, 1, 2, 4, 5, 7, 11, 14, 15]
PATH = [0.0, 4.0, 10.0, 18.0, 22.0, 31.0, 52.0, 70.0, 82.0]


def _frames(vals_per_axis):
    """[{bone: xyz}] with the pelvis on the given x values and the limbs carried with it."""
    out = []
    for x in vals_per_axis:
        b = {"pelvis": (x, 0.0, 100.0)}
        for p in CV.GATE_POINTS:
            b[p] = (x + 10.0, 0.0, 120.0)
        out.append(b)
    return out


def test_passes_through_keys():
    v = CV.sample(KEYS, PATH, KEYS)
    assert all(abs(a - b) < 1e-9 for a, b in zip(v, PATH))


def test_no_stop_on_interior_keys():
    span = list(range(16))
    v = CV.sample(KEYS, PATH, span)
    steps = [v[f + 1] - v[f] for f in range(15)]
    assert min(steps) > 1.0, steps                     # moves on every frame, keys included


def test_old_flat_build_stops_and_gate_fails():
    """The old build = flat tangent on every key: the gate must FAIL it and PASS the spline through the same keys."""
    span = list(range(16))
    old = CV.sample(KEYS, PATH, span, flat=KEYS)
    new = CV.sample(KEYS, PATH, span)
    ok_old, line_old, _ = CV.stop_and_go(_frames(old), [0, 15])
    ok_new, line_new, _ = CV.stop_and_go(_frames(new), [0, 15])
    assert not ok_old, line_old
    assert ok_new, line_new


def test_hold_key_is_flat_and_exempt():
    span = list(range(16))
    v = CV.sample(KEYS, PATH, span, flat=[7])
    assert abs(CV.slopes(KEYS, PATH, flat=[7])[KEYS.index(7)]) == 0.0
    held = CV.held_frames([7], KEYS)
    assert {6, 7} <= held
    ok, line, d = CV.stop_and_go(_frames(v), [0, 15], held)
    assert ok, line


def test_k7_pelvis_series_fails():
    """Pelvis cm/frame measured on K7 (3 2 0 0 5 3 3 0 1 2 3 1 4 9 12): still frames -> FAIL."""
    sp = [3, 2, 0, 0, 5, 3, 3, 0, 1, 2, 3, 1, 4, 9, 12]
    x = [0.0]
    for s in sp:
        x.append(x[-1] + s)
    ok, line, d = CV.stop_and_go(_frames(x), [0, 15])
    assert not ok and 2 in d["stops"] and 7 in d["stops"], line


def test_jump_ratio():
    x = [0.0]
    for s in [5, 5, 5, 5, 5, 40, 5, 5, 5]:
        x.append(x[-1] + s)
    ok, line, d = CV.stop_and_go(_frames(x), [0, 9])
    assert not ok and d["ratio"] == 8.0, line


def test_monotone_never_overshoots():
    ts, vs = [0, 1, 9, 10], [0.0, 10.0, 11.0, 30.0]        # uneven spacing: catmull-rom overshoots here
    v = CV.sample(ts, vs, list(range(11)))
    assert all(10.0 - 1e-9 <= y <= 11.0 + 1e-9 for y in v[1:10]), v
    cr = CV.sample(ts, vs, list(range(11)), mode="catmull_rom")
    assert any(y < 10.0 - 1e-6 or y > 11.0 + 1e-6 for y in cr[1:10])


def test_reversal_is_zero_slope_on_that_channel_only():
    m = CV.slopes([0, 5, 10], [0.0, 20.0, 5.0])
    assert m[1] == 0.0 and m[0] != 0.0


def test_euler_short_way():
    v = CV.sample([0, 4], [170.0, -170.0], [0, 2, 4], angle=True)
    assert abs(v[1] - 180.0) < 1e-6 and abs(v[2] - 190.0) < 1e-6     # through 180, not through 0


def test_long_stills_cover_landing():
    x = [0.0]
    for s in [6, 6, 6, 6, 6, 6, 1, 1, 0, 0, 1]:          # moves, then lands and freezes 5 steps
        x.append(x[-1] + s)
    assert CV.long_stills(_frames(x)) == [(6, 10)]
    assert CV.long_stills(_frames(x), held=range(6, 11)) == []
    assert CV.long_stills(_frames(x[:10])) == []         # 3 still steps are allowed


def test_near_equal_keys():
    x = [0.0, 6.0, 12.0, 13.0, 14.0, 20.0]
    F = _frames(x)
    assert CV.near_equal_keys(F, [0, 2, 4, 5]) == [(2, 4, 2.0)]
    assert CV.near_equal_keys(F, [0, 2, 4, 5], hold_keys=[2, 4]) == []


def _parts(arm, head, leg):
    F = []
    for i in range(len(arm)):
        b = {"pelvis": (0.0, 0.0, 100.0), "hand_l": (arm[i], 0.0, 120.0), "hand_r": (arm[i], 0.0, 120.0),
             "head": (head[i], 0.0, 170.0), "foot_l": (leg[i], 0.0, 0.0), "foot_r": (leg[i], 0.0, 0.0)}
        F.append(b)
    return F


def test_same_frame_arrivals():
    ramp = [0, 8, 16, 20, 21, 21, 21, 21]                 # arrives at frame 3 (speed drops after 8 cm steps)
    late = [0, 0, 8, 16, 20, 21, 21, 21]                  # same motion one frame later: overlap
    assert CV.same_frame_arrivals(_parts(ramp, ramp, ramp)) == [3]
    assert CV.same_frame_arrivals(_parts(ramp, late, ramp)) == []


def test_checks_integration():
    from animpipe import checks as K
    span = list(range(16))
    F = _frames(CV.sample(KEYS, PATH, span))
    c = {"poses": [{"frame": k} for k in KEYS], "curve": "spline"}
    ok, line = K._stop_and_go(c, F, {"active": [14, 15]})
    assert ok, line
    near, arr = K._key_warnings(c, F, {"active": [14, 15]})
    assert near.startswith("no ") and arr.startswith("no "), (near, arr)
    F_old = _frames(CV.sample(KEYS, PATH, span, flat=KEYS))
    assert not K._stop_and_go(c, F_old, {"active": [14, 15]})[0]


if __name__ == "__main__":
    for n, f in list(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
