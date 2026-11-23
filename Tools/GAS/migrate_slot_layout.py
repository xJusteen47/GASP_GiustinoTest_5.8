# IO-2c migration: slot set assets filled before IO-2c carry the default buttons but not the layout function of each one, so the kit
# compile cannot move them to the match's button layout. This gives every slot that today's standard or movement builder would make
# (same command, same pattern) its layout functions (UGSTN_StandardSlots.AddLayoutFunctionsToSlotSet) and saves the changed assets.
# Slots a designer changed are left alone. Idempotent: a second run changes and saves nothing. Saves .uasset files only, never maps.
#
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/migrate_slot_layout.py [<report file>]"
# Writes its report to the file (default Saved/IO2c/migrate_slot_layout.txt) and logs one summary line.
import os
import sys

import unreal

STANDARD_AND_MOVEMENT_COMMANDS = (
    'Command.Attack.', 'Command.Strong.', 'Command.Special.', 'Command.Combo.AttackSpecial.', 'Command.Movement.Dash',
    'Command.Defence.Dodge', 'Command.Interact.Grab', 'Command.Interact.PickUp')


def _assets_of(cls):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    path = unreal.TopLevelAssetPath(cls.static_class().get_outer().get_path_name(), cls.static_class().get_name())
    return [data for data in registry.get_assets_by_class(path, True) if str(data.package_name).startswith('/Game/')]


def run(report_path=None):
    report_path = report_path or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'IO2c', 'migrate_slot_layout.txt')
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    lines = []
    migrate = getattr(getattr(unreal, 'GSTN_StandardSlots', None), 'add_layout_functions_to_slot_set', None)
    if not migrate:
        lines.append('ABORT: unreal.GSTN_StandardSlots.add_layout_functions_to_slot_set not found (editor built before IO-2c?)')
    else:
        saved = 0
        for data in _assets_of(unreal.GSTN_InputSlotSet):
            asset = unreal.EditorAssetLibrary.load_asset(str(data.package_name))
            changed = migrate(asset) if asset else 0
            if changed:
                ok = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
                saved += 1 if ok else 0
                lines.append(f'{data.package_name}: {changed} slots got their layout functions, saved={ok}')
            else:
                lines.append(f'{data.package_name}: nothing to migrate')
        # Kits keep their own slots in CommandSet.AddedSlots; this script does not rewrite kits (a kit is edited by hand or by its own
        # setup script), it only reports the ones that hold standard or movement commands so the next thread can decide
        kit_class = getattr(unreal, 'GSTN_PrimaryKitDefinition', None)
        for data in (_assets_of(kit_class) if kit_class else []):
            kit = unreal.EditorAssetLibrary.load_asset(str(data.package_name))
            added = kit.get_editor_property('command_set').get_editor_property('added_slots') if kit else []
            # The tag's name is not exposed to Python: read it from its text form, (TagName="Command...")
            names = [entry.get_editor_property('inline').get_editor_property('command').export_text() for entry in added]
            hits = [n for n in names if any('"' + prefix in n for prefix in STANDARD_AND_MOVEMENT_COMMANDS)]
            if hits:
                lines.append(f'KIT {data.package_name}: added slots with standard/movement commands (not migrated): {", ".join(hits)}')
        lines.append(f'SUMMARY: slot sets saved={saved}')
    with open(report_path, 'w', encoding='utf-8') as out:
        out.write('\n'.join(lines) + '\n')
    unreal.log(f'migrate_slot_layout: {lines[-1]} (report {report_path})')


if __name__ == '__main__':
    run(sys.argv[1] if len(sys.argv) > 1 else None)
