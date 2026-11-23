"""Thumb of the open (villain) hand in animpipe/author.py on a synthetic rig (no editor, no template cache): fists keep
the template thumb, fingers = 1 lays it straight beside the index ("along", default) or spreads it ("open").
Run: python tests/test_thumb.py"""
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe import author as A


def rig():
    """Idle positions (rotations identity), template locals = offsets; fingers and thumbs curled 35 deg per joint (fist)."""
    P = {"pelvis": (0, 0, 95)}
    for i, b in enumerate(A.SPINE):
        P[b] = (0, 0, 100 + 8 * i)
    P.update({"neck_01": (0, 0, 146), "neck_02": (0, 0, 150), "head": (0, 0, 156)})
    for s, x in (("l", 1), ("r", -1)):
        P.update({f"clavicle_{s}": (4 * x, 0, 142), f"upperarm_{s}": (17 * x, 0, 142), f"lowerarm_{s}": (17 * x, -2, 115),
                  f"hand_{s}": (17 * x, 0, 90), f"thigh_{s}": (10 * x, 0, 92), f"calf_{s}": (10 * x, 3, 50),
                  f"foot_{s}": (10 * x, 0, 8), f"ball_{s}": (10 * x, 12, 2)})
        for t in ("upperarm_twist_01", "upperarm_twist_02"):
            P[f"{t}_{s}"] = (17 * x, 0, 130)
        for t in ("lowerarm_twist_01", "lowerarm_twist_02"):
            P[f"{t}_{s}"] = (17 * x, 0, 100)
        for k, fg in enumerate(("index", "middle", "ring", "pinky")):
            y = 3 - 2 * k                                            # index forward (+Y), pinky back: the knuckle line
            P[f"{fg}_metacarpal_{s}"] = (17 * x, y, 88)
            for j, z in (("01", 81), ("02", 77), ("03", 74)):
                P[f"{fg}_{j}_{s}"] = (17 * x, y, z)
        P.update({f"thumb_01_{s}": (17 * x, 4, 88), f"thumb_02_{s}": (17 * x, 6, 85), f"thumb_03_{s}": (17 * x, 8, 82)})
    P = {k: np.array(v, float) for k, v in P.items()}
    R = {b: np.eye(3) for b in P}
    lt = {b: P[b] - P[p] for b, p in A.PARENT.items() if p is not None}
    curl = A.rv([0, 35, 0])
    lr = {b: (curl if b.split("_")[0] in ("index", "middle", "ring", "pinky", "thumb") and not b.split("_")[1] == "metacarpal"
              else np.eye(3)) for b in lt}
    return P, R, lt, lr


def thumb_vs_index(P, s):
    t = P[f"thumb_03_{s}"] - P[f"thumb_01_{s}"]; i = P[f"index_01_{s}"] - P[f"hand_{s}"]
    bend = P[f"thumb_02_{s}"] - P[f"thumb_01_{s}"]
    c = lambda a, b: math.degrees(math.acos(np.clip(np.dot(a, b) / np.linalg.norm(a) / np.linalg.norm(b), -1, 1)))
    return c(t, i), c(t, bend)


def test_fist_keeps_template_thumb():
    pz = A.Poser(rig=rig())
    P0, _ = A.Poser(rig=rig()).solve({})
    P1, _ = pz.solve({"fingers": [0, 0, 0]})
    assert all(np.allclose(P0[b], P1[b]) for b in A.THUMBS)


def test_along_and_open():
    for mode in ("along", "open"):
        P, _ = A.Poser(mode, rig=rig()).solve({"fingers": [1, 0, 0]})
        for s in "lr":
            ang, straight = thumb_vs_index(P, s)
            assert abs(ang - A.THUMB[mode]) < 0.5 and straight < 0.5, (mode, s, ang, straight)
            side = np.dot(P[f"thumb_03_{s}"] - P[f"thumb_01_{s}"], P[f"index_01_{s}"] - P[f"pinky_01_{s}"])
            assert side > 0, (mode, s, "thumb turned toward the pinky")
            print(f"  {mode} {s}: thumb {ang:.1f} deg from the index line, joints {straight:.1f} deg from straight")


def test_bad_thumb():
    try:
        A.Poser("wide", rig=rig())
    except ValueError:
        return
    raise AssertionError("thumb 'wide' accepted")


if __name__ == "__main__":
    for n, f in list(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
