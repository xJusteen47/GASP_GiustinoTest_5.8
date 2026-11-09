# Sets up the M8 minion test content (plan m8-minion.md G3) in the GSTNMinion plugin's own content, /GSTNMinion/Test. Idempotent:
# running it again rewrites the same values and creates nothing new. Needs the GSTNMinion plugin enabled (its classes and the tags of
# Plugins/GSTNMinion/Config/Tags/GSTNMinion.ini). Never touches /Game content, a shared Blueprint or a map that already exists.
#
# Run from the editor Python console:
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_minion_assets as s; s.run()
# or headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_minion_assets.py"
#
# Creates:
#  1. HBS_Minion_Grunt / HBS_Minion_Heavy: one body capsule each (the heavy one larger, with a stagger threshold).
#  2. TL_Minion_Swing (32 frames, a sphere hitbox frames 10-14 in front of the root, 8 damage), TL_Minion_Hitstun (20 frames) and
#     TL_Minion_Death (40 frames): the reaction timelines IsDataValid requires. No animation (the puppets come with G7).
#  3. DA_Minion_Grunt (Minion.Type.Grunt, 40 HP, walk 200, one move Command.Minion.Attack.Swing -> TL_Minion_Swing) and
#     DA_Minion_Heavy (Minion.Type.Heavy, 120 HP, walk 140, weight 200, the same swing with a longer cooldown).
#  3b. DA_MinionWave_Test (UGSTN_MinionWaveDefinition, acceptance 3 of M8 13): 6 grunts at once plus 2 heavies 125 frames (2 s) later,
#     spawned around the spawner actor. Play it without C++: a spawner actor with this Wave, or
#     `gstn.Minion.Bench.WaveAsset /GSTNMinion/Test/DA_MinionWave_Test.DA_MinionWave_Test` then `gstn.Minion.Bench.Wave`.
#     The Brain field of both definitions is left empty: the test StateTree is built in code by the editor module (transient), there is no
#     StateTree asset. A designer assigns their own StateTree asset (schema UGSTN_MinionStateTreeSchema) in the definition.
#  4. L_MinionYard: a flat floor (engine plane, 100 m), a PlayerStart and a directional + sky light. Only created when missing: once it
#     exists the owner edits it by hand and this script never opens or saves it again.
# Existing assets are copied to Saved/Backups/GAS before being changed.
import os, shutil
import unreal

TEST_DIR = '/GSTNMinion/Test'
PLUGIN_CONTENT = 'Plugins/GSTNMinion/Content/'
YARD_MAP = f'{TEST_DIR}/L_MinionYard'


def log(msg):
    unreal.log(f'[setup_minion_assets] {msg}')


def _disk_path(asset_path, ext='.uasset'):
    """The file of a /GSTNMinion/ asset on disk (plugin content, not /Game)"""
    rel = asset_path[len('/GSTNMinion/'):]
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + PLUGIN_CONTENT + rel + ext)


def backup(asset_path, ext='.uasset'):
    src = _disk_path(asset_path, ext)
    if not os.path.exists(src):
        return
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, os.path.basename(src))
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        log(f'backup {src} -> {dst}')


def _make_tag(name):
    tag = unreal.GameplayTag()
    # TagName is read-only from Python: go through the text form.
    tag.import_text(f'(TagName="{name}")')
    return tag


def _get_or_create(name, asset_class, factory):
    path = f'{TEST_DIR}/{name}'
    asset = unreal.load_asset(path)
    if asset:
        backup(path)
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, TEST_DIR, asset_class, factory)
    log(f'created {path}')
    return asset, True


def minion_specs():
    """name: (type tag, max health, walk, run, weight, capsule radius, capsule half height, swing cooldown, stagger threshold)"""
    return {
        'Grunt': ('Minion.Type.Grunt', 40.0, 200.0, 450.0, 100.0, 34.0, 88.0, 60, 0.0),
        'Heavy': ('Minion.Type.Heavy', 120.0, 140.0, 300.0, 200.0, 50.0, 100.0, 120, 12.0),
    }


def setup_hurtbox_set(name, radius, half_height, threshold):
    asset, _ = _get_or_create(f'HBS_Minion_{name}', unreal.GSTN_HurtboxSetDefinition, unreal.DataAssetFactory())
    shape = unreal.GSTN_HitShape()
    shape.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
    shape.set_editor_property('extent', unreal.Vector(radius, radius, half_height))
    shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(0.0, 0.0, half_height)))
    box = unreal.GSTN_Hurtbox()
    box.set_editor_property('name', 'Body')
    box.set_editor_property('shape', shape)
    asset.set_editor_property('hurtboxes', [box])
    if threshold > 0.0:
        stagger = unreal.GSTN_StaggerThreshold()
        stagger.set_editor_property('value', threshold)
        asset.set_editor_property('stagger_threshold', stagger)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset


def _hit_data(damage):
    hit = unreal.GSTN_HitData()
    hit.set_editor_property('damage', damage)
    hit.set_editor_property('base_knockback', 300.0)
    hit.set_editor_property('angle_deg', 20.0)
    return hit


