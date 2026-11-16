# TEST CONTENT ONLY (weakpoint slice 3, scope B): makes the strike kind recoil placeholders visible in PIE. Not a real move, no design value.
# Idempotent: an asset is saved only when something changed (an existing asset is backed up to Saved/Backups/GAS first), and an asset that had
# unsaved in-memory changes when the run started is never saved (run it headless or in a fresh editor). Machine output: Saved/WP3/setup_strike_kinds.txt
#
# Writes, under /Game/GASNPPTest/Combat:
#   TL_Test_StrikeKinds    the timeline of UGSTN_GA_TestStrikeKinds: group 0 Swing (frames 8..14, sweeps in from the right), group 1 Stab (frames
#                          22..26, straight ahead), no recoil of its own (UGSTN_TestStrikeKindsLibrary::FillStrikeKindsTimeline has the numbers)
#   KIT_Test_StrikeKinds   a copy of KIT_Test_Hits made once: Command.Attack.Neutral plays UGSTN_GA_TestStrikeKinds, the kit's own recoil fallback off,
#                          so a ricochet reaches the kind defaults of Config/DefaultGSTNCombat.ini (B_A for Swing, B_V for Stab)
#
# Headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_strike_kinds_test.py" -unattended -nullrhi
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_strike_kinds_test as s; s.run()
# PIE:       Tools/GASNPPTest/test_hub.py, h.strike_kinds(0) (kit, forced ricochet, a dummy 120 cm ahead)
import hashlib
import os
import shutil

import unreal

FOLDER = '/Game/GASNPPTest/Combat'
TIMELINE = 'TL_Test_StrikeKinds'
KIT = 'KIT_Test_StrikeKinds'
SOURCE_KIT = 'KIT_Test_Hits'
COMMAND = 'Command.Attack.Neutral'
BACKUP_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'GAS')
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'WP3', 'setup_strike_kinds.txt')
_lines = []


def log(msg):
    _lines.append(msg)
    unreal.log(f'[setup_strike_kinds_test] {msg}')


def _file_of(path):
    return os.path.normpath(unreal.Paths.convert_relative_path_to_full(os.path.join(unreal.Paths.project_content_dir(), path.split('/', 2)[2] + '.uasset')))


def _save(path, asset, created, changed, dirty_at_start):
    if path in dirty_at_start:
        log(f'NOT saved {path}: it had unsaved in-memory changes before this run')
        return
    if not (created or changed):
        log(f'unchanged {path}')
        return
    f = _file_of(path)
    if not created and os.path.exists(f):
        os.makedirs(BACKUP_DIR, exist_ok=True)
        shutil.copy2(f, os.path.join(BACKUP_DIR, os.path.basename(f) + '.bak'))
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log(f'{"created" if created else "wrote"} {path}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def run():
    del _lines[:]
    dirty_at_start = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    lib = unreal.GSTN_TestStrikeKindsLibrary

    path = f'{FOLDER}/{TIMELINE}'
    timeline = unreal.load_asset(path)
    created = False
    if not timeline:
        timeline = unreal.AssetToolsHelpers.get_asset_tools().create_asset(TIMELINE, FOLDER, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        created = True
    if not timeline:
        log(f'ERROR: could not create {path}')
        return False
    _save(path, timeline, created, lib.fill_strike_kinds_timeline(timeline), dirty_at_start)

    path = f'{FOLDER}/{KIT}'
    created = False
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(f'{FOLDER}/{SOURCE_KIT}', path):
            log(f'ERROR: could not copy {SOURCE_KIT} to {KIT}')
            return False
        created = True
    kit = unreal.load_asset(path)
    changed = lib.bind_kit_command(kit, _tag(COMMAND), unreal.GSTN_GA_TestStrikeKinds.static_class())
    changed = lib.clear_kit_ricochet_fallback(kit) or changed
    _save(path, kit, created, changed, dirty_at_start)
    log('recoil chain: ' + lib.describe_recoil_chain(kit))

    for p in (f'{FOLDER}/{TIMELINE}', f'{FOLDER}/{KIT}'):
        f = _file_of(p)
        log(f'hash {hashlib.sha256(open(f, "rb").read()).hexdigest()[:16] if os.path.exists(f) else "missing"} {p}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as fh:
        fh.write('\n'.join(_lines) + '\n')
    return True


if __name__ == '__main__':
    run()
