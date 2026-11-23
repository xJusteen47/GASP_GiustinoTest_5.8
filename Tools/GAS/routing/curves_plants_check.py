"""Foot plants vs contact curves on a clip (runstart-v6, coordinator check): per 60 fps frame, ball_l/r height above its clip
minimum and horizontal speed, contact_l/r and phase curves. Env PLANTS_CLIP. Out Saved/Routing/plants_<name>.json. Headless."""
import json, math, os
import unreal
AL = unreal.AnimationLibrary
p = os.environ.get("PLANTS_CLIP", "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r5c")
s = unreal.load_asset(p); L = s.get_play_length(); n = int(round(L * 60)) + 1
opts = unreal.AnimPoseEvaluationOptions(); rows = []
for i in range(n):
    t = min(i / 60.0, L)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(s, t, opts)
    g = lambda b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
    r = {"f": i}
    for side in "lr":
        b = g("ball_" + side); r["b" + side] = [b.x, b.y, b.z]
    for c in ("contact_l", "contact_r", "phase"):
        if AL.does_curve_exist(s, c, unreal.RawCurveTrackTypes.RCT_FLOAT):
            r[c] = round(AL.get_float_value_at_time(s, c, t), 3)
    rows.append(r)
json.dump(rows, open(unreal.Paths.project_saved_dir() + "Routing/plants_" + p.rsplit("/", 1)[-1] + ".json", "w"))
