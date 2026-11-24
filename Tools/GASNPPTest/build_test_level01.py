# M12 G7: assembles L_Test_Level01 (the example level of the Levels mode) through the editor Python API, in a fresh blank map.
#
#   UnrealEditor-Cmd GASP_Giustino2.uproject -ExecCmds="py Tools/GASNPPTest/build_test_level01.py" -unattended -nullrhi        (dry run)
#   ... "py Tools/GASNPPTest/build_test_level01.py --save"                                                                      (owner, once)
#
# Dry run (default): builds the level and the test data in memory, reports what it placed, saves NOTHING, so a headless run never dirties Content/.
# --save: writes the map to /Game/Levels/Test/ and the data assets (level set, experience, mission, game rules) to /Game/GSTN/_Test/Maps/Level01/
# (test-maps.md rule 1; both folders are Git LFS, no locks). It refuses to overwrite an existing map unless --overwrite is given.
#
# Layout, along +X (1 unit = 1 cm, every platform 20 x 10 m, 5 m gaps a jump clears, a red KO zone 10 m below):
#   P1 start (x -1000..1000): 4 PlayerStarts 3 m apart, checkpoint A, a ground kit (when GSTNPickups is loaded)
#   P2 (x 1500..3500):        checkpoint B, collectible 1, a minion spawner (M8, only when the class exists)
#   P3 (x 4000..6000):        collectible 2, the goal zone
# Only actors that already exist in the plugins are placed (test-maps.md rule 6): no Blueprint or C++ is touched. A missing optional class is
# written to the report, not built.
import argparse
import json
import sys

import unreal

LOG = '[build_test_level01]'
MAP_DIR = '/Game/Levels/Test'
MAP_NAME = 'L_Test_Level01'
DATA_DIR = '/Game/GSTN/_Test/Maps/Level01'

CUBE = '/Engine/BasicShapes/Cube.Cube'
FLOOR_SIZE = unreal.Vector(20.0, 10.0, 1.0)   # cube scale: 20 m x 10 m x 1 m
PLATFORMS = (('P1_Start', 0.0), ('P2_Middle', 2500.0), ('P3_Goal', 5000.0))   # centre X in cm


def _log(msg):
    unreal.log(f'{LOG} {msg}')


def _warn(msg):
    unreal.log_warning(f'{LOG} {msg}')


class Report:
    """What was placed, what was skipped and why. Printed as one JSON line the run script can grep."""

    def __init__(self):
        self.placed = {}
        self.skipped = []
        self.problems = []

    def add(self, kind, label):
        self.placed.setdefault(kind, []).append(label)

    def skip(self, what, why):
        self.skipped.append(f'{what}: {why}')
        _warn(f'skipped {what}: {why}')

    def problem(self, what):
        self.problems.append(what)
        unreal.log_error(f'{LOG} {what}')

    def dump(self, saved):
        out = {'placed': {k: len(v) for k, v in self.placed.items()}, 'skipped': self.skipped, 'problems': self.problems, 'saved': saved}
        print(f'{LOG} RESULT {json.dumps(out, sort_keys=True)}')


def _tag(name):
    """A GameplayTag from its name (ini leaves may be missing in a bare checkout: None then)."""
    try:
        tag = unreal.GameplayTag()
        tag.import_text(f'(TagName="{name}")')
        return tag
    except Exception as e:   # noqa: BLE001
        _warn(f'tag {name} not set: {e!r}')
        return None


def _spawn(cls, label, location, rot=None, report=None, kind=None):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = subsystem.spawn_actor_from_class(cls, location, rot or unreal.Rotator(0, 0, 0))
    if actor is None:
        if report:
            report.problem(f'could not spawn {label} ({cls})')
        return None
    actor.set_actor_label(label)
    if report and kind:
        report.add(kind, label)
    return actor


def _optional_class(name):
    return getattr(unreal, name, None)


def _make_floor(label, center_x, report):
    mesh = unreal.EditorAssetLibrary.load_asset(CUBE)
    actor = _spawn(unreal.StaticMeshActor, label, unreal.Vector(center_x, 0.0, -50.0), report=report, kind='floor')
    if actor and mesh:
        comp = actor.get_editor_property('static_mesh_component')
        comp.set_static_mesh(mesh)
        actor.set_actor_scale3d(FLOOR_SIZE)
    return actor


