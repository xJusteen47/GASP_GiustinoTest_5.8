# Fills the OPEN level with the playable test hub /Game/Levels/Test/L_Test_Hub (TH-1, docs/notes/handovers/test-level-brief.md). It never saves
# the map: the owner makes the empty level (File > New Level > Empty Level) and saves it with Ctrl+S. It only saves the hub's colour material
# instances under /Game/GASNPPTest/TestMaps/Hub (test-maps.md section 0.1).
#
#   editor:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import build_test_hub_map as m; m.run()
#
# Idempotent: every actor it spawns carries the tag TestHub; a run deletes those first and leaves anything else in the level alone.
# Zones (centres in m; test-maps.md section 0 colours: blue walkable, orange wall slide, red KO, green trampoline, yellow target):
#   Z0 plaza          ( 0,  0)  40x40 m, empty middle (the harness lanes at -15 m X/Y stay free), 4 PlayerStarts 3 m apart, the board
#   Z1 hits M4        (+40, 0)  the stations of L_Test_Combat (wall, glass, KO, 3 trampolines; build_test_combat_map.specs) + 30 m knockback lane
#   Z2 duel M6        (+75, 0)  a 20 m round arena on a 2 m bridge, a KO volume under it
#   Z3 piggyback M7   ( 0, +32) 20x25 m pad, a 4 m ledge, a wall, a 10 m throw ruler and an 8 m height ruler for the boost
#   Z4 spin/drill     ( 0, +65) ring of 4 target spots, a 3 m dive platform, a block
#   Z5 traversal      (-45, +5) 2 lanes of L_WallSlideAB (flat, 90 deg convex corner; build_wall_slide_ab_map.build_plan), steps, jump gaps, ramps
#   Z6 animations     ( 0, -40) one looping SkeletalMeshActor per current move (jab v12, ftilt v31, dair v4 VarM) with its asset name
# Tools/GASNPPTest/test_hub.py reads ZONES / PARK below to park the PIE pawns, so the two stay in step.
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_test_combat_map as combat   # noqa: E402
import build_wall_slide_ab_map as wallab  # noqa: E402

TAG = 'TestHub'
MAP_PATH = '/Game/Levels/Test/L_Test_Hub'
GAME_MODE = '/Game/GASNPPTest/GM_GASNPPTest.GM_GASNPPTest_C'
CUBE = '/Engine/BasicShapes/Cube.Cube'
CYLINDER = '/Engine/BasicShapes/Cylinder.Cylinder'
MAT_PARENT = '/Engine/BasicShapes/BasicShapeMaterial'
MAT_DIR = '/Game/GASNPPTest/TestMaps/Hub'
MANNEQUIN = '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'
COLOURS = {
    'Blue': (0.10, 0.25, 0.80), 'DarkBlue': (0.05, 0.10, 0.40), 'Orange': (1.0, 0.35, 0.02), 'Red': (0.9, 0.03, 0.03),
    'Green': (0.05, 0.7, 0.1), 'Yellow': (1.0, 0.85, 0.05), 'White': (0.9, 0.9, 0.9), 'Grey': (0.35, 0.35, 0.35),
}

