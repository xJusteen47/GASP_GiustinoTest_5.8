# Editor-side, read-only: is the Live Link AnimSequence dirty (needs a manual save)? Root bone local rotation at frame 0.
import unreal, json

def main():
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    a = unreal.load_asset("/Game/AS_Pilot_JabLiveLink")
    out = {"dirty_content": dirty, "asset_dirty": "/Game/AS_Pilot_JabLiveLink" in dirty,
           "on_disk": unreal.EditorAssetLibrary.does_asset_exist("/Game/AS_Pilot_JabLiveLink")}
    try:
        out["frames"] = a.get_editor_property("number_of_sampled_keys") if hasattr(a, "get_editor_property") else None
    except Exception as e:
        out["frames_err"] = str(e)[:120]
    return json.dumps(out)
