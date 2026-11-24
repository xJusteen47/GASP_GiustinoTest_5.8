"""greybox-capsula 2A (owner 2026-11-24): the Playtest jab gets a reference montage, so the capsule's montage override can show it.

KIT_Test_Playtest's Attack.Neutral is GA_Playtest_GroundHit (Blueprint child of UGSTN_GA_TestHitMove): its timeline had hitboxes only and no
Montage, so nothing presented the move (the driver stood still, the capsule had nothing to follow). This copies the Montage, MontageSection
and MontagePlayRate of the Overlord kit's jab timeline (TL_Overlord_Jab1, setup_overlord_jab_kit.py) into the ability's own timeline. Frames,
hitboxes and cancel windows are left as they are.

Idempotent: a rerun with the montage already set changes nothing. The Blueprint is copied to Saved/Backups/GAS before the first change
(like setup_gas_assets.py). Dry run unless GSTN_SAVE=1 (Tools/CI/Save-Assets.ps1). Report: Saved/Greybox/set_playtest_jab_montage.txt.
"""
import os
import shutil

import unreal

ABILITY = '/Game/GASNPPTest/Combat/Playtest/GA_Playtest_GroundHit'
SOURCE_TIMELINE = '/Game/GASNPPTest/Combat/Overlord/TL_Overlord_Jab1'
SIM_HZ = 62.5
SAVE = os.environ.get('GSTN_SAVE') == '1'
OUT = unreal.Paths.project_saved_dir() + 'Greybox/set_playtest_jab_montage.txt'
lines = []
errors = []


def p(*args):
    lines.append(' '.join(str(a) for a in args))


def backup(asset_path):
    src = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, os.path.basename(src))
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        p('backup', src, '->', dst)


def run():
    p('set_playtest_jab_montage', 'SAVE' if SAVE else 'DRY RUN')
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/GASNPPTest'], True)
    source = unreal.EditorAssetLibrary.load_asset(SOURCE_TIMELINE)
    if not source:
        errors.append('source timeline missing: ' + SOURCE_TIMELINE)
        return
    src = source.get_editor_property('data')
    montage = src.get_editor_property('montage')
    section = src.get_editor_property('montage_section')
    rate = src.get_editor_property('montage_play_rate')
    if not montage:
        errors.append('TL_Overlord_Jab1 has no montage')
        return
    length = montage.get_play_length()
    p('source', SOURCE_TIMELINE, 'montage', montage.get_path_name(), 'section', section, 'rate', rate, 'length %.3f s' % length,
      'total_frames', src.get_editor_property('total_frames'))

    bp = unreal.EditorAssetLibrary.load_asset(ABILITY)
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    data = cdo.get_editor_property('timeline')
    total = data.get_editor_property('total_frames')
    shown = length / max(rate, 1e-6)
    p('ability', ABILITY, 'total_frames', total, '(%.3f s at %.1f Hz)' % (total / SIM_HZ, SIM_HZ),
      'montage at rate: %.3f s = %.1f sim frames' % (shown, shown * SIM_HZ))
    current = data.get_editor_property('montage')
    if current == montage and data.get_editor_property('montage_section') == section and abs(data.get_editor_property('montage_play_rate') - rate) < 1e-6:
        p('already set, nothing to do')
        return
    p('was montage', current.get_path_name() if current else None)
    if SAVE:
        backup(ABILITY)
    data.set_editor_property('montage', montage)
    data.set_editor_property('montage_section', section)
    data.set_editor_property('montage_play_rate', rate)
    cdo.set_editor_property('timeline', data)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    check = cdo.get_editor_property('timeline')
    if check.get_editor_property('montage') != montage or check.get_editor_property('total_frames') != total:
        errors.append('timeline did not take the montage (or lost its frames)')
        return
    p('set montage', montage.get_path_name())
    if SAVE:
        p('saved', unreal.EditorAssetLibrary.save_asset(ABILITY, only_if_is_dirty=False))


try:
    run()
except Exception as exc:
    import traceback
    errors.append('exception: %s' % exc)
    p(traceback.format_exc())
for e in errors:
    p('ERROR', e)
p('RESULT:', 'PASS' if not errors else 'FAIL')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
print('[set_playtest_jab_montage] %s, report %s' % ('PASS' if not errors else 'FAIL: ' + errors[0][:160], OUT))
