# Builds /Game/GASNPPTest/Combat/L_Test_Combat, the combat test map (TM2, first stations; M4 H13a). Idempotent: it recreates the map from the specs below.
#
#   headless: UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/build_test_combat_map.py" -unattended -NullRHI
#   editor:   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import build_test_combat_map as m; m.run()
#
# Stations along +X from the player starts (all on one big floor):
#   wall        a thick box wall (Surface.Type.Wall is the default type of unmarked geometry; PM_Test_Wall on the mesh)
#   glass       AGSTN_TestSurface with Surface.Type.Glass: a provider that breaks (BreakSurface removes the collision), PM_Test_Glass on its look-alike
#   KO zone     AGSTN_KOZone far out on the side: anything that enters it is KO'd
#   trampolines flat / tilted / wall-mounted, three sizes and speeds (AGSTN_Trampoline of GSTNLevelObjects, static data)
# The map sets GM_GASNPPTest as its game mode, so it plays with the test character; the harness (Tools/GASNPPTest/gasnpp_test.py) can run on it:
# open it in the editor instead of DefaultLevel_Harness, then setup_editor(True) as usual (see docs/architecture/testing.md "Test map").
# The scenarios spawn their own props at run time and clear only what they spawned; the stations here are level actors and stay.
#
# Never saves DefaultLevel_Harness or any other map: it builds a new blank world and saves only L_Test_Combat.
import unreal

MAP_PATH = '/Game/GASNPPTest/Combat/L_Test_Combat'
COMBAT_DIR = '/Game/GASNPPTest/Combat'
CUBE = '/Engine/BasicShapes/Cube.Cube'
GAME_MODE = '/Game/GASNPPTest/GM_GASNPPTest.GM_GASNPPTest_C'


def log(msg):
    unreal.log(f'[build_test_combat_map] {msg}')


def specs():
    """The stations as plain data. Positions in cm; boxes are (centre, full size, yaw)"""
    return {
        'floor': ((1500.0, 0.0, -50.0), (6000.0, 4000.0, 100.0)),
        'starts': [((0.0, -300.0, 100.0), 0.0), ((0.0, 300.0, 100.0), 0.0), ((-300.0, 0.0, 100.0), 0.0)],
        'wall': ((2400.0, -900.0, 300.0), (80.0, 800.0, 600.0)),
        'glass': ((2400.0, 0.0, 300.0), (10.0, 300.0, 300.0)),  # HalfExtent of the provider is half of this
        'ko_zone': ((1500.0, 1700.0, 150.0), (400.0, 150.0, 400.0)),  # AGSTN_KOZone.HalfExtent
        # (centre, up vector, size, speed): flat / tilted 25 deg / wall mounted (a face 70 deg from the horizontal), different sizes and speeds
        'trampolines': [
            ((900.0, -700.0, 20.0), (0.0, 0.0, 1.0), (300.0, 300.0, 40.0), 900.0),
            ((1500.0, -700.0, 60.0), (0.423, 0.0, 0.906), (200.0, 200.0, 40.0), 1100.0),
            ((1500.0, 800.0, 240.0), (0.940, 0.0, 0.342), (600.0, 400.0, 60.0), 1300.0),
        ],
    }


def _spawn(actor_class, location, rotation=None):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return subsystem.spawn_actor_from_class(actor_class, unreal.Vector(*location), rotation or unreal.Rotator(0.0, 0.0, 0.0))


def _box_actor(label, centre, size, yaw=0.0, phys_material=None):
    mesh = unreal.load_asset(CUBE)
    actor = _spawn(unreal.StaticMeshActor, centre, unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(label)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    # The engine cube is 100 cm a side
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if phys_material:
        material = unreal.load_asset(f'{COMBAT_DIR}/{phys_material}')
        body = component.get_editor_property('body_instance')
        body.set_editor_property('phys_material_override', material)
        component.set_editor_property('body_instance', body)
    return actor


def _rotation_from_up(up):
    """A rotator whose local Z is the given up vector (yaw 0 around it)"""
    vector = unreal.Vector(*up)
    return unreal.MathLibrary.make_rot_from_z(vector.normal())


def run():
    data = specs()
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    _box_actor('Floor', data['floor'][0], data['floor'][1])
    for index, (location, yaw) in enumerate(data['starts']):
        start = _spawn(unreal.PlayerStart, location, unreal.Rotator(0.0, 0.0, yaw))
        start.set_actor_label(f'PlayerStart_{index}')

    _box_actor('Station_Wall', data['wall'][0], data['wall'][1], phys_material='PM_Test_Wall')

    # The glass panel: a provider that breaks. A look-alike mesh with PM_Test_Glass sits inside the provider's box (no collision of its own)
    glass_centre, glass_size = data['glass']
    glass = _spawn(unreal.GSTN_TestSurface, glass_centre)
    glass.set_actor_label('Station_Glass')
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="Surface.Type.Glass")')
    glass.set_editor_property('surface_type', tag)
    glass.set_editor_property('half_extent', unreal.Vector(glass_size[0] * 0.5, glass_size[1] * 0.5, glass_size[2] * 0.5))
    look = _box_actor('Station_Glass_Look', glass_centre, glass_size, phys_material='PM_Test_Glass')
    look.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    look.attach_to_actor(glass, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)

    centre, extent = data['ko_zone']
    zone = _spawn(unreal.GSTN_KOZone, centre)
    zone.set_actor_label('Station_KOZone')
    zone.set_editor_property('half_extent', unreal.Vector(*extent))

    for index, (centre, up, size, speed) in enumerate(data['trampolines']):
        trampoline = _spawn(unreal.GSTN_Trampoline, centre, _rotation_from_up(up))
        trampoline.set_actor_label(f'Station_Trampoline_{index}')
        trampoline.set_editor_property('size', unreal.Vector(*size))
        launch = trampoline.get_editor_property('launch')
        launch.set_editor_property('direction_mode', unreal.GSTN_SurfaceLaunchDirection.LOCAL_DIRECTION)
        launch.set_editor_property('local_direction', unreal.Vector(0.0, 0.0, 1.0))
        launch.set_editor_property('speed_mode', unreal.GSTN_SurfaceLaunchSpeed.FIXED)
        launch.set_editor_property('speed', speed)
        trampoline.set_editor_property('launch', launch)

    light = _spawn(unreal.DirectionalLight, (0.0, 0.0, 800.0), unreal.Rotator(-45.0, 30.0, 0.0))
    light.set_actor_label('Sun')
    sky = _spawn(unreal.SkyLight, (0.0, 0.0, 900.0))
    sky.set_actor_label('Sky')

    settings = world.get_world_settings()
    game_mode = unreal.load_class(None, GAME_MODE)
    if game_mode:
        settings.set_editor_property('default_game_mode', game_mode)
    else:
        log(f'WARNING: game mode {GAME_MODE} not found, the map keeps the project default')

    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
    log(f'{MAP_PATH} saved: {saved}')
    return saved


if __name__ == '__main__':
    run()
