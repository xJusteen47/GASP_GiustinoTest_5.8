"""Dropkick K5 (2026-11-21, owner feedback on the r4 videos): the KICK itself, not the tech bounce.
Owner: no oomph, does not read as a dropkick with a strong sweetspot at the start ("someone extending the legs"); arms NOT crossed;
while falling he must look smug ("compiaciuto di se stesso, con quella posa").
Writes overlord_dropkick_{startup,loop}_k5.json next to the originals (the originals and the game copies are untouched: variant).
Curves: ../overlord_dropkick_k5_curves.md.  Run: python make_k5_cards.py
"""
import copy, json, os

HERE = os.path.dirname(os.path.abspath(__file__))
DK = os.path.dirname(HERE)

# Arms crossed like Bison: forearms flat across the chest, each hand on the opposite upper arm, elbows forward and out,
# left forearm over the right one. a0c7bf9f pushed r0's hands 12-18 cm forward (penetration fix) with the elbows at the sides:
# the forearms then pointed AWAY from the chest (in game they read as a raised guard). Here the hands go further across and lower,
# just outside the chest surface, and the elbow poles come forward so the forearms lie along the chest.
# Folded: elbow under the shoulder and forward (upper arm ~29 cm), forearm (~26 cm) straight across, hand at the far chest edge.
# Check 1 (clearance): forearms 3-7 cm into spine_03/04 and into each other -> hands 5-8 cm further out, left 2 cm higher.
# Targets fitted (k5/fit, chest frame of spine_05) so the wrists sit at -6/+6 cm across the midline, 19/15 cm out of spine_05,
# and the elbows at +15/-16 cm to the sides and 12/11 cm forward: both forearms flat across the chest, left over right.
CROSS = {"arm_l": {"hand": [-13.0, 22.0, -12.7], "pole": [85.7, 33.7, -59.1]},
         "arm_r": {"hand": [-0.3, 20.4, -18.6], "pole": [-92.6, -3.9, -95.5]}}
TIGHT = {"arm_l": {"hand": [-14.5, 21.6, -12.7], "pole": [85.7, 33.7, -59.1]},     # squeeze during the coil
         "arm_r": {"hand": [1.2, 20.0, -18.6], "pole": [-92.6, -3.9, -95.5]}}


def legs(fl, fr, pole, rot):
    return {"leg_l": {"foot": fl, "pole": [pole[0], pole[1], pole[2]], "foot_rot": [rot, 0, 0]},
            "leg_r": {"foot": fr, "pole": [-pole[0], pole[1], pole[2]], "foot_rot": [rot, 0, 0]}}


def pose(pl, pr, spine, head, arms, lg, fingers=0.0):
    p = {"pelvis": {"loc": pl, "rot": pr}, "spine": spine, "head": head}
    p.update(copy.deepcopy(arms))
    p.update(lg)
    p["fingers"] = [fingers, 0.0, 0.0]
    return p


# STRIKE = startup f15 = the loop's base pose (startup f15 == loop f11 -> f0, the seam of record)
STRIKE = pose([0, 50, -10], [82.0, 0, 0], [-8.0, 0, 0], [-20.0, 0, 6.0], CROSS,
              legs([2.0, -6.0, -84.0], [-8.0, -6.0, -84.0], [15.0, 15.0, -45.0], -50.0))

startup = {
    # f0 unchanged from r0 restore (breaks from the air pose; arms lifting toward the cross)
    "3": pose([0, 5, -1], [12.0, 0, 0], [-10.0, 0, 0], [-15.0, 0, 0], CROSS,              # arms SNAP crossed by f3 (attitude first)
              legs([8.0, 14.0, -44.0], [-8.0, 14.0, -44.0], [25.0, 45.0, -8.0], 20.0)),
    "7": pose([0, 18, -4], [50.0, 0, 0], [-22.0, 0, 0], [-38.0, 0, 0], TIGHT,             # tight ball, body already laid back
              legs([5.0, 16.0, -16.0], [-7.0, 16.0, -16.0], [20.0, 65.0, 20.0], 40.0)),
    "11": pose([0, 24, -6], [66.0, 0, 0], [-25.0, 0, 0], [-40.0, 0, 0], TIGHT,            # chamber: knees at the chest, eyes on target
               legs([5.0, 12.0, -13.0], [-7.0, 12.0, -13.0], [18.0, 62.0, 25.0], 45.0)),
    "13": pose([0, 27, -7], [70.0, 0, 0], [-26.0, 0, 0], [-42.0, 0, 0], TIGHT,            # last coil (small: the load before the shot)
               legs([5.0, 10.0, -11.0], [-7.0, 10.0, -11.0], [18.0, 60.0, 27.0], 45.0)),
    "14": pose([0, 38, -9], [77.0, 0, 0], [-16.0, 0, 0], [-36.0, 0, 0], CROSS,            # smear: legs 2/3 out in one frame
               legs([3.0, 4.0, -60.0], [-7.0, 4.0, -60.0], [15.0, 25.0, -30.0], -20.0)),
    "15": STRIKE,                                                                         # locked straight, toes pointed, chin up
}

def lerp(a, b, t):
    if isinstance(a, dict):
        return {k: lerp(a[k], b[k], t) for k in a}
    if isinstance(a, list):
        return [lerp(x, y, t) for x, y in zip(a, b)]
    return round(a + (b - a) * t, 3)