# Zone table: name, sign text, sign location (cm). Park spots: where test_hub.park() puts player slot 0..3 (x, y; the floor is found by the teleport)
ZONES = {
    'Z0': ('Z0  PIAZZA', (0.0, 0.0)),
    'Z1': ('Z1  COLPI M4\nmuro - vetro - KO - trampolini\ncorsia knockback 30 m', (2300.0, -1900.0)),
    'Z2': ('Z2  DUELLO M6\narena 20 m, bordi KO\nscudo parry prese lanci tech mash', (6300.0, 0.0)),
    'Z3': ('Z3  PIGGYBACK M7\nmount drop colpo lancio boost\nrighelli 10 m / 8 m', (0.0, 2150.0)),
    'Z4': ('Z4  SPIN / DRILL\nanello 4 bersagli - piattaforma 3 m\nruota lo stick', (0.0, 4650.0)),
    'Z5': ('Z5  TRAVERSAL\nwall slide (piatto, spigolo 90)\ngradini - salti - rampe', (-2150.0, 1000.0)),
    'Z6': ('Z6  ANIMAZIONI\nslomo 0.25 per guardarle', (0.0, -2150.0)),
}
PARK = {
    'Z0': [(0.0, -450.0), (0.0, -150.0), (0.0, 150.0), (0.0, 450.0)],
    'Z1': [(3200.0, 300.0), (3500.0, 300.0), (3800.0, 300.0), (2600.0, 300.0)],       # 3200 faces the glass, 3500/3800 targets in front
    'Z1_lane': [(2300.0, -1700.0), (2600.0, -1700.0), (2900.0, -1700.0), (3200.0, -1700.0)],
    'Z2': [(7200.0, 0.0), (7800.0, 0.0), (7500.0, 400.0), (7500.0, -400.0)],
    'Z3': [(0.0, 2600.0), (200.0, 2600.0), (-300.0, 2900.0), (300.0, 2900.0)],
    'Z4': [(0.0, 5600.0), (0.0, 6000.0), (200.0, 6200.0), (-200.0, 6200.0)],          # slot 0 in the middle of the ring, 1..3 ring spots
    'Z5': [(-3400.0, -1400.0), (-3400.0, -200.0), (-3300.0, 600.0), (-3300.0, 1300.0)],
    'Z6': [(-300.0, -3000.0), (300.0, -3000.0), (0.0, -3300.0), (600.0, -3300.0)],
}
ANIMS = [
    ('Jab v12', ['/Game/AnimPipeline/Rig/AS_JabV12_Build', '/Game/AnimPipeline/Pilot/AS_Pilot_JabV12']),
    ('Ftilt v31', ['/Game/AnimPipeline/Rig/AS_GanonFTiltV31_Build', '/Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V3']),
    ('Dair v4 VarM', ['/Game/AnimPipeline/Rig/AS_OverlordDAirV4_Build_VarM', '/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDAir_V4_VarM']),
]
# Kit pads (D1 "pedana", Python only until TH-1b's AGSTN_KitPickup is in this branch): standing on one gives that loadout (test_hub's watcher,
# server side like the C15 test loadout). (kit, support weapon, label, (x, y))
KIT_PADS = [
    ('KIT_Test_Hits', '', 'COLPI + SPIN', (1200.0, -1050.0)),
    ('KIT_Test_Link', '', 'PRESE', (1200.0, -700.0)),
    ('KIT_Test_Mount', '', 'PIGGYBACK', (1200.0, -350.0)),
    ('KIT_Test_DefenceLink', '', 'DIFESA', (1200.0, 0.0)),
    ('KIT_Test_Alpha', '', 'KIT ALPHA', (1200.0, 350.0)),
    ('KIT_Test_Beta', '', 'ARMA: LANCIA', (1200.0, 700.0)),
    ('KIT_Test_Alpha', 'WPN_Test_Support', 'ARMA SUPPORTO', (1200.0, 1050.0)),
]
PAD_RADIUS = 120.0
DUMMY_CLASS = '/Game/GASNPPTest/BP_GASNPP_TestCharacter'
# Dummies: the test character placed in the level, possessed by an AI controller on the server (stands still, takes hits). (zone, (x, y), yaw)
DUMMIES = [
    ('Z1', (3500.0, 300.0), 180.0), ('Z1', (3500.0, -400.0), 180.0), ('Z1', (2700.0, -1700.0), 180.0),
    ('Z2', (7800.0, 0.0), 180.0), ('Z2', (7500.0, 500.0), 270.0),
    ('Z3', (300.0, 2800.0), 270.0),
    ('Z4', (0.0, 6000.0), 270.0), ('Z4', (200.0, 6200.0), 270.0), ('Z4', (-200.0, 6200.0), 270.0), ('Z4', (0.0, 6400.0), 270.0),
]
WALL_LANES = ['a_flat', 'b_convex90']
WALL_PLANE_X = -3000.0
WALL_LANE_Y = [-2000.0, -800.0]

_report = []


def log(msg):
    _report.append(msg)
    unreal.log(f'[build_test_hub_map] {msg}')


# ---- spawning ----------------------------------------------------------------------------------------------------------------------------------
def _spawn(cls, loc, rot=(0.0, 0.0, 0.0), label=None):
    """rot = (pitch, yaw, roll)"""
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = sub.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    tags = list(actor.get_editor_property('tags'))
    tags.append(unreal.Name(TAG))
    actor.set_editor_property('tags', tags)
    if label:
        actor.set_actor_label(label)
    return actor


