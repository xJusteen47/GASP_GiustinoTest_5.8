"""Rig points -> skeleton bone transforms, without Cascadeur (owner, 01/11: "if you act directly in Unreal, one can use
Blender too"). The motion models write Cascadeur-style rig points per frame; this turns them into component-space bone
transforms of the UE mannequin, ready to be written into an AnimSequence (ue_write_take.py) or any DCC.

Each solved bone gets a frame from three of its rig points: origin O (its joint), aim A (the next joint or its own
direction point) and side S (the limb's bend direction or its own additional point). The solver is CALIBRATED on one pose
known both ways (a take with model points and the Unreal bones Cascadeur produced from them): for every frame t
    R_bone(t) = F(t) F(cal)^T R_bone(cal)          pos_bone(t) = O(t) + F(t) F(cal)^T (pos_bone(cal) - O(cal))
so each bone keeps the calibration pose's offset and roll relative to its points. Bones without points of their own
(spine_01/03/05, neck_02, twist bones, fingers) follow their neighbours: the rotation change since the calibration pose is
slerped between the two solved bones around them (spine, twist) or copied from the parent (fingers: the calibration's fists).

Cascadeur space: X left, Y up, Z forward; UE component space: X left, Y forward, Z up (swap Y and Z).
Usage: python pose_solver.py <targets.json> <out_bones.json> [--cal-targets=ganon_v2_targets.json]
       [--cal-bones=sheets/raw/ganon_v2_legbones.json] [--cal-frame=0] [--validate]
--validate solves the calibration take itself on every frame and reports the joint errors against its Unreal bones."""
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import legibility as L

SW = np.array([[1, 0, 0], [0, 0, 1], [0, 1, 0]], dtype=float)     # Cascadeur <-> UE (its own inverse)

# bone: (origin, aim, side) rig points
SPEC = {"pelvis": ("pelvis_MainPoint", "spine_02_MainPoint", "thigh_MainPoint_l"),
        "spine_02": ("spine_02_MainPoint", "spine_04_MainPoint", "spine_02_AdditionalPoint"),
        "spine_04": ("spine_04_MainPoint", "neck_01_MainPoint", "spine_04_AdditionalPoint"),
        "neck_01": ("neck_01_MainPoint", "head_MainPoint", "neck_01_AdditionalPoint"),
        "head": ("head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint")}
for s in "lr":
    SPEC.update({f"clavicle_{s}": (f"clavicle_MainPoint_{s}", f"upperarm_MainPoint_{s}", f"clavicle_AdditionalPoint_{s}"),
                 f"upperarm_{s}": (f"upperarm_MainPoint_{s}", f"lowerarm_MainPoint_{s}", f"lowerarm_LimbDir_{s}"),
                 f"lowerarm_{s}": (f"lowerarm_MainPoint_{s}", f"hand_MainPoint_{s}", f"lowerarm_LimbDir_{s}"),
                 f"hand_{s}": (f"hand_MainPoint_{s}", f"hand_DirectionPoint_{s}", f"hand_AdditionalPoint_{s}"),
                 f"thigh_{s}": (f"thigh_MainPoint_{s}", f"calf_MainPoint_{s}", f"calf_LimbDir_{s}"),
                 f"calf_{s}": (f"calf_MainPoint_{s}", f"foot_MainPoint_{s}", f"calf_LimbDir_{s}"),
                 f"foot_{s}": (f"foot_MainPoint_{s}", f"ball_MainPoint_{s}", f"ball_AdditionalPoint_{s}"),
                 f"ball_{s}": (f"ball_MainPoint_{s}", f"ball_DirectionPoint_{s}", f"ball_AdditionalPoint_{s}")})
# in-between bones: (bone, solved bone a, solved bone b, share of b, parent for the position)
BETWEEN = [("spine_01", "pelvis", "spine_02", 0.5, "pelvis"), ("spine_03", "spine_02", "spine_04", 0.5, "spine_02"),
           ("spine_05", "spine_04", "neck_01", 0.5, "spine_04"), ("neck_02", "neck_01", "head", 0.5, "neck_01")]
for s in "lr":
    BETWEEN += [(f"upperarm_twist_01_{s}", f"upperarm_{s}", f"upperarm_{s}", 0.0, f"upperarm_{s}"),
                (f"upperarm_twist_02_{s}", f"upperarm_{s}", f"upperarm_{s}", 0.0, f"upperarm_{s}"),
                (f"lowerarm_twist_01_{s}", f"lowerarm_{s}", f"hand_{s}", 0.33, f"lowerarm_{s}"),
                (f"lowerarm_twist_02_{s}", f"lowerarm_{s}", f"hand_{s}", 0.66, f"lowerarm_{s}")]


def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


def frame(P, f, spec):
    O, A, S = (np.asarray(P[n][f], float) for n in spec)
    x = unit(A - O); y = S - O; y = unit(y - x * np.dot(y, x)); z = np.cross(x, y)
    return O, np.stack([x, y, z], axis=1)


