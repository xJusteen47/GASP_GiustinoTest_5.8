# M11 K3: the three test kit archetypes of the hub (light / medium / heavy), so a playtester feels three really different kits.
# Run headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>"   or   import setup_test_kit_archetypes as k; k.run()
# Writes, under /Game/GASNPPTest/Combat (new assets only; KIT_Test_Hits, KIT_Test_Link and the test character are never edited):
#   Archetypes/GA_<Kit>_Jab, GA_<Kit>_Special   Blueprint children of UGSTN_GA_TestKitStrike (frame data by UGSTN_TestKitContentLibrary)
#   Archetypes/GA_<Kit>_Throw{Forward,Back,Up,Down}   Blueprint children of the common throws UGSTN_GA_LinkThrow* (UGSTN_LinkContentLibrary)
#   WPN_Test_<Kit>   a copy of WPN_Test_Hits whose neutral attack is the kit's jab
#   KIT_Test_<Kit>   a copy of KIT_Test_Link (the link slots and moves, the dodges) with its own weapon, stats (UGSTN_GE_TestKitArchetypeInit),
#                    special and four throws in its link extension (the loadout extension GSTNLinkThrows binds them, M11 K2)
# Every number is a PLACEHOLDER for the playtest and every montage a GASP placeholder: retune the Blueprints' Class Defaults or the specs below
# and rerun (a rerun rewrites the specs over hand edits). The hub places the kits through test_hub.spawn_kits (build_test_hub_map.KIT_PADS).
# Output: Saved/Logs/k3/setup_archetypes.log, at most 5 printed lines.
import os

import unreal

DIR = '/Game/GASNPPTest/Combat'
ABILITY_DIR = DIR + '/Archetypes'
SOURCE_KIT = 'KIT_Test_Link'
SOURCE_WEAPON = 'WPN_Test_Hits'
INIT_EFFECT = 'GSTN_GE_TestKitArchetypeInit'
SHOVE = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_{}_A'
TAKEDOWN = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_{}_A'
TAKEDOWN_WALK = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_walk_{}_A'

