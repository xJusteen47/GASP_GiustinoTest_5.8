import unreal

def run(name="GanonFTiltV31_Build_VarD"):
    full = name if name.startswith("/Game/") else f"/Game/AnimPipeline/Rig/LS_{name}"
    seq = unreal.load_asset(full)
    if seq:
        LSE = unreal.LevelSequenceEditorBlueprintLibrary
        LSE.open_level_sequence(seq)
        LSE.refresh_current_level_sequence()
        LSE.set_current_time(0)
        return {"opened": seq.get_path_name()}
    return {"error": f"Asset not found: {full}"}
