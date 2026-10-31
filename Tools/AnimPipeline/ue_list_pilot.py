import unreal, json
def main():
    a = unreal.EditorAssetLibrary.list_assets("/Game/AnimPipeline/Pilot", recursive=False)
    sk = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin")
    names = []
    try:
        ref = sk.get_editor_property("reference_skeleton") if sk else None
    except Exception:
        ref = None
    return json.dumps({"assets": [x.split(".")[0] for x in a]})
