"""Dair variant J from I (2026-11-03): I unchanged plus ONE correction from the cloud visual review of I.

Review of I: the follow-through after the stomp is invisible, trunk, crossed arms and head drop as one block with the pelvis
(absolute z minimum of hands and head at anim 19 = the pelvis). The crossed arms have no room left (chest clearance ~0.4 cm),
so the follow-through goes through the TRUNK: an extra pitch on spine_03 + spine_05 lagging the pelvis, peak at 21, a small
overshoot at 24-25, back to I (and to the -22 deg hold) by 28, damped. Sign: on this rig Rotation X positive raises the head
(I measured head z +0.22 cm/deg on spine_03), so "down" is NEGATIVE X. Nothing before 19 and nothing after 27 changes.
Hands: upperarm FK X at 19/20 shifted so their absolute minimum also falls after the pelvis (19 a bit higher, 20 a bit lower;
20 stays above the rest height relative to the pelvis, the clearance limit found on I).
Usage: python dair_varJ_make.py [S3] [S5]  (peak degrees on spine_03 / spine_05)"""
import json, subprocess, sys
S3 = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
S5 = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0
J = "cards/overlord/overlord_dair_v4_varJ.json"
I = json.loads(subprocess.check_output(["git", "show", "057bd21:Tools/AnimPipeline/cards/overlord/overlord_dair_v4_varI.json"]))
j = json.load(open(J))
c = json.loads(json.dumps(I))
for k in ("move", "anim", "variant"):
    c[k] = j[k]
c["source_anim"] = "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDAir_V4_VarJ"
c["author"]["take"] = "AS_Pilot_OverlordDAir_V4_VarJ"
WHY = ("review 03/11 J (da I): follow-through nel tronco, pitch avanti/giu su spine_03+spine_05 in ritardo sul bacino "
       "(picco a 21, overshoot 24-25, rientro a 28)")
# unit profile of the trunk pitch, 1 = peak (down), negative = overshoot back up
P = {19: 0.0, 20: 0.75, 21: 1.0, 22: 0.65, 23: 0.2, 24: -0.3, 25: -0.375, 26: -0.25, 27: -0.1}
HAND = {19: -0.8, 20: +1.8}      # extra upperarm X: 19 lifts (+0.3 cm), 20 lowers (-0.7 cm) relative to the pelvis
E = c["edits"]
def bump(ctrl, f, d):
    if abs(d) < 0.01:
        return
    for e in E:
        if e["control"] == ctrl and e["frames"] == [f] and e["attr"] == "Rotation":
            e["delta"][0] = round(e["delta"][0] + d, 2); e["why"] += " | " + WHY
            return
    E.append({"control": ctrl, "attr": "Rotation", "frames": [f], "delta": [round(d, 2), 0, 0], "why": WHY})
for f, u in P.items():
    bump("spine_03_ctrl", f, -S3 * u)
    bump("spine_05_ctrl", f, -S5 * u)
for f, d in HAND.items():
    bump("upperarm_l_fk_ctrl", f, d); bump("upperarm_r_fk_ctrl", f, d)
c["edits"] = sorted(E, key=lambda e: (e["frames"][0], e["control"]))
c.setdefault("_varJ", {}).update(s3=S3, s5=S5, profile=P, hand=HAND)
json.dump(c, open(J, "w"), indent=1, ensure_ascii=False)
print(len(E), "edits; spine_03", {f: round(-S3 * u, 1) for f, u in P.items()})
