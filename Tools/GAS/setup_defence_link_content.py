# Writes the plugin-owned test content of GSTNDefence under /GSTNDefence/Test (M6 F7, used by Tools/GASNPPTest/defence_scenarios.py) and of GSTNLink
# under /GSTNLink/Test (M6 F10, used by Tools/GASNPPTest/link_scenarios.py). Idempotent: the data assets are rewritten from the specs below on every
# run; an asset that changes is backed up to Saved/Backups/GAS first. No /Game/ reference. Each plugin's part runs only when that plugin is enabled.
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
#   DA_LinkType_Grab_Test    (GSTNLink) the grab link type of the harness: Combat.Link.Type.Grab, grabber / grabbed, the grabbed parked 90 cm in front
#                            of the grabber's ROOT facing it (no socket: the S1 spike measures the link, not a hand animation; the hand socket path is
#                            the unit tests'), the grab rule with its defaults, the latch as its alternative (F12b). The registry finds it by its primary
#                            asset type under /GSTNLink
#   DA_LinkType_Latch_Test   (GSTNLink, F12b) the latch: Combat.Link.Type.Latch, latcher (parked on the latched one's back, acting) / latched (acting,
#                            its release is the shake-off), the latch rule with its defaults
# The abilities are native classes of GSTNDefenceNP (UGSTN_GA_Shield, whose frames match these rules, and UGSTN_GA_CounterSample), so no Blueprint
# is created. The kits, the input config and the combat rules are adjusted IN MEMORY by the scenarios, never here.
import os
import shutil
import unreal

TEST_DIR = '/GSTNDefence/Test'
LINK_DIR = '/GSTNLink/Test'
LINK_TYPE = 'DA_LinkType_Grab_Test'
LATCH_TYPE = 'DA_LinkType_Latch_Test'
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
    plugin = asset_path.split('/')[1]
    src = unreal.SystemLibrary.convert_to_absolute_path(unreal.Paths.convert_relative_path_to_full(
        os.path.join(unreal.Paths.project_plugins_dir(), plugin, 'Content', asset_path[len('/' + plugin + '/'):] + '.uasset')))
    if os.path.exists(src):
        os.makedirs(BACKUP_DIR, exist_ok=True)
        shutil.copy2(src, os.path.join(BACKUP_DIR, os.path.basename(src) + '.bak'))


def _get_or_create(name, asset_class, folder=TEST_DIR):
    path = f'{folder}/{name}'
    asset = unreal.load_asset(path)
    if asset:
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, unreal.DataAssetFactory())
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


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _write_link_type():
    asset, created = _get_or_create(LINK_TYPE, unreal.GSTN_LinkTypeDefinition, LINK_DIR)
    if not created:
        _backup(f'{LINK_DIR}/{LINK_TYPE}')
    asset.set_editor_property('link_type', _tag('Combat.Link.Type.Grab'))
    asset.set_editor_property('channel', unreal.GSTN_LinkChannel.BODY)
    grabber = asset.get_editor_property('initiator_role')
    grabber.set_editor_property('role_tag', _tag('Combat.Link.Role.Grabber'))
    grabber.set_editor_property('parked', False)
    grabber.set_editor_property('acts_while_linked', True)
    asset.set_editor_property('initiator_role', grabber)
    grabbed = asset.get_editor_property('target_role')
    grabbed.set_editor_property('role_tag', _tag('Combat.Link.Role.Grabbed'))
    grabbed.set_editor_property('parked', True)
    grabbed.set_editor_property('acts_while_linked', False)
    grabbed.set_editor_property('collision_with_partner', False)
    grabbed.set_editor_property('socket_on_partner', 'None')
    grabbed.set_editor_property('socket_offset', unreal.Transform(location=[90.0, 0.0, 0.0], rotation=[0.0, 0.0, 180.0], scale=[1.0, 1.0, 1.0]))
    asset.set_editor_property('target_role', grabbed)
    asset.set_editor_property('rules', [unreal.new_object(unreal.GSTN_LinkRule_Grab, asset)])
    asset.set_editor_property('initiator_predicts', False)
    # F12b: the latch is tried first, so one tap latches onto a target heavy enough for the latch rule and grabs a lighter one
    asset.set_editor_property('alternatives', [_tag('Combat.Link.Type.Latch')])
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f'wrote {LINK_DIR}/{LINK_TYPE}')
    return asset


def _write_latch_type():
    """F12b: the latch link type of the harness. The latcher (initiator) is parked on the latched enemy's back (45 cm behind its root, 60 cm up, facing
    the same way) and acts while linked; the latched enemy acts too, and its own release is a shake-off. The latch rule with its defaults (the kits
    of the scenarios lower MinLatchWeight in memory so a standard fighter counts as big)"""
    asset, created = _get_or_create(LATCH_TYPE, unreal.GSTN_LinkTypeDefinition, LINK_DIR)
    if not created:
        _backup(f'{LINK_DIR}/{LATCH_TYPE}')
    asset.set_editor_property('link_type', _tag('Combat.Link.Type.Latch'))
    asset.set_editor_property('channel', unreal.GSTN_LinkChannel.BODY)
    latcher = asset.get_editor_property('initiator_role')
    latcher.set_editor_property('role_tag', _tag('Combat.Link.Role.Latcher'))
    latcher.set_editor_property('parked', True)
    latcher.set_editor_property('acts_while_linked', True)
    latcher.set_editor_property('collision_with_partner', False)
    latcher.set_editor_property('socket_on_partner', 'None')
    latcher.set_editor_property('socket_offset', unreal.Transform(location=[-45.0, 0.0, 60.0], rotation=[0.0, 0.0, 0.0], scale=[1.0, 1.0, 1.0]))
    asset.set_editor_property('initiator_role', latcher)
    latched = asset.get_editor_property('target_role')
    latched.set_editor_property('role_tag', _tag('Combat.Link.Role.Latched'))
    latched.set_editor_property('parked', False)
    latched.set_editor_property('acts_while_linked', True)
    latched.set_editor_property('release_break_reason', _tag('Combat.Link.BreakReason.ShakeOff'))
    asset.set_editor_property('target_role', latched)
    asset.set_editor_property('rules', [unreal.new_object(unreal.GSTN_LinkRule_Latch, asset)])
    asset.set_editor_property('initiator_predicts', False)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f'wrote {LINK_DIR}/{LATCH_TYPE}')
    return asset


def run_link():
    """The GSTNLink part only (the defence assets are left untouched)"""
    out = []
    if _write_latch_type():
        out.append(LATCH_TYPE)
    if _write_link_type():
        out.append(LINK_TYPE)
    return out


def run():
    out = {}
    if hasattr(unreal, 'GSTN_DefenceRules'):
        for name, spec in specs().items():
            out[name] = _write_rules(name, spec)
    if hasattr(unreal, 'GSTN_LinkTypeDefinition'):
        out[LATCH_TYPE] = _write_latch_type()
        out[LINK_TYPE] = _write_link_type()
    return sorted(out)


if __name__ == '__main__':
    run()
