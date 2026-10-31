# Writes the M6 test content of GSTNDefence and GSTNLink (plan F13) and the project test kits that carry it. Idempotent with identical hashes: every
# property is compared before it is written and an asset is saved only when something changed, so a rerun on up-to-date content saves nothing (run()
# returns and logs the SHA-256 of every asset it owns: compare two runs). An existing asset that changes is backed up to Saved/Backups/GAS first.
# Each plugin's part runs only when that plugin is enabled.
#
# Run in the editor Python console (both plugins enabled):
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_defence_link_content as s; s.run()
# or headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_defence_link_content.py" -stdout
#               (append --plugin-only inside the -script quotes to leave the project kits and the standard rules alone)
#
# Plugin content (no /Game/ reference), /GSTNDefence/Test:
#   DA_DefenceRules_Test     PvP rules with the short numbers of m6 spec 11 (the test pawn's origin is its capsule middle, so the bubble sits there with a
#                            95 cm radius that covers the HBS_Test_Pawn hurtboxes): shield 100, drain 0.4 a frame, regen 0.2 after 10 frames, a five frame
#                            release-anchored perfect shield, tech window 20, lockout 40, profiles Shield (0) and Counter (1)
#   DA_DefenceRules_TestPvE  the same with bShieldEnabled false: nothing of the shield exists, the counter move still parries
#   GA_Shield_Test, GA_CounterTest, GA_TechInPlace_Test, GA_TechRoll_Test, GA_TechWall_Test, GA_GroundBounce_Test, GA_GetupRoll_Test, GA_GetupNeutral_Test
#                            thin Blueprint children of the plugin's native abilities (the frame data is the native class's; a game's own kit overrides it)
# /GSTNLink/Test:
#   DA_LinkType_Grab_Test    the grab link type: Combat.Link.Type.Grab, grabber / grabbed, the grabbed parked 90 cm in front of the grabber's ROOT facing it
#                            (no socket: the S1 spike measures the link, not a hand animation; the hand socket path is the unit tests'), the grab rule with
#                            the test mash (base 90 frames), the latch as its alternative (F12b). The registry finds it by its primary asset type
#   DA_LinkType_Latch_Test   the latch: Combat.Link.Type.Latch, latcher (parked on the latched one's back, acting) / latched (acting, its release is the
#                            shake-off), the latch rule with its defaults
#   GA_Grab_Test, GA_Pummel_Test, GA_Release_Test, GA_ThrowForward_Test, GA_ThrowBack_Test, GA_ThrowUp_Test, GA_ThrowDown_Test, GA_LatchAttack_Test
#                            thin Blueprint children of the harness's native link moves (UGSTN_GA_LinkTest*), one per command
# Project content (the owner approved these edits, plan section 6; new kits, so KIT_Test_Hits and the M1-M4b scenarios that use it are unchanged), under
# /Game/GASNPPTest/Combat, each a copy of KIT_Test_Hits made once:
#   KIT_Test_Defence         + defence extension: DA_DefenceRules_Test, GA_Shield_Test
#   KIT_Test_DefencePvE      + defence extension: DA_DefenceRules_TestPvE, GA_Shield_Test; Command.Special.Neutral plays GA_CounterTest
#   KIT_Test_DefenceLanding  + defence extension: PvP rules, the shield, the get-up set (ground bounce, roll, neutral get-up)
#   KIT_Test_DefenceTech     + the landing kit's extension and the tech set (in place, roll, wall)
#   KIT_Test_Link            + the standard link slots, the link moves above, a link extension (default rules, grab mash base 90 frames)
#   KIT_Test_DefenceLink     + the defence extension of KIT_Test_Defence and the link content of KIT_Test_Link (scenario grab_shield)
# and the rules the test pawns play, /GSTNCombat/Rules/GSTN_CombatRules_Standard, get the steps GrabImmunity, LinkRequest (GSTNLink), Parry, Shield
# (GSTNDefence) and LinkBreakOnHit (GSTNLink) in m6 spec 9 order. Rerun this script after Tools/GAS/setup_gas_assets.py --h13-content, which rewrites
# that asset from FillStandard. With a plugin off its steps are soft references the rules skip (logged, not an error).
import hashlib
import os
import shutil
import sys

import unreal

