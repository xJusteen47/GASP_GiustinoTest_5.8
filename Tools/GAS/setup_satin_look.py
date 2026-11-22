"""Creates /Game/GSTN/Materials/MI_GSTN_Mannequin_Satin (owner 2026-11-19): child of the mannequin's MI_UEFN_Mannequin_Mover, same colours,
Astro Bot satin metal PBR. The runtime toggle is gstn.Debug.SatinMetal (UGSTN_SatinLookSubsystem copies these four scalars). Idempotent.
Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> . Writes Saved/Satin/setup.txt."""
import os, unreal

PARENT = "/Game/Characters/UEFN_Mannequin/Materials/MI_UEFN_Mannequin_Mover"
FOLDER, NAME = "/Game/GSTN/Materials", "MI_GSTN_Mannequin_Satin"
# Satin metal: partly metallic (0.85 turned the grey into black chrome in the hub, r1 2026-11-19), soft wide highlights (roughness ~0.3)
VALUES = {"Primary Metallic": 0.35, "Primary Roughness": 0.30, "Secondary Metallic": 0.55, "Secondary Roughness": 0.34}
OUT = unreal.Paths.project_saved_dir() + "Satin/setup.txt"
log = []

parent = unreal.load_asset(PARENT)
mel = unreal.MaterialEditingLibrary
for n in VALUES:
    log.append("parent default %s = %s" % (n, mel.get_material_instance_scalar_parameter_value(parent, n)))
path = FOLDER + "/" + NAME
mi = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not mi:
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi, parent)
for n, v in VALUES.items():
    mel.set_material_instance_scalar_parameter_value(mi, n, v)
    log.append("satin %s = %s" % (n, mel.get_material_instance_scalar_parameter_value(mi, n)))
mel.update_material_instance(mi)
log.append("saved %s" % unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