def build_level(report):
    """Spawns every actor of the level into the open (blank) map."""
    for label, x in PLATFORMS:
        _make_floor(label, x, report)

    # Four starts 3 m apart (test-maps rule 5), facing +X
    for i in range(4):
        _spawn(unreal.PlayerStart, f'PlayerStart_{i}', unreal.Vector(-800.0, -450.0 + 300.0 * i, 100.0), report=report, kind='player_start')

    # KO zone under the whole course: a fall out of the level, or into a gap, is a KO (red = KO, test-maps rule 2)
    ko = _spawn(unreal.GSTN_KOZone, 'KOZone_Pit', unreal.Vector(2500.0, 0.0, -1300.0), report=report, kind='ko_zone')
    if ko:
        ko.set_editor_property('half_extent', unreal.Vector(4000.0, 1200.0, 300.0))

    # Two checkpoints, in level order (the team comes back at the last activated one)
    for order, (label, x) in enumerate((('Checkpoint_A', 700.0), ('Checkpoint_B', 2900.0)), start=1):
        cp = _spawn(unreal.GSTN_Checkpoint, label, unreal.Vector(x, 0.0, 100.0), report=report, kind='checkpoint')
        if cp:
            cp.set_editor_property('order', order)
            cp.set_editor_property('spawn_points', [unreal.Transform(unreal.Vector(0.0, 0.0, 20.0)), unreal.Transform(unreal.Vector(0.0, 150.0, 20.0))])

    # Two collectibles of the Levels mode
    for label, x in (('Collectible_1', 3100.0), ('Collectible_2', 4600.0)):
        _spawn(unreal.GSTN_LevelCollectible, label, unreal.Vector(x, 0.0, 120.0), report=report, kind='collectible')

    # The goal: one alive player starts the grace timer (GM.5)
    goal = _spawn(unreal.GSTN_LevelGoalZone, 'Goal', unreal.Vector(5600.0, 0.0, 150.0), report=report, kind='goal')
    if goal:
        tag = _tag('Mission.Objective.Goal')
        if tag:
            goal.set_editor_property('zone_tag', tag)

    # Optional pieces: placed only when their plugin is loaded in this editor
    spawner = _optional_class('GSTN_MinionSpawnerActor')
    if spawner:
        _spawn(spawner, 'MinionSpawner_M8', unreal.Vector(2500.0, 400.0, 100.0), report=report, kind='minion_spawner')
    else:
        report.skip('minion spawner', 'GSTN_MinionSpawnerActor is not loaded (GSTNMinion off); add it by hand and re-run')

    kit = _optional_class('GSTN_KitPickup')
    if kit:
        _spawn(kit, 'GroundKit', unreal.Vector(800.0, 300.0, 60.0), report=report, kind='ground_kit')
    else:
        report.skip('ground kit', 'GSTN_KitPickup is not loaded (GSTNPickups off)')

    # Light, so the map is not black when the owner opens it
    _spawn(unreal.DirectionalLight, 'Sun', unreal.Vector(0.0, 0.0, 800.0), unreal.Rotator(0.0, -45.0, 30.0), report=report, kind='light')
    _spawn(unreal.SkyLight, 'Sky', unreal.Vector(0.0, 0.0, 900.0), report=report, kind='light')

    _label_board(report)


def _label_board(report):
    """One board at the start listing the stations (test-maps rule 3)."""
    actor = _spawn(unreal.TextRenderActor, 'Board', unreal.Vector(-900.0, -600.0, 250.0), unreal.Rotator(0.0, 0.0, 0.0), report=report, kind='label')
    if actor:
        comp = actor.get_editor_property('text_render')
        comp.set_text('L_Test_Level01\n1: start, kit, checkpoint A\n2: gap 5 m, checkpoint B, collectible 1, spawner\n3: gap 5 m, collectible 2, goal\nRed box below: KO zone')


def _ref_world(map_path):
    """TSoftObjectPtr<UWorld> value for a map package path (best effort across engine builds)."""
    return unreal.SoftObjectPath(f'{map_path}.{map_path.rsplit("/", 1)[-1]}')


