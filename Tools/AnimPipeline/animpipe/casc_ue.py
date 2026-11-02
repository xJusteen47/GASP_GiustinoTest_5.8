"""Unreal side of the Cascadeur round trip (run through the bridge's run_python_file; see animpipe/cascadeur.py).

export_fbx: an AnimSequence (the card's source take) -> FBX for Cascadeur's import_animation. The take is on the UEFN
mannequin skeleton, the same joint names as Cascadeur's UEFN rig, so the import needs no retarget.
"""
import json
import unreal


def export_fbx(asset, path):
    anim = unreal.load_asset(asset)
    if anim is None:
        return json.dumps({"ok": False, "error": f"no asset {asset}"})
    task = unreal.AssetExportTask()
    task.set_editor_property("object", anim)
    task.set_editor_property("filename", path)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("exporter", unreal.AnimSequenceExporterFBX())
    opt = unreal.FbxExportOption()
    for prop, val in (("collision", False), ("level_of_detail", False), ("export_morph_targets", False),
                      ("export_preview_mesh", True)):
        try:
            opt.set_editor_property(prop, val)
        except Exception:
            pass
    task.set_editor_property("options", opt)
    ok = unreal.Exporter.run_asset_export_task(task)
    return json.dumps({"ok": bool(ok), "path": path, "frames": anim.get_editor_property("number_of_sampled_frames")
                       if hasattr(anim, "number_of_sampled_frames") else None, "length": anim.get_play_length()})
