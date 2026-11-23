"""Dump stock GASP run start curves (phase, enable_warping, movedata_speed, contact_l/r) + root speed at 30 fps, for curves_author
kind start_stock (runstart-v6). Headless. Out Saved/Routing/stock_start_curves.json; prints nothing."""
import json, math
import unreal
AL = unreal.AnimationLibrary
CLIPS = ["/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Lfoot",
         "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Rfoot",
         "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r5",
         "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F"]
NAMES = ["phase", "enable_warping", "movedata_speed", "contact_l", "contact_r"]
out = {}
for p in CLIPS:
    s = unreal.load_asset(p)
    if not s:
        out[p] = "missing"; continue
    L = s.get_play_length(); n = int(round(L * 30)) + 1
    d = {"len": L, "t": [], "root_speed": []}
    opts = unreal.AnimPoseEvaluationOptions()
    prev = None
    for i in range(n):
        t = min(i / 30.0, L); d["t"].append(round(t, 4))
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(s, t, opts)
        r = unreal.AnimPoseExtensions.get_bone_pose(pose, "root", unreal.AnimPoseSpaces.WORLD).translation
        d["root_speed"].append(0 if prev is None else round(math.hypot(r.x - prev.x, r.y - prev.y) * 30, 1)); prev = r
    for nm in NAMES:
        if AL.does_curve_exist(s, nm, unreal.RawCurveTrackTypes.RCT_FLOAT):
            d[nm] = [round(AL.get_float_value_at_time(s, nm, t), 3) for t in d["t"]]
    out[p.rsplit("/", 1)[-1]] = d
json.dump(out, open(unreal.Paths.project_saved_dir() + "Routing/stock_start_curves.json", "w"), indent=0)
