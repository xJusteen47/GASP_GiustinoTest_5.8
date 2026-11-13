"""Creates the two starter backpack data assets (M11 L7b), idempotent: an asset that exists is left alone.

Run in the editor (Python console or `py Tools/GAS/create_backpack_assets.py`). The classes fill everything (identity tag, movement or the
dome move slot), so nothing is wired by hand; tune the numbers in the assets. Prints one line per asset.
"""

import unreal

FOLDER = "/Game/GAS/Backpacks"
ASSETS = {
    "DA_Backpack_JetPack": "/Script/GSTNBackpackGAS.GSTN_BackpackDefinition_JetPack",
    "DA_Backpack_ShieldDome": "/Script/GSTNBackpackGAS.GSTN_BackpackDefinition_ShieldDome",
}


def ensure(name, class_path):
    full = f"{FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        return f"{name}: exists"
    cls = unreal.load_class(None, class_path)
    if cls is None:
        return f"{name}: class {class_path} not found (is GSTNBackpack enabled and built?)"
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, cls, factory)
    if asset is None:
        return f"{name}: create failed"
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return f"{name}: created"


for asset_name, path in ASSETS.items():
    unreal.log(ensure(asset_name, path))
