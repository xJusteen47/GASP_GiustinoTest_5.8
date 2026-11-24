"""Bake the skid's turn curve from the skid clip's root yaw into a locomotion profile (anim-setup-2). Headless (-run=pythonscript).

The run skid's body turn (FGSTN_RunSkidTuning::TurnCurve) follows the clip: share of the whole turn (0..1) at each 1/TurnCurveRate s of
skid time. A designer who swaps the skid clip runs this once (or edits the array by hand); nothing else knows the clip's timing.
Env: SKID_CLIP (default SS_180_Turn_Root180), PROFILE (default DA_Locomotion_Default), RATE (default 30).
Result line in Saved/AnimSetup2/skid_turn_curve.txt.
skid-fix-1: also bakes RootTrack (the clip root path, cm, X along the clip's first-0.2-s motion = the old run direction, Y to its right),
so the skid capsule follows the clip root to the clip's end instead of the stick. SKID_ROOT_TRACK=0 clears it (old brake + exit rule).
"""
import math, os, unreal

CLIP = os.environ.get("SKID_CLIP", "/Game/GSTN/Animation/Ported57/SkidOwner/SS_180_Turn_Root180")
PROFILE = os.environ.get("PROFILE", "/Game/GSTN/Data/Locomotion/DA_Locomotion_Default")
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
track, fwd = [], None
if os.environ.get("SKID_ROOT_TRACK", "1") == "1":
    def root_xy(t):
        v = unreal.AnimationLibrary.get_bone_pose_for_time(seq, "root", min(t, length), False).translation
        return v.x, v.y
    x0, y0 = root_xy(0.0)
    x1, y1 = root_xy(min(0.2, length))
    n = math.hypot(x1 - x0, y1 - y0)
    if n < 1.0:
        raise RuntimeError("clip %s root does not move in its first 0.2 s (%.2f cm)" % (CLIP, n))
    fwd = ((x1 - x0) / n, (y1 - y0) / n)
    right = (-fwd[1], fwd[0])   # +90 yaw from forward (UE: X turns to Y)
    for i in range(count):
        x, y = root_xy(i / RATE)
        dx, dy = x - x0, y - y0
        track.append(unreal.Vector2D(round(dx * fwd[0] + dy * fwd[1], 2), round(dx * right[0] + dy * right[1], 2)))
skid.set_editor_property("root_track", track)
stages.set_editor_property("skid", skid)
profile.set_editor_property("run_stages", stages)
saved = unreal.EditorAssetLibrary.save_loaded_asset(profile, False)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("clip %s side %s turn %.1f deg, %d samples at %g Hz, saved %s: %s\nroot track fwd %s, %d samples: %s\n" % (
    CLIP, "Right" if end > 0 else "Left", end, len(curve), RATE, saved, curve, fwd, len(track), [(t.x, t.y) for t in track]))
