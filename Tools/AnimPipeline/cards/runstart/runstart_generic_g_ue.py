"""Generic run start g<N> into UE (runstart-generic-desktop, 2026-11-21): headless through Tools/CI/Save-Assets.ps1 (GSTN_SAVE=1).
Env RSGG_TAG (default g1): Saved/AnimPipeline/runstart/RunStart_Generic_<tag>.fbx -> /Game/GSTN/Animation/RunStart/
AS_GSTN_RunStart_Generic_<tag>. Steps as runstart_v4_ue.py (import on the mannequin skeleton, stock loop root motion settings,
ik_foot = feet, Pose Search Block Transition from 0.05 s) but the curves are the STOCK set, not curves_author:
  contact_l, contact_r, phase (stock triangle)  = M_Relaxed_Run_Loop_F's own curves sampled at the composer's warped loop time c(u)
  movedata_speed                                = the clip's own root speed (stock convention: the stock start carries 0 -> 375)
  enable_warping                                = M_Relaxed_Run_Start_F_Lfoot's curve, frame for frame (0 -> 1 like stock)
from generic_<tag>_curves.json (runstart_generic_compose.py) and Saved/RunLoop/export/Relaxed_RunStart_F_L.meta.json.
Report Saved/AnimPipeline/runstart/AS_GSTN_RunStart_Generic_<tag>_build.json; prints <= 4 lines."""
import json
import math
import os

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FOLDER = "/Game/GSTN/Animation/RunStart"
TAG = os.environ.get("RSGG_TAG", "g1")
NAME = "AS_GSTN_RunStart_Generic_" + TAG
SKEL_FROM = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Lfoot"
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")
rep = {}


def imp(fbx, name):
    skel = unreal.load_asset(SKEL_FROM).get_editor_property("skeleton")
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


def cs(pose, bone):
    return unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)


FOLEY_SRC = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F"
FOLEY_TRACKS = {"l": "Footstep Left", "r": "Footstep Right"}
FOLEY_FOOT_MAX_CM = 15.0      # foot bone height (component space) at the plant; the stock plants sit at ~10 cm
FOLEY_MIN_GAP_F = 5           # same foot: no second event closer than this (no double fire on a curve wobble)


def add_foley(seq, curves, poses):
    """Stock GASP footstep foley (owner 2026-11-21, rule 14: stock behaviour). Same notify classes, tracks and event settings as
    M_Relaxed_Run_Loop_F (BP_AnimNotify_FoleyEvent_Run_L/_R on 'Footstep Left'/'Footstep Right', the stock notify objects' own
    properties copied), one event per real plant: the frame where contact_<s> goes 0 -> 1 (stock times sit on that crossing,
    within 1-2 frames), only if the foot is on the ground there. Re-running replaces the tracks, so the import stays reproducible."""
    src = unreal.load_asset(FOLEY_SRC)
    proto = {}
    for s, tr in FOLEY_TRACKS.items():
        evs = AL.get_animation_notify_events_for_track(src, tr)
        proto[s] = evs[0]
    for tr in FOLEY_TRACKS.values():
        if tr in [str(x) for x in AL.get_animation_notify_track_names(seq)]:
            AL.remove_animation_notify_track(seq, tr)
    out = {}
    for s, tr in FOLEY_TRACKS.items():
        AL.add_animation_notify_track(seq, tr, unreal.LinearColor(0.9, 0.6, 0.1, 1.0))
        pev = proto[s]
        pn = pev.get_editor_property("notify")
        c, last, out[s] = curves["contact_" + s], -99, []
        for f in range(1, len(c)):
            if not (c[f - 1] < 0.5 <= c[f]) or f - last < FOLEY_MIN_GAP_F:
                continue
            z = cs(poses[f], "foot_" + s).translation.z
            if z > FOLEY_FOOT_MAX_CM:
                out[s].append({"f": f, "skipped_z": round(z, 1)})
                continue
            n = AL.add_animation_notify_event(seq, tr, f / 30.0, pn.get_class())
            for k in ("event", "side", "volume_multiplier", "pitch_multiplier"):
                try:
                    n.set_editor_property(k, pn.get_editor_property(k))
                except Exception:
                    pass
            last = f
            out[s].append({"f": f, "t": round(f / 30.0, 3), "z": round(z, 1)})
        # stock event settings (threshold, chance, tick type, filter) on the new events of this track
        evs = AL.get_animation_notify_events_for_track(seq, tr)
        for e in evs:
            for k in ("trigger_weight_threshold", "notify_trigger_chance", "trigger_on_dedicated_server", "trigger_on_follower",
                      "notify_filter_type", "notify_filter_lod", "montage_tick_type"):
                try:
                    e.set_editor_property(k, pev.get_editor_property(k))
                except Exception:
                    pass
        out[s + "_events"] = len(evs)
    rep["foley"] = out


if os.environ.get("RSGG_FOLEY_ONLY") == "1":
    # notifies only, on the clip as it is (no re-import): for clips already built from this script (g3)
    seq = unreal.load_asset(FOLDER + "/" + NAME)
    n = int(seq.get_editor_property("number_of_sampled_keys"))
    poses = [unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, unreal.AnimPoseEvaluationOptions()) for f in range(n)]
    cv = {}
    for s in ("l", "r"):
        t, v = AL.get_float_keys(seq, "contact_" + s)
        cv["contact_" + s] = [v[min(range(len(t)), key=lambda i: abs(t[i] - f / 30.0))] for f in range(n)]
    add_foley(seq, cv, poses)
    EAL.save_loaded_asset(seq, False)
    json.dump(rep, open(os.path.join(OUT, NAME + "_foley.json"), "w"), indent=1)
    print("RSGG foley %s: L %s R %s" % (NAME, [e.get("f") for e in rep["foley"]["l"]], [e.get("f") for e in rep["foley"]["r"]]))

