"""casc_to_author.py proportions on a synthetic pair of rigs (no editor, no sheets/raw cache): an "owner" rig 10% taller
with a 4 cm longer forearm. With rescale the hand targets keep the owner's direction from the shoulder and the arm-chain
ratio; --no-rescale reproduces the previous mapping. Run: python tests/test_casc_scale.py"""
import json, os, subprocess, sys, tempfile
import numpy as np
TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)
import casc_to_author as C

UE = {"pelvis": (0, 0, 95), "spine_02": (0, 0, 110), "spine_04": (0, 0, 126), "spine_05": (0, 0, 134), "neck_01": (0, 0, 146),
      "head": (0, 2, 156)}
for sd, x in (("l", 1), ("r", -1)):
    UE.update({f"clavicle_{sd}": (4 * x, 0, 142), f"upperarm_{sd}": (17 * x, -2, 142), f"lowerarm_{sd}": (22 * x, -4, 115),
               f"hand_{sd}": (24 * x, 2, 90), f"middle_01_{sd}": (25 * x, 4, 81), f"thigh_{sd}": (10 * x, 0, 92),
               f"calf_{sd}": (11 * x, 2, 50), f"foot_{sd}": (11 * x, -2, 8), f"ball_{sd}": (11 * x, 12, 2)})
UE = {k: np.array(v, float) for k, v in UE.items()}


def owner(arm_dir=(0.3, 0.8, 0.2)):
    """Owner rig: UE x1.1 about the feet, forearm +4 cm; lead hand pushed along arm_dir (UE axes) from the shoulder."""
    O = {k: v * 1.1 for k, v in UE.items()}
    for sd in "lr":
        e, h = O[f"lowerarm_{sd}"], O[f"hand_{sd}"]
        u = (h - e) / np.linalg.norm(h - e); O[f"hand_{sd}"] = h + 4 * u; O[f"middle_01_{sd}"] += 4 * u
    a = O["upperarm_l"]; d = np.array(arm_dir) / np.linalg.norm(arm_dir)
    l1 = np.linalg.norm(O["lowerarm_l"] - a); l2 = np.linalg.norm(O["hand_l"] - O["lowerarm_l"])
    O["hand_l"] = a + d * 0.9 * (l1 + l2)
    O["lowerarm_l"] = a + d * 0.45 * (l1 + l2) + np.array([0.0, 0.0, -0.3 * l1])
    return {k: [float(v[0]), float(v[2]), float(v[1])] for k, v in O.items()}     # back to Cascadeur axes (Y up, Z fwd)


RIG = (UE, C.seg_lengths(UE))


def test_ratios():
    L_o = C.seg_lengths({k: C.ue(v) for k, v in owner().items()})
    R = C.ratios(RIG[1], L_o)
    assert abs(R["thigh_l"] - 1 / 1.1) < 1e-3 and abs(R["leg_r"] - 1 / 1.1) < 1e-3
    fa = np.linalg.norm(UE["hand_r"] - UE["lowerarm_r"]); ua = np.linalg.norm(UE["lowerarm_r"] - UE["upperarm_r"])
    assert abs(R["forearm_r"] - fa / (1.1 * fa + 4)) < 1e-3
    assert abs(R["arm_r"] - (fa + ua) / (1.1 * (fa + ua) + 4)) < 1e-3


def test_hand_keeps_direction_and_chain_ratio():
    O = owner(); L_o = C.seg_lengths({k: C.ue(v) for k, v in owner(arm_dir=(0, -0.2, -1)).items()})   # rest-pose lengths
    pose, s, R = C.convert(O, C.ue(O["pelvis"])[2], True, RIG, L_o)
    sh = UE["upperarm_l"] - UE["spine_05"]
    got = np.array(pose["arm_l"]["hand"]) - sh
    Ou = {k: C.ue(v) for k, v in O.items()}
    want = Ou["hand_l"] - Ou["upperarm_l"]
    cos = np.dot(got, want) / np.linalg.norm(got) / np.linalg.norm(want)
    assert cos > 0.99999, cos
    assert abs(np.linalg.norm(got) - np.linalg.norm(want) * R["arm_l"]) < 0.15
    chain_ue = RIG[1]["upperarm_l"] + RIG[1]["forearm_l"]
    assert np.linalg.norm(got) < chain_ue                            # 90% reach on the owner = 90% on UE: no clamped arm


def test_no_rescale_is_the_old_mapping():
    """--no-rescale must match the module as it was before proportions (git HEAD~ version, same synthetic idle)."""
    try:
        old = subprocess.run(["git", "show", "HEAD:Tools/AnimPipeline/casc_to_author.py"], cwd=TOOLS, capture_output=True, text=True, check=True).stdout
    except Exception:
        print("  (git not available: old-mapping comparison skipped)"); return
    if "rescale" in old:          # HEAD already has this change: compare with its parent
        old = subprocess.run(["git", "log", "--format=%H", "-n", "1", "--", "casc_to_author.py"], cwd=TOOLS, capture_output=True, text=True).stdout.strip()
        old = subprocess.run(["git", "show", f"{old}~1:Tools/AnimPipeline/casc_to_author.py"], cwd=TOOLS, capture_output=True, text=True).stdout
    d = tempfile.mkdtemp()
    idle = os.path.join(d, "idle.json")
    json.dump({"frames": {"0": {k: v.tolist() for k, v in UE.items()}}}, open(idle, "w"))
    ns = {"__name__": "old_c2a", "__file__": os.path.join(TOOLS, "casc_to_author.py")}; exec(old, ns); ns["IDLE"] = idle
    O = owner()
    po, so = ns["convert"](O, C.ue(O["pelvis"])[2])
    pn, sn, _ = C.convert(O, C.ue(O["pelvis"])[2], False, RIG)
    assert so == sn and json.dumps(po, sort_keys=True) == json.dumps(pn, sort_keys=True)


if __name__ == "__main__":
    for n, f in list(globals().items()):
        if n.startswith("test_"):
            f(); print("ok", n)
