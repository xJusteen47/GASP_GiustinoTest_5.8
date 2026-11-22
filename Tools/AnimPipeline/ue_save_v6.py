# Editor-side: copy the Live Link AnimSequence (v6 streamed from Cascadeur) to the Pilot folder and save ONLY the copy.
import unreal, json
SRC = "/Game/AS_Pilot_JabLiveLink"
DST = "/Game/AnimPipeline/Pilot/AS_Pilot_JabV6"

def main():
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(DST):
        eal.delete_asset(DST)
    a = eal.duplicate_asset(SRC, DST)
    ok = eal.save_asset(DST, only_if_is_dirty=False)
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    return json.dumps({"duplicated": bool(a), "saved": ok, "dst_dirty": DST in dirty, "still_dirty": dirty})