TEST_DIR = '/GSTNDefence/Test'
LINK_DIR = '/GSTNLink/Test'
PROJECT_DIR = '/Game/GASNPPTest/Combat'
SOURCE_KIT = 'KIT_Test_Hits'
STANDARD_RULES = '/GSTNCombat/Rules/GSTN_CombatRules_Standard'
LINK_TYPE = 'DA_LinkType_Grab_Test'
LATCH_TYPE = 'DA_LinkType_Latch_Test'
MASH_BASE_FRAMES = 90
BACKUP_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Backups', 'GAS')

DEFENCE_ABILITIES = [('GA_Shield_Test', 'GSTN_GA_Shield'), ('GA_CounterTest', 'GSTN_GA_CounterSample'), ('GA_TechInPlace_Test', 'GSTN_GA_Tech'),
                     ('GA_TechRoll_Test', 'GSTN_GA_Tech'), ('GA_TechWall_Test', 'GSTN_GA_Tech'), ('GA_GroundBounce_Test', 'GSTN_GA_GroundBounce'),
                     ('GA_GetupRoll_Test', 'GSTN_GA_GetupRoll'), ('GA_GetupNeutral_Test', 'GSTN_GA_GetupNeutral')]
LINK_ABILITIES = [('GA_Grab_Test', 'GSTN_GA_LinkTestGrab'), ('GA_Pummel_Test', 'GSTN_GA_LinkTestPummel'), ('GA_Release_Test', 'GSTN_GA_LinkTestRelease'),
                  ('GA_ThrowForward_Test', 'GSTN_GA_LinkTestThrow'), ('GA_ThrowBack_Test', 'GSTN_GA_LinkTestThrow'), ('GA_ThrowUp_Test', 'GSTN_GA_LinkTestThrow'),
                  ('GA_ThrowDown_Test', 'GSTN_GA_LinkTestThrow'), ('GA_LatchAttack_Test', 'GSTN_GA_LinkTestLatchAttack')]
# name: (rules, shield, landing, tech, counter, link)
KITS = {
    'KIT_Test_Defence': ('DA_DefenceRules_Test', True, False, False, False, False),
    'KIT_Test_DefencePvE': ('DA_DefenceRules_TestPvE', True, False, False, True, False),
    'KIT_Test_DefenceLanding': ('DA_DefenceRules_Test', True, True, False, False, False),
    'KIT_Test_DefenceTech': ('DA_DefenceRules_Test', True, True, True, False, False),
    'KIT_Test_Link': (None, False, False, False, False, True),
    'KIT_Test_DefenceLink': ('DA_DefenceRules_Test', True, False, False, False, True),
}
_owned = []   # every asset path this run wrote or checked, for hashes()


def log(msg):
    unreal.log(f'[setup_defence_link_content] {msg}')


def has_defence():
    return hasattr(unreal, 'GSTN_DefenceRules')


def has_link():
    return hasattr(unreal, 'GSTN_LinkTypeDefinition')


# ---- files, backups, hashes -----------------------------------------------------------------------------------------------------------------------------
def _file_of(asset_path):
    """The .uasset file of /Game/... or /<Plugin>/... (a plugin's content folder is found by its .uplugin under the project's Plugins folder)"""
    mount, rest = asset_path.split('/')[1], asset_path.split('/', 2)[2]
    if mount == 'Game':
        root = unreal.Paths.project_content_dir()
    else:
        root = None
        for dirpath, _dirs, files in os.walk(unreal.Paths.project_plugins_dir()):
            if f'{mount}.uplugin' in files:
                root = os.path.join(dirpath, 'Content')
                break
        if root is None:
            return None
    return os.path.normpath(unreal.Paths.convert_relative_path_to_full(os.path.join(root, rest + '.uasset')))


def _backup(asset_path):
    src = _file_of(asset_path)
    if src and os.path.exists(src):
        os.makedirs(BACKUP_DIR, exist_ok=True)
        shutil.copy2(src, os.path.join(BACKUP_DIR, os.path.basename(src) + '.bak'))


def hashes():
    """{asset path: SHA-256 prefix} of every asset of the last run (what two runs must agree on)"""
    out = {}
    for path in sorted(set(_owned)):
        f = _file_of(path)
        out[path] = hashlib.sha256(open(f, 'rb').read()).hexdigest()[:16] if f and os.path.exists(f) else 'missing'
    return out


