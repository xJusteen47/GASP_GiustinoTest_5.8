"""Turn owner key poses read from Cascadeur (main control points, world cm) into `author` pose dicts for a move card.

    python casc_to_author.py <owner_poses.json> <frame> [<frame> ...]   -> prints {"<frame>": pose} as JSON

Input: {"<frame>": {"pelvis": [x,y,z], "thigh_l": ..., "hand_l": ..., ...}} in Cascadeur axes (Y up, Z forward,
X = character's left), as `pose.get` returns them. The owner's rig (Cascy) is taller than the UE mannequin, so limb
offsets are scaled by the ratio of leg lengths. Trunk and pelvis rotations come from matching frames built on the same
landmarks in the owner pose and in the UE idle (left axis from the paired joints, up axis along the spine); hands and
feet are then expressed in the frames author.py uses (hands from spine_05 in the chest frame, feet from the pelvis in the
pelvis frame). Poles are the elbow/knee pushed out from the limb's mid line, so the hinge plane is the owner's.
The pelvis keeps the owner's height change relative to the first input frame (aerial moves: no root motion here).
"""
import json, math, sys, os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
IDLE = os.path.join(HERE, "sheets", "raw", "idle_neutral_legbones.json")


def ue(p):
    return np.array([p[0], p[2], p[1]], float)


def frame(left, up):
    x = left / np.linalg.norm(left)
    z = up - x * np.dot(up, x); z /= np.linalg.norm(z)
    return np.column_stack([x, np.cross(z, x), z])


def rotvec(m):
    a = math.acos(max(-1.0, min(1.0, (np.trace(m) - 1) / 2)))
    if a < 1e-6:
        return [0.0, 0.0, 0.0]
    v = np.array([m[2, 1] - m[1, 2], m[0, 2] - m[2, 0], m[1, 0] - m[0, 1]]) / (2 * math.sin(a))
    return (v * math.degrees(a)).round(1).tolist()


def align(a, b):
    a = a / np.linalg.norm(a); b = b / np.linalg.norm(b)
    v = np.cross(a, b); s = np.linalg.norm(v); c = np.dot(a, b)
    if s < 1e-9:
        return np.eye(3)
    K = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + K + K @ K * ((1 - c) / s ** 2)


def convert(O, ref_pelvis_z):
    I = {b: np.array(v[:3]) for b, v in json.load(open(IDLE))["frames"]["0"].items()}
    O = {k: ue(v) for k, v in O.items()}
    leg_i = np.linalg.norm(I["calf_l"] - I["thigh_l"]) + np.linalg.norm(I["foot_l"] - I["calf_l"])
    leg_o = np.linalg.norm(O["calf_l"] - O["thigh_l"]) + np.linalg.norm(O["foot_l"] - O["calf_l"])
    s = leg_i / leg_o
    Fp_i = frame(I["thigh_l"] - I["thigh_r"], I["spine_02"] - I["pelvis"])
    Fp_o = frame(O["thigh_l"] - O["thigh_r"], O["spine_02"] - O["pelvis"])
    Dp = Fp_o @ Fp_i.T
    Fc_i = frame(I["upperarm_l"] - I["upperarm_r"], I["neck_01"] - I["spine_04"])
    Fc_o = frame(O["upperarm_l"] - O["upperarm_r"], O["neck_01"] - O["spine_04"])
    Dc = Fc_o @ Fc_i.T
    pose = {"pelvis": {"loc": [0, 0, round((O["pelvis"][2] - ref_pelvis_z) * s, 1)], "rot": rotvec(Dp)},
            "spine": rotvec(Dc @ Dp.T)}
    hv_i = Dc @ (I["head"] - I["neck_01"]); hv_o = O["head"] - O["neck_01"]
    pose["head"] = rotvec(align(hv_i, hv_o))
    sh_mid_i = (I["upperarm_l"] + I["upperarm_r"]) / 2; sh_mid_o = (O["upperarm_l"] + O["upperarm_r"]) / 2
    for sd in "lr":
        def chest(p):
            return (Dc.T @ (p - sh_mid_o) * s + (sh_mid_i - I["spine_05"])).round(1)
        h, e, a = O[f"hand_{sd}"], O[f"lowerarm_{sd}"], O[f"upperarm_{sd}"]
        pole = e + (e - (a + h) / 2) * 1.5
        pose[f"arm_{sd}"] = {"hand": chest(h).tolist(), "pole": chest(pole).tolist()}

        def pel(p):
            return (Dp.T @ (p - O[f"thigh_{sd}"]) * s + (I[f"thigh_{sd}"] - I["pelvis"])).round(1)
        f, k, t = O[f"foot_{sd}"], O[f"calf_{sd}"], O[f"thigh_{sd}"]
        pole = k + (k - (t + f) / 2) * 1.5
        # plantar flexion: owner's shin-to-foot angle against the idle's, about the character's left axis
        def ang(knee, foot, ball):
            u = knee - foot; v = ball - foot
            return math.degrees(math.acos(np.dot(u, v) / np.linalg.norm(u) / np.linalg.norm(v)))
        sdl = sd
        flex = ang(O[f"calf_{sd}"], f, O[f"ball_{sd}"]) - ang(I[f"calf_{sdl}" if sdl == "l" else "calf_l"], I["foot_l"], I["ball_l"])
        pose[f"leg_{sd}"] = {"foot": pel(f).tolist(), "pole": pel(pole).tolist(), "foot_rot": [round(-flex, 1), 0, 0]}
    return pose, round(s, 3)


if __name__ == "__main__":
    data = json.load(open(sys.argv[1]))
    frames = sys.argv[2:]
    ref = ue(data[frames[0]]["pelvis"])[2]
    out = {}
    for fr in frames:
        out[fr], s = convert(data[fr], ref)
    print(json.dumps({"scale": s, "poses": out}))
