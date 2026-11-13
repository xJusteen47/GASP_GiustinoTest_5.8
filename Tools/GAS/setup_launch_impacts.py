# Launch, ground bounce, wall splat and knockdown content (owner asks of 2026-11-13). Idempotent: a rerun rewrites the same values.
# Run headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>"   or   import setup_launch_impacts as s; s.run()
#   1. GSTN_CombatRules_Standard: the weight driven model's PercentGrowth = 1 (knockback doubles every 100 %, uncapped; FillStandard has the
#      same value for the built-in copy) and a surface impact table, GSTN_SurfaceImpacts_Standard, with one row for Surface.Type.Wall (the
#      surface type of every collider without one, floors included): no default outcome, everything allowed. Nothing changes for an ordinary
#      hit; a hit whose Impact profile forces Splat or Bounce now gets it (before, no table = no impact at all).
#   2. Two clone moves of GA_Example_Strong's source in /Game/GASNPPTest/Examples: GA_Example_GroundBounce (a spike, angle -45, Impact Force
#      Bounce: bounces once off the floor) and GA_Example_WallSplat (low and fast, Impact Force Splat), on WPN_Example_Impacts (light X = ground
#      bounce, strong RT = wall splat) and KIT_Example_Impacts (label IMPATTI), a copy of KIT_Example.
#   3. The landing of a tumble (mini bounce, downed, get-up roll from the stick, neutral get-up on jump, no get-up attack, no shield tech: the
#      owner's spec) on the hub kits: FGSTN_DefenceKitExtension with the /GSTNDefence/Test get-up set, no shield, no tech set.
# Output: Saved/LaunchImpacts/setup.log, at most 5 printed lines.
import os

import unreal

RULES = '/GSTNCombat/Rules/GSTN_CombatRules_Standard'
TABLE_DIR, TABLE = '/GSTNCombat/Rules', 'GSTN_SurfaceImpacts_Standard'
DIR = '/Game/GASNPPTest/Examples'
SRC = '/Game/GASNPPTest/Combat/Archetypes/GA_Heavy_Jab'
DEF_TEST = '/GSTNDefence/Test'
TAKEDOWN = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_{}_A'
STRIKE = dict(total_frames=42, first_actionable_frame=36, active_start=14, active_end=18, radius=70.0, reach_from=90.0, reach_to=210.0,
              height=10.0, damage=12.0, knockback_growth=0.0, hitstop_frames=10, attacker_hitstop_frames=8, montage_play_rate=0.9)
MOVES = {
    'GA_Example_GroundBounce': (['Command.Attack.Neutral', 'Command.Attack.Directional'], 'BOUNCE', dict(base_knockback=1900.0, angle_deg=-45.0)),
    'GA_Example_WallSplat': (['Command.Strong.Neutral', 'Command.Strong.Directional'], 'SPLAT', dict(base_knockback=2000.0, angle_deg=15.0)),
}
LANDING_KITS = ['/Game/GASNPPTest/Combat/KIT_Test_Hits', '/Game/GASNPPTest/Combat/KIT_Test_Light',
                '/Game/GASNPPTest/Combat/KIT_Test_Medium', '/Game/GASNPPTest/Combat/KIT_Test_Heavy',
                '/Game/GASNPPTest/Combat/KIT_Test_Ranger', f'{DIR}/KIT_Example', f'{DIR}/KIT_Example_Impacts']

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[launch_impacts] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _copy(source, path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    if not unreal.EditorAssetLibrary.duplicate_asset(source, path):
        log(f'ERROR: could not copy {source} to {path}')
        return None
    log(f'created {path.rsplit("/", 1)[-1]}')
    return unreal.load_asset(path)


def _find_asset(path):
    """The asset at path, else the first asset of that name anywhere under /Game/GASNPPTest (archetype kits may live in another folder)"""
    asset = unreal.load_asset(path)
    if asset:
        return asset
    name = path.rsplit('/', 1)[-1]
    for found in unreal.EditorAssetLibrary.list_assets('/Game/GASNPPTest', True, False):
        if found.split('.')[0].rsplit('/', 1)[-1] == name:
            return unreal.load_asset(found.split('.')[0])
    return None


def _rules(saves):
    rules = unreal.load_asset(RULES)
    model = rules.get_editor_property('knockback_model')
    model.set_editor_property('percent_growth', 1.0)
    path = f'{TABLE_DIR}/{TABLE}'
    table = unreal.load_asset(path)
    if not table:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.GSTN_SurfaceImpactTable)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(TABLE, TABLE_DIR, unreal.GSTN_SurfaceImpactTable, factory)
        log(f'created {TABLE}')
    row = unreal.GSTN_SurfaceImpactRow()
    row.set_editor_property('surface', _tag('Surface.Type.Wall'))
    row.set_editor_property('default_outcome', unreal.GSTN_ImpactOutcome.NONE)
    row.set_editor_property('min_impact_speed', 300.0)
    row.set_editor_property('restitution', 0.6)
    row.set_editor_property('splat_frames', 40)
    table.set_editor_property('rows', [row])
    rules.set_editor_property('surface_impact_table', table)
    saves += [table, rules]
    log(f'rules: percent growth 1, impact table {TABLE}')


