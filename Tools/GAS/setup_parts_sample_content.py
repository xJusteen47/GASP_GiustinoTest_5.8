# Writes the plugin-owned sample content of GSTNParts under /GSTNParts/Samples (M4b P14). Idempotent: assets that exist are rewritten from the specs
# below (data assets) or left alone (Blueprints, a designer's graph). No /Game/ reference: the part sets name hurtboxes (FName) and the table is a soft
# reference, so the folder works in any project that enables the plugin.
#
# Run in the editor Python console (the GSTNParts plugin enabled):
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_parts_sample_content as s; s.run()
# or headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_parts_sample_content.py" -stdout
#
# Assets:
#   PT_Sample_Default     penetration table, the three default bands (above hardness full damage, equal about 65 %, below ricochet)
#   PS_Sample_Fighter     six parts (Torso, Head weak point, ArmL, ArmR, Legs, Core with Thickness 0.5), table PT_Sample_Default
#   PS_Sample_Player      one part: a player is a single part whose break is death
#   BP_PartStats_Example  Blueprint child of UGSTN_GE_PartStats (the Stats break effect's class; add modifiers on the combat attribute set in the editor)
#   BP_HitStep_PartResolve_Example  Blueprint child of UGSTN_HitStep_PartResolve (override Resolve to extend the step)
import unreal

SAMPLE_DIR = '/GSTNParts/Samples'


def log(msg):
    unreal.log(f'[setup_parts_sample_content] {msg}')


def samples_specs():
    """Plain data. Parts: (name, hurtbox, hardness, thickness, cap, main transfer, weak point, broken policy). Hurtbox names are the ones of a fighter's
    hurtbox set (the six of HBS_Test_PawnParts in the test project)"""
    return {
        'table': 'PT_Sample_Default',
        'fighter': 'PS_Sample_Fighter',
        'fighter_parts': [
            ('Torso', 'torso', 0, 0.0, 120.0, 1.0, False, 'KEEP'),
            ('Head', 'head', 1, 0.0, 40.0, 2.0, True, 'KEEP'),
            ('Legs', 'feet', 2, 0.0, 60.0, 0.5, False, 'KEEP'),
            ('ArmL', 'arm_l', 3, 0.0, 50.0, 0.3, False, 'DISABLE'),
            ('ArmR', 'arm_r', 3, 0.0, 50.0, 0.3, False, 'DISABLE'),
            ('Core', 'core', 1, 0.5, 30.0, 1.5, True, 'KEEP'),
        ],
        'player': 'PS_Sample_Player',
        'player_part': ('Body', 'torso', 0, 0.0, 100.0, 1.0, False, 'KEEP'),
        # (name, parent class name in unreal.*)
        'blueprints': [
            ('BP_PartStats_Example', 'GSTN_GE_PartStats'),
            ('BP_HitStep_PartResolve_Example', 'GSTN_HitStep_PartResolve'),
        ],
    }


def _get_or_create(name, asset_class, factory):
    asset = unreal.load_asset(f'{SAMPLE_DIR}/{name}')
    if asset:
        return asset
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, SAMPLE_DIR, asset_class, factory)
    log(f'created {SAMPLE_DIR}/{name}')
    return asset


def _make_part(spec):
    name, hurtbox, hardness, thickness, cap, transfer, weak, policy = spec
    part = unreal.GSTN_PartDefinition()
    part.set_editor_property('name', name)
    part.set_editor_property('hurtboxes', [hurtbox])
    part.set_editor_property('hardness', hardness)
    part.set_editor_property('thickness', thickness)
    part.set_editor_property('part_cap', cap)
    part.set_editor_property('main_transfer', transfer)
    part.set_editor_property('weak_point', weak)
    part.set_editor_property('broken_policy', getattr(unreal.GSTN_BrokenHurtboxPolicy, policy))
    part.set_editor_property('on_break', [])
    return part


def setup_data_assets():
    specs = samples_specs()
    # A new table starts as the three-band default of the constructor, so there is nothing to fill in
    table = _get_or_create(specs['table'], unreal.GSTN_PenetrationTable, unreal.DataAssetFactory())
    unreal.EditorAssetLibrary.save_loaded_asset(table)

    fighter = _get_or_create(specs['fighter'], unreal.GSTN_PartSetDefinition, unreal.DataAssetFactory())
    fighter.set_editor_property('parts', [_make_part(p) for p in specs['fighter_parts']])
    fighter.set_editor_property('table', table)
    # Arms: losing one stops its own hit groups (indices 3 and 4); the Core adds 25 % main damage at the break; the Legs throw the victim
    unreal.GSTN_PartsContentLibrary.set_break_disable_hit_groups(fighter, 3, 1 << 1)
    unreal.GSTN_PartsContentLibrary.set_break_disable_hit_groups(fighter, 4, 1 << 2)
    unreal.GSTN_PartsContentLibrary.set_break_main_damage(fighter, 5, 25.0, False)
    unreal.GSTN_PartsContentLibrary.set_break_launch(fighter, 2, 900.0, 0.6)
    unreal.EditorAssetLibrary.save_loaded_asset(fighter)

    player = _get_or_create(specs['player'], unreal.GSTN_PartSetDefinition, unreal.DataAssetFactory())
    player.set_editor_property('parts', [_make_part(specs['player_part'])])
    unreal.EditorAssetLibrary.save_loaded_asset(player)


def setup_blueprints():
    """Created once; a rerun leaves an existing Blueprint (a designer's graph) alone"""
    for name, parent_name in samples_specs()['blueprints']:
        if unreal.load_asset(f'{SAMPLE_DIR}/{name}'):
            continue
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', getattr(unreal, parent_name).static_class())
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, SAMPLE_DIR, unreal.Blueprint, factory)
        if bp is None:
            log(f'ERROR: could not create {name}')
            continue
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        log(f'created {SAMPLE_DIR}/{name} (parent {parent_name})')


def run():
    if not hasattr(unreal, 'GSTN_PartSetDefinition'):
        log('ERROR: the GSTNParts plugin is not enabled: sample content skipped')
        return False
    setup_data_assets()
    setup_blueprints()
    log(f'sample content written under {SAMPLE_DIR}')
    return True


if __name__ == '__main__':
    run()
