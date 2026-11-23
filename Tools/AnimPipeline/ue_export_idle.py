"""Editor-side: export an AnimSequence to FBX (AssetExportTask). On M_Neutral_Stand_Idle_Loop it wrote an empty 2 KB file;
idle_points.py derives the idle rig points from the bone samples instead."""
import unreal, json, os
_DIR = os.environ.get("ANIMPIPELINE_DIR") or os.path.dirname(os.path.abspath(__file__))
def main(asset="/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop", out=_DIR.replace("\\", "/") + "/refs/idle/M_Neutral_Stand_Idle_Loop.fbx"):
    anim = unreal.load_asset(asset)
    t = unreal.AssetExportTask()
    t.object = anim; t.filename = out; t.automated = True; t.prompt = False; t.replace_identical = True
    opt = unreal.FbxExportOption(); opt.set_editor_property("export_morph_targets", False)
    t.options = opt
    ok = unreal.Exporter.run_asset_export_task(t)
    return json.dumps({"ok": ok, "frames": anim.get_editor_property("number_of_sampled_frames") if hasattr(anim, "get_editor_property") else None})
