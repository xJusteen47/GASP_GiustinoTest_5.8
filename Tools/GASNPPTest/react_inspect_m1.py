import unreal, os
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "react_inspect.txt")
L = []
try:
    for n in ["AS_React_HitStand","AS_React_Tumble","AS_React_Splat","AS_React_GetUp","AS_React_Roll","AS_React_GroundHit","AS_React_LieUp","AS_React_RollBack"]:
        a = unreal.load_asset("/Game/GSTN/Combat/Reactions/" + n)
        info = [n, a.get_class().get_name()]
        for p in ("skeleton","retarget_source","additive_anim_type","enable_root_motion","sequence_length"):
            try: info.append(f"{p}={a.get_editor_property(p)}")
            except Exception as e: info.append(f"{p}=ERR")
        try:
            ref = unreal.AnimationLibrary.get_animation_source_file_name if False else None
        except Exception: pass
        try:
            ai = a.get_editor_property("asset_import_data")
            info.append("import=" + str(ai.get_first_filename()) if ai else "import=None")
        except Exception as e: info.append("import=ERR")
        try:
            ctrl = a.get_controller()
            info.append("frames=%d" % ctrl.get_model().get_number_of_frames())
        except Exception: pass
        L.append(" | ".join(info))
    sk = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin")
    L.append("SK bones=" + str(len(sk.get_reference_pose() if hasattr(sk,'get_reference_pose') else [])))
    L.append("SK_class=" + str(sk.get_class().get_name()))
except Exception:
    import traceback; L.append(traceback.format_exc())
open(OUT,"w").write("\n".join(L))
unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
