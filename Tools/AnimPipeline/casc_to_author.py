"""Turn owner key poses read from Cascadeur (main control points, world cm) into `author` pose dicts for a move card.

    python casc_to_author.py <owner_poses.json> <frame> [<frame> ...] [--no-rescale]
        -> prints {"scale", "ratios", "poses": {"<frame>": pose}} as JSON on stdout, the applied ratios (2 lines) on stderr

Input: {"<frame>": {"pelvis": [x,y,z], "thigh_l": ..., "hand_l": ..., ...}} in Cascadeur axes (Y up, Z forward,
X = character's left), as `pose.get` returns them. The owner's rig (Cascy) is taller than the UE mannequin, so limb
offsets are scaled by the ratio of leg lengths. Trunk and pelvis rotations come from matching frames built on the same
landmarks in the owner pose and in the UE idle (left axis from the paired joints, up axis along the spine); hands and
feet are then expressed in the frames author.py uses (hands from spine_05 in the chest frame, feet from the pelvis in the
pelvis frame). Poles are the elbow/knee pushed out from the limb's mid line, so the hinge plane is the owner's.
The pelvis keeps the owner's height change relative to the first input frame (aerial moves: no root motion here).

Proportions (default on): Cascy's forearm is longer than the mannequin's, so copying the owner's hand offsets put the UE
forearm into the head (dair v3/v4c, -6 cm at frame 5). Each segment (clavicle, upper arm, forearm, hand, neck+head,
thigh, shin) is measured on both rigs: the owner's from the pose file, the UE one from the template's local translations
(the lengths the author IK actually uses), the idle bones as fallback. Hand and foot targets then keep the owner's
direction from the shoulder / hip, start at the UE shoulder / hip, and are scaled by that side's chain ratio
(UE upper arm + forearm over the owner's; thigh + shin for legs): the same arm read, the same bend, on the shorter chain.
Poles move the same way. --no-rescale keeps the old mapping (shoulder-midpoint offsets scaled by the leg ratio).
Clavicle, hand and neck+head ratios are printed for information only (no clavicle or head length change is authored).
"""
import json, math, sys, os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
IDLE = os.path.join(HERE, "sheets", "raw", "idle_neutral_legbones.json")
TMPL = os.path.join(HERE, "sheets", "raw", "AS_Pilot_GanonFTilt_V2_frame0.json")
# segment -> (parent joint, child joint) per side; "neck_head" is central
SEGS = {"clavicle": ("clavicle", "upperarm"), "upperarm": ("upperarm", "lowerarm"), "forearm": ("lowerarm", "hand"),
        "hand": ("hand", "middle_01"), "thigh": ("thigh", "calf"), "shin": ("calf", "foot")}


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


def seg_lengths(P, local=None):
    """{segment_side: cm} from joint positions P (UE axes); a child's local translation wins when given (template)."""
    out = {}
    for name, (a, b) in SEGS.items():
        for sd in "lr":
            pa, pb = f"{a}_{sd}", f"{b}_{sd}"
            if local and pb in local and name != "hand":
                out[f"{name}_{sd}"] = float(np.linalg.norm(local[pb][:3]))
            elif pa in P and pb in P:
                out[f"{name}_{sd}"] = float(np.linalg.norm(np.asarray(P[pb]) - np.asarray(P[pa])))
    if local and "neck_02" in local and "head" in local:
        out["neck_head"] = float(np.linalg.norm(local["neck_02"][:3]) + np.linalg.norm(local["head"][:3]))
    elif "neck_01" in P and "head" in P:
        out["neck_head"] = float(np.linalg.norm(np.asarray(P["head"]) - np.asarray(P["neck_01"])))
    return out


def ue_rig():
    """UE idle joint positions and segment lengths (template local translations where the cache has them)."""
    I = {b: np.array(v[:3]) for b, v in json.load(open(IDLE))["frames"]["0"].items()}
    local = json.load(open(TMPL)).get("local") if os.path.exists(TMPL) else None
    return I, seg_lengths(I, local)


