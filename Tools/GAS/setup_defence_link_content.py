# Writes the plugin-owned test content of GSTNDefence under /GSTNDefence/Test (M6 F7, used by Tools/GASNPPTest/defence_scenarios.py). Idempotent:
# the data assets are rewritten from the specs below on every run; an asset that changes is backed up to Saved/Backups/GAS first. No /Game/ reference.
#
# Run in the editor Python console (GSTNDefence enabled):
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_defence_link_content as s; s.run()
# or headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_defence_link_content.py" -stdout
#
# Assets:
#   DA_DefenceRules_Test     PvP rules with short numbers (m6 spec 11; the test pawn's origin is its capsule middle, so the bubble sits there
#                            with a 95 cm radius that covers the HBS_Test_Pawn hurtboxes): shield 100, drain 0.4 a frame, regen 0.2 after 10 frames, a five frame
#                            release-anchored perfect shield, profiles Shield (0) and Counter (1)
#   DA_DefenceRules_TestPvE  the same with bShieldEnabled false: nothing of the shield exists, the counter move still parries
# The abilities are native classes of GSTNDefenceNP (UGSTN_GA_Shield, whose frames match these rules, and UGSTN_GA_CounterSample), so no Blueprint
# is created. The kits, the input config and the combat rules are adjusted IN MEMORY by the scenarios, never here.
import os
import shutil
import unreal

TEST_DIR = '/GSTNDefence/Test'
BACKUP_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'GAS')


def log(msg):
    unreal.log(f'[setup_defence_link_content] {msg}')


def specs():
    shield = {'max_shield': 100.0, 'drain_per_frame': 0.4, 'regen_per_frame': 0.2, 'regen_delay_frames': 10, 'startup_frames': 1, 'release_frames': 6,
              'radius_cm': 95.0, 'centre_height_cm': 0.0, 'break_stun_frames': 90, 'break_launch': 900.0}
    parry = {'window_frames': 5, 'min_held_frames_before_release': 0}
    profiles = [('Shield', 8, 4, 20, 5.0), ('Counter', 8, 4, 20, 5.0)]
    return {
        'DA_DefenceRules_Test': {'shield_enabled': True, 'shield': shield, 'parry': parry, 'profiles': profiles},
        'DA_DefenceRules_TestPvE': {'shield_enabled': False, 'shield': shield, 'parry': parry, 'profiles': profiles},
    }


def _backup(asset_path):
    src = unreal.SystemLibrary.convert_to_absolute_path(unreal.Paths.convert_relative_path_to_full(
        os.path.join(unreal.Paths.project_plugins_dir(), 'GSTNDefence', 'Content', asset_path[len('/GSTNDefence/'):] + '.uasset')))
    if os.path.exists(src):
        os.makedirs(BACKUP_DIR, exist_ok=True)
        shutil.copy2(src, os.path.join(BACKUP_DIR, os.path.basename(src) + '.bak'))


def _get_or_create(name, asset_class):
    path = f'{TEST_DIR}/{name}'
    asset = unreal.load_asset(path)
    if asset:
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, TEST_DIR, asset_class, unreal.DataAssetFactory())
    log(f'created {path}')
    return asset, True


def _profile(name, hitstop, recovery, stun, meter):
    p = unreal.GSTN_ParryProfile()
    p.set_editor_property('name', name)
    p.set_editor_property('hitstop_frames', hitstop)
    p.set_editor_property('defender_recovery_frames', recovery)
    p.set_editor_property('attacker_stun_frames', stun)
    p.set_editor_property('meter_gain', meter)
    return p


def _write_rules(name, spec):
    rules, created = _get_or_create(name, unreal.GSTN_DefenceRules)
    if not created:
        _backup(f'{TEST_DIR}/{name}')
    rules.set_editor_property('shield_enabled', spec['shield_enabled'])
    shield = rules.get_editor_property('shield')
    for k, v in spec['shield'].items():
        shield.set_editor_property(k, v)
    rules.set_editor_property('shield', shield)
    parry = rules.get_editor_property('parry')
    for k, v in spec['parry'].items():
        parry.set_editor_property(k, v)
    parry.set_editor_property('anchor', unreal.GSTN_ParryAnchor.RELEASE)
    parry.set_editor_property('profile_index', 0)
    rules.set_editor_property('parry', parry)
    rules.set_editor_property('parry_profiles', [_profile(*p) for p in spec['profiles']])
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    log(f'wrote {TEST_DIR}/{name}')
    return rules


def run():
    out = {}
    for name, spec in specs().items():
        out[name] = _write_rules(name, spec)
    return sorted(out)


if __name__ == '__main__':
    run()
