"""Overlord F. throw v1 (2026-11-23, fthrow-overlord-1): two-hand neck hold -> short head-back charge -> wide top-to-bottom headbutt.
Source: pose plan "Plan F throw from Zetterburn reference" (project files fthrow/fthrow-piano-2026-11-23.md, 8 cards) with the owner's
choices 1C (impact game frame 20, FAF 46), 2A (planted, ONE step <= 40 cm), 3A (head back + a jerk of the hands as anticipation).
Identity traits (rule 13a): 1) both hands on the neck until the impact; 2) headbutt from above to below on a wide arc; 3) planted, one step.
Zetterburn's 28-frame backward charge is NOT kept: the charge is a8-a14.

Game frames -> anim frames: round(g * 0.96). Impact g20 -> a19, FAF g46 -> a44, clip 0..50 (a45-a50 = tail back to idle).

Geometry (component space of the author Poser: X = character's LEFT, Y = forward, Z = up, origin at the feet).
The victim is placed by the grab code at its neck = the kit's HoldPointCm (grabber root space = capsule centre, X fwd, Y right).
Both hands are authored on that neck point and KEPT there (+-1 cm) through the impact: the victim does not follow the hands in game
(the anchor is a fixed point, GSTN_GrabHoldTypes.h), so a pull of the victim would detach the hands. The pull toward Overlord is read
from the body travelling forward into the victim while the hands stay (elbows fold); a real 10 cm pull of the victim needs a per-frame
anchor = code queue item (label "gioco"). The kit variant sets HoldPointCm to NECK below (minus the capsule half height).

Run (cwd Tools/AnimPipeline): python cards/overlord/fthrow/make_fthrow_cards.py -> writes overlord_fthrow_v1.json and prints
the reach / forehead / feet report (also in %TEMP%/fthrow/fthrow_report.txt).
"""
import copy, json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.getcwd())
from animpipe import author as A   # noqa: E402

po = A.Poser("along")
P0, R0 = po.solve({})
ARM = np.linalg.norm(po.lt["lowerarm_l"]) + np.linalg.norm(po.lt["hand_l"])   # shoulder (upperarm) -> wrist reach

# --- the victim (component space). Neck = hold point; its face looks back at Overlord (-Y).
NECK = np.array([5.0, 38.0, 136.0])     # victim neck_01; a same-size victim stands ~9 cm into the floor: see handover
FACE = NECK + np.array([0.0, -9.0, 15.0])          # front surface of the victim's face/forehead (headbutt target)
GRIP = {"l": NECK + np.array([8.0, -8.0, -1.0]), "r": NECK + np.array([-8.0, -8.0, -1.0])}   # wrists either side of the neck
ELBOW = {"l": [40.0, -5.0, -45.0], "r": [-40.0, -5.0, -45.0]}                               # elbow poles, chest frame: down/out
FOOT0 = {s: P0[f"foot_{s}"].copy() for s in "lr"}
KNEE_POLE = lambda a: [a[0], a[1] + 45.0, 50.0]


def rotvec(m):
    q = A.m2q(m); w = max(-1.0, min(1.0, q[3])); ang = 2 * math.acos(w); s = math.sqrt(max(1 - w * w, 1e-12))
    return (np.array(q[:3]) / s * math.degrees(ang)).tolist() if ang > 1e-6 else [0.0, 0.0, 0.0]


def pose(pel=(0, 0, 0), prot=(0, 0, 0), spine=(0, 0, 0), head=(0, 0, 0), clav=(0, 0), hands=None, hand_dw=None, feet=None,
         fingers=0.0, wrist_out=0.0, elbow=None):
    """A key pose from body angles plus WORLD targets for the wrists and ankles (converted into the chest / pelvis frames)."""
    p = {"pelvis": {"loc": list(map(float, pel)), "rot": list(map(float, prot))}, "spine": list(map(float, spine)),
         "head": list(map(float, head)), "clavicle_l": [0.0, 0.0, float(clav[0])], "clavicle_r": [0.0, 0.0, -float(clav[1])],
         "fingers": [float(fingers), 0, 0]}
    if wrist_out:
        p["wrist_out"] = [float(wrist_out), 0, 0]
    P, R = po.solve(p)
    Dch = A.rv(spine) @ A.rv(prot); Dp = A.rv(prot)
    hands = hands or GRIP
    for s in "lr":
        w = np.array(hands[s], float) + np.array((hand_dw or {}).get(s, (0, 0, 0)), float)
        p[f"arm_{s}"] = {"hand": (Dch.T @ (w - P["spine_05"])).tolist(), "pole": (elbow or ELBOW)[s]}
    feet = feet or {}
    for s in "lr":
        a = np.array(feet.get(s, FOOT0[s]), float)
        p[f"leg_{s}"] = {"foot": (Dp.T @ (a - P["pelvis"])).tolist(), "pole": (Dp.T @ (np.array(KNEE_POLE(a)) - P["pelvis"])).tolist(),
                         "foot_rot": [0.0, 0.0, 0.0]}
    P, R = po.solve(p)
    for s in "lr":                                   # keep the foot's idle world orientation (flat, same toe-out)
        cur = R[f"calf_{s}"] @ po.lr[f"foot_{s}"]
        lift = feet.get(f"{s}_toe", 0.0)             # toe down while the foot travels (deg, about the character's X)
        p[f"leg_{s}"]["foot_rot"] = rotvec(A.rv([-lift, 0, 0]) @ R0[f"foot_{s}"] @ cur.T)
    return p


