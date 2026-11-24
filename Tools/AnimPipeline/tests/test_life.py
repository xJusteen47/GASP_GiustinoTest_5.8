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
def _held_clip(core=0.3):
    """A held pose 5-10: every point under life_dead_cm per step (still), the pelvis drifting `core` cm per step
    (0.3 x 5 = 1.5 cm: a LIVE hold; 0.1 x 5 = 0.5 cm: a dead one)."""
    pel = _cum([5, 5, 5, 5, 5] + [core] * 5 + [5, 5, 5])
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


def test_dead_hold_fails_on_every_class():
    """Owner 2026-11-24 (pose holds): a hold is normal, never a block of concrete. Pelvis 0.5 cm over 5 frames = dead."""
    for cls in ("tilt", "jab", ""):
        r = life.run(_held_clip(core=0.1), holds=[(5, 10, "held pose")], cls=cls)
        assert not r["block"]["holds"][0] and "dead" in r["block"]["holds"][1], (cls, r["block"]["holds"])


# ------------------------------------------------------------------ slowmo review 2026-11-24: jab v12 FAIL, ftilt v3.1 PASS
# Asset dumps committed by the life-calib run (1bc74996); verdicts from the frame-by-frame review (branch
# claude/project-thread-mkedi4, docs/notes/handovers/slowmo-jab-ftilt.md).
DUMPS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                     "docs", "notes", "life-calib-run", "dumps")
JAB = os.path.join(DUMPS, "joints_AS_JabV12_Build.json")
FTILT = os.path.join(DUMPS, "joints_AS_GanonFTiltV31_Build.json")
KICK_HOLD = [(10, 15, "kick hold: leg extended after the kick")]


def _ss(r):
    return float(r["block"]["stop_sync"][1].split()[0])


def test_fix1_stop_sync_skips_live_declared_hold_only():
    ft = life.load_clip(FTILT)
    assert _ss(life.run(ft, cls="tilt")) == 0.35                       # undeclared: the kick hold counts
    r = life.run(ft, holds=KICK_HOLD, cls="tilt")
    assert r["block"]["holds"][0] and _ss(r) == 0.0 and r["verdict"] == "PASS", r["block"]
    jab = life.load_clip(JAB)
    base = life.run(jab, cls="jab")
    assert _ss(base) == 0.82 and not base["block"]["stop_sync"][0], base["block"]["stop_sync"]
    # declaring the jab's dead core does not buy an exemption: the hold fails and its minima still count
    r = life.run(jab, holds=[(5, 9, "chamber")], cls="jab")
    assert not r["block"]["holds"][0] and _ss(r) == 0.82, r["block"]


def test_fix1_flat_tail_is_one_minimum_per_part():
    """The 20 identical frames of the jab tail were ~17 minima per part (0.98 of 145); now one per part."""
    r = life.run(life.load_clip(JAB), cls="jab")
    assert " of 17 speed minima" in r["block"]["stop_sync"][1], r["block"]["stop_sync"]
    # events are reported, not blocking: 2 of 4 would read 0.5 = the max and let the jab through (rule 17)
    assert "2 of 4 stop events shared" in r["block"]["stop_sync"][1], r["block"]["stop_sync"]


def test_fix2_live_vs_dead_hold_on_real_clips():
    jab, ft = life.load_clip(JAB), life.load_clip(FTILT)
    assert life.hold_core_cm(jab, 5, 9) < 0.5                          # slowmo: pelvis 0.4 cm in 5 frames
    assert 1.0 <= life.hold_core_cm(ft, 10, 15) < 2.0                  # slowmo: pelvis 1.5 cm
    T = life.thresholds()
    assert life.judge_holds(jab, [(5, 9, "chamber")], "jab", "hand_l", T)
    assert not life.judge_holds(ft, KICK_HOLD, "tilt", "foot_l", T)


def test_fix3_no_dedupe_on_asset_dumps():
    """Identical frames in an asset ARE the freeze: the jab dead tail f18-37 must fail still_runs."""
    jab = life.load_clip(JAB)
    assert len(jab) == 38 and len(life.dedupe(jab)) == 19              # dedupe would hide 19 frames
    r = life.run(jab, cls="jab")
    assert not r["block"]["still_runs"][0] and "18-37" in r["block"]["still_runs"][1], r["block"]["still_runs"]
    assert life.run(life.dedupe(jab), cls="jab")["block"]["still_runs"][0]   # the old false negative


def test_fix3_captures_still_deduped(tmp_path=None):
    import json, tempfile
    d = tmp_path or tempfile.mkdtemp()
    p = os.path.join(str(d), "frames.jsonl")
    with open(p, "w") as f:
        for x in (0, 10, 20, 20, 30, 40):
            f.write(json.dumps({"body": {"pelvis": [x, 0, 100], "hand_l": [20, 0, 20], "actor": [0, 0, 0]}}) + "\n")
    assert life.is_capture(p) and len(life.load_clip(p)) == 5
    assert not life.is_capture(JAB)


def test_fix4_audit_table_whole_clip_at_dead_cm():
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "gstn_anim_mcp"))
    import audit_stop_table as A
    ft = A.row(FTILT, {})
    assert ft[2] == "OK" and ft[3] == "-", ft                          # was FAIL 10-14 at STOP_CM 3.5 (class D)
    ft = A.row(FTILT, {"AS_GanonFTiltV31_Build": "10-15:kick hold"})
    assert ft[2] == "OK" and ft[10].startswith("0.0 of"), ft
    jab = A.row(JAB, {})
    assert jab[2] == "FAIL" and jab[3] == "18-37" and "stop_sync" in jab[11], jab
    jab = A.row(JAB, {"AS_JabV12_Build": "5-9:chamber"})
    assert jab[2] == "FAIL" and "dead" in jab[11], jab


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
