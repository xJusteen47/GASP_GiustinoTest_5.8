# Runs the data validators on the M6 defence and link test content (plan F13) and prints their findings, the way validate_combat_content.py does for
# the combat content: the plugin content under /GSTNDefence/Test and /GSTNLink/Test (rules, link types, ability Blueprints), the project test kits of
# Tools/GAS/setup_defence_link_content.py and the standard rules that carry the defence and link steps.
#
# Headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/validate_defence_content.py" -stdout
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import validate_defence_content as v; v.run()
#
# The same IsDataValid output the Content Browser shows (right click > Asset Actions > Validate Assets). A folder whose plugin is disabled is skipped
# (said in the log). Exit status of the commandlet is not touched; the last line says "validation: N assets, E errors, W warnings" so a script can
# grep it, and a missing asset of the expected list counts as an error. The kits are copies of KIT_Test_Hits: a message KIT_Test_Hits itself has
# (validated first, as the baseline) is listed as inherited and not counted, so only what the defence and link content adds is.
import unreal

PLUGIN_FOLDERS = {'/GSTNDefence/Test': 'GSTN_DefenceRules', '/GSTNLink/Test': 'GSTN_LinkTypeDefinition'}
PROJECT_FOLDER = '/Game/GASNPPTest/Combat'
KITS = ['KIT_Test_Defence', 'KIT_Test_DefencePvE', 'KIT_Test_DefenceLanding', 'KIT_Test_DefenceTech', 'KIT_Test_Link', 'KIT_Test_DefenceLink']
STANDARD_RULES = '/GSTNCombat/Rules/GSTN_CombatRules_Standard'
EXPECTED = {
    '/GSTNDefence/Test': ['DA_DefenceRules_Test', 'DA_DefenceRules_TestPvE', 'GA_Shield_Test', 'GA_CounterTest', 'GA_TechInPlace_Test', 'GA_TechRoll_Test',
                          'GA_TechWall_Test', 'GA_GroundBounce_Test', 'GA_GetupRoll_Test', 'GA_GetupNeutral_Test'],
    '/GSTNLink/Test': ['DA_LinkType_Grab_Test', 'DA_LinkType_Latch_Test', 'GA_Grab_Test', 'GA_Pummel_Test', 'GA_Release_Test', 'GA_ThrowForward_Test',
                       'GA_ThrowBack_Test', 'GA_ThrowUp_Test', 'GA_ThrowDown_Test', 'GA_LatchAttack_Test'],
}


def _validate(subsystem, data):
    settings = unreal.ValidateAssetsSettings()
    settings.set_editor_property('skip_excluded_directories', False)
    result = subsystem.validate_assets_with_settings([data], settings)
    if isinstance(result, tuple):
        result = next(x for x in result if hasattr(x, 'get_editor_property'))
    invalid, warnings = result.get_editor_property('num_invalid'), result.get_editor_property('num_warnings')
    unreal.log(f'[validate] {data.package_name}: {invalid} invalid, {warnings} warnings')
    return invalid, warnings


SOURCE_KIT = 'KIT_Test_Hits'


def _messages(subsystem, asset):
    """(errors, warnings) of one object as text, or None when this engine does not expose the per-object call to Python"""
    for call in (lambda: subsystem.is_object_valid(asset, unreal.DataValidationUsecase.MANUAL), lambda: subsystem.is_object_valid(asset)):
        try:
            r = call()
        except Exception:
            continue
        lists = [x for x in (r if isinstance(r, tuple) else ()) if isinstance(x, (list, unreal.Array))]
        if len(lists) >= 2:
            return [str(t) for t in lists[0]], [str(t) for t in lists[1]]
    return None


def _validate_kits(subsystem, kits):
    """Each kit's messages less the baseline's (KIT_Test_Hits). Returns (errors, warnings) added by the content"""
    base = _messages(subsystem, unreal.load_asset(f'{PROJECT_FOLDER}/{SOURCE_KIT}'))
    if base is None:
        unreal.log_warning('[validate] per-object messages not available: kits validated by asset counts, the baseline subtracted')
        b_inv, b_warn = _validate(subsystem, unreal.EditorAssetLibrary.find_asset_data(f'{PROJECT_FOLDER}/{SOURCE_KIT}'))
        totals = [_validate(subsystem, k) for k in kits]
        return sum(max(i - b_inv, 0) for i, _ in totals), sum(max(w - b_warn, 0) for _, w in totals)
    for text in base[0] + base[1]:
        unreal.log(f'[validate] baseline {SOURCE_KIT}: {text}')
    errors = warnings = 0
    for data in kits:
        e, w = _messages(subsystem, data.get_asset())
        new_e, new_w = [t for t in e if t not in base[0]], [t for t in w if t not in base[1]]
        for text in new_e:
            unreal.log_error(f'[validate] {data.package_name}: {text}')
        for text in new_w:
            unreal.log_warning(f'[validate] {data.package_name}: {text}')
        unreal.log(f'[validate] {data.package_name}: {len(new_e)} errors, {len(new_w)} warnings ({len(e) - len(new_e)} errors and '
                   f'{len(w) - len(new_w)} warnings inherited from {SOURCE_KIT})')
        errors += len(new_e)
        warnings += len(new_w)
    return errors, warnings


def run():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    subsystem = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    targets, errors, warnings = [], 0, 0
    for folder, marker in PLUGIN_FOLDERS.items():
        if not hasattr(unreal, marker):
            unreal.log(f'[validate] {folder}: skipped, its plugin is not enabled')
            continue
        registry.scan_paths_synchronous([folder], True)
        found = {str(d.asset_name): d for d in registry.get_assets_by_path(folder, recursive=True)}
        for name in EXPECTED[folder]:
            if name in found:
                targets.append(found[name])
            else:
                unreal.log_error(f'[validate] {folder}/{name}: missing (run Tools/GAS/setup_defence_link_content.py)')
                errors += 1
    registry.scan_paths_synchronous([PROJECT_FOLDER], True)
    found = {str(d.asset_name): d for d in registry.get_assets_by_path(PROJECT_FOLDER, recursive=False)}
    needs = {'KIT_Test_Link': ('GSTN_LinkTypeDefinition',), 'KIT_Test_DefenceLink': ('GSTN_DefenceRules', 'GSTN_LinkTypeDefinition')}
    kits = []
    for name in KITS:
        if not all(hasattr(unreal, m) for m in needs.get(name, ('GSTN_DefenceRules',))):
            unreal.log(f'[validate] {name}: skipped, its plugin is not enabled')
        elif name in found:
            kits.append(found[name])
        else:
            unreal.log_error(f'[validate] {PROJECT_FOLDER}/{name}: missing (run Tools/GAS/setup_defence_link_content.py)')
            errors += 1
    registry.scan_paths_synchronous(['/GSTNCombat/Rules'], True)
    targets.append(unreal.EditorAssetLibrary.find_asset_data(STANDARD_RULES))
    for data in targets:
        e, w = _validate(subsystem, data)
        errors += e
        warnings += w
    if kits:
        e, w = _validate_kits(subsystem, kits)
        errors += e
        warnings += w
    unreal.log(f'validation: {len(targets) + len(kits)} assets, {errors} errors, {warnings} warnings')
    return errors


if __name__ == '__main__':
    run()