STEP = 30.0                                          # 2A: one step forward, <= 40 cm
L_STEP = FOOT0["l"] + np.array([0.0, STEP, 0.0])
L_MID = FOOT0["l"] + np.array([0.0, 17.0, 7.0])
L_BACK = FOOT0["l"] + np.array([0.0, 14.0, 4.0])
YAW = -18.0                                          # chest squared to the victim (idle chest is turned ~24 deg)
GRIPF, OPEN = 0.3, 1.0                               # fingers: 0 = template fist, 1 = villain hand (open, flat)
TIGHT = {"l": [25.0, -10.0, -50.0], "r": [-25.0, -10.0, -50.0]}   # charge: elbows in and down
TUG = {"l": (0, -3.5, -2.5), "r": (0, -3.5, -2.5)}   # 3A: the hands jerk (a few cm: the victim does not follow in game)

K = {
    # P0 grip: both hands on the neck, chest squared, looking at the victim, already a held tension (not idle)
    0: pose(pel=(0, 2, -2), prot=(0, 0, -7), spine=(-3, 0, -11), head=(-3, 0, 2), clav=(6, 6), fingers=GRIPF),
    # jerk: shoulders up, hands tug, knees load, head starts back
    4: pose(pel=(-1, 0, -5), prot=(2, 0, -7), spine=(2, 0, -11), head=(6, 0, 2), clav=(10, 10), hand_dw=TUG, fingers=0.2),
    # P1 charge: the body rises out of the load, trunk and head back, weight onto the back (right) foot, elbows tight
    8: pose(pel=(-1, -3, -1.5), prot=(3, 0, -7), spine=(6, 0, -11), head=(14, 0, 2), clav=(4, 4), fingers=0.2, elbow=TIGHT),
    # P2 peak: forehead at its highest and furthest back, held a12-a14 with a small drift (moving hold)
    12: pose(pel=(-2, -4, -0.5), prot=(4, 0, -7), spine=(8, 0, -11), head=(20, 0, 2), clav=(3, 3), fingers=0.2, elbow=TIGHT),
    14: pose(pel=(-2, -4, -0.5), prot=(4.5, 0, -7), spine=(8.5, 0, -11), head=(22, 0, 2), clav=(3, 3), fingers=0.2, elbow=TIGHT),
    # P3 snap: the trunk passes the vertical, the step is in the air, the head leads down
    17: pose(pel=(0, 3, -2), prot=(-1, 0, -7), spine=(-2, 0, -11), head=(-2, 0, 2), clav=(8, 8), fingers=0.2,
             feet={"l": L_MID, "l_toe": 12.0}),
    # P4 impact (throw key g20): forehead on the victim's face from above, shoulders closed, front foot planted, hands on the neck
    19: pose(pel=(0, 9, -7), prot=(-2, 0, -7), spine=(-6, 0, -11), head=(-12, 0, 2), clav=(14, 14), fingers=0.15, feet={"l": L_STEP}),
    # overshoot 1 frame (follow-through: the head keeps going down)
    20: pose(pel=(0, 10, -8), prot=(-3, 0, -7), spine=(-8, 0, -11), head=(-18, 0, 2), clav=(14, 14), fingers=0.15, feet={"l": L_STEP}),
    # P5 release: the hands open forward (villain hands), the head still goes down
    23: pose(pel=(0, 9, -8), prot=(-3, 0, -7), spine=(-9, 0, -11), head=(-26, 0, 2), clav=(10, 10), fingers=OPEN, wrist_out=18,
             hands={s: GRIP[s] + np.array([(6 if s == "l" else -6), 8, -8]) for s in "lr"}, feet={"l": L_STEP}),
    # P6 settle: trunk rises, hands come down and in, the front foot comes back
    28: pose(pel=(0, 6, -7), prot=(-2, 0, -5), spine=(-5, 0, -8), head=(-8, 0, 1), clav=(4, 4), fingers=0.7, wrist_out=10,
             hands={s: P0[f"hand_{s}"] + np.array([0, 22, 22]) for s in "lr"}, feet={"l": L_STEP}),
    34: pose(pel=(0, 2, -3), prot=(-1, 0, -2), spine=(-1, 0, -3), head=(2, 0, 0), clav=(1, 1), fingers=0.3,
             hands={s: P0[f"hand_{s}"] + np.array([0, 10, 10]) for s in "lr"}, feet={"l": L_BACK, "l_toe": 6.0}),
    # P7 FAF (g46 -> a44): back on the idle stance, fists, a small settle past upright at a39
    39: pose(pel=(0, -1, -1.5), prot=(1, 0, 0), spine=(2, 0, 0), head=(1, 0, 0), clav=(0, 0), fingers=0.0,
             hands={s: P0[f"hand_{s}"] + np.array([0, 3, 3]) for s in "lr"}),
    44: {},
    50: {},
}
SPACING = {0: "arrive_1_2", 4: "ease_from", 8: "charge_long", 12: "hold_recoil", 14: "charge_long", 17: "hold_recoil",
           19: "hold_recoil", 20: "limb_led_recovery", 23: "limb_led_recovery", 28: "limb_led_recovery", 34: "ease_into", 39: "ease_into",
           44: "hold_recoil"}