def _material(colour):
    name = f'MI_Hub_{colour}'
    path = f'{MAT_DIR}/{name}'
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(MAT_PARENT))
        r, g, b = COLOURS[colour]
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(r, g, b, 1.0))
        unreal.MaterialEditingLibrary.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return unreal.load_asset(path)


def box(label, centre, size, colour='Blue', rot=(0.0, 0.0, 0.0), collision=True, mesh=CUBE):
    """An engine cube (100 cm a side) scaled to `size` (full size, cm), centred on `centre`"""
    actor = _spawn(unreal.StaticMeshActor, centre, rot, label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset(mesh))
    comp.set_material(0, _material(colour))
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def floor(label, x0, x1, y0, y1, colour='Blue'):
    return box(label, ((x0 + x1) / 2.0, (y0 + y1) / 2.0, -50.0), (x1 - x0, y1 - y0, 100.0), colour)


def sign(label, text, loc, face_to=(0.0, 0.0), size=55.0, colour=(1.0, 1.0, 1.0), height=320.0):
    """A TextRender that faces `face_to` (its text reads from that side)"""
    yaw = math.degrees(math.atan2(face_to[1] - loc[1], face_to[0] - loc[0]))
    actor = _spawn(unreal.TextRenderActor, (loc[0], loc[1], height), (0.0, yaw, 0.0), label)
    comp = actor.text_render
    comp.set_text(text)
    comp.set_world_size(size)
    comp.set_text_render_color(unreal.Color(r=int(colour[0] * 255), g=int(colour[1] * 255), b=int(colour[2] * 255), a=255))
    try:
        comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        comp.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    except Exception as e:
        log(f'WARNING: text alignment not set on {label}: {e!r}')
    return actor


def ruler(label, start, direction, metres, colour='White', tall=False):
    """1 m marks (a long mark every 5 m) from `start` along the unit vector `direction`; tall = a vertical 10 cm column with side marks"""
    if tall:
        box(f'{label}_Column', (start[0], start[1], metres * 50.0), (10.0, 10.0, metres * 100.0), 'Grey', collision=False)
        for m in range(1, metres + 1):
            w = 60.0 if m % 5 == 0 else 30.0
            box(f'{label}_{m}m', (start[0] + w / 2.0, start[1], m * 100.0), (w, 12.0, 3.0), colour, collision=False)
            sign(f'{label}_{m}m_Text', f'{m}', (start[0] + 60.0, start[1], 0.0), face_to=(start[0] + 500.0, start[1]), size=22.0, height=m * 100.0)
        return
    for m in range(0, metres + 1):
        p = (start[0] + direction[0] * m * 100.0, start[1] + direction[1] * m * 100.0)
        long_mark = m % 5 == 0
        across = (-direction[1], direction[0])
        length = 120.0 if long_mark else 50.0
        yaw = math.degrees(math.atan2(across[1], across[0]))
        box(f'{label}_{m}m', (p[0], p[1], 0.5), (length, 4.0 if not long_mark else 8.0, 1.0), colour, rot=(0.0, yaw, 0.0), collision=False)
        if long_mark:
            sign(f'{label}_{m}m_Text', f'{m} m', (p[0] + across[0] * 90.0, p[1] + across[1] * 90.0), face_to=(0.0, 0.0), size=30.0, height=40.0)


def clear():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    gone = [a for a in sub.get_all_level_actors() if a.actor_has_tag(TAG)]
    for a in gone:
        sub.destroy_actor(a)
    return len(gone)


