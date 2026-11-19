"""Bake the skid's turn curve from the skid clip's root yaw into a locomotion profile (anim-setup-2). Headless (-run=pythonscript).

The run skid's body turn (FGSTN_RunSkidTuning::TurnCurve) follows the clip: share of the whole turn (0..1) at each 1/TurnCurveRate s of
skid time. A designer who swaps the skid clip runs this once (or edits the array by hand); nothing else knows the clip's timing.
Env: SKID_CLIP (default SS_180_Turn_Root180), PROFILE (default DA_Locomotion_Default), RATE (default 30).
Result line in Saved/AnimSetup2/skid_turn_curve.txt.
"""
import math, os, unreal

CLIP = os.environ.get("SKID_CLIP", "/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn_Root180")
PROFILE = os.environ.get("PROFILE", "/Game/GASNPPTest/Combat/DA_Locomotion_Default")
RATE = float(os.environ.get("RATE", "30"))
OUT = unreal.Paths.project_saved_dir() + "AnimSetup2/skid_turn_curve.txt"


def root_yaw(seq, t):
    r = unreal.AnimationLibrary.get_bone_pose_for_time(seq, "root", t, False).rotation
    return math.degrees(math.atan2(2 * (r.w * r.z + r.x * r.y), 1 - 2 * (r.y * r.y + r.z * r.z)))


seq = unreal.load_asset(CLIP)
length = unreal.AnimationLibrary.get_sequence_length(seq)
count = int(math.floor(length * RATE + 1e-4)) + 1
yaws, total, prev = [], 0.0, None
for i in range(count):
    y = root_yaw(seq, min(i / RATE, length))
    if prev is not None:
        total += (y - prev + 180.0) % 360.0 - 180.0  # unwrapped: a turn past 180 keeps counting
    prev = y
    yaws.append(total)
end = yaws[-1]
if abs(end) < 1.0:
    raise RuntimeError("clip %s has no root turn (%.1f deg)" % (CLIP, end))
# Share of the whole turn, never going back (a wobble of the root does not turn the capsule back and forth)
curve, best = [], 0.0
for y in yaws:
    best = max(best, min(max(y / end, 0.0), 1.0))
    curve.append(round(best, 4))
curve[-1] = 1.0

profile = unreal.load_asset(PROFILE)
stages = profile.get_editor_property("run_stages")
skid = stages.get_editor_property("skid")
skid.set_editor_property("turn_curve", curve)
skid.set_editor_property("turn_curve_rate", RATE)
# The side of a near-reverse is the clip's side too (plan item 2: the designer never types it): +yaw = Right
skid.set_editor_property("turn_side", unreal.GSTN_SkidTurnSide.RIGHT if end > 0 else unreal.GSTN_SkidTurnSide.LEFT)
stages.set_editor_property("skid", skid)
profile.set_editor_property("run_stages", stages)
saved = unreal.EditorAssetLibrary.save_loaded_asset(profile, False)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("clip %s side %s turn %.1f deg, %d samples at %g Hz, saved %s: %s\n" % (CLIP, "Right" if end > 0 else "Left", end, len(curve), RATE, saved, curve))
