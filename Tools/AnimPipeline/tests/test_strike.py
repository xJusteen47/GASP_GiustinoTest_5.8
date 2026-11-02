"""Strike block of legibility.py on synthetic bones (no editor, no Saved/ cache): defaults and the old strike_limb
fallback, the card-move lookup by asset, and judge() on a measured stick figure: a punch keeps every pose check, a kick
or the dair legs drop the punch-only ones (open villain hand, elbows out). Run: python tests/test_strike.py"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.environ.setdefault("LEG_WORKERS", "1")
import legibility as L


def stick(f, open_hands=True):
    """A standing figure, lead (left) arm reaching forward with f; all rotations zero, fingers straight (curl 0 = open)."""
    P = {"pelvis": (0, 0, 95), "spine_01": (0, 0, 102), "spine_02": (0, 0, 110), "spine_03": (0, 0, 118), "spine_04": (0, 0, 126),
         "spine_05": (0, 0, 134), "neck_01": (0, 0, 146), "neck_02": (0, 0, 150), "head": (0, 0, 156),
         "clavicle_l": (4, 0, 142), "clavicle_r": (-4, 0, 142), "upperarm_l": (18, 0, 142), "upperarm_r": (-18, 0, 142),
         "lowerarm_l": (20, 14 + 10 * f, 130), "hand_l": (20, 30 + 25 * f, 135), "lowerarm_r": (-22, -14, 118), "hand_r": (-20, 8, 125),
         "thigh_l": (10, 0, 92), "thigh_r": (-10, 0, 92), "calf_l": (10, 6 + 20 * f, 50), "calf_r": (-10, 4, 50),
         "foot_l": (10, 10 * f, 8), "foot_r": (-10, 0, 8), "ball_l": (10, 12 + 10 * f, 2), "ball_r": (-10, 12, 2)}
    fr = {b: list(map(float, p)) + [0.0, 0.0, 0.0] for b, p in P.items()}
    for b in L.BONES:
        if b not in fr:
            s = b[-1]; base = P.get(f"hand_{s}") or P.get(b.rsplit("_", 2)[0] + f"_{s}", (0, 0, 100))
            fr[b] = list(map(float, base)) + [0.0, 0.0, 0.0 if open_hands else 30.0]
    return fr


FRAMES = {str(f): stick(f / 2) for f in range(3)}


def run(sheet):
    L.configure("synthetic", sheet=sheet)
    res = {"take": "synthetic", "views": L.VNAMES, "parts": L.PNAMES,
           "frames": {k: L.measure_frame(k, v) for k, v in FRAMES.items()}}
    return res, L.judge(res, (1, 1), 2)


def checks(flags):
    return {f["check"] for f in flags}


def test_defaults():
    assert L.strike_of({}) == {"limb": "arm_l", "hiders": L.REAR_ARM, "pose_checks": list(L.POSE_CHECKS_ALL)}
    k = L.strike_of({"strike_limb": "leg_l", "strike_side": "leg_l"})
    assert k["limb"] == "leg_l" and k["pose_checks"] == [] and set(k["hiders"]) == set(L.ARMS["l"] + L.ARMS["r"])
    d = L.strike_of({"strike_limb": "leg_l", "strike": {"limb": "legs", "hiders": ["arms", "torso"]}})
    assert d["limb"] == "legs" and "torso" in d["hiders"] and "hand_r" in d["hiders"]
    assert L.strike_of({"strike": {"limb": "arm_r"}})["hiders"] == L.ARMS["l"]
    for bad in ({"strike": {"limb": "tail"}}, {"strike": {"pose_checks": ["fists"]}}):
        try:
            L.strike_of(bad)
        except ValueError:
            continue
        raise AssertionError(bad)


def test_card_lookup():
    """checks.py writes the entry under the move name and passes the asset name: the dair must be found as a kick."""
    ms = __import__("json").load(open(os.path.join(L.HERE, "move_sheets.json")))
    for key, e in ms.items():
        if isinstance(e, dict) and e.get("anim", "").startswith("/Game/") and key.startswith("overlord_dair"):
            take = e["anim"].rsplit("/", 1)[1][3:]
            assert L.sheet_entry(take, e["anim"]).get("strike_limb") == "leg_l", key
            break
    else:
        print("  (no overlord_dair entry in move_sheets.json: lookup checked only on synthetic data)")
    assert L.sheet_entry("no_such_take", "/Game/none/AS_no_such_take") == {}


def test_punch_keeps_pose_checks():
    _, fl = run({})
    assert {"hand_l open", "hand_r open"} <= checks(fl), checks(fl)       # open hands still fail a punch


def test_kick_and_legs_skip_punch_checks():
    for sheet in ({"strike_limb": "leg_l"}, {"strike": {"limb": "legs", "hiders": ["arms"]}}):
        res, fl = run(sheet)
        assert not any(c.endswith(" open") or "elbow" in c for c in checks(fl)), (sheet, checks(fl))
        assert "fist_rear_steal" in res["frames"]["1"]["views"]["f180"] or not L.HIDERS
    L.configure("synthetic", sheet={"strike": {"limb": "legs"}})
    assert L.ATTS == ["foot_l", "foot_r"] and "foot_r" in L.PNAMES and L.HIDERS
    L.configure("synthetic", sheet={})
    assert "foot_l" not in L.PNAMES and L.ATTS == ["hand_l"]        # rebuilt, not stacked


def test_no_hiders_turns_rule_b_off():
    res, fl = run({"strike": {"limb": "legs", "hiders": []}})
    assert "rear arm hides the fist" not in checks(fl)
    assert all("fist_rear_steal" not in v for v in res["frames"]["1"]["views"].values())


if __name__ == "__main__":
    for n, f in list(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
