"""Approved-pose locks on the real dair v4 card (no editor). v4 with leg lock 13,14 ref = itself passes; a copy with a
knee pole moved 3 cm fails with frame, part and field; --unlock skips it; diff sees the change. Run: python tests/test_locks.py
(or python -m pytest tests). Writes only to a temp folder under cards/overlord (removed afterwards)."""
import copy, json, os, shutil, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import card as C, locks as L

V4 = os.path.join(C.TOOLS, "cards", "overlord", "overlord_dair_v4.json")


def _bad_copy(dx=3.0):
    c = json.load(open(V4))
    c["move"] = "overlord_dair_v4_test"
    c["approved"] = [{"frames": [13, 14], "parts": ["leg_l", "leg_r"], "ref": "overlord/overlord_dair_v4.json", "by": "t", "date": "x", "why": "t"}]
    c["author"]["poses"]["14"]["leg_l"]["pole"][0] += dx
    d = tempfile.mkdtemp(dir=os.path.join(C.TOOLS, "cards", "overlord"))
    p = os.path.join(d, "bad.json")
    json.dump(c, open(p, "w"))
    return d, p


def test_v4_passes():
    c = C.load(V4)
    assert any(e["parts"] == ["leg_l", "leg_r"] and e["frames"] == [13, 14] for e in c["approved"])


def test_moved_pole_fails():
    d, p = _bad_copy(3.0)
    try:
        try:
            C.load(p); raise AssertionError("expected ValueError")
        except ValueError as e:
            m = str(e)
            assert "frame 14 part leg_l field leg_l.pole[0]" in m and "3.00 cm" in m, m
        C.load(p, unlock={("leg_l", 14)})                      # the guard is skipped for that pair only
        d2, p2 = _bad_copy(0.4)                                # inside the 0.5 cm tolerance
        C.load(p2); shutil.rmtree(d2)
    finally:
        shutil.rmtree(d)


def test_snapshot_self_ref():
    """ref = the card itself compares with the approve snapshot: a card edited after approve fails."""
    c = json.load(open(V4)); c["author"]["poses"]["13"]["leg_r"]["foot"][2] += 2
    d = tempfile.mkdtemp(dir=os.path.join(C.TOOLS, "cards", "overlord"))
    try:
        p = os.path.join(d, "overlord_dair_v4.json"); os.makedirs(os.path.join(d, "approved"))
        shutil.copy(os.path.join(os.path.dirname(V4), "approved", "overlord_dair_v4.json"), os.path.join(d, "approved"))
        c["approved"][0]["ref"] = os.path.relpath(p, os.path.join(C.TOOLS, "cards")).replace(os.sep, "/")
        json.dump(c, open(p, "w"))
        try:
            C.load(p); raise AssertionError("expected ValueError")
        except ValueError as e:
            assert "frame 13 part leg_r" in str(e), e
    finally:
        shutil.rmtree(d)


def test_diff_and_parts():
    a = json.load(open(V4))["author"]["poses"]["14"]; b = copy.deepcopy(a); b["leg_l"]["pole"][0] += 3
    assert L.pose_delta(a, b, "leg_l")[0] == 3.0 and L.pose_delta(a, b, "leg_r") == (0.0, 0.0) and L.pose_delta(a, b, "arm_l") == (0.0, 0.0)
    assert L.parse_unlock("leg_l@14,arm_l@3") == {("leg_l", 14), ("arm_l", 3)}
    assert L._ranges([13, 14, 3, 5, 6]) == "3,5-6,13-14"


if __name__ == "__main__":
    for n, f in sorted(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
