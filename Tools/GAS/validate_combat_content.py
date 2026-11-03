# Runs the data validators on the combat test content and prints their findings, the reaction matrices included (M4 H13a).
#
# Headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/validate_combat_content.py" -stdout
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import validate_combat_content as v; v.run()
#
# The same IsDataValid output the Content Browser shows (right click > Asset Actions > Validate Assets): the rules' reaction matrix, the hurtbox
# sets' matrices and warnings, the timelines' hit matrices and the hitstop override check. Exit status of the commandlet is not touched; the
# last line says "validation: N assets, E errors, W warnings" so a script can grep it.
import unreal

FOLDERS = ['/GSTNCombat/Rules', '/Game/GASNPPTest/Combat']
CLASSES = ('GSTN_CombatRules', 'GSTN_HurtboxSetDefinition', 'GSTN_CombatTimeline', 'GSTN_PartSetDefinition', 'GSTN_PenetrationTable')


def run(folders=FOLDERS):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(list(folders), True)
    assets = []
    for folder in folders:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) in CLASSES:
                assets.append(data)
    subsystem = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    errors = warnings = 0
    for data in assets:
        settings = unreal.ValidateAssetsSettings()
        settings.set_editor_property('skip_excluded_directories', False)
        result = subsystem.validate_assets_with_settings([data], settings)
        if isinstance(result, tuple):
            result = next(x for x in result if hasattr(x, 'get_editor_property'))
        unreal.log(f'[validate] {result}')
        unreal.log(f'[validate] {data.get_asset().get_path_name()}: {result.get_editor_property("num_invalid")} invalid, '
                   f'{result.get_editor_property("num_warnings")} warnings')
        errors += result.get_editor_property('num_invalid')
        warnings += result.get_editor_property('num_warnings')
    unreal.log(f'validation: {len(assets)} assets, {errors} errors, {warnings} warnings')
    return errors


if __name__ == '__main__':
    import sys
    # --parts validates the GSTNParts sample content only (P14), plus the combat folders as before
    run(FOLDERS + ['/GSTNParts/Samples'] if '--parts' in sys.argv else FOLDERS)
