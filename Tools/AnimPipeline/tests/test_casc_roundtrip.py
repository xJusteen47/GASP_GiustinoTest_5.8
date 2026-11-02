"""Offline tests of the card <-> Cascadeur merge (animpipe/cascadeur.py), no editor and no Cascadeur.

    python -m pytest Tools/AnimPipeline/tests/test_casc_roundtrip.py -q   (or run the file)
The "Cascadeur bones" are made with author.Poser from the dair v4 card, edited the way Cascadeur would (world-space).
"""
import copy, json, os, sys
import numpy as np

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)
from animpipe import card as C, cascadeur as K
from animpipe.author import Poser, PARENT, rv

CARD = os.path.join(TOOLS, "cards", "overlord", "overlord_dair_v4.json")
POSER = Poser()


def _load():
    c = C.load(CARD); c.pop("approved", None)   # the live card may carry locks; each test sets its own
    return c


def _keys(c):
    return {int(f): POSER.solve(p) for f, p in c["author"]["poses"].items()}


def _rotate_subtree(P, R, root, M, pivot=None):
    """Rotate a bone and all its descendants by world rotation M about the bone's pivot (what an FK edit does)."""
    pivot = P[root].copy() if pivot is None else pivot
    kids = {root}
    changed = True
    while changed:
        changed = False
        for b, p in PARENT.items():
            if p in kids and b not in kids:
                kids.add(b); changed = True
    for b in kids:
        P[b] = pivot + M @ (P[b] - pivot); R[b] = M @ R[b]


def test_no_edit_is_identity():
    c = _load()
    new, rep = K.merge(c, _keys(c), poser=POSER)
    assert rep["frames"] == {}, rep["frames"]
    assert json.dumps(new, sort_keys=True) == json.dumps(c, sort_keys=True)


def test_fit_reproduces_every_key_pose():
    c = _load()
    for f, pose in c["author"]["poses"].items():
        P, R = POSER.solve(pose)
        Pn, _ = POSER.solve(K.fit(POSER, P, R, pose))
        cm, b = K.residual(P, Pn)
        assert cm < 0.3, (f, cm, b)


def test_arm_edit_changes_only_that_part():
    c = _load()
    bones = _keys(c)
    P, R = (copy.deepcopy(x) for x in bones[5])
    _rotate_subtree(P, R, "upperarm_r", rv([0, 0, 25]))       # swing the right arm 25 deg at the shoulder
    _rotate_subtree(P, R, "hand_r", rv([20, 0, 0]))           # and bend the wrist
    bones[5] = (P, R)
    new, rep = K.merge(c, bones, poser=POSER)
    assert list(rep["frames"]) == [5] and list(rep["frames"][5]["parts"]) == ["arm_r"], rep
    assert rep["frames"][5]["residual_cm"] < K.FIT_CM
    old5, new5 = c["author"]["poses"]["5"], new["author"]["poses"]["5"]
    for k in old5:
        if k not in ("arm_r", "clavicle_r"):
            assert old5[k] == new5[k], k
    Pn, Rn = POSER.solve(new5)
    assert K.ang(Rn["hand_r"], R["hand_r"]) < 1.0


def test_trunk_and_leg_edit():
    c = _load()
    bones = _keys(c)
    P, R = (copy.deepcopy(x) for x in bones[14])
    for b in ("spine_01", "spine_02", "spine_03", "spine_04", "spine_05"):   # a smooth bend, as the Cascadeur spine does
        _rotate_subtree(P, R, b, rv([-12 / 5, 0, 6 / 5]))
    _rotate_subtree(P, R, "thigh_l", rv([30, 0, 0]))
    for b in P:                                                 # and lift the whole body 5 cm
        P[b] = P[b] + np.array([0, 0, 5.0])
    bones[14] = (P, R)
    new, rep = K.merge(c, bones, poser=POSER)
    parts = rep["frames"][14]["parts"]
    assert "trunk" in parts and "leg_l" in parts, parts
    assert rep["frames"][14]["residual_cm"] < K.FIT_CM, rep["frames"][14]


def test_uneven_spine_is_reported():
    c = _load()
    bones = _keys(c)
    P, R = (copy.deepcopy(x) for x in bones[12])
    _rotate_subtree(P, R, "spine_04", rv([-35, 0, 0]))          # all the bend in one vertebra: author spreads it evenly
    bones[12] = (P, R)
    _, rep = K.merge(c, bones, poser=POSER)
    assert rep["frames"][12]["residual_cm"] > 0.0


def test_new_key_becomes_pose():
    c = _load()
    bones = _keys(c)
    from animpipe.author import frames
    P, R = (copy.deepcopy(x) for x in POSER.solve(frames(c["author"])[8]))
    _rotate_subtree(P, R, "upperarm_l", rv([0, 0, -20]))
    bones[8] = (P, R)
    new, rep = K.merge(c, bones, poser=POSER)
    assert rep["new"] == [8] and "8" in new["author"]["poses"]
    assert [p["frame"] for p in new["poses"]] == sorted(p["frame"] for p in new["poses"])
    assert any(p["frame"] == 8 for p in new["poses"])


def test_lock_blocks_and_unlock_passes():
    c = _load()
    c["approved"] = [{"frames": [5], "parts": ["arm_r"], "ref": "overlord/overlord_dair_v4.json", "why": "test"}]
    bones = _keys(c)
    P, R = (copy.deepcopy(x) for x in bones[5])
    _rotate_subtree(P, R, "upperarm_r", rv([0, 0, 25]))
    bones[5] = (P, R)
    try:
        K.merge(c, bones, poser=POSER)
        assert False, "lock ignored"
    except ValueError as e:
        assert "arm_r@5" in str(e)
    _, rep = K.merge(c, bones, unlock={"arm_r@5"}, poser=POSER)
    assert 5 in rep["frames"]


def test_ref_cancels_cascadeur_distortion():
    """Cascadeur's readback (ref) twists the thighs and shortens a leg a little; the owner then raises the right hand.
    Only the hand edit reaches the card, the distortion does not."""
    c = _load()
    card = _keys(c)
    ref, new = {}, {}
    for f, (P, R) in card.items():
        P, R = copy.deepcopy(P), copy.deepcopy(R)
        for s in "lr":
            _rotate_subtree(P, R, f"thigh_{s}", rv([0, 0, 10]))      # rig distortion, same in ref and new
        ref[f] = (P, R)
        new[f] = (copy.deepcopy(P), copy.deepcopy(R))
    P, R = new[31]
    _rotate_subtree(P, R, "upperarm_r", rv([-20, 0, 0]))
    out, rep = K.merge(c, new, poser=POSER, ref=ref)
    assert list(rep["frames"]) == [31] and list(rep["frames"][31]["parts"]) == ["arm_r"], rep
    Pa, _ = POSER.solve(c["author"]["poses"]["31"]); Pb, _ = POSER.solve(out["author"]["poses"]["31"])
    Pr, _ = ref[31]; Pn, _ = new[31]
    want = (Pn["hand_r"] - Pn["spine_05"]) - (Pr["hand_r"] - Pr["spine_05"])
    got = (Pb["hand_r"] - Pb["spine_05"]) - (Pa["hand_r"] - Pa["spine_05"])
    assert np.linalg.norm(want - got) < 1.0, (want, got)
    assert np.linalg.norm(Pb["foot_l"] - Pa["foot_l"]) < 1e-6


if __name__ == "__main__":
    for n, f in list(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
