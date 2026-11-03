"""Dair variant L = K + one correction (2026-11-03, review of K): in the front view f21-24 the knees splay outward with the feet
together ("frog knees"). Only the knee poles of the legs move, f19-26: the thighs rotate inward so the knees point forward and
stay within hip width. The fold's amplitude and timing (foot z, pelvis) are K's; frames 0-17, 31/37/42, arms, head: untouched.
Usage: python dair_varL_make.py [DX [DY]]   pole delta at the plateau (cm, DX inward on each leg, DY forward)."""
import json, sys
DX = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
DY = float(sys.argv[2]) if len(sys.argv) > 2 else 40.0
SX = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0    # sign of the inward direction for the left leg in the control space
c = json.load(open("cards/overlord/overlord_dair_v4_varK.json"))
c["move"] = "overlord_dair_v4_varL"
c["anim"] = c["anim"].replace("VarK", "VarL"); c["source_anim"] = c["source_anim"].replace("VarK", "VarL")
c["author"]["take"] = "AS_Pilot_OverlordDAir_V4_VarL"
c["variant"] = {"of": "overlord_dair_v4", "letter": "L", "why": "L = K + ginocchia: cosce ruotate verso l'interno a f20-25, ginocchia avanti dentro la larghezza dei fianchi",
                "base_sha": c["variant"]["base_sha"], "at": "20261103_235000"}
W = {19: 0.4, 20: 1, 21: 1, 22: 1, 23: 1, 24: 1, 25: 1, 26: 0.4}
for f, w in W.items():
    for s, sg in (("l", -SX), ("r", -SX)):   # control x axis is flipped on both legs (measured on the build dump: attempt 1)
        c["edits"].append({"control": f"leg_{s}_pv_ik_ctrl", "attr": "Location", "frames": [f],
                           "delta": [round(sg * DX * w, 2), round(DY * w, 2), 0], "why": "owner 03/11 L: ginocchia in dentro, non a rana (cosce ruotate verso l'interno)"})
c["edits"].sort(key=lambda e: (e["frames"][0], e["control"]))
c["_varL"] = {"dx": DX, "dy": DY, "sx": SX}
json.dump(c, open("cards/overlord/overlord_dair_v4_varL.json", "w"), indent=1, ensure_ascii=False)
print("L", len(c["edits"]), "edits")
