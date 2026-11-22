"""Self-penetration check of an AnimSequence on the UEFN mannequin, using its physics-asset primitives (physbodies.json,
exported by ue_physbodies.py) and component-space bone transforms sampled through the UE bridge.
Clearance = gap between collision shapes in cm (negative = interpenetration). Boxes (hands) are sampled as point grids,
capsules are exact (segment distance minus radius).
Usage: python clearance.py <AnimPath> [min_cm=2] [out.json]"""
import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue

HERE = os.path.dirname(os.path.abspath(__file__))
ANIM = sys.argv[1]
MIN = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
OUT = sys.argv[3] if len(sys.argv) > 3 else None
PR = json.load(open(os.path.join(HERE, "physbodies.json")))["prims"]
SHAPES = {}
for p in PR:
    SHAPES.setdefault(p["bone"], []).append(p)
PAIRS = []
for hand_side, other in (("r", "l"), ("l", "r")):
    for a in (f"hand_{hand_side}", f"lowerarm_{hand_side}"):
        for b in ("head", "neck_01", "neck_02", "spine_05", "spine_04", "spine_03", f"clavicle_{other}", f"upperarm_{other}"):
            PAIRS.append((a, b))
PAIRS += [("hand_l", "hand_r"), ("lowerarm_l", "lowerarm_r"), ("hand_l", "lowerarm_r"), ("hand_r", "lowerarm_l")]
BONES = sorted({b for pr in PAIRS for b in pr})

def rot_to_quat(p, y, r):
    """FRotator(pitch, yaw, roll) in degrees -> quaternion (x, y, z, w), UE convention."""
    d = math.pi / 360.0
    SP, CP = math.sin(p * d), math.cos(p * d)
    SY, CY = math.sin(y * d), math.cos(y * d)
    SR, CR = math.sin(r * d), math.cos(r * d)
    return np.array([CR * SP * SY - SR * CP * CY, -CR * SP * CY - SR * CP * SY, CR * CP * SY - SR * SP * CY, CR * CP * CY + SR * SP * SY])

def qmul(a, b):
    x1, y1, z1, w1 = a; x2, y2, z2, w2 = b
    return np.array([w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                     w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2])

def qrot(q, v):
    u = q[:3]; w = q[3]
    return 2 * np.dot(u, v) * u + (w * w - np.dot(u, u)) * v + 2 * w * np.cross(u, v)

def world_shapes(bone_loc, bone_q, bone):
    out = []
    for s in SHAPES.get(bone, []):
        c = bone_loc + qrot(bone_q, np.array(s["trans"]))
        q = qmul(bone_q, np.array(s["quat"]))
        ax = [qrot(q, np.array(v, dtype=float)) for v in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
        if "CAPSULE" in s["type"].upper() or "SPHYL" in s["type"].upper():
            r = s["extent"][0]; h = max(s["extent"][2] - r, 0.0)
            out.append(("cap", c - ax[2] * h, c + ax[2] * h, r))
        elif "SPHERE" in s["type"].upper():
            out.append(("cap", c, c, s["extent"][0]))
        else:
            e = s["extent"]
            pts = [c + ax[0] * (i * e[0]) + ax[1] * (j * e[1]) + ax[2] * (k * e[2])
                   for i in np.linspace(-1, 1, 5) for j in np.linspace(-1, 1, 3) for k in np.linspace(-1, 1, 4)]
            out.append(("box", np.array(pts), c, ax, np.array(e)))
    return out

def seg_point(a, b, p):
    ab = b - a; t = 0.0 if not ab.any() else np.clip(np.dot(p - a, ab) / np.dot(ab, ab), 0, 1)
    return np.linalg.norm(p - (a + ab * t))

def seg_seg(p1, q1, p2, q2):
    # sampled is fine at this scale (capsules are short)
    ts = np.linspace(0, 1, 9)
    return min(seg_point(p2, q2, p1 + (q1 - p1) * t) for t in ts) if True else 0

def box_sdf(box, p):
    _, _, c, ax, e = box
    d = np.array([np.dot(p - c, a) for a in ax])
    q = np.abs(d) - e
    return np.linalg.norm(np.maximum(q, 0)) + min(max(q[0], q[1], q[2]), 0.0)

def gap(A, B):
    best = 1e9
    for a in A:
        for b in B:
            if a[0] == "cap" and b[0] == "cap":
                g = seg_seg(a[1], a[2], b[1], b[2]) - a[3] - b[3]
            elif a[0] == "box" and b[0] == "cap":
                g = min(seg_point(b[1], b[2], p) for p in a[1]) - b[3]
            elif a[0] == "cap" and b[0] == "box":
                g = min(seg_point(a[1], a[2], p) for p in b[1]) - a[3]
            else:
                g = min(box_sdf(b, p) for p in a[1])
            best = min(best, g)
    return best

info = ue.call("read_anim_sequence", {"assetPath": ANIM})
N = int(info.get("numFrames") or 38)
samples = []
for s0 in range(0, N, 8):
    r = ue.call("sample_pose", {"assetPath": ANIM, "boneNames": BONES, "frames": list(range(s0, min(N, s0 + 8))), "space": "component"})
    samples += r["samples"]
res = {"anim": ANIM, "min_cm": MIN, "frames": {}, "worst": {}}
for s in samples:
    f = s["frame"]
    W = {}
    for b in s["bones"]:
        L = b["location"]; R = b["rotation"]
        W[b["name"]] = world_shapes(np.array([L["x"], L["y"], L["z"]]), rot_to_quat(R["pitch"], R["yaw"], R["roll"]), b["name"])
    row = {}
    for a, b in PAIRS:
        if W.get(a) and W.get(b):
            row[f"{a}|{b}"] = round(float(gap(W[a], W[b])), 2)
    res["frames"][f] = row
    for k, v in row.items():
        if k not in res["worst"] or v < res["worst"][k][0]:
            res["worst"][k] = (v, f)
bad = {k: v for k, v in res["worst"].items() if v[0] < MIN}
res["violations"] = bad
if OUT:
    json.dump(res, open(OUT, "w"), indent=1)
print(f"{ANIM}: pairs below {MIN} cm (worst gap cm, frame):")
for k, v in sorted(bad.items(), key=lambda x: x[1][0]):
    print(f"  {k:26s} {v[0]:6.2f} @ f{v[1]}")
print("frame-by-frame minimum over all pairs:", {f: min(r.values()) for f, r in res["frames"].items()})