# mash_frames: the grab's mash base frames (the hold without mashing; KIT_Test_Link's test value is 90).
# Strike: (total, first actionable, active start, active end, radius, reach from, reach to, height, damage, knockback, angle, hitstop, attacker hitstop,
#          montage, play rate)
# Throw:  (total, first actionable, throw frame, damage, knockback, angle (above 90 = behind the thrower), montage, play rate)
KITS = {
    # Light and fast: quick jab, a pop-up special, low flat throws that keep the partner close (combo throws); floaty double jump, carries little
    'Light': {
        'label': 'LEGGERO', 'pad': (800.0, -350.0), 'mash_frames': 90,
        'stats': {'WalkSpeed': 760.0, 'JumpVelocity': 820.0, 'Weight': 80.0, 'MaxCarryWeight': 60.0, 'Acceleration': 3200.0,
                  'AirControl': 0.55, 'FallGravityScale': 1.1, 'AirJumpCount': 2.0},
        'jab': (16, 13, 3, 5, 40.0, 70.0, 140.0, 0.0, 3.0, 300.0, 30.0, 4, 3, SHOVE.format('F'), 1.8),
        'special': (24, 20, 5, 8, 55.0, 40.0, 110.0, 40.0, 5.0, 700.0, 80.0, 5, 4, TAKEDOWN.format('L'), 1.5),
        'throws': {'Forward': (22, 18, 5, 5.0, 1300.0, 20.0, SHOVE.format('F'), 1.6),
                   'Back': (26, 22, 8, 6.0, 1500.0, 160.0, SHOVE.format('B'), 1.6),
                   'Up': (24, 20, 6, 4.0, 1500.0, 90.0, SHOVE.format('L'), 1.5),
                   'Down': (26, 22, 9, 4.0, 700.0, 70.0, SHOVE.format('R'), 1.5)},
    },
    # Medium: the reference fighter (standard weight and speed), mid-range strikes, throws at classic Smash angles
    'Medium': {
        'label': 'MEDIO', 'pad': (800.0, 0.0), 'mash_frames': 120,
        'stats': {'WalkSpeed': 600.0, 'JumpVelocity': 720.0, 'Weight': 100.0, 'MaxCarryWeight': 90.0, 'Acceleration': 2048.0,
                  'AirControl': 0.35, 'FallGravityScale': 1.25, 'AirJumpCount': 1.0},
        'jab': (28, 24, 6, 9, 50.0, 80.0, 170.0, 0.0, 7.0, 550.0, 35.0, 6, 4, SHOVE.format('F'), 1.2),
        'special': (34, 30, 9, 13, 60.0, 90.0, 190.0, 0.0, 10.0, 850.0, 30.0, 8, 6, TAKEDOWN.format('R'), 1.2),
        'throws': {'Forward': (30, 25, 9, 8.0, 1700.0, 45.0, TAKEDOWN.format('F'), 1.2),
                   'Back': (34, 29, 12, 10.0, 1900.0, 135.0, TAKEDOWN.format('B'), 1.2),
                   'Up': (32, 27, 11, 7.0, 1800.0, 85.0, TAKEDOWN.format('L'), 1.2),
                   'Down': (34, 29, 14, 6.0, 1000.0, 75.0, TAKEDOWN.format('R'), 1.2)},
    },
    # Heavy and slow: a long-startup hammer, a wide slam around itself, steep strong throws that send far; falls fast, lifts anyone
    'Heavy': {
        'label': 'PESANTE', 'pad': (800.0, 350.0), 'mash_frames': 180,
        'stats': {'WalkSpeed': 430.0, 'JumpVelocity': 620.0, 'Weight': 140.0, 'MaxCarryWeight': 200.0, 'Acceleration': 1200.0,
                  'AirControl': 0.2, 'FallGravityScale': 1.6, 'AirJumpCount': 1.0},
        'jab': (44, 40, 14, 18, 70.0, 90.0, 200.0, 0.0, 14.0, 900.0, 40.0, 10, 8, TAKEDOWN.format('F'), 0.85),
        'special': (56, 52, 20, 24, 140.0, 0.0, 60.0, -40.0, 18.0, 1200.0, 60.0, 12, 10, TAKEDOWN.format('B'), 0.8),
        'throws': {'Forward': (40, 34, 16, 13.0, 2300.0, 55.0, TAKEDOWN_WALK.format('F'), 0.9),
                   'Back': (46, 40, 20, 15.0, 2500.0, 120.0, TAKEDOWN_WALK.format('B'), 0.9),
                   'Up': (44, 38, 18, 11.0, 2600.0, 89.0, TAKEDOWN_WALK.format('L'), 0.9),
                   'Down': (48, 42, 22, 10.0, 1300.0, 85.0, TAKEDOWN_WALK.format('R'), 0.9)},
    },
}
THROW_PARENTS = {'Forward': 'GSTN_GA_LinkThrowForward', 'Back': 'GSTN_GA_LinkThrowBack', 'Up': 'GSTN_GA_LinkThrowUp', 'Down': 'GSTN_GA_LinkThrowDown'}

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[k3_archetypes] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _montage(path):
    m = unreal.load_asset(path)
    if not m:
        log(f'WARNING: no montage {path} (the move plays without one)')
    return m


def _ability(name, parent_name):
    """A thin Blueprint child of a native ability class, created once. Returns (Blueprint, generated class)"""
    path = f'{ABILITY_DIR}/{name}'
    bp = unreal.load_asset(path)
    if not bp:
        factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', getattr(unreal, parent_name).static_class())
        asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ABILITY_DIR, asset_class, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'created {path}')
    return bp, unreal.EditorAssetLibrary.load_blueprint_class(path)


def _strike_spec(row):
    total, faf, a0, a1, radius, reach0, reach1, height, damage, kb, angle, hs, ahs, montage, rate = row
    spec = unreal.GSTN_TestStrikeSpec()
    for prop, value in (('total_frames', total), ('first_actionable_frame', faf), ('active_start', a0), ('active_end', a1), ('radius', radius),
                        ('reach_from', reach0), ('reach_to', reach1), ('height', height), ('damage', damage), ('base_knockback', kb),
                        ('angle_deg', angle), ('hitstop_frames', hs), ('attacker_hitstop_frames', ahs), ('montage', _montage(montage)),
                        ('montage_play_rate', rate)):
        spec.set_editor_property(prop, value)
    return spec


def _copy(source, name):
    path = f'{DIR}/{name}'
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(f'{DIR}/{source}', path):
            log(f'ERROR: could not copy {source} to {name}')
            return None
        log(f'created {path} (copy of {source})')
    return unreal.load_asset(path)


