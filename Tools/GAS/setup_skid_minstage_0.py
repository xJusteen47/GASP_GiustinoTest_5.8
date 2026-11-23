# Skid at the base run (handover hitstop-downed-jab-1, correction 11-18): FGSTN_RunSkidTuning::MinStage is 0-based, default 1 = sprint 1, so a
# reversal at the base run never skidded. Sets MinStage 0 on EVERY locomotion profile (shared system: all test kits). Prints ESITO.
import unreal

reg = unreal.AssetRegistryHelpers.get_asset_registry()
changed, seen, errors = [], 0, []
for ad in reg.get_assets_by_path('/Game', recursive=True):
    if str(ad.asset_class_path.asset_name) != 'GSTN_LocomotionProfile':
        continue
    seen += 1
    path = str(ad.package_name)
    try:
        prof = unreal.EditorAssetLibrary.load_asset(path)
        stages = prof.get_editor_property('run_stages')
        skid = stages.get_editor_property('skid')
        if skid.get_editor_property('min_stage') != 0:
            skid.set_editor_property('min_stage', 0)
            stages.set_editor_property('skid', skid)
            prof.set_editor_property('run_stages', stages)
            unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
            changed.append(path)
    except Exception as e:
        errors.append(f'{path}: {e}')
for e in errors:
    unreal.log_warning('[skid_minstage] ' + e)
print(f'ESITO: {"OK" if not errors and seen else "FAIL"} skid MinStage 0: {seen} profiles, changed {changed}, {len(errors)} errors')