def setup_timelines():
    """The swing and the two required reactions. Rewritten on every run (same values)"""
    timelines = {}
    for name, total in (('TL_Minion_Swing', 32), ('TL_Minion_Hitstun', 20), ('TL_Minion_Death', 40)):
        asset, _ = _get_or_create(name, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        data = unreal.GSTN_CombatTimelineData()
        data.set_editor_property('total_frames', total)
        data.set_editor_property('first_actionable_frame', total)
        if name == 'TL_Minion_Swing':
            part = unreal.GSTN_PartSelector()
            part.set_editor_property('group_name', 'Body')
            spec = unreal.GSTN_HitboxSpec()
            shape = unreal.GSTN_HitShape()
            shape.set_editor_property('type', unreal.GSTN_ShapeType.SPHERE)
            shape.set_editor_property('extent', unreal.Vector(40.0, 40.0, 40.0))
            shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(90.0, 0.0, 90.0)))
            spec.set_editor_property('shape', shape)
            spec.set_editor_property('space', unreal.GSTN_HitboxSpace.ROOT)
            spec.set_editor_property('hit_group', 0)
            spec.set_editor_property('data', _hit_data(8.0))
            data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, 10, 15, part, [spec])
        asset.set_editor_property('data', data)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        timelines[name] = asset
    log('timelines written')
    return timelines


def setup_definition(name, spec, timelines):
    tag, health, walk, run, weight, radius, half_height, cooldown, threshold = spec
    hurtboxes = setup_hurtbox_set(name, radius, half_height, threshold)
    asset, _ = _get_or_create(f'DA_Minion_{name}', unreal.GSTN_MinionDefinition, unreal.DataAssetFactory())
    asset.set_editor_property('type_tag', _make_tag(tag))

    stats = unreal.GSTN_MinionStats()
    stats.set_editor_property('max_health', health)
    stats.set_editor_property('walk_speed', walk)
    stats.set_editor_property('run_speed', run)
    stats.set_editor_property('weight', weight)
    stats.set_editor_property('capsule_radius', radius)
    stats.set_editor_property('capsule_half_height', half_height)
    asset.set_editor_property('stats', stats)
    asset.set_editor_property('hurtbox_set', hurtboxes)

    move = unreal.GSTN_MinionMove()
    move.set_editor_property('command', _make_tag('Command.Minion.Attack.Swing'))
    move.set_editor_property('timeline', timelines['TL_Minion_Swing'])
    move.set_editor_property('cooldown_frames', cooldown)
    move.set_editor_property('min_range', 0.0)
    move.set_editor_property('max_range', 180.0)
    asset.set_editor_property('moves', [move])

    reactions = unreal.GSTN_MinionReactionTimelines()
    reactions.set_editor_property('hitstun', timelines['TL_Minion_Hitstun'])
    reactions.set_editor_property('death', timelines['TL_Minion_Death'])
    asset.set_editor_property('reactions', reactions)

    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f'definition DA_Minion_{name} written')
    return asset


def setup_wave(definitions):
    """DA_MinionWave_Test: two entries (grunts now, heavies 2 s later). Rewritten on every run (same values)"""
    asset, _ = _get_or_create('DA_MinionWave_Test', unreal.GSTN_MinionWaveDefinition, unreal.DataAssetFactory())
    entries = []
    for name, count, delay in (('Grunt', 6, 0), ('Heavy', 2, 125)):
        entry = unreal.GSTN_MinionWaveEntry()
        entry.set_editor_property('definition', definitions[name])
        entry.set_editor_property('count', count)
        entry.set_editor_property('delay_frames', delay)
        entry.set_editor_property('ring_radius', 300.0)
        entries.append(entry)
    asset.set_editor_property('entries', entries)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log('wave DA_MinionWave_Test written')
    return asset


def setup_yard():
    """L_MinionYard, created only when missing (never reopened or saved afterwards: the owner may have it open)"""
    if unreal.EditorAssetLibrary.does_asset_exist(YARD_MAP):
        log(f'{YARD_MAP} exists: left as it is')
        return False
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(YARD_MAP):
        log(f'could not create {YARD_MAP}')
        return False
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
    floor.set_actor_label('Floor')
    floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    floor.set_actor_scale3d(unreal.Vector(100.0, 100.0, 1.0))  # the plane is 1 m: 100 m x 100 m

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 100.0))
    start.set_actor_label('PlayerStart')

    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0), unreal.Rotator(0.0, -50.0, 30.0))
    sun.set_actor_label('Sun')
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 1000.0))
    sky.set_actor_label('SkyLight')

    levels.save_current_level()
    log(f'{YARD_MAP} created')
    return True


def run():
    timelines = setup_timelines()
    definitions = {}
    for name, spec in minion_specs().items():
        definitions[name] = setup_definition(name, spec, timelines)
    setup_wave(definitions)
    yard = setup_yard()
    # 5-line summary
    log(f'1/5 timelines: {", ".join(timelines)} in {TEST_DIR}')
    log(f'2/5 definitions: {", ".join("DA_Minion_" + n for n in definitions)} (hurtbox sets HBS_Minion_*)')
    log(f'3/5 wave: DA_MinionWave_Test (6 Grunt now, 2 Heavy at +125 frames)')
    log(f'4/5 Brain empty on both types: no StateTree asset exists (the test tree is transient, built by the editor module); assign one by hand')
    log(f'5/5 yard {YARD_MAP}: {"created" if yard else "left as it is"}; backups in Saved/Backups/GAS; try gstn.Minion.Bench.WaveAsset')


if __name__ == '__main__':
    run()