def _rebind(bindings, command, cls):
    """The bindings with `command`'s ability replaced by cls (requirements kept). Returns (bindings, how many replaced)"""
    out, n = [], 0
    for b in bindings:
        if str(b.get_editor_property('command').get_editor_property('tag_name')) == command:
            b.set_editor_property('ability', cls)
            n += 1
        out.append(b)
    return out, n


def run_kit(key, spec):
    saves = []
    strikes = {}
    for move in ('jab', 'special'):
        bp, cls = _ability(f'GA_{key}_{move.capitalize()}', 'GSTN_GA_TestKitStrike')
        unreal.GSTN_TestKitContentLibrary.write_strike_move(cls, _strike_spec(spec[move]))
        strikes[move] = cls
        saves.append(bp)
    throws = {}
    for direction, row in spec['throws'].items():
        bp, cls = _ability(f'GA_{key}_Throw{direction}', THROW_PARENTS[direction])
        total, faf, frame, damage, kb, angle, montage, rate = row
        unreal.GSTN_LinkContentLibrary.write_throw_move(cls, total, faf, frame, damage, kb, angle, _montage(montage), rate)
        throws[direction] = cls
        saves.append(bp)

    weapon = _copy(SOURCE_WEAPON, f'WPN_Test_{key}')
    kit = _copy(SOURCE_KIT, f'KIT_Test_{key}')
    if not weapon or not kit:
        return []
    weapon.set_editor_property('display_name', f'WPN_Test_{key}')
    weapon.set_editor_property('weapon_tags', unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array(
        [_tag(f'Weapon.Test.{key}'), _tag('Test.Role.Primary')]))
    bindings, n = _rebind(list(weapon.get_editor_property('bindings')), 'Command.Attack.Neutral', strikes['jab'])
    weapon.set_editor_property('bindings', bindings)
    if not n:
        log(f'WARNING: WPN_Test_{key} has no Command.Attack.Neutral binding to give the jab')

    kit.set_editor_property('display_name', spec['label'])
    kit.set_editor_property('kit_tags', unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_tag(f'Kit.Test.{key}')]))
    kit.set_editor_property('primary_weapon', weapon)
    stats = unreal.GSTN_KitStats()
    values = []
    for stat, value in spec['stats'].items():
        v = unreal.GSTN_StatValue()
        v.set_editor_property('stat', _tag(f'Combat.SetByCaller.Stat.{stat}'))
        v.set_editor_property('value', value)
        values.append(v)
    stats.set_editor_property('values', values)
    kit.set_editor_property('stats', stats)
    kit.set_editor_property('init_stats_effect', getattr(unreal, INIT_EFFECT))
    bindings, n = _rebind(list(kit.get_editor_property('kit_bindings')), 'Command.Special.Neutral', strikes['special'])
    kit.set_editor_property('kit_bindings', bindings)
    if not n:
        log(f'WARNING: KIT_Test_{key} has no Command.Special.Neutral binding to give the special')
    # The grip: how long the grabbed side is held without mashing (frames at 60 Hz): the heavy's hold lasts longest
    unreal.GSTN_LinkContentLibrary.set_kit_grab_mash_frames(kit, spec['mash_frames'])
    if not unreal.GSTN_LinkContentLibrary.set_kit_throws(kit, throws['Forward'], throws['Back'], throws['Up'], throws['Down']):
        log(f'KIT_Test_{key}: throws unchanged (or no link extension)')
    return saves + [weapon, kit]


def run():
    del _lines[:]
    if not unreal.EditorAssetLibrary.does_directory_exist(ABILITY_DIR):
        unreal.EditorAssetLibrary.make_directory(ABILITY_DIR)
    saved = []
    for key, spec in KITS.items():
        for asset in run_kit(key, spec):
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            saved.append(asset.get_path_name())
    log(f'saved {len(saved)} assets: ' + ', '.join(p.rsplit('/', 1)[-1].split('.')[0] for p in saved))
    path = os.path.join(unreal.Paths.project_saved_dir(), 'Logs', 'k3', 'setup_archetypes.log')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'WARNING' in l or 'ERROR' in l][:4] + [_lines[-1][:300]]:
        print(line)
    return saved


if __name__ == '__main__':
    run()
