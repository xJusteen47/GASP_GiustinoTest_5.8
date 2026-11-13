import unreal, json

EAL = unreal.EditorAssetLibrary

def run():
    assets = EAL.list_assets("/Game/AnimPipeline/Rig", recursive=True, include_folder=False)
    str_assets = [str(x) for x in assets]
    return str_assets

if __name__ == "__main__":
    run()
