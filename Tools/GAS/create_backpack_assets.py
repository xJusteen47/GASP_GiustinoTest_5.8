"""Creates the two starter backpack data assets (M11 L7b), idempotent: an asset that exists is left alone.

Run in the editor (Python console or `py Tools/GAS/create_backpack_assets.py`). The classes fill everything (identity tag, movement or the
dome move slot), so nothing is wired by hand; tune the numbers in the assets. Prints one line per asset.

M11 L7c: also puts the BackpackDome step (the dome blocks hits from outside) into the standard combat rules the match plays, right after
TeamFilter, through UGSTN_BackpackContentLibrary.AddDomeStepToRules; a rules asset that already holds it is not touched or saved.
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


STANDARD_RULES = "/GSTNCombat/Rules/GSTN_CombatRules_Standard"


def ensure_dome_step():
    if not unreal.EditorAssetLibrary.does_asset_exist(STANDARD_RULES):
        return f"{STANDARD_RULES}: missing (run setup_gas_assets.py --h13-content first)"
    rules = unreal.load_asset(STANDARD_RULES)
    names = " ".join(str(p) for p in rules.get_editor_property("hit_pipeline"))
    if "HitStep_BackpackDome" in names:
        return f"{STANDARD_RULES}: dome step present"
    if not unreal.GSTN_BackpackContentLibrary.add_dome_step_to_rules(rules):
        return f"{STANDARD_RULES}: dome step not added"
    unreal.EditorAssetLibrary.save_loaded_asset(rules, False)
    return f"{STANDARD_RULES}: dome step added after TeamFilter"


for asset_name, path in ASSETS.items():
    unreal.log(ensure(asset_name, path))
unreal.log(ensure_dome_step())
