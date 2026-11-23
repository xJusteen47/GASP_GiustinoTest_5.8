"""Run start v3 into UE (runstart-v3-1). GUI editor only (boot runstart_v3_boot_vd2.py under Tools/CI/Run-Timed.ps1 -Preset Capture).
  1. imports Saved/AnimPipeline/runstart/RunStart_v3.fbx as /Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v3 (Frantic untouched, rule 13b)
     and RunStart_v3base.fbx (the un-restyled splice) as a temp asset, deleted after the check;
  2. Cascadeur round-trip check: local bone rotations of v3base f0-7 (= Frantic f0-7 spliced) and v3 f0 against AS_GSTN_RunStart_Frantic.
     Median > RT_LIMIT deg = the known 8-11 deg offset: the script STOPS (no wiring), the diff fix is a second attempt;
  3. root motion: travel on root -> EnableRootMotion + force root lock (as the stock run loop: mesh stays on the capsule);
  4. ik_foot_l/r = the feet (GASP leg IK pulls the feet to them) when they drift > 2 cm;
  5. curves via Tools/GAS/routing/curves_author.py kind 'pivot' (contact_l/r + phase from the clip's own bones; no movedata_speed,
     no enable_warping: see runstart_curves_v3.md "UE");
  6. hand-off phase: v3 f42-45 = M_Neutral_Run_Loop_F at rate 1.0 ending on source frame RUN_SRC_END (runstart_v3_gate.json); the loop
     time at EXIT_AT is written to Saved/AnimPipeline/runstart/v3_build.json for run_start_states.py (checked by a feet match).
Prints <= 5 lines."""
import json
import math
import os
import runpy

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FOLDER = "/Game/GSTN/Animation/RunStart"
NAME, TMP = "AS_GSTN_RunStart_v3", "AS_GSTN_RunStart_v3base_RTCHECK"
FRANTIC = FOLDER + "/AS_GSTN_RunStart_Frantic"
LOOP = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F"
RT_LIMIT = float(os.environ.get("RSV3_RT_LIMIT", "4.0"))
EXIT_AT = float(os.environ.get("RSV3_EXIT_AT", "1.4"))   # f42: cadence and stride back to 1.0 (sheet), only stock loop frames after it
RUN_SRC_END = 56.68                                       # runstart_v3_gate.json compose.run_src_end_frame (loop frame at v3 f45)
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


seq = imp(PROJ + "Saved/AnimPipeline/runstart/RunStart_v3.fbx", NAME)
base = imp(PROJ + "Saved/AnimPipeline/runstart/RunStart_v3base.fbx", TMP)
fr = unreal.load_asset(FRANTIC)
n = nk(seq)
rep.update(frames=n, length=round(seq.get_play_length(), 4), fps=round((n - 1) / max(seq.get_play_length(), 1e-6), 2),
           tracks=len(AL.get_animation_track_names(seq)))

# 2. round trip
rep["rt_v3base_vs_frantic_f0_7"] = rot_diff(base, fr, [(f, f) for f in range(8)])
rep["rt_v3_vs_frantic_f0"] = rot_diff(seq, fr, [(0, 0)])
pf, pb = AL.get_bone_pose_for_frame(fr, "pelvis", 0, False).translation, AL.get_bone_pose_for_frame(base, "pelvis", 0, False).translation
rep["rt_pelvis_f0_cm"] = round(math.dist((pf.x, pf.y, pf.z), (pb.x, pb.y, pb.z)), 2)
EAL.delete_asset(FOLDER + "/" + TMP)
med = rep["rt_v3base_vs_frantic_f0_7"]["median"] or 0.0
rep["rt_offset"] = med > RT_LIMIT
os.makedirs(OUT, exist_ok=True)
if rep["rt_offset"]:
    json.dump(rep, open(os.path.join(OUT, "v3_build.json"), "w"), indent=1)
    print("RSV3 STOP: round-trip offset median %.1f deg (top %s): diff fix needed" % (med, rep["rt_v3base_vs_frantic_f0_7"]["top"][:2]))
    raise RuntimeError("round-trip offset")

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
json.dump(rep, open(os.path.join(OUT, "v3_build.json"), "w"), indent=1)
print("RSV3 import: %d f %.3f s, rt median %.1f deg top %s, pelvis f0 %.1f cm" % (
    n, rep["length"], med, rep["rt_v3base_vs_frantic_f0_7"]["top"][:1], rep["rt_pelvis_f0_cm"]))
print("RSV3 root %s (%s), ik drift %.1f cm" % (rep["root_travel_cm"], rep["root_motion"][:16], drift))
print("RSV3 curves %s onsets L %s R %s" % (rep["curves"], rep["contact_l_onsets_f30"][:4], rep["contact_r_onsets_f30"][:4]))
print("RSV3 hand-off %s" % rep["handoff"])
