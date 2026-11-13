# Editor side of ingame_judge.py (run through UE_MCP_Bridge run_python_file): one montage per built move, next to it.
import unreal, json


def montage(anim_path):
    """AM_<name> from AS_<name> (default slot), created once; returns its path."""
    anim = unreal.load_asset(anim_path)
    folder, name = anim_path.rsplit("/", 1)
    path = f"{folder}/AM_{name[3:]}"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        fac = unreal.AnimMontageFactory()
        fac.set_editor_property("source_animation", anim)
        fac.set_editor_property("target_skeleton", anim.get_editor_property("skeleton"))
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(f"AM_{name[3:]}", folder, unreal.AnimMontage, fac)
        unreal.EditorAssetLibrary.save_asset(path)
    m = unreal.load_asset(path)
    return json.dumps({"montage": path, "length": m.get_play_length()})
