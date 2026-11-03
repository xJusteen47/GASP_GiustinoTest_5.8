"""Dair variant H from G (2026-11-03): bigger follow-through after the stomp (edits on keys 18-30) and the FAF trunk
'un pelo avanti' at the owner's key 37 (anim frame: the old video labels f<N> were anim frames). Usage: python dair_varH_make.py [k37] [scale] [hand_k]"""
import json, subprocess, sys, numpy as np
K37 = float(sys.argv[1]) if len(sys.argv) > 1 else 0.2      # blend of key 37's trunk toward 31 (G: 0.5)
S = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0         # pelvis driver scale vs G
HK = float(sys.argv[3]) if len(sys.argv) > 3 else 1.4        # upperarm deg per cm of hand lag (G: -17 deg broke the chest clearance)
H = "cards/overlord/overlord_dair_v4_varH.json"
G = json.loads(subprocess.check_output(["git", "show", "0614992:Tools/AnimPipeline/cards/overlord/overlord_dair_v4_varG.json"]))
O = json.load(open("cards/overlord/overlord_dair_v4.json"))
h = json.load(open(H))
c = json.loads(json.dumps(G))
for k in ("move", "anim", "variant"):
    c[k] = h[k]
c["source_anim"] = "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDAir_V4_VarH"
c["author"]["take"] = "AS_Pilot_OverlordDAir_V4_VarH"
WHY = "owner 03/11 H: follow-through ~x2.6 su G (bacino -10 cm al f19, overshoot e assestamento smorzato), spina+1, testa+2 (nod ~20), mani+3 (picco dopo il bacino, compensate a 18-20), ginocchia +2 (~10 cm)"
for p in c["poses"]:
    if 18 <= p["frame"] <= 30:
        p["why"] = p.get("why", "").replace("owner 03/11 G", "owner 03/11 H (da G)")
# pelvis driver: G's body_ctrl Z deltas (24 missing in G, near zero) scaled
# attempt 2: eased entry at 18 (17->18 snap gate), peak 19, overshoot 22-23, damped settle to 0 at 31
g = {18: -5.0, 19: -10.0, 20: -7.0, 21: -1.0, 22: 4.0, 23: 4.5, 24: 2.0, 25: -1.5, 26: -2.5, 27: -1.5, 28: 0.0, 29: 0.5, 30: 0.2}
D = lambda f: S * g.get(f, 0.0)
E = [e for e in c["edits"] if e["frames"] == [12]]             # keep A's arm co_motion at key 12
def add(ctrl, attr, f, d):
    if any(abs(x) > 0.01 for x in d):
        E.append({"control": ctrl, "attr": attr, "frames": [f], "delta": [round(x, 2) for x in d], "why": WHY})
for f in range(18, 31):
    body = D(f)
    fold = max(0.0, -1.0 * D(f - 2)) if f - 2 >= 18 else 0.0           # knees: feet rise relative to the body, 2 frames late
    add("body_ctrl", "Location", f, [0, 0, body])
    add("foot_l_ik_ctrl", "Location", f, [0, 0, body + fold]); add("foot_r_ik_ctrl", "Location", f, [0, 0, body + fold])
    add("spine_03_ctrl", "Rotation", f, [-0.91 * D(f - 1) if f > 18 else 0, 0, 0])
    add("head_ctrl", "Rotation", f, [-2.2 * D(f - 2) if f > 19 else 0, 0, 0])
    # hands: partly held up while the body drops (18-20, capped by the chest clearance), then follow the dip 3 frames late
    rot = (HK * D(f - 3) if f > 20 else 0.0) + (min(8.0, -0.8 * body) if f <= 20 and body < 0 else 0.0)
    add("upperarm_l_fk_ctrl", "Rotation", f, [rot, 0, 0]); add("upperarm_r_fk_ctrl", "Rotation", f, [rot, 0, 0])
c["edits"] = E
# FAF: key 37 trunk = owner 37 blended K37 toward 31 (G used 0.5 -> lean -4.3, back)
def q_nlerp(a, b, t):
    a, b = np.array(a), np.array(b)
    if np.dot(a, b) < 0: b = -b
    q = a + t * (b - a); return list(q / np.linalg.norm(q))
o37, o31, n37 = O["author"]["poses"]["37"], O["author"]["poses"]["31"], c["author"]["poses"]["37"]
L = lambda a, b: [x + K37 * (y - x) for x, y in zip(a, b)]
n37["pelvis"] = {"loc": L(o37["pelvis"]["loc"], o31["pelvis"]["loc"]), "rot": L(o37["pelvis"]["rot"], o31["pelvis"]["rot"])}
n37["spine"] = L(o37["spine"], o31["spine"])
n37["spine_chain"] = {k: L(v, o31["spine_chain"][k]) for k, v in o37["spine_chain"].items()}
for b, q in o37.get("bones", {}).items():
    if b == "pelvis" or b.startswith("spine_"):
        n37["bones"][b] = q_nlerp(q, o31["bones"][b], K37)
c.setdefault("_varH", {})["k37"] = K37
json.dump(c, open(H, "w"), indent=1, ensure_ascii=False)
import lean as LN
print(len(E), "edits; lean y:", {f: v[1] for f, v in LN.lean(c, [31, 34, 35, 36, 37, 40, 42]).items()})
