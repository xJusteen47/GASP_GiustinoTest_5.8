"""Run start v4 pose round 1 into UE (runstart-v3-pose-1): headless through Tools/CI/Save-Assets.ps1 (GSTN_SAVE=1).
Same steps as runstart_v3_ue.py on Saved/AnimPipeline/runstart/RunStart_v4r1.fbx -> /Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r1
(v3 untouched, rule 13b variant), 69 f at 30 fps = 1.5 s authored + 0.75 s stock run loop tail (stock "almost complete" exit).
No round-trip temp import: f0-11 are bit-identical to v3 (Cascadeur drift 0.001 cm), whose round trip passed (v3_build.json).
Extra: notify state "Pose Search: Block Transition" (UAnimNotifyState_PoseSearchBlockTransition) from BLOCK_FROM s to the end, so the
pose-matched chooser row can only ENTER the clip at its start (Unreal Assistant 2026-11-20: stock way to bound the entry window;
base 1 entered v3 at 0.15 s and lost the wind-up). Report Saved/AnimPipeline/runstart/v4r1_build.json; prints <= 5 lines."""
import json
import math
import os
import runpy

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FOLDER = "/Game/GSTN/Animation/RunStart"
NAME = os.environ.get("RSV4_NAME", "AS_GSTN_RunStart_v4r1")
BLOCK_FROM = float(os.environ.get("RSV4_BLOCK_FROM", "0.05"))
FRANTIC = FOLDER + "/AS_GSTN_RunStart_Frantic"
LOOP = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F"
RT_LIMIT = float(os.environ.get("RSV3_RT_LIMIT", "4.0"))
EXIT_AT = float(os.environ.get("RSV3_EXIT_AT", "1.4"))   # f42: cadence and stride back to 1.0 (sheet), only stock loop frames after it
RUN_SRC_END = 79.68                                       # runstart_compose_casc_v4.py src_end (loop frame at v4 f68)
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")
rep = {}


def imp(fbx, name):
    skel = unreal.load_asset(FRANTIC).get_editor_property("skeleton")
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("skeleton", skel)
    ad = ui.get_editor_property("anim_sequence_import_data")
    ad.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    ad.set_editor_property("import_bone_tracks", True)
    t = unreal.AssetImportTask()
    for k, v in (("filename", fbx), ("destination_path", FOLDER), ("destination_name", name), ("automated", True),
                 ("replace_existing", True), ("save", True), ("options", ui)):
        t.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return unreal.load_asset(FOLDER + "/" + name)


def nk(a):
    return int(a.get_editor_property("number_of_sampled_keys"))


def qang(a, b):
    d = abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)
    return math.degrees(2 * math.acos(min(1.0, d)))


def rot_diff(a, b, frames):
    bones = sorted(set(str(x) for x in AL.get_animation_track_names(a)) & set(str(x) for x in AL.get_animation_track_names(b)))
    per = []
    for bone in bones:
        if bone.startswith(("ik_", "interaction", "center_of_mass", "VB ")):
            continue
        d = [qang(AL.get_bone_pose_for_frame(a, bone, fa, False).rotation, AL.get_bone_pose_for_frame(b, bone, fb, False).rotation)
             for fa, fb in frames]
        per.append((round(max(d), 2), bone))
    per.sort(reverse=True)
    vals = sorted(x[0] for x in per)
    return {"median": vals[len(vals) // 2] if vals else None, "top": per[:4], "bones": len(per)}


def cs(pose, bone):
    return unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)


seq = imp(PROJ + "Saved/AnimPipeline/runstart/" + os.environ.get("RSV4_FBX", "RunStart_v4r1.fbx"), NAME)
n = nk(seq)
rep.update(frames=n, length=round(seq.get_play_length(), 4), fps=round((n - 1) / max(seq.get_play_length(), 1e-6), 2),
           tracks=len(AL.get_animation_track_names(seq)))
med = 0.0
os.makedirs(OUT, exist_ok=True)

# 3. root motion
r0, r1 = AL.get_bone_pose_for_frame(seq, "root", 0, False).translation, AL.get_bone_pose_for_frame(seq, "root", n - 1, False).translation
p0, p1 = AL.get_bone_pose_for_frame(seq, "pelvis", 0, False).translation, AL.get_bone_pose_for_frame(seq, "pelvis", n - 1, False).translation
rep["root_travel_cm"] = [round(r1.x - r0.x, 1), round(r1.y - r0.y, 1), round(r1.z - r0.z, 1)]
rep["pelvis_local_travel_cm"] = [round(p1.x - p0.x, 1), round(p1.y - p0.y, 1), round(p1.z - p0.z, 1)]
if math.hypot(r1.x - r0.x, r1.y - r0.y) > 50:
    seq.set_editor_property("enable_root_motion", True)
    seq.set_editor_property("force_root_lock", True)
    seq.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
    rep["root_motion"] = "enabled + force root lock (stock loop settings)"
else:
    rep["root_motion"] = "root static: left off (WARNING if pelvis travels)"

# 4. ik_foot = feet
opts = unreal.AnimPoseEvaluationOptions()
poses = [unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0 if rep["fps"] > 29 else f / rep["fps"], opts) for f in range(n)]
names = set(str(x) for x in AL.get_animation_track_names(seq))
drift = 0.0
for pose in poses:
    for s in ("l", "r"):
        a, b = cs(pose, "ik_foot_" + s).translation, cs(pose, "foot_" + s).translation
        drift = max(drift, math.dist((a.x, a.y, a.z), (b.x, b.y, b.z)))