def _data_assets(report, save):
    """The level set, the Experience, a Reach mission and the GameRules asset. In memory unless save is on (then real assets under DATA_DIR)."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    def make(cls_name, name):
        cls = _optional_class(cls_name)
        if cls is None:
            report.skip(name, f'{cls_name} is not loaded')
            return None
        try:
            if save:
                factory = unreal.DataAssetFactory()
                factory.set_editor_property('data_asset_class', cls)
                return tools.create_asset(name, DATA_DIR, cls, factory)
            return unreal.new_object(cls)   # transient: dies with the editor
        except Exception as e:   # noqa: BLE001
            report.problem(f'could not create {name}: {e!r}')
            return None

    map_path = f'{MAP_DIR}/{MAP_NAME}'
    level_set = make('GSTN_LevelSetDefinition', 'LS_Test_Level01')
    if level_set:
        try:
            entry = unreal.GSTN_LevelEntry()
            entry.set_editor_property('level_id', 'Level01')
            entry.set_editor_property('map', _ref_world(map_path))
            level_set.set_editor_property('levels', [entry])
            report.add('data', 'LevelSet')
        except Exception as e:   # noqa: BLE001
            report.skip('level set entry', repr(e))

    experience = make('GSTN_ExperienceDefinition', 'EXP_Test_Levels')
    if experience:
        try:
            experience.set_editor_property('game_features_to_enable', ['GSTNMode_Levels'])
            tag = _tag('Experience.Mode.Levels')
            if tag:
                experience.set_editor_property('mode_tag', tag)
            report.add('data', 'Experience')
        except Exception as e:   # noqa: BLE001
            report.skip('experience fields', repr(e))

    mission = make('GSTN_MissionDefinition', 'MD_Test_Level01')
    if mission:
        try:
            rule = unreal.new_object(unreal.GSTN_ObjectiveRule_Reach, outer=mission)
            goal_tag = _tag('Mission.Objective.Goal')
            if goal_tag:
                rule.set_editor_property('zone_tag', goal_tag)
            spec = unreal.GSTN_ObjectiveSpec()
            if goal_tag:
                spec.set_editor_property('objective_tag', goal_tag)
            spec.set_editor_property('rule', rule)
            mission.set_editor_property('objectives', [spec])
            report.add('data', 'Mission')
        except Exception as e:   # noqa: BLE001
            report.skip('mission objective', repr(e))

    rules = make('GSTN_GameRules', 'GR_Test_Level01')
    if rules:
        try:
            pieces = []
            for cls_name in ('GSTN_LevelsRules', 'GSTN_RespawnRules', 'GSTN_MissionRules'):
                cls = _optional_class(cls_name)
                if cls is None:
                    report.skip(f'rules piece {cls_name}', 'class not loaded')
                    continue
                piece = unreal.new_object(cls, outer=rules)
                piece.set_editor_property('enabled', True)
                if cls_name == 'GSTN_LevelsRules' and level_set:
                    piece.set_editor_property('level_set', level_set)
                if cls_name == 'GSTN_MissionRules' and mission:
                    piece.set_editor_property('missions', [mission])
                pieces.append(piece)
            rules.set_editor_property('pieces', pieces)
            # The level picks these rules by being listed here (GameRules selection: URL option, then level, then game mode)
            rules.set_editor_property('levels', [_ref_world(map_path)])
            report.add('data', 'GameRules')
        except Exception as e:   # noqa: BLE001
            report.skip('game rules pieces', repr(e))
    return {'level_set': level_set, 'experience': experience, 'mission': mission, 'rules': rules}


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--save', action='store_true', help='write the map and the data assets (default: dry run, saves nothing)')
    parser.add_argument('--overwrite', action='store_true', help='with --save, replace an existing L_Test_Level01')
    args, _ = parser.parse_known_args(argv)

    report = Report()
    map_path = f'{MAP_DIR}/{MAP_NAME}'
    if args.save and unreal.EditorAssetLibrary.does_asset_exist(map_path) and not args.overwrite:
        report.problem(f'{map_path} exists; pass --overwrite to replace it')
        report.dump(False)
        return 1

    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        report.problem('could not create a blank map')
        report.dump(False)
        return 1

    build_level(report)
    assets = _data_assets(report, args.save)

    saved = False
    if args.save:
        saved = bool(unreal.EditorLoadingAndSavingUtils.save_map(world, map_path))
        if not saved:
            report.problem(f'save_map failed for {map_path}')
        for name, asset in assets.items():
            if asset is not None:
                unreal.EditorAssetLibrary.save_loaded_asset(asset)
        _log(f'saved {map_path} and the data assets under {DATA_DIR}')
    else:
        _log('dry run: nothing saved (pass --save to write the map and the data assets)')

    # Required pieces of the plan: start, two checkpoints, a KO zone, goal, two collectibles
    need = {'player_start': 4, 'checkpoint': 2, 'ko_zone': 1, 'goal': 1, 'collectible': 2}
    for kind, count in need.items():
        have = len(report.placed.get(kind, []))
        if have != count:
            report.problem(f'{kind}: placed {have}, expected {count}')

    report.dump(saved)
    return 1 if report.problems else 0


code = main(sys.argv[1:])
if code:
    unreal.log_error(f'{LOG} finished with problems (see RESULT line)')
