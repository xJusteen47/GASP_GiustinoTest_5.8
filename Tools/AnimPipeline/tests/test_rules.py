"""Synthetic tests of the README-rule checks (animpipe/checks.py run_rules). No editor, no numpy beyond the checks' own.
Run: python tests/test_rules.py   (or python -m pytest tests)"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import checks as K

BONES = ["pelvis", "spine_05", "head", "hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "middle_01_l", "middle_01_r",
         "foot_l", "foot_r", "calf_l", "calf_r"]


def pose(**kw):
    """A standing pose: knees 10 cm in front of the pelvis, feet 20 cm apart, straight wrists; kw override bones."""
    b = {n: np.zeros(3) for n in BONES}
    b.update({"pelvis": np.array([0., 0, 100]), "spine_05": np.array([0., 0, 140]), "head": np.array([0., 0, 170]),
              "calf_l": np.array([10., 10, 50]), "calf_r": np.array([-10., 10, 50]),
              "foot_l": np.array([10., 0, 0]), "foot_r": np.array([-10., 0, 0]),
              "lowerarm_l": np.array([40., 0, 120]), "hand_l": np.array([50., 0, 120]), "middle_01_l": np.array([60., 0, 120]),
              "lowerarm_r": np.array([-40., 0, 120]), "hand_r": np.array([-50., 0, 120]), "middle_01_r": np.array([-60., 0, 120])})
    b.update({k: np.array(v, float) for k, v in kw.items()})
    return b


def test_frame_0():
    idle = pose()
    charge = pose(hand_l=[50, 0, 160], hand_r=[-50, 0, 160], foot_l=[10, 0, 40], foot_r=[-10, 0, 40],
                  spine_05=[0, 0, 160], head=[0, 0, 190])
    p = {"charge": 2, "min_cm": 5, "max_frac": 0.7}
    mid = pose(hand_l=[50, 0, 140], hand_r=[-50, 0, 140], foot_l=[10, 0, 20], foot_r=[-10, 0, 20], spine_05=[0, 0, 150], head=[0, 0, 180])
    assert K.rule_frame_0([mid, mid, charge], idle, p)[0]                    # halfway: a visible break, not the charge
    assert not K.rule_frame_0([idle, mid, charge], idle, p)[0]               # still the idle
    assert not K.rule_frame_0([charge, mid, charge], idle, p)[0]             # already the charge


def test_knees_front():
    ok = pose()
    back = pose(calf_r=[-10, -15, 50])
    assert K.rule_knees_front([ok, ok], {"frames": [0, 1]})[0]
    r = K.rule_knees_front([ok, back], {"frames": [0, 1]})
    assert not r[0] and "(1, 'r'" in r[1]
    assert K.rule_knees_front([ok, back], {"frames": [1], "tol": 20})[0]


def test_feet_side_by_side():
    ok = pose()
    assert K.rule_feet_side_by_side([ok], {"frames": [0], "min_cm": 15})[0]
    crossed = pose(foot_l=[-5, 0, 0], foot_r=[5, 0, 0])
    r = K.rule_feet_side_by_side([ok, crossed], {"frames": [0, 1], "min_cm": 15})
    assert not r[0] and "crossed [1]" in r[1]
    close = pose(foot_l=[3, 0, 0], foot_r=[-3, 0, 0])
    assert "too close [0]" in K.rule_feet_side_by_side([close], {"frames": [0], "min_cm": 15})[1]


def test_villain_hand():
    straight = pose()
    bent = pose(middle_01_l=[58, 0, 130], middle_01_r=[-58, 0, 130])         # ~50 deg off the forearm line
    p = {"frames": [0], "min_deg": 25}
    assert K.rule_villain_hand([bent], p)[0]
    r = K.rule_villain_hand([straight], p)
    assert not r[0] and "0l" in r[1]
    assert not K.rule_villain_hand([pose(middle_01_l=[58, 0, 130])], p)[0]   # one side straight


if __name__ == "__main__":
    for n, f in sorted(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