def m2q(M):
    t = np.trace(M)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2; w = 0.25 * s
        x, y, z = (M[2, 1] - M[1, 2]) / s, (M[0, 2] - M[2, 0]) / s, (M[1, 0] - M[0, 1]) / s
    elif M[0, 0] > M[1, 1] and M[0, 0] > M[2, 2]:
        s = math.sqrt(1.0 + M[0, 0] - M[1, 1] - M[2, 2]) * 2
        w, x, y, z = (M[2, 1] - M[1, 2]) / s, 0.25 * s, (M[0, 1] + M[1, 0]) / s, (M[0, 2] + M[2, 0]) / s
    elif M[1, 1] > M[2, 2]:
        s = math.sqrt(1.0 + M[1, 1] - M[0, 0] - M[2, 2]) * 2
        w, x, y, z = (M[0, 2] - M[2, 0]) / s, (M[0, 1] + M[1, 0]) / s, 0.25 * s, (M[1, 2] + M[2, 1]) / s
    else:
        s = math.sqrt(1.0 + M[2, 2] - M[0, 0] - M[1, 1]) * 2
        w, x, y, z = (M[1, 0] - M[0, 1]) / s, (M[0, 2] + M[2, 0]) / s, (M[1, 2] + M[2, 1]) / s, 0.25 * s
    q = np.array([x, y, z, w]); return q / np.linalg.norm(q)


def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def slerp(a, b, t):
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = float(np.dot(a, b))
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        q = a + t * (b - a); return q / np.linalg.norm(q)
    th = math.acos(d); s = math.sin(th)
    return (math.sin((1 - t) * th) * a + math.sin(t * th) * b) / s


class Solver:
    def __init__(self, cal_targets, cal_bones, cal_frame=0):
        P = json.load(open(cal_targets))["targets"]
        B = json.load(open(cal_bones))["frames"][str(cal_frame)]
        self.bones = sorted(B)
        self.cal = {}                                         # bone -> (R_bone in Cascadeur space, pos in Cascadeur space)
        for b in self.bones:
            p, q = L.bone_tf(B, b)
            self.cal[b] = (SW @ q2m(q) @ SW, SW @ p)
        self.calF = {b: frame(P, cal_frame, sp) for b, sp in SPEC.items()}

    def solve(self, P, f):
        """{bone: (pos_ue, quat_ue)} component space at frame f of the points P."""
        out_R, out_p, delta = {}, {}, {}
        for b, sp in SPEC.items():
            O, F = frame(P, f, sp); O0, F0 = self.calF[b]; R0, p0 = self.cal[b]
            D = F @ F0.T
            delta[b] = D; out_R[b] = D @ R0; out_p[b] = O + D @ (p0 - O0)
        for b, a, c, t, par in BETWEEN:
            if b not in self.cal:
                continue
            qd = slerp(m2q(delta[a]), m2q(delta[c]), t); D = q2m(qd)
            delta[b] = D; out_R[b] = D @ self.cal[b][0]
            out_p[b] = out_p[par] + delta[par] @ (self.cal[b][1] - self.cal[par][1])
        for b in self.bones:                                  # fingers and anything left: rigid with the hand (or parent)
            if b in out_R:
                continue
            par = "hand_" + b[-1] if b.endswith(("_l", "_r")) and ("hand_" + b[-1]) in out_R else "pelvis"
            delta[b] = delta[par]; out_R[b] = delta[par] @ self.cal[b][0]
            out_p[b] = out_p[par] + delta[par] @ (self.cal[b][1] - self.cal[par][1])
        return {b: ((SW @ out_p[b]).tolist(), m2q(SW @ out_R[b] @ SW).tolist()) for b in out_R}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[1:] if a.startswith("--"))
    ct = os.path.join(HERE, opt.get("cal-targets") or "ganon_v2_targets.json")
    cb = os.path.join(HERE, opt.get("cal-bones") or os.path.join("sheets", "raw", "ganon_v2_legbones.json"))
    S = Solver(ct, cb, int(opt.get("cal-frame") or 0))
    if "validate" in opt:
        P = json.load(open(ct))["targets"]; B = json.load(open(cb))["frames"]
        n = min(len(P["pelvis_MainPoint"]), len(B))
        worst = {}
        for f in range(n):
            sol = S.solve(P, f)
            for b in ("pelvis", "spine_04", "head", "hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r"):
                p, q = L.bone_tf(B[str(f)], b)
                dp = float(np.linalg.norm(np.array(sol[b][0]) - p)); da = L.qangle(np.array(sol[b][1]), q)
                w = worst.get(b, (0, 0, 0, 0))
                worst[b] = (max(w[0], dp), max(w[1], da), w[2] + dp / n, w[3] + da / n)
        print("validation on the calibration take (all frames): bone  max cm  max deg  mean cm  mean deg")
        for b, (a, c, d, e) in worst.items():
            print(f"  {b:<12} {a:6.2f} {c:7.2f} {d:7.2f} {e:8.2f}")
    if args:
        P = json.load(open(os.path.join(HERE, args[0])))["targets"]
        n = len(P["pelvis_MainPoint"])
        frames = {str(f): S.solve(P, f) for f in range(n)}
        json.dump({"source": args[0], "space": "ue_component", "frames": frames}, open(os.path.join(HERE, args[1]), "w"))
        print(f"written {args[1]}: {n} frames, {len(frames['0'])} bones")


if __name__ == "__main__":
    main()