def _moves(saves):
    classes = {}
    for name, (_commands, force, extra) in MOVES.items():
        bp = _copy(SRC, f'{DIR}/{name}')
        if not bp:
            return None
        cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/{name}')
        spec = unreal.GSTN_TestStrikeSpec()
        for prop, value in dict(STRIKE, **extra).items():
            spec.set_editor_property(prop, value)
        spec.set_editor_property('montage', unreal.load_asset(TAKEDOWN.format('F')))
        impact = unreal.GSTN_ImpactProfile()
        impact.set_editor_property('force', getattr(unreal.GSTN_ImpactOutcome, force))
        spec.set_editor_property('impact', impact)
        unreal.GSTN_TestKitContentLibrary.write_strike_move(cls, spec)
        classes[name] = cls
        saves.append(bp)
    weapon = _copy(f'{DIR}/WPN_Example', f'{DIR}/WPN_Example_Impacts')
    kit = _copy(f'{DIR}/KIT_Example', f'{DIR}/KIT_Example_Impacts')
    if not weapon or not kit:
        return None
    weapon.set_editor_property('display_name', 'WPN_Example_Impacts')
    weapon.set_editor_property('weapon_tags', unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array(
        [_tag('Weapon.Example.Impacts'), _tag('Test.Role.Primary')]))
    bindings = list(weapon.get_editor_property('bindings'))
    for b in bindings:
        command = str(b.get_editor_property('command').get_editor_property('tag_name'))
        for name, (commands, _f, _e) in MOVES.items():
            if command in commands:
                b.set_editor_property('ability', classes[name])
    weapon.set_editor_property('bindings', bindings)
    kit.set_editor_property('display_name', 'IMPATTI')
    kit.set_editor_property('kit_tags', unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_tag('Kit.Example.Impacts')]))
    kit.set_editor_property('primary_weapon', weapon)
    saves += [weapon, kit]
    log('moves: X ground bounce, RT wall splat on KIT_Example_Impacts')
    return classes


def _landing(saves):
    def d(n):
        # load_asset, not load_blueprint_class: a headless run has not scanned plugin content into the asset registry
        bp = unreal.load_asset(f'{DEF_TEST}/{n}')
        return bp.generated_class() if bp else None
    done = []
    for path in LANDING_KITS:
        kit = _find_asset(path)
        if not kit:
            log(f'WARNING: no kit {path}')
            continue
        unreal.GSTN_DefenceContentLibrary.write_kit_defence_extension(
            kit, None, None, d('GA_GroundBounce_Test'), d('GA_GetupRoll_Test'), d('GA_GetupNeutral_Test'), None, None, None)
        saves.append(kit)
        done.append(kit.get_name())
    log('landing on: ' + ', '.join(done))


def run():
    del _lines[:]
    saves = []
    _rules(saves)
    _moves(saves)
    _landing(saves)
    for asset in saves:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log(f'saved {len(saves)}')
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'LaunchImpacts')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    return _lines[-5:]


if __name__ == '__main__':
    for line in run():
        print(line)
