# Camera anchor setup (owner 2026-11-17: the camera shakes up and down on the dair stomp and on knockdown). Optional: without the asset the
# camera anchor uses its C++ defaults (GSTN_CameraAnchor.h). Creates /Game/Blueprints/Data/DA_CameraAnchor_Default with those defaults so the
# vertical damping can be tuned as data. Idempotent: an existing asset is left as it is (its values are the designer's).
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GASNPPTest/camera_anchor_setup.py" -unattended -nullrhi
import unreal

DA_DIR = '/Game/Blueprints/Data'
DA_NAME = 'DA_CameraAnchor_Default'
path = f'{DA_DIR}/{DA_NAME}'

if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.log(f'[camera_anchor_setup] {path} exists, left untouched')
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.GSTN_CameraAnchorSettingsAsset)
    asset = tools.create_asset(DA_NAME, DA_DIR, unreal.GSTN_CameraAnchorSettingsAsset, factory)
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log(f'[camera_anchor_setup] created {path}')
    else:
        unreal.log_error(f'[camera_anchor_setup] could not create {path}')