def ratios(L_ue, L_o):
    """UE / owner per segment, and the chain ratios applied to the hand and foot targets (None: segment not in both)."""
    r = {k: round(L_ue[k] / L_o[k], 3) for k in L_ue if k in L_o and L_o[k] > 1e-6}
    for sd in "lr":
        for chain, (a, b) in (("arm", ("upperarm", "forearm")), ("leg", ("thigh", "shin"))):
            ka, kb = f"{a}_{sd}", f"{b}_{sd}"
            if all(k in L_ue and k in L_o for k in (ka, kb)):
                r[f"{chain}_{sd}"] = round((L_ue[ka] + L_ue[kb]) / (L_o[ka] + L_o[kb]), 3)
    return r


def convert(O, ref_pelvis_z, rescale=True, rig=None, L_o=None):
    I, L_ue = rig or ue_rig()
    O = {k: ue(v) for k, v in O.items()}
    R = ratios(L_ue, L_o or seg_lengths(O))
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
        k_arm = R.get(f"arm_{sd}", s)

        def chest(p):
            if rescale:   # owner's direction from the shoulder, on the UE shoulder, scaled by the arm-chain ratio
                return (Dc.T @ (p - a) * k_arm + (I[f"upperarm_{sd}"] - I["spine_05"])).round(1)
            return (Dc.T @ (p - sh_mid_o) * s + (sh_mid_i - I["spine_05"])).round(1)
        h, e, a = O[f"hand_{sd}"], O[f"lowerarm_{sd}"], O[f"upperarm_{sd}"]
        pole = e + (e - (a + h) / 2) * 1.5
        pose[f"arm_{sd}"] = {"hand": chest(h).tolist(), "pole": chest(pole).tolist()}

        k_leg = R.get(f"leg_{sd}", s) if rescale else s

        def pel(p):   # already from the hip; with rescale each leg uses its own chain ratio
            return (Dp.T @ (p - O[f"thigh_{sd}"]) * k_leg + (I[f"thigh_{sd}"] - I["pelvis"])).round(1)
        f, k, t = O[f"foot_{sd}"], O[f"calf_{sd}"], O[f"thigh_{sd}"]
        pole = k + (k - (t + f) / 2) * 1.5
        # plantar flexion: owner's shin-to-foot angle against the idle's, about the character's left axis
        def ang(knee, foot, ball):
            u = knee - foot; v = ball - foot
            return math.degrees(math.acos(np.dot(u, v) / np.linalg.norm(u) / np.linalg.norm(v)))
        sdl = sd
        flex = ang(O[f"calf_{sd}"], f, O[f"ball_{sd}"]) - ang(I[f"calf_{sdl}" if sdl == "l" else "calf_l"], I["foot_l"], I["ball_l"])
        pose[f"leg_{sd}"] = {"foot": pel(f).tolist(), "pole": pel(pole).tolist(), "foot_rot": [round(-flex, 1), 0, 0]}
    return pose, round(s, 3), R


def owner_lengths(data, frames):
    """Owner segment lengths, mean over the given frames (Cascadeur's points can stretch a little between poses)."""
    per = [seg_lengths({k: ue(v) for k, v in data[f].items()}) for f in frames]
    return {k: float(np.mean([p[k] for p in per if k in p])) for k in set().union(*per)}


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    rescale = "--no-rescale" not in sys.argv
    data = json.load(open(args[0]))
    frames = args[1:]
    ref = ue(data[frames[0]]["pelvis"])[2]
    rig = ue_rig(); L_o = owner_lengths(data, frames)
    out = {}
    for fr in frames:
        out[fr], s, R = convert(data[fr], ref, rescale, rig, L_o)
    print(json.dumps({"scale": s, "rescale": rescale, "ratios": R, "poses": out}))
    seg = ", ".join(f"{k} {v}" for k, v in sorted(R.items()) if not k.startswith(("arm_", "leg_")))
    print(f"segments UE/owner: {seg}", file=sys.stderr)
    print(("applied " + ", ".join(f"{k} {R[k]}" for k in ("arm_l", "arm_r", "leg_l", "leg_r") if k in R)) if rescale
          else f"applied: none (--no-rescale, old leg-ratio scale {s})", file=sys.stderr)