class _Asset:
    """One asset being written: properties are compared before they are set, and save() writes the package only when one changed (backing the old
    file up first) or when the asset was just created"""

    def __init__(self, path, asset, created):
        self.path, self.asset, self.created, self.changed = path, asset, created, False
        _owned.append(path)

    def set(self, prop, value, obj=None):
        """A scalar, an enum or a gameplay tag (compared as text, floats with a tolerance)"""
        obj = obj or self.asset
        cur = obj.get_editor_property(prop)
        if isinstance(value, float) and isinstance(cur, float):
            same = abs(cur - value) < 1e-6
        elif hasattr(value, 'export_text'):
            same = cur.export_text() == value.export_text()
        else:
            same = cur == value
        if not same:
            obj.set_editor_property(prop, value)
            self.changed = True

    def struct(self, prop, obj=None, **fields):
        """Sets fields of a struct property: a copy is changed and compared as text with the current value (float32 on both sides), then written"""
        obj = obj or self.asset
        cur = obj.get_editor_property(prop)
        new = obj.get_editor_property(prop)
        for k, v in fields.items():
            new.set_editor_property(k, v)
        if new.export_text() != cur.export_text():
            obj.set_editor_property(prop, new)
            self.changed = True
        return new

    def array(self, prop, values):
        """An array of structs, compared element by element as text"""
        cur = list(self.asset.get_editor_property(prop))
        if [c.export_text() for c in cur] != [v.export_text() for v in values]:
            self.asset.set_editor_property(prop, values)
            self.changed = True

    def mark(self, changed):
        self.changed = self.changed or bool(changed)

    def save(self):
        if self.created or self.changed:
            if not self.created:
                _backup(self.path)
            unreal.EditorAssetLibrary.save_loaded_asset(self.asset, False)
            log(f'{"created" if self.created else "wrote"} {self.path}')
            return True
        log(f'unchanged {self.path}')
        return False


def _data_asset(folder, name, asset_class):
    path = f'{folder}/{name}'
    asset = unreal.load_asset(path)
    created = False
    if not asset:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, unreal.DataAssetFactory())
        created = True
    return _Asset(path, asset, created)


def _ability(folder, name, parent_name):
    """A thin Blueprint child of a native ability class: created once, reparented only when its parent is not the one named. Returns its class"""
    parent = getattr(unreal, parent_name).static_class()
    path = f'{folder}/{name}'
    bp = unreal.load_asset(path)
    entry = None
    if not bp:
        factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        entry = _Asset(path, bp, True)
    else:
        entry = _Asset(path, bp, False)
        # The Blueprint's ParentClass (not a Python property): its asset registry tag, "/Script/<Module>.<Class>" possibly quoted with the class type
        tag = str(unreal.EditorAssetLibrary.find_asset_data(path).get_tag_value('ParentClass') or '')
        if not tag.rstrip("'").endswith('.' + parent.get_name()):
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            entry.mark(True)
    entry.save()
    return unreal.EditorAssetLibrary.load_blueprint_class(path)


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


# ---- GSTNDefence ------------------------------------------------------------------------------------------------------------------------------------------
def specs():
    shield = {'max_shield': 100.0, 'drain_per_frame': 0.4, 'regen_per_frame': 0.2, 'regen_delay_frames': 10, 'startup_frames': 1, 'release_frames': 6,
              'radius_cm': 95.0, 'centre_height_cm': 0.0, 'break_stun_frames': 90, 'break_launch': 900.0}
    parry = {'window_frames': 5, 'min_held_frames_before_release': 0}
    tech = {'window_frames': 20, 'lockout_frames': 40}
    profiles = [('Shield', 8, 4, 20, 5.0), ('Counter', 8, 4, 20, 5.0)]
    return {
        'DA_DefenceRules_Test': {'shield_enabled': True, 'shield': shield, 'parry': parry, 'tech': tech, 'profiles': profiles},
        'DA_DefenceRules_TestPvE': {'shield_enabled': False, 'shield': shield, 'parry': parry, 'tech': tech, 'profiles': profiles},
    }


def _profile(name, hitstop, recovery, stun, meter):
    p = unreal.GSTN_ParryProfile()
    p.set_editor_property('name', name)
    p.set_editor_property('hitstop_frames', hitstop)
    p.set_editor_property('defender_recovery_frames', recovery)
    p.set_editor_property('attacker_stun_frames', stun)
    p.set_editor_property('meter_gain', meter)
    return p


