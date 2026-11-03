"""Dair variant K from I (2026-11-03, owner option A + permission to open the forearms a little).

Review 2 of I: the whole silhouette drops as one block, head/fist minima on the pelvis minimum (anim 19-20), lag invisible.
K: the pelvis is HELD low 19-23 instead of rebounding at once; head and fists keep descending for 3-4 frames on the held
pelvis (absolute-z minima at 23 vs pelvis 20), then everything rises. Only the keys of the parts involved move:
  body_ctrl z (hold), foot ik z (= pelvis + a knee fold that peaks during the hold), spine_03/05 + neck_01 (head sinks),
  upperarm X (fists held up on the drop, then below rest during the hold), forearms opened forward 20-25 (lowerarm/upperarm)
  so the fists can go below their rest height without entering the chest (I: ~0.4 cm margin at rest).
Nothing before 18 and nothing after 30 changes (owner keys 0/3/8/15/17/31/37/42, FAF 37, hit window [15,17]).
Usage: python dair_varK_make.py probe            I + isolated test deltas, to measure the sensitivities on the build dump
       python dair_varK_make.py final [OPEN]     the K design; OPEN = scale of the forearm opening (default 1)"""
import json, subprocess, sys
MODE = sys.argv[1] if len(sys.argv) > 1 else "final"
OPEN = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
K = "cards/overlord/overlord_dair_v4_varK.json"
I = json.loads(subprocess.check_output(["git", "show", "057bd21:Tools/AnimPipeline/cards/overlord/overlord_dair_v4_varI.json"]))
k = json.load(open(K))
c = json.loads(json.dumps(I))
for key in ("move", "anim", "variant"):
    c[key] = k[key]
c["source_anim"] = "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDAir_V4_VarK"
c["author"]["take"] = "AS_Pilot_OverlordDAir_V4_VarK"
WHY = ("owner 03/11 K (da I, opzione A): bacino fermo in basso 19-23, testa e pugni continuano a scendere fino a 23, "
       "avambracci aperti dal petto 20-25")
E = c["edits"]


def setd(ctrl, attr, f, axis, v):
    """Absolute delta on one axis at one frame (replaces I's value on that axis)."""
    for e in E:
        if e["control"] == ctrl and e["frames"] == [f] and e["attr"] == attr:
            e["delta"][axis] = round(v, 2); e["why"] = WHY
            return
    if abs(v) > 0.01:
        d = [0, 0, 0]; d[axis] = round(v, 2)
        E.append({"control": ctrl, "attr": attr, "frames": [f], "delta": d, "why": WHY})


def both(stem, attr, f, axis, v):
    setd(f"{stem}_l_fk_ctrl", attr, f, axis, v); setd(f"{stem}_r_fk_ctrl", attr, f, axis, v)


if MODE == "probe":
    WHY = "probe K: misura sensibilita (non per l'owner)"
    both("lowerarm", "Rotation", 22, 0, 10)
    both("lowerarm", "Rotation", 24, 1, 10)
    both("lowerarm", "Rotation", 26, 2, 10)
    both("upperarm", "Rotation", 28, 1, 10); setd("neck_01_ctrl", "Rotation", 28, 0, -10)
    both("upperarm", "Rotation", 30, 2, 10)
    setd("spine_05_ctrl", "Rotation", 20, 0, -10)
else:
    PEL = {18: -5, 19: -10, 20: -10.6, 21: -10.5, 22: -10.4, 23: -10.1, 24: -9.4, 25: -7.2, 26: -4.2, 27: -1.6, 28: 0, 29: 0.4, 30: 0.1}
    KNEE = {18: 0, 19: 0, 20: 2, 21: 5, 22: 7.5, 23: 6.5, 24: 4, 25: 2, 26: 0.5, 27: 0, 28: 1.5, 29: 1.2, 30: 0}
    SP3 = {18: 5, 19: 6.5, 20: 4, 21: -1, 22: -6, 23: -10, 24: -11, 25: -8, 26: -4, 27: -0.5, 28: 1, 29: 0.5, 30: 0}
    UA = {18: -3, 19: -6, 20: -5, 21: -1, 22: 3, 23: 5, 24: 5, 25: 2, 26: -2, 27: -2.5, 28: -1.5, 29: -0.5, 30: 0}
    for f in range(18, 31):
        setd("body_ctrl", "Location", f, 2, PEL[f])
        setd("foot_l_ik_ctrl", "Location", f, 2, PEL[f] + KNEE[f]); setd("foot_r_ik_ctrl", "Location", f, 2, PEL[f] + KNEE[f])
        setd("spine_03_ctrl", "Rotation", f, 0, SP3[f])
        both("upperarm", "Rotation", f, 0, UA[f])
    # head_ctrl nod of I retimed onto the hold (I: peak +22 at 21; K: peak at 23)
    HEAD = {20: 4, 21: 10, 22: 17, 23: 20, 24: 14, 25: 4, 26: -5, 27: -7, 28: -3, 29: -1, 30: 0}
    NECK = {21: -3, 22: -6, 23: -8, 24: -7, 25: -4, 26: -1}      # probe: neck_01 X -10 -> head z -0.6 cm (sinks a bit more)
    for f in range(18, 31):
        setd("head_ctrl", "Rotation", f, 0, HEAD.get(f, 0))
        setd("neck_01_ctrl", "Rotation", f, 0, NECK.get(f, 0))
    for f, (spec) in json.load(open("dair_varK_open.json")).get("frames", {}).items():   # forearm opening, from the probe
        for ctrl, attr, axis, v in spec:
            setd(ctrl, attr, int(f), axis, OPEN * v)
c["edits"] = sorted([e for e in E if any(abs(x) > 0.01 for x in e["delta"])], key=lambda e: (e["frames"][0], e["control"]))
c["_varK"] = {"mode": MODE, "open": OPEN}
json.dump(c, open(K, "w"), indent=1, ensure_ascii=False)
print(MODE, len(c["edits"]), "edits")
