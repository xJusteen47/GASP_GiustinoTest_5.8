"""Arm-leg phase measure of runstart_pivot_gates.py (unified skill 2026-11-24): the skills said the check existed, the script
had none. Born as a MEASURE (owner 2A): these tests are its first calibration evidence, offline, on the approved run starts
w2 and w3 (dumps committed by the life-calib run) and on the same clips with the arms swapped (in phase = the known bad)."""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "cards", "runstart"))
import runstart_pivot_gates as G  # noqa: E402

DUMPS = os.path.join(HERE, "..", "..", "..", "docs", "notes", "life-calib-run", "dumps")


def _load(name):
    d = json.load(open(os.path.join(DUMPS, f"joints_AS_GSTN_RunStart_Generic_{name}.json")))
    return {k: np.array(v, float) for k, v in d["joints"].items()}


def _swap_arms(J):
    S = dict(J)
    S["hand_MainPoint_l"], S["hand_MainPoint_r"] = J["hand_MainPoint_r"], J["hand_MainPoint_l"]
    return S


def test_approved_run_starts_oppose():
    for n in ("w2", "w3"):
        J = _load(n)
        r, k = G.arm_leg_phase(J, range(len(J["foot_MainPoint_l"])))
        assert r is not None and r > 0.5 and k == 62, (n, r, k)


def test_arms_in_phase_would_fail():
    for n in ("w2", "w3"):
        J = _swap_arms(_load(n))
        r, _ = G.arm_leg_phase(J, range(len(J["foot_MainPoint_l"])))
        assert r is not None and r <= G.PHASE_MIN, (n, r)


def test_standing_clip_is_not_judged():
    n = 20
    J = {k: np.tile(np.array([[x, 50.0, 0.0]]), (n, 1)) for k, x in (("hand_MainPoint_l", 20), ("hand_MainPoint_r", -20),
         ("foot_MainPoint_l", 10), ("foot_MainPoint_r", -10), ("thigh_MainPoint_l", 10), ("thigh_MainPoint_r", -10))}
    assert G.arm_leg_phase(J, range(n))[0] is None


if __name__ == "__main__":
    bad = 0
    for k, fn in sorted(globals().items()):
        if k.startswith("test_") and callable(fn):
            try:
                fn(); print("ok  ", k)
            except AssertionError as e:
                bad += 1; print("FAIL", k, str(e)[:200])
    sys.exit(1 if bad else 0)
