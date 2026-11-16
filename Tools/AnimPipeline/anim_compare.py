"""Compare two AnimSequences bone by bone (component-space location and rotation), read-only via the UE bridge.
Usage: python -I anim_compare.py <animA> <animB>"""
import sys, os, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue

A, B = sys.argv[1], sys.argv[2]
BONES = ["root", "pelvis", "spine_05", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
         "upperarm_r", "lowerarm_r", "hand_r", "thigh_l", "calf_l", "foot_l", "foot_r", "ball_r", "index_02_l"]
def info(a):
    r = ue.call("read_anim_sequence", {"assetPath": a})
    return {k: r.get(k) for k in ("numFrames", "frameRate", "sequenceLength")}
ia, ib = info(A), info(B)
N = min(ia["numFrames"], ib["numFrames"])
def samples(a, space):
    out = []
    for s in range(0, N, 8):
        r = ue.call("sample_pose", {"assetPath": a, "boneNames": BONES, "frames": list(range(s, min(N, s + 8))), "space": space})
        if "samples" not in r:
            raise RuntimeError(str(r)[:300])
        out += r["samples"]
    return out
def qdist(r1, r2):
    # rotator angle difference (deg), approximate via axis components; good enough to spot mismatches
    return max(abs(((r1[k] - r2[k]) + 180) % 360 - 180) for k in ("pitch", "yaw", "roll"))
res = {"A": ia, "B": ib, "worst_loc_cm": {}, "worst_rot_deg": {}}
for space in ("component", "local"):
    sa, sb = samples(A, space), samples(B, space)
    wl, wr = {}, {}
    for fa, fb in zip(sa, sb):
        ba = {x["name"]: x for x in fa["bones"]}; bb = {x["name"]: x for x in fb["bones"]}
        for n in BONES:
            if n not in ba or n not in bb:
                continue
            la, lb = ba[n]["location"], bb[n]["location"]
            d = math.dist((la["x"], la["y"], la["z"]), (lb["x"], lb["y"], lb["z"]))
            q = qdist(ba[n]["rotation"], bb[n]["rotation"])
            if d > wl.get(n, (0, 0))[0]:
                wl[n] = (round(d, 3), fa["frame"])
            if q > wr.get(n, (0, 0))[0]:
                wr[n] = (round(q, 3), fa["frame"])
    res["worst_loc_cm"][space] = wl
    res["worst_rot_deg"][space] = wr
print(json.dumps(res, indent=1))
