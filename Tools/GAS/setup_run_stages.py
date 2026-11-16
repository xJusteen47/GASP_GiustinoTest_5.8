# Mario-style run P1 (docs/notes/corsa-mario-piano.md): data only.
#   DA_Locomotion_Default  a UGSTN_LocomotionProfile with the default 3 stages (tune it in the editor: speeds, frames, L3 window/mode)
#   every kit (GSTN_PrimaryKitDefinition) under /Game/GASNPPTest/Combat gets a FGSTN_LocomotionKitExtension naming it, unless it has one
# Run AFTER setup_player_input_contexts.py (makes IA_GASNPP_RunBoost on pad L3) and setup_gas_assets.py (input config button + press slot).
# Idempotent: the profile is created once (later runs keep the owner's tuning), a kit with an entry is left alone. Saves .uasset only.
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_run_stages.py"
# Writes Saved/RunStages/setup_run_stages.txt and prints one summary line.
import os

import unreal

FOLDER = '/Game/GASNPPTest/Combat'
PROFILE = FOLDER + '/DA_Locomotion_Default'


def _instanced(struct_value):
    inst = unreal.InstancedStruct()
    inst.import_text('%s%s' % (struct_value.static_struct().get_path_name(), struct_value.export_text()))
    return inst


def _profile(lines):
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(PROFILE):
        lines.append('%s: exists, left alone' % PROFILE)
        return unreal.load_asset(PROFILE)
    folder, name = PROFILE.rsplit('/', 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.GSTN_LocomotionProfile, unreal.DataAssetFactory())
    stages = asset.get_editor_property('run_stages')
    boost = unreal.GameplayTag()
    boost.import_text('(TagName="Command.Movement.RunBoost")')
    stages.set_editor_property('boost_command', boost)
    asset.set_editor_property('run_stages', stages)
    eal.save_loaded_asset(asset, False)
    lines.append('%s: created with the default 3 stages' % PROFILE)
    return asset


def _kits(profile, lines):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    done = 0
    for data in registry.get_assets_by_path(FOLDER, recursive=True):
        kit = data.get_asset()
        if not isinstance(kit, unreal.GSTN_PrimaryKitDefinition):
            continue
        ext = kit.get_editor_property('extensions')
        entries = list(ext.get_editor_property('extension_data'))
        if any('GSTN_LocomotionKitExtension' in e.export_text() for e in entries):
            lines.append('%s: has a locomotion entry, left alone' % kit.get_name())
            continue
        entry = unreal.GSTN_LocomotionKitExtension()
        entry.set_editor_property('profile', profile)
        entries.append(_instanced(entry))
        ext.set_editor_property('extension_data', entries)
        kit.set_editor_property('extensions', ext)
        unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
        lines.append('%s: locomotion entry added' % kit.get_name())
        done += 1
    return done


def run():
    lines = []
    profile = _profile(lines)
    added = _kits(profile, lines)
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'RunStages')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_run_stages.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    summary = 'setup_run_stages: %d kits got the profile; details in Saved/RunStages/setup_run_stages.txt' % added
    unreal.log(summary)
    print(summary)


if __name__ == '__main__':
    run()