rep["ik_foot_drift_cm"] = round(drift, 2)
if drift > 2.0:
    ctrl = seq.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("ik_foot = feet"), False)
    for s in ("l", "r"):
        bone = "ik_foot_" + s
        if bone not in names:
            ctrl.add_bone_curve(bone, False)
        ks = [cs(pose, "foot_" + s).make_relative(cs(pose, "ik_foot_root")) for pose in poses]
        ctrl.set_bone_track_keys(bone, [k.translation for k in ks], [k.rotation for k in ks], [k.scale3d for k in ks], False)
    ctrl.close_bracket(False)
    p2 = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, 0.3, opts)
    a, b = cs(p2, "ik_foot_l").translation, cs(p2, "foot_l").translation
    rep["ik_foot_fixed_check_cm"] = round(math.dist((a.x, a.y, a.z), (b.x, b.y, b.z)), 2)
# 4b. entry window: Pose Search Block Transition from BLOCK_FROM to the end
TRACK = "PoseSearch"
if TRACK not in [str(x) for x in AL.get_animation_notify_track_names(seq)]:
    AL.add_animation_notify_track(seq, TRACK, unreal.LinearColor(0.2, 0.6, 1.0, 1.0))
ev = AL.add_animation_notify_state_event(seq, TRACK, BLOCK_FROM, seq.get_play_length() - BLOCK_FROM,
                                         unreal.AnimNotifyState_PoseSearchBlockTransition)
rep["block_transition"] = [BLOCK_FROM, round(seq.get_play_length(), 4), type(ev).__name__ if ev else None]
EAL.save_loaded_asset(seq, False)

# 5. curves (curves_author saves the asset)
os.environ["CURVES_CLIPS"] = FOLDER + "/" + NAME + ":pivot"
os.environ.pop("CURVES_DRY", None)
runpy.run_path(PROJ + "Tools/GAS/routing/curves_author.py", run_name="__main__")
times = [i / 60.0 for i in range(int(seq.get_play_length() * 60) + 1)]
for c in ("contact_l", "contact_r"):
    v = [AL.get_float_value_at_time(seq, c, t) for t in times] if AL.does_curve_exist(seq, c, unreal.RawCurveTrackTypes.RCT_FLOAT) else []
    rep[c + "_onsets_f30"] = [round(i / 2, 1) for i in range(len(v)) if v[i] > 0.5 and (i == 0 or v[i - 1] <= 0.5)]
rep["curves"] = sorted(str(x) for x in AL.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT))

# 6. hand-off phase: analytic loop time at EXIT_AT, checked with a root-local feet match on the loop
loop = unreal.load_asset(LOOP)
L = loop.get_play_length()
t_an = ((RUN_SRC_END - (seq.get_play_length() - EXIT_AT) * 30.0) / 30.0) % L


def feet(a, t):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(a, t, opts)
    root = cs(pose, "root")
    out = []
    for bone in ("foot_l", "foot_r"):
        v = root.inverse_transform_location(cs(pose, bone).translation)
        out += [v.x, v.y, v.z]
    return out


f0, f1 = feet(seq, EXIT_AT), feet(seq, EXIT_AT + 1 / 30.0)
best = None
for i in range(int(L * 60)):
    t = i / 60.0
    g0, g1 = feet(loop, t), feet(loop, (t + 1 / 30.0) % L)
    cost = math.sqrt(sum((x - y) ** 2 for x, y in zip(f0, g0))) + 0.5 * math.sqrt(
        sum(((x1 - x0) - (y1 - y0)) ** 2 for x0, x1, y0, y1 in zip(f0, f1, g0, g1)))
    if best is None or cost < best[0]:
        best = (cost, t)
c_an = None
g0, g1 = feet(loop, t_an), feet(loop, (t_an + 1 / 30.0) % L)
c_an = math.sqrt(sum((x - y) ** 2 for x, y in zip(f0, g0))) + 0.5 * math.sqrt(
    sum(((x1 - x0) - (y1 - y0)) ** 2 for x0, x1, y0, y1 in zip(f0, f1, g0, g1)))
rep["handoff"] = {"exit_at": EXIT_AT, "loop_t_analytic": round(t_an, 4), "cost_analytic": round(c_an, 1),
                  "loop_t_match": round(best[1], 4), "cost_match": round(best[0], 1)}
# the analytic phase is the composer's own (same frames); the match only overrides it when clearly better
rep["handoff"]["loop_start"] = rep["handoff"]["loop_t_match"] if best[0] < 0.5 * c_an - 2 else rep["handoff"]["loop_t_analytic"]
json.dump(rep, open(os.path.join(OUT, "v4r1_build.json"), "w"), indent=1)
print("RSV4 import %s: %d f %.3f s, block transition %s" % (NAME, n, rep["length"], rep["block_transition"]))
print("RSV4 root %s (%s), ik drift %.1f cm" % (rep["root_travel_cm"], rep["root_motion"][:16], drift))
print("RSV4 curves %s onsets L %s R %s" % (rep["curves"], rep["contact_l_onsets_f30"][:4], rep["contact_r_onsets_f30"][:4]))
print("RSV4 hand-off %s" % rep["handoff"])
