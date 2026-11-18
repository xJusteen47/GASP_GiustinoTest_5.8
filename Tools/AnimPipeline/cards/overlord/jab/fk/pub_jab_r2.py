# Headless round 2: write the FK jab takes into AS_OverlordJab{1,2}V3 (r1 backups exist), jab 2 root lock AnimFirstFrame,
# jab 1 montage blend-in, and report root-motion settings + sampled feet. Results -> pub_r2_result.txt
import json, unreal, traceback
S = r"C:/Users/jacop/AppData/Local/Temp/claude/8c81644554604783/4d8340e3-7023-46c2-bdb4-eb4fbf90299f/scratchpad/jab"
OUT = S + "/pub_r2_result.txt"
eal = unreal.EditorAssetLibrary
lines = []

def bake_ik_feet(anim):
    # GASP's ABP places the feet on ik_foot_l/r (Tools/GAS/setup_overlord_dair_kit.py _bake_ik_feet): bake them from foot_l/r every frame
    opts = unreal.AnimPoseEvaluationOptions()
    count = int(round(anim.get_play_length() * 60)) + 1
    keys = {s: ([], [], []) for s in "lr"}
    for i in range(count):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, i / 60.0, opts)
        root = unreal.AnimPoseExtensions.get_bone_pose(pose, "ik_foot_root", unreal.AnimPoseSpaces.WORLD)
        for s in "lr":
            foot = unreal.AnimPoseExtensions.get_bone_pose(pose, f"foot_{s}", unreal.AnimPoseSpaces.WORLD)
            loc = unreal.MathLibrary.make_relative_transform(foot, root)
            keys[s][0].append(loc.translation); keys[s][1].append(loc.rotation); keys[s][2].append(unreal.Vector(1, 1, 1))
    ctrl = anim.get_editor_property("controller")
    ctrl.open_bracket("Bake ik_foot bones from the feet", False)
    for s in "lr":
        b = f"ik_foot_{s}"
        ctrl.remove_bone_track(b, False); ctrl.add_bone_curve(b, False)
        ctrl.set_bone_track_keys(b, keys[s][0], keys[s][1], keys[s][2], False)
    ctrl.close_bracket(False)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, 9 / 60.0, opts)
    return [round(unreal.AnimPoseExtensions.get_bone_pose(pose, f"ik_foot_{s}", unreal.AnimPoseSpaces.WORLD).translation.distance(
        unreal.AnimPoseExtensions.get_bone_pose(pose, f"foot_{s}", unreal.AnimPoseSpaces.WORLD).translation), 2) for s in "lr"]
def safe(f):
    try: return f()
    except Exception as e: return "ERR " + str(e)[:120]
try:
    for n, rm in ((1, False), (2, True)):
        game = f"/Game/GASNPPTest/Combat/Overlord/AS_OverlordJab{n}V3"
        bk = f"/Game/AnimPipeline/Backup/AS_OverlordJab{n}V3_r2pre_2026-11-18"
        if not eal.does_asset_exist(bk):
            eal.duplicate_asset(game, bk); eal.save_asset(bk, only_if_is_dirty=False)
        d = json.load(open(f"{S}/sheets/raw/AS_OverlordJab{n}FK_tracks.json"))
        anim = eal.load_asset(game); ctrl = anim.get_editor_property("controller")
        k = int(d["keys"])
        ctrl.open_bracket(unreal.Text("GSTN FK jab r2"), True)
        ctrl.set_number_of_frames(unreal.FrameNumber(k - 1), True)
        ok = 0
        for b, keys in d["bones"].items():
            pos = [unreal.Vector(x[0], x[1], x[2]) for x in keys]
            rot = [unreal.Quat(x[3], x[4], x[5], x[6]) for x in keys]
            sc = [unreal.Vector(1, 1, 1) for _ in keys]
            ok += bool(ctrl.set_bone_track_keys(unreal.Name(b), pos, rot, sc, True))
        ctrl.close_bracket(True)
        anim.set_editor_property("enable_root_motion", rm)
        if rm:
            anim.set_editor_property("force_root_lock", False)
            anim.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
        gap = bake_ik_feet(anim)
        eal.save_asset(game, only_if_is_dirty=False)
        lines.append(f"ik gap a9 {gap}")
        lines.append(f"jab{n}: {ok}/{len(d['bones'])} tracks, len {anim.get_play_length():.3f} s, rm {anim.get_editor_property('enable_root_motion')}, lock {anim.get_editor_property('root_motion_root_lock')}")
        # sample feet/root in component space at a few frames (as authored, root track included)
        for f in ((0, 3, 6, 9, 11, 24) if n == 1 else (0, 2, 5, 8, 9, 12)):
            t = f / 60.0
            row = []
            for b in ("root", "pelvis", "foot_l", "foot_r"):
                tr = unreal.AnimationLibrary.get_bone_pose_for_time(anim, b, t, True)
                row.append(f"{b} {tr.translation.x:.1f},{tr.translation.y:.1f},{tr.translation.z:.1f}")
            lines.append(f"  a{f}: " + " | ".join(row))
    for n in (1, 2):
        am = eal.load_asset(f"/Game/GASNPPTest/Combat/Overlord/AM_OverlordJab{n}V3")
        bi = am.get_editor_property("blend_in"); bo = am.get_editor_property("blend_out")
        lines.append(f"AM jab{n}: len {am.get_play_length():.3f} blend_in {bi.get_editor_property('blend_time'):.3f} blend_out {bo.get_editor_property('blend_time'):.3f} bo_trigger {am.get_editor_property('blend_out_trigger_time'):.3f} rm {safe(lambda: am.has_root_motion())}")
        if n == 1 and bi.get_editor_property("blend_time") < 0.05:
            bi.set_editor_property("blend_time", 0.1); am.set_editor_property("blend_in", bi)
            eal.save_asset(am.get_path_name().split(".")[0], only_if_is_dirty=False)
            lines.append("  AM jab1 blend_in -> 0.100 s")
    for p in ("/Game/Blueprints/SandboxCharacter_Mover_ABP", "/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion"):
        bp = eal.load_asset(p)
        cdo = unreal.get_default_object(bp.generated_class()) if bp else None
        lines.append(f"{p.split('/')[-1]} root_motion_mode {safe(lambda: cdo.get_editor_property('root_motion_mode'))}")
except Exception:
    lines.append("ERROR " + traceback.format_exc()[-900:])
open(OUT, "w").write("\n".join(lines) + "\n")
