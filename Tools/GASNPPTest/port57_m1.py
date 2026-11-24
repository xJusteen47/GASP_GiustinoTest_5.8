"""M1 step 1: move staged 5.7 assets (/Game/GiustinoMoves) into /Game/GSTN/Animation/Ported57, root motion off.
Run inside the editor. Writes Saved/Logs/ci/port57_m1.txt. Never saves maps."""
import unreal, os

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "port57_m1.txt")
os.makedirs(os.path.dirname(OUT), exist_ok=True)
lines = []
def log(s):
    lines.append(str(s))

SRC = "/Game/GiustinoMoves"
DST = "/Game/GSTN/Animation/Ported57"
MOVES = {
    "Skid/Skid_Revised": "Skid/Skid_Revised",
    "Skid/Skid_Revised/Driving_Skid_Revised": "Skid/Driving_Skid_Revised",
    "Skid/SS_180_Turn": "Skid/SS_180_Turn",
    "Skid/SS_180_Turn1": "Skid/SS_180_Turn1",
    "Skid/SS_180_Turn2": "Skid/SS_180_Turn2",
    "WallSlide/WallSlideAnims/WallSlideIdle": "WallSlide/WallSlideIdle",
    "WallSlide/WallSlideAnims/Driving_WallSlideIdle": "WallSlide/Driving_WallSlideIdle",
    "WallSlide/WallSlideAnims/SK_UEFN_Mannequin_WallSlideLeftPose": "WallSlide/SK_UEFN_Mannequin_WallSlideLeftPose",
    "WallSlide/WallSlideAnims/SK_UEFN_Mannequin_WallSlideRightPose": "WallSlide/SK_UEFN_Mannequin_WallSlideRightPose",
    "WallSlide/CR_WallIkControlRig": "WallSlide/CR_WallIkControlRig",
}
try:
    at = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary
    for s, d in MOVES.items():
        a = unreal.load_asset(f"{SRC}/{s}")
        log(f"load {s}: {a.get_class().get_name() if a else None}")
    datas = []
    for s, d in MOVES.items():
        a = unreal.load_asset(f"{SRC}/{s}")
        if a is None:
            continue
        dst_path = f"{DST}/{d}"
        pkg = os.path.dirname(dst_path)
        datas.append(unreal.AssetRenameData(a, pkg, os.path.basename(dst_path)))
    ok = at.rename_assets(datas)
    log(f"rename_assets ok={ok}")
    # redirectors
    try:
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        reg.scan_paths_synchronous([SRC], True)
        reds = [a.get_asset() for a in reg.get_assets_by_path(SRC, recursive=True) if str(a.asset_class_path.asset_name) == "ObjectRedirector"]
        log(f"redirectors {len(reds)}")
        if reds:
            at.fix_up_redirectors(reds)
    except Exception as e:
        log(f"redirector err {e}")
    # settings
    for d in MOVES.values():
        a = unreal.load_asset(f"{DST}/{d}")
        if a is None:
            log(f"MISSING {d}")
            continue
        info = [d, a.get_class().get_name()]
        if isinstance(a, unreal.AnimSequence):
            try:
                a.set_editor_property("enable_root_motion", False)
                info.append("rootmotion=off")
            except Exception as e:
                info.append(f"rm err {e}")
            try:
                sk = a.get_editor_property("skeleton")
                info.append("skel=" + (sk.get_path_name() if sk else "None"))
            except Exception:
                pass
            try:
                cn = unreal.AnimationLibrary.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT)
                info.append("curves=" + ",".join(str(c) for c in cn))
            except Exception as e:
                info.append(f"curves err {e}")
            info.append(f"len={a.get_editor_property('sequence_length'):.3f}")
        log(" | ".join(info))
        eal.save_loaded_asset(a)
    # staging cleanup
    if eal.does_directory_exist(SRC):
        log("delete staging " + str(eal.delete_directory(SRC)))
except Exception as e:
    import traceback
    log("EXC " + traceback.format_exc())
open(OUT, "w").write("\n".join(lines))
unreal.log("port57 done")
unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
