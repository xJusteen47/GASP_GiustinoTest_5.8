"""Life metrics (animpipe/life.py): calibration proofs on real clips in git and on the K7 numbers, plus synthetic strikes.
No editor, no numpy. Run: python tests/test_life.py   (or python -m pytest tests/test_life.py)

Real data (tests/data/life, cut from captures committed under docs/notes/reviews, see each file's "source"):
  stock_*          GASP stock motion-matched mocap (land idle/fwd, skid, jump squat): MUST PASS (alive reference)
  jabfk3_tangents  Overlord jab 1 round 3, built with the old flat-tangent card build: lockstep stops, MUST FAIL stop_sync
Approved moves (ftilt v3.1, dair, jab v12, run start w3) and K7/K8/K6 settle dumps are not in git: life_calibrate.py runs
them from the PC (life_calibration.json lists clips and expected verdicts)."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import curves as CV
from animpipe import life

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "life")


def _clip(name):
    return life.load(os.path.join(DATA, name + ".json"))


def test_stock_mocap_passes():
    for n in ("stock_land_idle", "stock_land_fwd", "stock_skid", "stock_jumpsquat"):
        r = life.run(_clip(n))
        assert r["verdict"] == "PASS", (n, r["block"])
        assert float(r["block"]["stop_sync"][1].split()[0]) <= 0.26, (n, r["block"]["stop_sync"])


def test_stock_landing_would_fail_at_action_threshold():
    """Why still_runs uses life_dead_cm and not 3.5: stock landing recoveries stay at 1-3 cm/frame for many frames."""
    F = _clip("stock_land_idle")
    assert CV.long_stills(F, (), CV.STOP_CM, CV.STILL_RUN, points=life._pts(F))
    assert not CV.long_stills(F, (), life.DEF["life_dead_cm"], CV.STILL_RUN, points=life._pts(F))


def test_tangent_built_jab_fails_lockstep():
    r = life.run(_clip("jabfk3_tangents"))
    assert not r["block"]["stop_sync"][0], r["block"]["stop_sync"]


def test_duplicate_tick_is_not_a_stop():
    F = [{"pelvis": (float(x), 0.0, 100.0), "hand_l": (x + 20.0, 0.0, 120.0)} for x in (0, 10, 20, 20, 30, 40)]
    assert len(life.dedupe(F)) == 5


# ------------------------------------------------------------------ synthetic bodies
def _body(pel, hand, foot=None, head=None, arm=None):
    """Frames from per-frame x values; every point also carries a small independent wobble so only the tested part matters."""
    out = []
    for f in range(len(pel)):
        w = lambda k: 0.8 * math.sin(0.9 * f + k)
        fr = {"pelvis": (pel[f], 0.0, 100.0),
              "hand_l": (pel[f] + hand[f], 30.0, 130.0),
              "lowerarm_l": (pel[f] + (arm[f] if arm else 0.6 * hand[f]), 25.0, 125.0),
              "head": (pel[f] + (head[f] if head else w(1)), 0.0, 170.0),
              "foot_l": (pel[f] + (foot[f] if foot else w(2)), 10.0, 0.0)}
        out.append(fr)
    return out


def _cum(steps):
    x = [0.0]
    for s in steps:
        x.append(x[-1] + s)
    return x


def test_k7_pelvis_series_fails_stops():
    """K7 startup, pelvis cm/frame from the diagnosis dump: 3 2 0 0 5 3 3 0 1 2 3 1 4 9 12, limbs carried along."""
    pel = _cum([3, 2, 0, 0, 5, 3, 3, 0, 1, 2, 3, 1, 4, 9, 12])
    F = [{"pelvis": (x, 0.0, 100.0), **{p: (x + 10.0, 0.0, 120.0) for p in CV.GATE_POINTS}} for x in pel]
    r = life.run(F, window=[0, 15])
    assert not r["block"]["stops"][0], r["block"]["stops"]


def test_frozen_settle_fails_still_runs():
    """K6 settle (diagnosis: still for ~6 frames on the ground): a moving clip that freezes for 6 frames."""
    pel = _cum([6, 5, 4, 3, 2, 1.5] + [0.05] * 6 + [1, 1.5, 2])
    F = [{"pelvis": (x, 0.0, 100.0), "hand_l": (x + 20, 0, 120), "head": (x, 0, 170), "foot_l": (x, 10, 0)} for x in pel]
    r = life.run(F)
    assert not r["block"]["still_runs"][0], r["block"]["still_runs"]


# a strike: 8-frame build with an anticipation back, 2-frame snap, then recovery; pelvis leads, forearm, then hand
N = 16
PEL = [0, -1, -2, -2.5, -2, -1, 0, 3, 8, 11, 12.5, 13, 13.3, 13.5, 13.6, 13.7]
HAND = [0, -3, -6, -8, -9, -8.5, -7, -4, 0, 10, 28, 52, 50, 47, 45, 44]
ARM = [0, -2, -4, -5, -5.5, -5, -4, -2, 2, 10, 24, 30, 30, 29, 28, 27.5]


def test_good_strike_passes():
    r = life.run(_body(PEL, HAND, arm=ARM), contact=11, effector="hand_l")
    bad = {k: v for k, v in r["block"].items() if not v[0]}
    assert not bad, bad


def test_even_spacing_fails_impact():
    hand = [52.0 * f / 11 for f in range(12)] + [52, 51, 50, 49]
    r = life.run(_body(PEL, hand, arm=[0.6 * h for h in hand]), contact=11, effector="hand_l")
    assert not r["block"]["impact"][0], r["block"]["impact"]


def test_no_anticipation_fails():
    hand = [max(0.0, h) for h in HAND]
    pel = [max(0.0, p) for p in PEL]
    r = life.run(_body(pel, hand, arm=[max(0.0, a) for a in ARM]), contact=11, effector="hand_l")
    assert not r["block"]["anticipation"][0], r["block"]["anticipation"]


def test_lockstep_peaks_fail_cascade():
    """Pelvis, forearm and hand peak on the same frame: everything arrives together."""
    pel = [0, -1, -2, -2.5, -2, -1, 0, 1, 2, 3, 4, 14, 14.5, 14.8, 15, 15.1]
    r = life.run(_body(pel, HAND, arm=[0.6 * h for h in HAND]), contact=11, effector="hand_l")
    assert not r["block"]["cascade"][0], r["block"]["cascade"]


# ------------------------------------------------------------------ declared holds (owner 2026-11-24)
def _held_clip():
    pel = _cum([5, 5, 5, 5, 5, 0.1, 0.1, 0.1, 0.1, 0.1, 5, 5, 5])
    return [{"pelvis": (x, 0.0, 100.0), "hand_l": (x + 40, 0, 120), "head": (x, 0, 170), "foot_l": (x, 10, 0)} for x in pel]


def test_declared_hold_with_reason_exempts():
    r = life.run(_held_clip(), holds=[(5, 10, "Ryu-style held smash pose")], cls="tilt")
    assert r["block"]["still_runs"][0] and r["block"]["holds"][0], r["block"]


def test_hold_without_reason_fails():
    r = life.run(_held_clip(), holds=[(5, 10, "")])
    assert not r["block"]["holds"][0]


def test_undeclared_hold_fails():
    assert not life.run(_held_clip())["block"]["still_runs"][0]


def test_too_many_or_too_long_holds_fail():
    F = _held_clip()
    assert not life.run(F, holds=[(1, 2, "a"), (3, 4, "b"), (5, 10, "c")])["block"]["holds"][0]
    assert not life.run(F, holds=[(0, 12, "long")], cls="tilt")["block"]["holds"][0]


def test_frozen_hold_on_strong_move_fails():
    r = life.run(_held_clip(), holds=[(5, 10, "smash hold")], cls="smash", effector="hand_l")
    assert not r["block"]["holds"][0], r["block"]["holds"]


if __name__ == "__main__":
    fails = 0
    for k, fn in sorted(globals().items()):
        if k.startswith("test_") and callable(fn):
            try:
                fn()
                print("ok  ", k)
            except AssertionError as e:
                fails += 1
                print("FAIL", k, str(e)[:200])
    sys.exit(1 if fails else 0)