def _write_rules(name, spec):
    a = _data_asset(TEST_DIR, name, unreal.GSTN_DefenceRules)
    a.set('shield_enabled', spec['shield_enabled'])
    a.struct('shield', **spec['shield'])
    a.struct('parry', anchor=unreal.GSTN_ParryAnchor.RELEASE, profile_index=0, **spec['parry'])
    a.struct('tech', **spec['tech'])
    a.array('parry_profiles', [_profile(*p) for p in spec['profiles']])
    a.save()
    return a.asset


def run_defence():
    """The GSTNDefence plugin content: the two rules assets and the ability Blueprints. Returns {name: asset or class}"""
    out = {}
    for name, spec in specs().items():
        out[name] = _write_rules(name, spec)
    for name, parent in DEFENCE_ABILITIES:
        out[name] = _ability(TEST_DIR, name, parent)
    return out


# ---- GSTNLink ---------------------------------------------------------------------------------------------------------------------------------------------
def _role(a, prop, **fields):
    a.struct(prop, **fields)


def _rule(a, rule_class, configure=None):
    """The definition's one rule of rule_class (kept when it is the only rule, so a rerun does not make a new subobject), then configure(rule) -> changed"""
    rules = list(a.asset.get_editor_property('rules'))
    if len(rules) == 1 and rules[0] and rules[0].get_class() == rule_class.static_class():
        rule = rules[0]
    else:
        rule = unreal.new_object(rule_class, a.asset)
        a.asset.set_editor_property('rules', [rule])
        a.mark(True)
    if configure:
        a.mark(configure(rule))
    return rule


def _grab_mash(rule):
    rules = rule.get_editor_property('rules')
    mash = rules.get_editor_property('mash')
    if mash.get_editor_property('base_frames') == MASH_BASE_FRAMES:
        return False
    mash.set_editor_property('base_frames', MASH_BASE_FRAMES)
    rules.set_editor_property('mash', mash)
    rule.set_editor_property('rules', rules)
    return True


def _write_link_type():
    a = _data_asset(LINK_DIR, LINK_TYPE, unreal.GSTN_LinkTypeDefinition)
    a.set('link_type', _tag('Combat.Link.Type.Grab'))
    a.set('channel', unreal.GSTN_LinkChannel.BODY)
    _role(a, 'initiator_role', role_tag=_tag('Combat.Link.Role.Grabber'), parked=False, acts_while_linked=True)
    _role(a, 'target_role', role_tag=_tag('Combat.Link.Role.Grabbed'), parked=True, acts_while_linked=False, collision_with_partner=False,
          socket_on_partner='None', socket_offset=unreal.Transform(location=[90.0, 0.0, 0.0], rotation=[0.0, 0.0, 180.0], scale=[1.0, 1.0, 1.0]))
    _rule(a, unreal.GSTN_LinkRule_Grab, _grab_mash)
    a.set('initiator_predicts', False)
    # F12b: the latch is tried first, so one tap latches onto a target heavy enough for the latch rule and grabs a lighter one
    alternatives = [_tag('Combat.Link.Type.Latch')]
    if [t.export_text() for t in a.asset.get_editor_property('alternatives')] != [t.export_text() for t in alternatives]:
        a.asset.set_editor_property('alternatives', alternatives)
        a.mark(True)
    a.save()
    return a.asset


def _write_latch_type():
    """The latcher (initiator) is parked on the latched enemy's back (45 cm behind its root, 60 cm up, facing the same way) and acts while linked; the
    latched enemy acts too, and its own release is a shake-off. The latch rule with its defaults (the scenarios lower MinLatchWeight in memory so a
    standard fighter counts as big)"""
    a = _data_asset(LINK_DIR, LATCH_TYPE, unreal.GSTN_LinkTypeDefinition)
    a.set('link_type', _tag('Combat.Link.Type.Latch'))
    a.set('channel', unreal.GSTN_LinkChannel.BODY)
    _role(a, 'initiator_role', role_tag=_tag('Combat.Link.Role.Latcher'), parked=True, acts_while_linked=True, collision_with_partner=False,
          socket_on_partner='None', socket_offset=unreal.Transform(location=[-45.0, 0.0, 60.0], rotation=[0.0, 0.0, 0.0], scale=[1.0, 1.0, 1.0]))
    _role(a, 'target_role', role_tag=_tag('Combat.Link.Role.Latched'), parked=False, acts_while_linked=True,
          release_break_reason=_tag('Combat.Link.BreakReason.ShakeOff'))
    _rule(a, unreal.GSTN_LinkRule_Latch)
    a.set('initiator_predicts', False)
    a.save()
    return a.asset


