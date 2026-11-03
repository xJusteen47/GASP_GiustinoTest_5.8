"""Dair variant I from H (2026-11-03): same pelvis driver, knees and FAF key 37 as H; hands and spine re-authored.

Measured on the H/G dumps (V4 base: hand, head minus pelvis constant over 16-31, so every relative motion is an edit):
  upperarm_*_fk_ctrl Rotation X: -0.375 cm of hand z (minus pelvis) per degree -> NEGATIVE lifts the hands (H had it inverted).
  spine_03_ctrl Rotation X: head z (minus pelvis) +0.22 cm/deg, head y +0.44 cm/deg, hand z (minus pelvis) +0.13 cm/deg.
  head_ctrl Rotation X does not move the head bone (pivot): it is a visible nod only, the nod amplitude in cm comes from the spine.
So: hands held up while the body drops (18-20), late dip at 21-23 (positive lobe capped at +4 deg for the chest clearance),
overshoot up at 25-27; spine = -SP x pelvis velocity, i.e. the head stays up on the drop and comes down on the rebound
(relative minimum 2 frames after the pelvis minimum, like a head lagging 2 frames). Usage: python dair_varI_make.py [SP] [HSCALE]"""
import json, subprocess, sys
SP = float(sys.argv[1]) if len(sys.argv) > 1 else 1.3        # spine deg per cm/frame of pelvis velocity (y amp ~6.3 cm)
HS = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0        # hand profile scale (positive lobe stays <= +4 deg)
I = "cards/overlord/overlord_dair_v4_varI.json"
H = json.loads(subprocess.check_output(["git", "show", "eb532e4:Tools/AnimPipeline/cards/overlord/overlord_dair_v4_varH.json"]))
i = json.load(open(I))
c = json.loads(json.dumps(H))
for k in ("move", "anim", "variant"):
    c[k] = i[k]
c["source_anim"] = "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDAir_V4_VarI"
c["author"]["take"] = "AS_Pilot_OverlordDAir_V4_VarI"
WHY = ("owner 03/11 I (da H): mani tenute su mentre il bacino scende (18-20), poi in ritardo giu a 21-23 (<=+4 gradi) e su a 25-27; "
       "spina dalla velocita del bacino: testa su nella discesa, giu nel rimbalzo, 2 frame dopo il bacino")
for p in c["poses"]:
    if 18 <= p["frame"] <= 30:
        p["why"] = p.get("why", "").replace("owner 03/11 H (da G)", "owner 03/11 I (da H)")
D = {17: 0.0, 18: -5.0, 19: -10.0, 20: -7.0, 21: -1.0, 22: 4.0, 23: 4.5, 24: 2.0, 25: -1.5, 26: -2.5, 27: -1.5, 28: 0.0, 29: 0.5, 30: 0.2}
# attempt 1 ({21: 2.5, 22: 4, 23: 2} lobe) passed verify 13/13 but hand_l|spine_03 -1.86 cm @f22: the V4 rest pose leaves only
# ~0.4 cm, so the hands may not go below their rest height relative to the pelvis. Attempt 2: solved per frame from
# rel = -0.375 rot + 0.13 spine for targets +1.9/+3.1/+1.2/+0.4/-0.15/+0.3/+0.8/+2/+2/+0.8/+0.3/+0.3 cm (18-29): the hands are
# held up through the drop and come back to rest at 22 (shallow relative minimum, 3 frames after the pelvis), lift again at 25-26.
HAND = {18: -3, 19: -6, 20: -4.6, 21: -3.8, 22: -1.9, 23: -1.0, 24: -1.0, 25: -3.8, 26: -4.9, 27: -2.6, 28: -1.5, 29: -1.0}
keep = ("upperarm_l_fk_ctrl", "upperarm_r_fk_ctrl", "spine_03_ctrl")
E = [e for e in c["edits"] if e["frames"] == [12] or e["control"] not in keep]    # A's key 12 co_motion, H's body/feet/head
def add(ctrl, f, d):
    if abs(d) > 0.01:
        E.append({"control": ctrl, "attr": "Rotation", "frames": [f], "delta": [round(d, 2), 0, 0], "why": WHY})
for f in range(18, 31):
    add("spine_03_ctrl", f, -SP * (D[f] - D[f - 1]))
    r = HS * HAND.get(f, 0.0)
    r = min(r, 4.0)
    add("upperarm_l_fk_ctrl", f, r); add("upperarm_r_fk_ctrl", f, r)
for e in E:
    if 18 <= e["frames"][0] <= 30 and e["control"] not in keep:
        e["why"] = e["why"].replace("owner 03/11 H", "owner 03/11 I (da H)")
c["edits"] = sorted(E, key=lambda e: (e["frames"][0], e["control"]))
c.setdefault("_varI", {}).update(sp=SP, hand_scale=HS)
json.dump(c, open(I, "w"), indent=1, ensure_ascii=False)
print(len(E), "edits; spine", {f: round(-SP * (D[f] - D[f - 1]), 1) for f in range(18, 27)})