else:
    seq = imp(PROJ +"Saved/AnimPipeline/runstart/RunStart_Generic_%s.fbx" % TAG, NAME)
    n = int(seq.get_editor_property("number_of_sampled_keys"))
    rep.update(frames=n, length=round(seq.get_play_length(), 4))
    r0, r1 = AL.get_bone_pose_for_frame(seq, "root", 0, False).translation, AL.get_bone_pose_for_frame(seq, "root", n - 1, False).translation
    rep["root_travel_cm"] = [round(r1.x - r0.x, 1), round(r1.y - r0.y, 1), round(r1.z - r0.z, 1)]
    seq.set_editor_property("enable_root_motion", True)
    seq.set_editor_property("force_root_lock", True)
    seq.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
    # Turnarounds (runstart-turnaround-desktop): RSGG_SRC = meta json of the stock REFACE start the clip replaces
    # (runstart_turn_export_ue.py). The clip comes from Cascadeur in the root frame; the stock root yaw (180/90 -> 0, sampled by
    # the composer on its own timing: info.yaw_ue_deg) goes on the root bone, so the body turns in the component like stock.
    SRCM = os.environ.get("RSGG_SRC", "")
    if SRCM:
        yaw = json.load(open(os.path.join(OUT, "generic_%s_curves.json" % TAG)))["info"]["yaw_ue_deg"]
        tfs = [AL.get_bone_pose_for_frame(seq, "root", f, False) for f in range(n)]
        rep["root_rot_before_deg"] = round(max(abs(t.rotation.rotator().yaw) for t in tfs), 3)
        rot = [unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(yaw[min(f, len(yaw) - 1)])).quaternion() for f in range(n)]
        ctrl = seq.get_editor_property("controller")
        ctrl.open_bracket(unreal.Text("stock reface root yaw"), False)
        ctrl.set_bone_track_keys("root", [t.translation for t in tfs], rot, [t.scale3d for t in tfs], False)
        ctrl.close_bracket(False)
        rep["root_yaw_deg"] = [round(AL.get_bone_pose_for_frame(seq, "root", f, False).rotation.rotator().yaw, 1) for f in (0, 4, 8, 12, 16, 20, 24)]

    opts = unreal.AnimPoseEvaluationOptions()
    poses = [unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts) for f in range(n)]
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
    TRACK = "PoseSearch"
    if TRACK not in [str(x) for x in AL.get_animation_notify_track_names(seq)]:
        AL.add_animation_notify_track(seq, TRACK, unreal.LinearColor(0.2, 0.6, 1.0, 1.0))
        AL.add_animation_notify_state_event(seq, TRACK, 0.05, seq.get_play_length() - 0.05,
                                            unreal.AnimNotifyState_PoseSearchBlockTransition)

    # stock curve set
    cj = json.load(open(os.path.join(OUT, "generic_%s_curves.json" % TAG)))
    meta = json.load(open(os.path.join(PROJ, "Saved", "RunLoop", "export", "Relaxed_RunStart_F_L.meta.json")))
    mc = meta["curves"] if isinstance(meta["curves"], dict) else eval(meta["curves"])
    times = [i / 30.0 for i in range(n)]
    root_x = [AL.get_bone_pose_for_frame(seq, "root", f, False).translation for f in range(n)]
    spd = [0.0] + [math.hypot(root_x[f].x - root_x[f - 1].x, root_x[f].y - root_x[f - 1].y) * 30.0 for f in range(1, n)]
    spd[0] = spd[1]
    curves = {k: cj["curves"][k][:n] for k in ("contact_l", "contact_r", "phase")}
    curves["movedata_speed"] = [round(v, 2) for v in spd]
    ew = list(mc["enable_warping"])
    if SRCM:                                 # the reface clip's own enable_warping, frame for frame (stock keys sampled at 30 fps)
        sk = json.load(open(SRCM))["curves"]["enable_warping"]
        ew = [sk[1][max([i for i, t in enumerate(sk[0]) if t <= f / 30.0 + 1e-4] or [0])] for f in range(n)]
    curves["enable_warping"] = [ew[min(f, len(ew) - 1)] for f in range(n)]
    for name, vals in curves.items():
        if AL.does_curve_exist(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
            AL.remove_curve(seq, name, False)
        AL.add_curve(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        AL.add_float_curve_keys(seq, name, times, [float(v) for v in vals])
    add_foley(seq, curves, poses)
    EAL.save_loaded_asset(seq, False)
    rep["curves"] = sorted(str(x) for x in AL.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT))
    rep["phase_last5"] = curves["phase"][-5:]
    rep["movedata_speed"] = [curves["movedata_speed"][i] for i in (1, 5, 10, 15, 20, 25, n - 1)]
    json.dump(rep, open(os.path.join(OUT, NAME + "_build.json"), "w"), indent=1)
    print("RSGG %s: %d f %.3f s root %s ik drift %.1f" % (NAME, n, rep["length"], rep["root_travel_cm"], drift))
    print("RSGG curves %s speed %s" % (rep["curves"], rep["movedata_speed"]))
    print("RSGG foley L %s R %s" % ([e.get("f") for e in rep["foley"]["l"]], [e.get("f") for e in rep["foley"]["r"]]))