def run_link():
    """The GSTNLink plugin content: the two link types and the move Blueprints. Returns the names written or checked"""
    out = {LATCH_TYPE: _write_latch_type(), LINK_TYPE: _write_link_type()}
    for name, parent in LINK_ABILITIES:
        out[name] = _ability(LINK_DIR, name, parent)
    return sorted(out)


# ---- project kits and the standard rules ------------------------------------------------------------------------------------------------------------------
def _kit(name):
    path = f'{PROJECT_DIR}/{name}'
    created = False
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(f'{PROJECT_DIR}/{SOURCE_KIT}', path):
            log(f'ERROR: could not copy {SOURCE_KIT} to {name}')
            return None
        created = True
    return _Asset(path, unreal.load_asset(path), created)


def _class(folder, name):
    return unreal.EditorAssetLibrary.load_blueprint_class(f'{folder}/{name}')


def run_project():
    """The project test kits (copies of KIT_Test_Hits) and the steps on the standard rules. Kits that need a disabled plugin are skipped"""
    out = []
    d = lambda n: _class(TEST_DIR, n)
    k = lambda n: _class(LINK_DIR, n)
    for name, (rules_name, shield, landing, tech, counter, link) in KITS.items():
        if (rules_name and not has_defence()) or (link and not has_link()):
            log(f'skipped {name} (its plugin is not enabled)')
            continue
        a = _kit(name)
        if a is None:
            continue
        if rules_name:
            rules = unreal.load_asset(f'{TEST_DIR}/{rules_name}')
            a.mark(unreal.GSTN_DefenceContentLibrary.write_kit_defence_extension(
                a.asset, rules, d('GA_Shield_Test') if shield else None,
                d('GA_GroundBounce_Test') if landing else None, d('GA_GetupRoll_Test') if landing else None, d('GA_GetupNeutral_Test') if landing else None,
                d('GA_TechInPlace_Test') if tech else None, d('GA_TechRoll_Test') if tech else None, d('GA_TechWall_Test') if tech else None))
            if counter:
                a.mark(unreal.GSTN_DefenceContentLibrary.write_kit_command_ability(a.asset, _tag('Command.Special.Neutral'), d('GA_CounterTest')))
        if link:
            a.mark(unreal.GSTN_LinkContentLibrary.write_kit_link_content(
                a.asset, k('GA_Grab_Test'), k('GA_Pummel_Test'), k('GA_Release_Test'), k('GA_ThrowForward_Test'), k('GA_ThrowBack_Test'),
                k('GA_ThrowUp_Test'), k('GA_ThrowDown_Test'), k('GA_LatchAttack_Test'), MASH_BASE_FRAMES))
        a.save()
        out.append(name)

    rules = unreal.load_asset(STANDARD_RULES)
    a = _Asset(STANDARD_RULES, rules, False)
    names = ' '.join(str(p) for p in rules.get_editor_property('hit_pipeline'))
    if has_link() and not all(s in names for s in ('HitStep_GrabImmunity', 'HitStep_LinkRequest', 'HitStep_LinkBreakOnHit')):
        a.mark(unreal.GSTN_LinkContentLibrary.add_link_steps_to_rules(rules))
    if has_defence() and not all(s in names for s in ('HitStep_Parry', 'HitStep_Shield')):
        a.mark(unreal.GSTN_DefenceContentLibrary.add_defence_steps_to_rules(rules))
    a.save()
    log('standard rules pipeline: ' + ', '.join(p.get_name() if hasattr(p, 'get_name') else str(p) for p in rules.get_editor_property('hit_pipeline')))
    out.append(STANDARD_RULES)
    return out


def run(project=True):
    """Everything (project=False: the plugin content only). Returns {asset path: hash} after the run"""
    del _owned[:]
    if has_defence():
        run_defence()
    if has_link():
        run_link()
    if project:
        run_project()
    out = hashes()
    for path, digest in out.items():
        log(f'hash {digest} {path}')
    return out


if __name__ == '__main__':
    run(project='--plugin-only' not in sys.argv)