# Smug fall (owner 11-21: "compiaciuto di se stesso mentre cade, con quella posa"): same identity pose, chest out, chin up,
# a slow pleased head tilt and a small settle of the crossed arms; no flailing. Period 12, f0 = STRIKE so the seam is exact.
SMUG = pose([0, 50.5, -9.5], [81.0, 0, 0], [-5.0, 0, 0], [-15.0, 0, 12.0], CROSS,
            legs([2.0, -5.5, -84.0], [-8.0, -5.5, -84.0], [15.0, 15.0, -45.0], -48.0))
SMUG["arm_l"]["hand"] = [-13.5, 22.6, -11.7]
SMUG["arm_r"]["hand"] = [0.2, 21.0, -17.6]
loop = {"0": STRIKE, "6": SMUG, "11": lerp(STRIKE, SMUG, 0.067)}

# Round 2 (review k5b FAIL, posa: the round-1 cross read as a guard: elbows at the sides, hands pushed 5-8 cm out by the
# clearance fix, fists ahead of the chest). The arms of every key are replaced by the Bison FOLD from fold.py (fold_r2.json):
# shoulders rounded forward (clavicles ~25 deg), elbows forward and in (left 16/29/-7, right -7/20/-17 cm in the chest frame),
# forearms level and stacked left over right, right fist bent back under the left elbow, no fist ahead of the elbows, every
# clearance pair >= 0.6 cm offline (fold.py's shapes reproduce the build check within 0.3 cm on round 1).
# The legs, trunk, timing and the snap a13 -> a15 are round 1's (passed): only arms and clavicles change.
R2 = json.load(open(os.path.join(HERE, "fold_r2.json")))
for _poses, _sec in ((startup, "startup"), (loop, "loop")):
    for _k, _p in _poses.items():
        _poses[_k] = dict(_p, **copy.deepcopy(R2[_sec][_k]))
# f2 breakdown: eased straight from f0 to the fold, the right fist passes 2 cm through the left upper arm at f2. The right arm
# (the lower one) leads and the left (top) one lags, so the right forearm is already under the left elbow when it closes.
_old0 = json.load(open(os.path.join(DK, "overlord_dropkick_startup.json")))["author"]["poses"]["0"]
_f2 = {k: v for k, v in lerp({k: _old0[k] for k in ("pelvis", "spine", "head")}, {k: startup["3"][k] for k in ("pelvis", "spine", "head")}, 0.6).items()}
_f2.update({k: _old0[k] for k in ("leg_l", "leg_r", "fingers")})
_f2["leg_l"], _f2["leg_r"] = lerp(_old0["leg_l"], startup["3"]["leg_l"], 0.6), lerp(_old0["leg_r"], startup["3"]["leg_r"], 0.6)
_z = {"hand": [0, 0, 0], "pole": [0, 0, 0], "hand_rot": [0, 0, 0]}
F2_FWD = float(os.environ.get("K5_F2_FWD", 6.0))
for _s, _t in (("l", 0.45), ("r", float(os.environ.get("K5_F2_TR", 0.85)))):
    _a = dict(_z, **_old0[f"arm_{_s}"])
    _f2[f"arm_{_s}"] = {k: lerp(_a[k], startup["3"][f"arm_{_s}"][k], _t if k != "hand_rot" else _t) for k in ("hand", "pole", "hand_rot")}
    _f2[f"clavicle_{_s}"] = lerp([0.0, 0.0, 0.0], startup["3"][f"clavicle_{_s}"], _t)
_f2["arm_r"]["hand"][1] += F2_FWD; _f2["arm_r"]["pole"][1] += F2_FWD     # and passes in front of the chest
# r2 capture: the game frame a<N> shows anim frame ~N-1, so with the fold on key 3 the a3 frame still showed the breakdown (a guard).
# The breakdown moves to key 1 and the arms are folded from key 2 (body and legs keep their f0 -> f3 path), so a3 reads as the fold.
_f1 = copy.deepcopy(_f2)
for _k in ("pelvis", "spine", "head", "leg_l", "leg_r"):
    _f1[_k] = lerp(_old0[_k], startup["3"][_k], 0.3)
_f2.update({k: copy.deepcopy(startup["3"][k]) for k in ("arm_l", "arm_r", "clavicle_l", "clavicle_r")})
startup["1"], startup["2"] = _f1, _f2


def write(name, src, poses, delays, extra_change):
    c = json.load(open(os.path.join(DK, src)))
    old0 = c["author"]["poses"].get("0")
    c["move"] = name
    c["anim"] = c["anim"] + "K5"
    c["source_anim"] = c["source_anim"] + "K5"
    c["author"]["take"] = c["author"]["take"] + "K5"
    keep0 = {"0": old0} if name.endswith("startup_k5") else {}
    c["author"]["poses"] = dict(keep0, **poses)
    c["part_delays"] = delays
    c["_changes"] = ("k5 (2026-11-21, owner on the r4 videos): " + extra_change + " Previous: " + str(c.get("_changes", ""))[:300])
    out = os.path.join(DK, name + ".json")
    with open(out, "w") as f:
        json.dump(c, f, indent=1)
    print(out)


write("overlord_dropkick_startup_k5", "overlord_dropkick_startup.json", startup, {"chest": 0, "head": 0, "forearm": 0, "hand": 0},
      "arms snap crossed by f3 (Bison cross, forearms on the chest); tuck to a ball laid back by f7, chamber f11-f13, smear f14, "
      "strike f15 with legs locked and chin up. part_delays 0 so the strike is on f15 (delays put the head 2 f late).")
write("overlord_dropkick_loop_k5", "overlord_dropkick_loop.json", loop, {"chest": 0, "head": 0, "forearm": 0, "hand": 0},
      "smug fall: strike pose held, chest out, chin up, slow pleased head tilt, arms crossed.")
