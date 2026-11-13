"""source_fidelity (animpipe/fidelity.py) on synthetic cards: the owner-pose conversion is stubbed (the real one needs
sheets/raw, desktop only). Run: python tests/test_fidelity.py (or python -m pytest tests)."""
import copy, os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import fidelity as F

REF = "cards/overlord/refs/overlord_dair_owner_poses.json"     # exists in git; its content is not read (stubbed)
OWNER = {"pelvis": {"loc": [0, 0, 5.0], "rot": [1.0, 2.0, 3.0]}, "spine": [4.0, 0, 0], "head": [0, 1.0, 0],
         "arm_l": {"hand": [10.0, 20.0, 30.0], "pole": [1.0, 2.0, 3.0]}, "arm_r": {"hand": [-10.0, 20.0, 30.0], "pole": [0, 0, 0]},
         "leg_l": {"foot": [8.0, 0, -80.0], "pole": [0, 30.0, 0], "foot_rot": [10.0, 0, 0]},
         "leg_r": {"foot": [-8.0, 0, -80.0], "pole": [0, 30.0, 0], "foot_rot": [10.0, 0, 0]}}


def _card(pose=None, src=True, origin=None, file=REF, keep=None):
    pose = copy.deepcopy(pose or OWNER)
    pose["fingers"] = {"l": 1.0}; pose["wrist_out"] = [35.0]; pose["pelvis"]["loc"] = [0, 0, -2.0]   # pipeline's own fields
    s = {"file": file, "frame": 15, "tool": "casc_to_author"} if src else None
    if s and keep:
        s["keep"] = keep
    p = {"name": "charge", "frame": 5, "source": s}
    if origin:
        p["origin"] = origin
    return {"move": "t", "poses": [p], "author": {"poses": {"5": pose}}}


def _stub():
    F._rig_ok = lambda: True
    F._expected_points = lambda data, frame: copy.deepcopy(OWNER)


def test_match_ignores_pipeline_fields():
    _stub()
    ok, line, _ = F.check(_card())
    assert ok is True and "1 owner poses match" in line, line


def test_moved_hand_fails_and_keep_declares_it():
    _stub()
    bad = copy.deepcopy(OWNER); bad["arm_l"]["hand"][0] += 1.0
    ok, line, d = F.check(_card(bad))
    assert ok is False and "arm_l.hand[0]" in line and d[5]["off"], line
    ok, _, _ = F.check(_card(bad, keep={"arm_l.hand": "owner 03/11: hand 1 cm forward"}))
    assert ok is True
    small = copy.deepcopy(OWNER); small["head"][1] += 0.5      # inside 1 deg
    assert F.check(_card(small))[0] is True


def test_source_outside_refs_and_owner_without_source():
    _stub()
    ok, line, _ = F.check(_card(file="Saved/AnimPipeline/casc/x.json"))
    assert ok is False and "refs" in line
    ok, line, _ = F.check(_card(src=False, origin="owner"))
    assert ok is False and "without source" in line
    try:
        F.guard(_card(src=False, origin="owner"), "t"); raise AssertionError("expected ValueError")
    except ValueError as e:
        assert "source_fidelity FAIL" in str(e)
    F.guard(_card(src=False), "t")                              # old-format card: never stopped at load


def test_no_rig_is_na():
    F._rig_ok = lambda: False
    ok, line, _ = F.check(_card())
    assert ok is None and "n/a" in line


if __name__ == "__main__":
    for n, f in sorted(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
