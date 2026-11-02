"""Timing analysis of an imported jab AnimSequence (read-only, via the UE bridge).
For each frame: world-ish (component space) speed of key bones, and each arm bone's speed RELATIVE to the chest
(spine_05). An arm whose relative speed is ~0 while the chest moves is being carried rigidly by the torso.
Usage: python -I anim_timing.py /Game/AnimPipeline/Pilot/AS_Pilot_JabV5 [out.json]"""
import sys, os, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue

ANIM = sys.argv[1]
OUT = sys.argv[2] if len(sys.argv) > 2 else None
BONES = ["pelvis", "spine_03", "spine_05", "neck_01", "head", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
         "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "foot_l", "foot_r"]
info = ue.call("read_anim_sequence", {"assetPath": ANIM})
N = int(info.get("numFrames") or 38)
samples = []
for a in range(0, N, 8):
    r = ue.call("sample_pose", {"assetPath": ANIM, "boneNames": BONES, "frames": list(range(a, min(N, a + 8))), "space": "component"})
    if "samples" not in r:
        raise RuntimeError(str(r)[:500])
    samples += r["samples"]
P = {b: [] for b in BONES}
Q = {b: [] for b in BONES}
for s in samples:
    for b in s["bones"]:
        L = b["location"]; P[b["name"]].append((L["x"], L["y"], L["z"]))
        Q[b["name"]].append(b.get("rotation"))
def sub(a, c): return tuple(x - y for x, y in zip(a, c))
def norm(v): return math.sqrt(sum(x * x for x in v))
rows = []
for f in range(1, N):
    row = {"f": f}
    for b in BONES:
        row[b] = round(norm(sub(P[b][f], P[b][f - 1])), 2)
    for b in ("hand_l", "lowerarm_l", "hand_r", "lowerarm_r", "head"):
        rel_now = sub(P[b][f], P["spine_05"][f]); rel_prev = sub(P[b][f - 1], P["spine_05"][f - 1])
        row[b + "_rel"] = round(norm(sub(rel_now, rel_prev)), 2)
    rows.append(row)
cols = ["pelvis", "spine_05", "head", "head_rel", "hand_l", "hand_l_rel", "lowerarm_l_rel", "hand_r", "hand_r_rel", "lowerarm_r_rel", "clavicle_l"]
print(f"{ANIM}  frames={N}  speeds in cm/frame (component space); *_rel = relative to spine_05")
print("  f " + " ".join(f"{c[:13]:>13s}" for c in cols))
for row in rows:
    print(f"{row['f']:3d} " + " ".join(f"{row[c]:13.2f}" for c in cols))
if OUT:
    json.dump({"anim": ANIM, "N": N, "pos": P, "rows": rows}, open(OUT, "w"), indent=0)