NAMES = {0: "grip", 4: "jerk", 8: "charge", 12: "peak", 14: "peak_hold", 17: "snap", 19: "impact", 20: "overshoot", 23: "release",
         28: "settle", 34: "foot_back", 39: "settle_past", 44: "faf", 50: "tail_end"}


def report():
    rows = []
    for f in sorted(K):
        P, R = po.solve(K[f])
        Dh = A.rv(K[f].get("head", [0, 0, 0])) @ A.rv(K[f].get("spine", [0, 0, 0])) @ A.rv(K[f].get("pelvis", {}).get("rot", [0, 0, 0]))
        fh = P["head"] + Dh @ np.array([0.0, 11.0, 10.0])
        reach = {s: np.linalg.norm(np.array(P[f"hand_{s}"]) - np.array(P[f"upperarm_{s}"])) for s in "lr"}
        grip = {s: np.linalg.norm(np.array(P[f"hand_{s}"]) - GRIP[s]) for s in "lr"}
        rows.append(f"a{f:02d} forehead {np.round(fh, 1).tolist()} to_face {np.linalg.norm(fh - FACE):5.1f} | reach l {reach['l']:.1f} "
                    f"r {reach['r']:.1f} /{ARM:.1f} | grip_err l {grip['l']:.1f} r {grip['r']:.1f} | foot_l {np.round(P['foot_l'], 1).tolist()} "
                    f"foot_r {np.round(P['foot_r'], 1).tolist()} | pelvis_z {P['pelvis'][2]:.1f}")
    return rows


if __name__ == "__main__":
    card = {
        "move": "overlord_fthrow_v1", "class": "tilt",
        "anim": "/Game/AnimPipeline/Rig/Overlord/FThrow/AS_OverlordFThrowV1_Build", "rig": "/Game/Characters/UE5_Mannequins/Rigs/CR_Mannequin_Body",
        "fps": 60, "timing_csv": "cards/overlord/fthrow/OverlordFThrow_timing.csv", "faf_game": 46, "effector": "head",
        "source_anim": "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordFThrowV1",
        "_design": ("Overlord F. throw (owner 1C 2A 3A, 2026-11-23): both hands on the victim's neck; a4 jerk (shoulders up, hands tug, "
                    "knees load); a8-a14 short head-back charge on the back foot (moving hold a12-a14); a17 the trunk passes the vertical "
                    "mid-step; a19 = g20 impact, forehead on the victim's face from above, front foot planted 30 cm ahead; a20 overshoot; "
                    "a23 villain hands open forward, head still going down; a28-a39 rise, foot back, settle; a44 = g46 FAF; a50 idle."),
        "identity": ["both hands on the neck until the impact", "headbutt top to bottom on a wide arc", "planted, one step <= 40 cm"],
        "poses": [dict({"name": NAMES[f], "frame": f, "source": None}, **({"spacing": SPACING[f]} if f in SPACING else {})) for f in sorted(K)],
        "spacing": {"startup": "charge_long", "active": "hold_recoil", "recovery": "limb_led_recovery"},
        "part_delays": {"chest": 0, "head": 0, "forearm": 0, "hand": 0},
        "sheet_extra": {"hold_window": [19, 21], "strike_limb": "head", "strike_side": "head"},
        "planted": ["foot_r_ik_ctrl"],
        "exemptions": [{"check": "balance", "frames": list(range(15, 23)), "why": "the headbutt lunge: CoM goes over the stepping foot, caught at a19"},
                       {"check": "planted", "frames": list(range(15, 19)) + list(range(29, 39)), "why": "left foot steps out (a15-a18) and back (a29-a38)"}],
        "author": {"take": "AS_Pilot_OverlordFThrowV1", "frames": 51, "thumb": "along", "poses": {str(f): K[f] for f in sorted(K)}},
        "hold_point_component_cm": NECK.tolist(),
    }
    out = os.path.join(HERE, "overlord_fthrow_v1.json")
    json.dump(card, open(out, "w"), indent=1)
    rows = report()
    rep = os.path.join(os.environ.get("TEMP", "."), "fthrow", "fthrow_report.txt")
    os.makedirs(os.path.dirname(rep), exist_ok=True)
    open(rep, "w").write("\n".join(rows) + "\n")
    print(out); print(rep)
    print(max(rows, key=lambda r: float(r.split("grip_err l ")[1].split()[0]) + float(r.split(" r ")[2].split()[0]) if "grip_err" in r else 0)[:200])