# ---- zones -------------------------------------------------------------------------------------------------------------------------------------
def z0_plaza():
    floor('Z0_Floor', -2000.0, 2000.0, -2000.0, 2000.0)
    for i, (x, y) in enumerate(PARK['Z0']):
        _spawn(unreal.PlayerStart, (x, y, 100.0), (0.0, 0.0, 0.0), f'PlayerStart_{i}')
    board = ('TEST HUB  -  zone e comandi (console Python dell\'editor)\n'
             'PRIMA di Play:  import test_hub as h; h.prepare()\n'
             'Play: listen, 3 player, Run Under One Process\n'
             'Pedane kit davanti a te: ci sali sopra e cambi kit. Manichini nelle zone.\n'
             'h.park(1,\'Z1\')  h.kit(0,\'KIT_Test_Mount\')  h.mount(0,1)\n'
             'h.walker(2,True)  h.fighter(\'Z5\')  h.selftest()\n'
             'Z1 +X colpi | Z2 +X+ duello | Z3 +Y piggyback\n'
             'Z4 +Y+ spin/drill | Z5 -X traversal | Z6 -Y animazioni')
    sign('Z0_Board', board, (-1200.0, 0.0), face_to=(0.0, 0.0), size=42.0, height=330.0)
    box('Z0_Board_Back', (-1230.0, 0.0, 330.0), (20.0, 1500.0, 360.0), 'DarkBlue', collision=False)
    ruler('Z0_Ruler', (-1900.0, -1900.0), (1.0, 0.0), 10)


