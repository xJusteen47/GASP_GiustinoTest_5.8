"""Editor-side: save /Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V1 (and list other dirty packages, never saved here)."""
import unreal, json
def main():
    ok = unreal.EditorAssetLibrary.save_asset("/Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V1", only_if_is_dirty=False)
    return json.dumps({"saved": ok, "dirty": [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]})
