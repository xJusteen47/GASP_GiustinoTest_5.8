"""Run stop v2: write the pose-based stop clips into Unreal and point the locomotion profile at them (headless editor).
Input: Tools/AnimPipeline/sheets/raw/AS_RunStop_Ganon_<L|R>_V1_tracks.json (cards/runstop: run_stop_gen.py -> ue_write_take.py --no-write
-> run_stop_root.py). Steps: ue_take_writer.write into /Game/AnimPipeline/Pilot/ (a copy of the pilot template), then a copy in the
versioned folder /Game/GSTN/Animation/RunStop/ with the GASP stop clips' root motion settings (root motion on, root locked to the
ref pose); then DA_AnimSet_Locomotion_Default.RunStop: ClipLeftFoot/RightFoot = the copies, SettleRate 1 (the settle is authored at
real time), SettleClipSeconds = the clip after its stop. Mode is NOT changed (stays Off until a review PASS; captures use the cvar).
Report: Saved/RunStop2/write.txt. Env RUNSTOP_VERSION (default V1)."""
import json, os, sys, traceback
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
TOOLS = os.path.join(PROJ, "Tools", "AnimPipeline")
sys.path.insert(0, TOOLS)
OUT = os.path.join(PROJ, "Saved", "RunStop2", "write.txt")
VER = os.environ.get("RUNSTOP_VERSION", "V1")
TEMPLATE = "/Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V2"
FINAL = "/Game/GSTN/Animation/RunStop/"
DA = "/Game/GSTN/Animation/Layers/DA_AnimSet_Locomotion_Default"
log = []


def p(*a):
    log.append(" ".join(str(x) for x in a))


try:
    import ue_take_writer as W
    eal = unreal.EditorAssetLibrary
    finals = {}
    for side in "LR":
        name = f"AS_RunStop_Ganon_{side}_{VER}"
        src = os.path.join(TOOLS, "sheets", "raw", f"{name}_tracks.json").replace("\\", "/")
        # the pilot copy must not share the final clip's name (an ABP node picked by name took the unversioned pilot copy)
        pilot = "/Game/AnimPipeline/Pilot/AS_Pilot_RunStop_Ganon_" + side + "_" + VER
        old = "/Game/AnimPipeline/Pilot/" + name
        if eal.does_asset_exist(old):
            p(side, "deleted old pilot copy", old, eal.delete_asset(old))
        p(side, "pilot write:", W.write(src, TEMPLATE, pilot))
        dest = FINAL + name
        if not eal.does_asset_exist(dest):
            eal.duplicate_asset(pilot, dest)
            seq = eal.load_asset(dest)
        else:
            # rewrite in place: the DA and the ABP keep their references
            seq = eal.load_asset(dest)
            d = json.load(open(src)); ctrl = seq.get_editor_property("controller")
            ctrl.open_bracket(unreal.Text("GSTN run stop rewrite"), True)
            ctrl.set_number_of_frames(unreal.FrameNumber(int(d["keys"]) - 1), True)
            for b, keys in d["bones"].items():
                ctrl.set_bone_track_keys(unreal.Name(b), [unreal.Vector(*k[:3]) for k in keys], [unreal.Quat(*k[3:7]) for k in keys],
                                         [unreal.Vector(1.0, 1.0, 1.0) for _ in keys], True)
            ctrl.close_bracket(True)
        seq.set_editor_property("enable_root_motion", True)
        seq.set_editor_property("force_root_lock", True)
        seq.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
        eal.save_asset(dest, only_if_is_dirty=False)
        n = unreal.AnimationLibrary.get_num_frames(seq)
        r0 = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, "root", 0, False).translation
        r1 = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, "root", n, False).translation
        p(side, dest, "frames", n, "len", round(seq.get_play_length(), 4), "root y", round(r0.y, 1), "->", round(r1.y, 1))
        finals[side] = seq
    da = eal.load_asset(DA)
    rs = da.get_editor_property("run_stop")
    meta = json.load(open(os.path.join(TOOLS, "cards", "runstop", "run_stop_L_root.json")))
    settle = round((len(meta["root_y"]) - 1 - (meta["stop"] + 1)) / meta["fps"], 3)
    rs.set_editor_property("clip_left_foot", finals["L"])
    rs.set_editor_property("clip_right_foot", finals["R"])
    rs.set_editor_property("settle_rate", 1.0)
    rs.set_editor_property("settle_clip_seconds", settle)
    da.set_editor_property("run_stop", rs)
    eal.save_asset(DA, only_if_is_dirty=False)
    rs = eal.load_asset(DA).get_editor_property("run_stop")
    p("DA run_stop:", rs.get_editor_property("mode"), rs.get_editor_property("clip_left_foot"), rs.get_editor_property("clip_right_foot"),
      rs.get_editor_property("settle_rate"), rs.get_editor_property("settle_clip_seconds"))
    p("OK")
except Exception:
    p("ERROR", traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
unreal.SystemLibrary.quit_editor()