def z1_hits():
    floor('Z1_Floor', 2000.0, 6000.0, -2000.0, 2000.0)
    data = combat.specs()
    off = (2500.0, 0.0)

    def at(c):
        return (c[0] + off[0], c[1] + off[1], c[2])

    box('Z1_Wall', at(data['wall'][0]), data['wall'][1], 'Grey')
    _phys(combat.COMBAT_DIR + '/PM_Test_Wall', 'Z1_Wall')
    centre, size = data['glass']
    glass = _spawn(unreal.GSTN_TestSurface, at(centre), label='Z1_Glass')
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="Surface.Type.Glass")')
    glass.set_editor_property('surface_type', tag)
    glass.set_editor_property('half_extent', unreal.Vector(size[0] * 0.5, size[1] * 0.5, size[2] * 0.5))
    look = box('Z1_Glass_Look', at(centre), size, 'White', collision=False)
    _phys(combat.COMBAT_DIR + '/PM_Test_Glass', 'Z1_Glass_Look')
    look.attach_to_actor(glass, '', unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    sign('Z1_Glass_Text', 'vetro (si rompe)', (at(centre)[0] - 200.0, 0.0), face_to=(0.0, 0.0), size=30.0, height=520.0)
    centre, extent = data['ko_zone']
    zone = _spawn(unreal.GSTN_KOZone, at(centre), label='Z1_KOZone')
    zone.set_editor_property('half_extent', unreal.Vector(*extent))
    box('Z1_KOZone_Look', at(centre), (extent[0] * 2.0, extent[1] * 2.0, 4.0), 'Red', collision=False)
    for i, (centre, up, size, speed) in enumerate(data['trampolines']):
        t = _spawn(unreal.GSTN_Trampoline, at(centre), label=f'Z1_Trampoline_{i}')
        t.set_actor_rotation(combat._rotation_from_up(up), False)
        t.set_editor_property('size', unreal.Vector(*size))
        launch = t.get_editor_property('launch')
        launch.set_editor_property('direction_mode', unreal.GSTN_SurfaceLaunchDirection.LOCAL_DIRECTION)
        launch.set_editor_property('local_direction', unreal.Vector(0.0, 0.0, 1.0))
        launch.set_editor_property('speed_mode', unreal.GSTN_SurfaceLaunchSpeed.FIXED)
        launch.set_editor_property('speed', speed)
        t.set_editor_property('launch', launch)
        sign(f'Z1_Trampoline_{i}_Text', f'trampolino {i}: {int(speed)} cm/s', (at(centre)[0], at(centre)[1] - 250.0), face_to=(0.0, 0.0), size=28.0, height=180.0)
    # Knockback lane: 30 m along +X on the -Y side, marks every metre, plus a 10 m measuring strip across it
    box('Z1_Lane', (2000.0 + 1500.0 + 100.0, -1700.0, 0.5), (3000.0, 200.0, 1.0), 'DarkBlue', collision=False)
    ruler('Z1_LaneRuler', (2100.0, -1550.0), (1.0, 0.0), 30)
    ruler('Z1_Ruler10', (5800.0, -1800.0), (0.0, 1.0), 10, colour='Yellow')
    sign('Z1_Sign', ZONES['Z1'][0], ZONES['Z1'][1], face_to=(0.0, 0.0))


def _phys(path, label):
    material = unreal.load_asset(path)
    if not material:
        log(f'WARNING: {path} missing, {label} keeps the default physical material')
        return
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = next(a for a in sub.get_all_level_actors() if a.get_actor_label() == label)
    comp = actor.static_mesh_component
    body = comp.get_editor_property('body_instance')
    body.set_editor_property('phys_material_override', material)
    comp.set_editor_property('body_instance', body)


def z2_duel():
    centre = (7500.0, 0.0)
    box('Z2_Bridge', (6250.0, 0.0, -50.0), (500.0, 200.0, 100.0), 'Blue')
    box('Z2_Arena', (centre[0], centre[1], -50.0), (2000.0, 2000.0, 100.0), 'Blue', mesh=CYLINDER)
    box('Z2_Arena_Edge', (centre[0], centre[1], -102.0), (2060.0, 2060.0, 2.0), 'Red', mesh=CYLINDER, collision=False)
    zone = _spawn(unreal.GSTN_KOZone, (centre[0], centre[1], -1500.0), label='Z2_KOZone')
    zone.set_editor_property('half_extent', unreal.Vector(2500.0, 2500.0, 400.0))
    box('Z2_KOZone_Look', (centre[0], centre[1], -1100.0), (5000.0, 5000.0, 4.0), 'Red', collision=False)
    ruler('Z2_Ruler', (centre[0] - 1000.0, -150.0), (1.0, 0.0), 20, colour='Yellow')
    sign('Z2_Sign', ZONES['Z2'][0], ZONES['Z2'][1], face_to=(0.0, 0.0))


def z3_piggyback():
    floor('Z3_Floor', -1000.0, 1000.0, 2000.0, 4500.0)
    box('Z3_Ledge', (-700.0, 3700.0, 200.0), (600.0, 600.0, 400.0), 'Blue')
    sign('Z3_Ledge_Text', 'sporgenza 4 m', (-700.0, 3350.0), face_to=(0.0, 0.0), size=28.0, height=470.0)
    box('Z3_Wall', (900.0, 3600.0, 250.0), (60.0, 800.0, 500.0), 'Grey')
    ruler('Z3_ThrowRuler', (0.0, 2600.0), (0.0, 1.0), 10, colour='Yellow')
    ruler('Z3_BoostRuler', (-450.0, 2600.0), (0.0, 0.0), 8, tall=True, colour='Yellow')
    sign('Z3_Sign', ZONES['Z3'][0], ZONES['Z3'][1], face_to=(0.0, 0.0))


def z4_spin():
    floor('Z4_Floor', -1500.0, 1500.0, 4500.0, 8000.0)
    for i, (x, y) in enumerate(PARK['Z4'][1:] + [(0.0, 6400.0)]):
        box(f'Z4_Target_{i}', (x, y, 1.0), (120.0, 120.0, 2.0), 'Yellow', mesh=CYLINDER, collision=False)
    box('Z4_DivePlatform', (0.0, 7300.0, 150.0), (400.0, 400.0, 300.0), 'Blue')
    sign('Z4_DivePlatform_Text', 'piattaforma 3 m (picchiata)', (0.0, 7050.0), face_to=(0.0, 0.0), size=28.0, height=380.0)
    box('Z4_Block', (900.0, 6800.0, 100.0), (200.0, 200.0, 200.0), 'Yellow')
    sign('Z4_Block_Text', 'blocco: il rimbalzo vero = h.drill_immune(i)', (900.0, 6600.0), face_to=(0.0, 0.0), size=24.0, height=260.0)
    sign('Z4_Sign', ZONES['Z4'][0], ZONES['Z4'][1], face_to=(0.0, 0.0))


def z5_traversal():
    floor('Z5_Floor', -7500.0, -2000.0, -2000.0, 3000.0)
    plan = wallab.build_plan()
    for slot, lane in enumerate(WALL_LANES):
        marker = next(m for m in plan.markers if m['id'] == lane)
        y_from = marker['loc'][1] - wallab.START_DY
        dy = WALL_LANE_Y[slot] - y_from
        for b in plan.boxes:
            if b['name'].startswith(lane + '_') or b['name'] == lane:
                c = b['center']
                box(f"Z5_{b['name']}", (c[0] + WALL_PLANE_X, c[1] + dy, c[2]), b['size'], 'Orange', rot=tuple(b['rot']))
        loc = marker['loc']
        tp = _spawn(unreal.TargetPoint, (loc[0] + WALL_PLANE_X, loc[1] + dy, loc[2]), (0.0, marker['yaw'], 0.0), f'Z5_Start_{lane}')
        tags = list(tp.get_editor_property('tags')) + [unreal.Name(t) for t in (
            'WallABLane', 'lane:' + lane, 'group:' + marker['group'], 'launch:%.2f,%.2f,%.2f' % tuple(marker['launch']), 'stick:%.2f,%.2f' % tuple(marker['stick']))]
        tp.set_editor_property('tags', tags)
        sign(f'Z5_{lane}_Text', 'wall slide: ' + ('muro piatto' if slot == 0 else 'spigolo convesso 90'), (WALL_PLANE_X - 150.0, WALL_LANE_Y[slot] + 500.0),
             face_to=(-6000.0, WALL_LANE_Y[slot] + 500.0), size=34.0, height=300.0)
    # Steps (TM1 #2): 30..200 cm, 2 m apart
    for i, h in enumerate((30, 45, 60, 90, 120, 150, 200)):
        x = -3500.0 - i * 200.0
        box(f'Z5_Step_{h}', (x, 600.0, h / 2.0), (150.0, 150.0, float(h)), 'Blue')
        sign(f'Z5_Step_{h}_Text', f'{h} cm', (x, 450.0), face_to=(x, -1000.0), size=24.0, height=h + 40.0)
    # Jump gaps (TM1 #3 subset): 3 m platforms 1.5 m high, gaps of 2, 3, 4 m
    x = -3400.0
    for i, gap in enumerate((200.0, 300.0, 400.0, None)):
        box(f'Z5_Gap_{i}', (x - 150.0, 1300.0, 75.0), (300.0, 200.0, 150.0), 'Blue')
        if gap:
            sign(f'Z5_Gap_{i}_Text', f'salto {int(gap / 100)} m', (x - 300.0 - gap / 2.0, 1150.0), face_to=(x - 300.0 - gap / 2.0, -1000.0), size=24.0, height=200.0)
            x -= 300.0 + gap
    # Ramps (TM1 #5 subset): 15, 30, 45, 60 deg, 6 m long, rising toward -X from x = -3500
    for i, angle in enumerate((15.0, 30.0, 45.0, 60.0)):
        y = 1700.0 + i * 330.0
        a = math.radians(angle)
        cx, cz = -3500.0 - 300.0 * math.cos(a), 300.0 * math.sin(a)
        box(f'Z5_Ramp_{int(angle)}', (cx, y, cz), (600.0, 200.0, 20.0), 'Blue', rot=(angle, 180.0, 0.0))
        sign(f'Z5_Ramp_{int(angle)}_Text', f'rampa {int(angle)}', (-3350.0, y), face_to=(0.0, y), size=24.0, height=60.0)
    ruler('Z5_Height', (-6800.0, -1500.0), (0.0, 0.0), 10, tall=True)
    sign('Z5_Fighter_Text', 'test fighter: h.fighter(\'Z5\')', (-5800.0, -1000.0), face_to=(-2000.0, -1000.0), size=30.0, height=200.0)
    sign('Z5_Sign', ZONES['Z5'][0], ZONES['Z5'][1], face_to=(0.0, 0.0))


def z6_animations():
    floor('Z6_Floor', -2000.0, 2000.0, -6000.0, -2000.0)
    mesh = unreal.load_asset(MANNEQUIN)
    for i, (name, candidates) in enumerate(ANIMS):
        x = -600.0 + i * 600.0
        path = next((p for p in candidates if unreal.EditorAssetLibrary.does_asset_exist(p)), None)
        if path is None:
            log(f'MISSING animation for {name}: {candidates}')
            sign(f'Z6_{i}_Text', f'{name}\n(manca)', (x, -3900.0), face_to=(x, 0.0), size=30.0, height=250.0, colour=(1.0, 0.2, 0.2))
            continue
        anim = unreal.load_asset(path)
        actor = _spawn(unreal.SkeletalMeshActor, (x, -4000.0, 0.0), (0.0, 90.0, 0.0), f'Z6_{name.replace(" ", "_")}')
        comp = actor.skeletal_mesh_component
        if hasattr(comp, 'set_skinned_asset_and_update'):
            comp.set_skinned_asset_and_update(mesh)
        else:
            comp.set_skeletal_mesh(mesh)
        comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        comp.override_animation_data(anim, True, True, 0.0, 1.0)
        box(f'Z6_{i}_Plinth', (x, -4000.0, 1.0), (200.0, 200.0, 2.0), 'Yellow', mesh=CYLINDER, collision=False)
        sign(f'Z6_{i}_Text', f'{name}\n{path.rsplit("/", 1)[1]}', (x, -3750.0), face_to=(x, 0.0), size=26.0, height=230.0)
        log(f'Z6 {name}: {path}')
    sign('Z6_Sign', ZONES['Z6'][0], ZONES['Z6'][1], face_to=(0.0, 0.0))


def kit_pads_and_dummies():
    for kit_name, support, label, (x, y) in KIT_PADS:
        box(f'Z0_Pad_{label.replace(" ", "_").replace(":", "")}', (x, y, 5.0), (PAD_RADIUS * 2.0, PAD_RADIUS * 2.0, 10.0), 'Grey', mesh=CYLINDER)
        box(f'Z0_Pad_{label.replace(" ", "_").replace(":", "")}_Item', (x, y, 60.0), (40.0, 40.0, 80.0), 'Green' if support else 'Yellow', collision=False)
        text = f'{label}\n{kit_name}' + (f'\n+ {support}' if support else '')
        sign(f'Z0_Pad_{label.replace(" ", "_").replace(":", "")}_Text', text, (x + 30.0, y), face_to=(0.0, y), size=22.0, height=170.0)
    sign('Z0_Pads_Title', 'PEDANE KIT: salici sopra', (1200.0, -1450.0), face_to=(0.0, -1450.0), size=30.0, height=230.0)
    cls = unreal.load_asset(DUMMY_CLASS)
    if not cls:
        log(f'WARNING: {DUMMY_CLASS} missing, no dummies')
        return
    cls = cls.generated_class()
    for i, (zone, (x, y), yaw) in enumerate(DUMMIES):
        _spawn(cls, (x, y, 100.0), (0.0, yaw, 0.0), f'Dummy_{zone}_{i}')
        sign(f'Dummy_{zone}_{i}_Text', 'manichino', (x, y), face_to=(0.0, 0.0), size=20.0, height=230.0, colour=(1.0, 0.85, 0.1))


def lights():
    _spawn(unreal.DirectionalLight, (0.0, 0.0, 1000.0), (-50.0, 30.0, 0.0), 'Sun')
    _spawn(unreal.SkyLight, (0.0, 0.0, 1200.0), label='Sky')
    _spawn(unreal.ExponentialHeightFog, (0.0, 0.0, -200.0), label='Fog')


def run():
    """Fills the open level (never saves it). Refuses a level that already has a path other than L_Test_Hub: an untitled level or the hub only"""
    _report.clear()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    path = world.get_path_name().split('.')[0]
    # An unsaved new level is /Game/NewWorld (or /Temp/Untitled_N): anything else is someone's map
    if not (path.startswith('/Temp/') or path in ('/Game/NewWorld', MAP_PATH)):
        raise RuntimeError(f'the open level is {path}: open an empty level (File > New Level > Empty Level) or {MAP_PATH}')
    log(f'cleared {clear()} hub actors in {path}')
    for step in (z0_plaza, z1_hits, z2_duel, z3_piggyback, z4_spin, z5_traversal, z6_animations, kit_pads_and_dummies, lights):
        try:
            step()
        except Exception as e:   # one broken zone must not hide the others
            log(f'ERROR in {step.__name__}: {e!r}')
    game_mode = unreal.load_class(None, GAME_MODE)
    if game_mode:
        world.get_world_settings().set_editor_property('default_game_mode', game_mode)
    else:
        log(f'WARNING: {GAME_MODE} not found')
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    count = sum(1 for a in sub.get_all_level_actors() if a.actor_has_tag(TAG))
    log(f'done: {count} hub actors; the map is NOT saved (Ctrl+S -> {MAP_PATH})')
    out = os.path.join(unreal.Paths.project_saved_dir(), 'TestHub', 'build.txt')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(_report) + '\n')
    errors = [r for r in _report if r.startswith(('ERROR', 'MISSING', 'WARNING'))]
    return f'{count} actors, {len(errors)} problems, report {out}'
