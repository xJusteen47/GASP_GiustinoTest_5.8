# Builds the wall slide A/B test map /Game/Levels/L_WallSlideAB (wall-slide-v2.md section 4, W6). Idempotent: the geometry is a pure function
# of the lane table below, hashed into a tag on a "spec" actor; a run that finds the same hash in the saved map changes and saves nothing.
#
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/build_wall_slide_ab_map.py" -unattended -nullrhi -nosplash -nosound -NoP4
# or from the editor Python console:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import build_wall_slide_ab_map as b; b.run()
#
# Layout. Every lane is a wall whose face is the plane x = 0 (facing -X, towards the character), 12 m tall, 10 m wide along +Y; lanes are
# LANE_PITCH cm apart along Y, on one big floor (z = 0). The character starts at x = -START_GAP, z = START_HEIGHT (10 m), facing +X, and is
# launched into the wall (+X) and along it (+Y) so it grabs the wall while falling; a stick "right" is +Y for a control yaw of 0.
# The feature of the lane (corner, bump, recess...) sits FEATURE_DY cm after the start along Y.
# Geometry is plain engine cubes (/Engine/BasicShapes/Cube) used as thin plates behind the face (a plate has only its front face
# in play: the character is never behind it), simple collision, default material, no lighting polish. The moving wall is the C++ actor
# AGSTN_WallTestMover. Curved walls are a chain of plates (a curve is a chain of small slope changes, which is how the modes see it).
#
# Start markers are TargetPoints tagged WallABLane, lane:<id>, launch:<x>,<y>,<z> (cm/s for gstn.Wall.Launch), stick:<right>,<fwd>
# (gstn.Wall.DebugInput), group:<a..i, j>; tools/GASNPPTest/wall_ab.py reads them from the PIE world.
import json, hashlib, math
import unreal

MAP = '/Game/Levels/L_WallSlideAB'
GAME_MODE = '/Game/GASNPPTest/GM_GASNPPTest'
SPEC_VERSION = 1

LANE_PITCH = 1200.0
LANE_WIDTH = 1000.0
WALL_HEIGHT = 1200.0
# The slide falls at up to 600 cm/s and runs along the wall at 350-480: from 4 m the character reached the floor right at the feature (3 m along the
# lane) and the "after" window (80-200 cm past it) was never seen. From 10 m the feature is crossed at about 1 s of fall and the window at 1.4 s, with
# 4 s of wall left under the 3.5 s runs
START_HEIGHT = 1000.0
START_GAP = 50.0
# The moving walls start further from the wall: the teleport and the launch of the harness are applied to the server's and to the client's copy of the pawn
# at different frames (the client is ~25 frames ahead), which makes its own correction for the first ~0.6 s. A grab inside that window starts the ride from a
# state the correction is rewriting (the base record is rolled back with it) and the lane would measure the harness; at 300 cm/s into the wall the grab comes
# after ~1 s
MOVING_START_GAP = 300.0
START_DY = 100.0
FEATURE_DY = 300.0
PLATE_T = 20.0
# Plates behind a face that curves toward the character must be as thick as a wall: behind a thin one the look-ahead's lifted point is in the air and
# reads "the wall ends" (a concave corner as an outside edge)
CURVE_PLATE_T = 150.0
CONCAVE_PLATE_T = 150.0
FLOOR_THICK = 100.0

LAUNCH_INTO = 300.0   # cm/s into the wall
LAUNCH_ALONG = 400.0  # cm/s along the lane
# gstn.Wall.DebugInput sticks (right, forward; forward = into the wall for a control yaw of 0). The grab needs intent into the wall; the side
# part keeps the character travelling along it
SIDE_STICK = (0.7, 0.7)
INTO_STICK = (0.0, 1.0)

CUBE = '/Engine/BasicShapes/Cube'
TARGET_TAG = 'WallAB'

# ------------------------------------------------------------------------------------------------ plan (pure)


class Plan:
    """What the map holds: boxes, moving walls, surface walls, markers. Plain data, hashed for idempotency."""

    def __init__(self):
        self.boxes = []      # {name, center[3], size[3], rot[3] (pitch,yaw,roll), tag}
        self.movers = []     # {name, center, size, axis, speed, dist, yaw_rate, yaw_range}
        self.surfaces = []   # {box: name, behaviour: 'WallClimb' | 'Stand'}
        self.markers = []    # {id, group, loc[3], yaw, launch[3], stick[2]}

    def box(self, name, center, size, rot=(0.0, 0.0, 0.0)):
        self.boxes.append({'name': name, 'center': [round(v, 3) for v in center], 'size': [round(v, 3) for v in size], 'rot': [round(v, 4) for v in rot]})

    def to_json(self):
        return json.dumps({'v': SPEC_VERSION, 'boxes': self.boxes, 'movers': self.movers, 'surfaces': self.surfaces, 'markers': self.markers}, sort_keys=True)


def chain(plan, lane, y0, segments, thickness=PLATE_T, height=WALL_HEIGHT):
    """A polyline wall starting at (0, y0), heading +Y. segments = [(turn_deg, length)]: turn > 0 bends the wall AWAY from the character
    (convex), < 0 toward it (concave), applied before the segment. Each segment is a plate whose front face lies on the polyline."""
    px, py, h = 0.0, y0, 0.0
    prev_n = (-1.0, 0.0)
    for i, (turn, length) in enumerate(segments):
        h += math.radians(turn)
        d = (math.sin(h), math.cos(h))
        n = (-math.cos(h), math.sin(h))
        s0 = 0.0
        if i > 0 and turn > 0:
            # Convex: the back corner of the new plate must not stick out in front of the previous face (angles beyond 90 deg)
            th = math.radians(turn)
            s0 = max(0.0, -thickness * math.cos(th) / math.sin(th))
        mid = s0 + (length - s0) / 2.0
        cx = px + d[0] * mid - n[0] * thickness / 2.0
        cy = py + d[1] * mid - n[1] * thickness / 2.0
        plan.box(f'{lane}_seg{i}', (cx, cy, height / 2.0), (thickness, length - s0, height), (0.0, -math.degrees(h), 0.0))
        px, py = px + d[0] * length, py + d[1] * length
        prev_n = n
    return px, py


def lane_origin(k):
    return k * LANE_PITCH


def marker(plan, lane_id, group, y0, offset_y=START_DY, launch=None, stick=INTO_STICK, loc=None, yaw=0.0):
    plan.markers.append({
        'id': lane_id, 'group': group,
        'loc': [round(v, 2) for v in (loc or (-START_GAP, y0 + offset_y, START_HEIGHT))], 'yaw': yaw,
        'launch': [round(v, 2) for v in (launch or (LAUNCH_INTO, LAUNCH_ALONG, 0.0))], 'stick': list(stick),
    })


def build_plan():
    plan = Plan()
    k = 0
    ye = FEATURE_DY + START_DY   # feature position relative to the lane origin (the marker is START_DY from it)

    def next_lane():
        nonlocal k
        y0 = lane_origin(k)
        k += 1
        return y0

    # a: flat wall
    y0 = next_lane()
    chain(plan, 'a_flat', y0, [(0, LANE_WIDTH)])
    marker(plan, 'a_flat', 'a', y0, stick=SIDE_STICK)

    # b, c: convex and concave corners
    for group, sign in (('b', 1), ('c', -1)):
        for ang in (30, 45, 60, 90, 120):
            y0 = next_lane()
            lane = f'{group}_{"convex" if sign > 0 else "concave"}{ang}'
            chain(plan, lane, y0, [(0, ye), (sign * ang, 400.0)], thickness=(CONCAVE_PLATE_T if sign < 0 else PLATE_T))
            marker(plan, lane, group, y0, stick=SIDE_STICK)

    # d: bumps 2/5/10/15/25 cm deep: a vertical rib at the feature position (sideways) and a 30 cm tall ledge lower down (from above)
    for depth in (2, 5, 10, 15, 25):
        y0 = next_lane()
        lane = f'd_bump{depth}'
        chain(plan, lane, y0, [(0, LANE_WIDTH)])
        plan.box(f'{lane}_rib', (-depth / 2.0, y0 + ye + 10.0, WALL_HEIGHT / 2.0), (depth, 20.0, WALL_HEIGHT))
        plan.box(f'{lane}_ledge', (-depth / 2.0, y0 + ye + 450.0, 165.0), (depth, 300.0, 30.0))
        marker(plan, lane, 'd', y0, stick=SIDE_STICK)
        marker(plan, lane + '_down', 'd', y0, offset_y=ye + 450.0, launch=(LAUNCH_INTO, 0.0, 0.0), stick=INTO_STICK)
        # the down marker starts over the ledge band, centered on it

    # e: recesses 5/15 cm deep, 100 cm wide
    for depth in (5, 15):
        y0 = next_lane()
        lane = f'e_recess{depth}'
        plan.box(f'{lane}_a', (40.0 / 2.0, y0 + ye / 2.0, WALL_HEIGHT / 2.0), (40.0, ye, WALL_HEIGHT))
        plan.box(f'{lane}_back', (depth + 10.0, y0 + ye + 50.0, WALL_HEIGHT / 2.0), (20.0, 100.0, WALL_HEIGHT))
        end = y0 + ye + 100.0
        plan.box(f'{lane}_b', (40.0 / 2.0, (end + y0 + LANE_WIDTH) / 2.0, WALL_HEIGHT / 2.0), (40.0, y0 + LANE_WIDTH - end, WALL_HEIGHT))
        marker(plan, lane, 'e', y0, stick=SIDE_STICK)

    # f: slope changes 5/10/15/25 deg (facets, bending away)
    for ang in (5, 10, 15, 25):
        y0 = next_lane()
        lane = f'f_facet{ang}'
        chain(plan, lane, y0, [(0, ye), (ang, 500.0)])
        marker(plan, lane, 'f', y0, stick=SIDE_STICK)

    # g: curved walls r100 / r300, outside (convex) and inside (concave): arcs of 90 deg made of 10 deg chords
    for radius in (100, 300):
        for sign, name in ((1, 'out'), (-1, 'in')):
            y0 = next_lane()
            lane = f'g_r{radius}_{name}'
            step = 10.0
            n = 9
            chord = 2.0 * radius * math.sin(math.radians(step / 2.0))
            segs = [(0, ye), (sign * step / 2.0, chord)] + [(sign * step, chord)] * (n - 1) + [(sign * step / 2.0, 200.0)]
            chain(plan, lane, y0, segs, thickness=CURVE_PLATE_T)
            marker(plan, lane, 'g', y0, stick=SIDE_STICK)

    # h: moving walls (kinematic boxes), 150 and 300 cm/s along Y with a turning back and forth about the up axis
    for speed in (150, 300):
        y0 = next_lane()
        lane = f'h_moving{speed}'
        plan.movers.append({'name': lane, 'center': [50.0, y0 + LANE_WIDTH / 2.0, WALL_HEIGHT / 2.0], 'size': [100.0, 800.0, WALL_HEIGHT],
                            'axis': [0.0, 1.0, 0.0], 'speed': float(speed), 'dist': 250.0, 'yaw_rate': 20.0, 'yaw_range': 30.0})
        marker(plan, lane, 'h', y0, offset_y=LANE_WIDTH / 2.0, launch=(LAUNCH_INTO, 0.0, 0.0), stick=INTO_STICK,
               loc=(-MOVING_START_GAP, y0 + LANE_WIDTH / 2.0, START_HEIGHT))

    # i: a 60 deg slanted wall and 100 / 110 deg overhangs (angle from the ground on the character's side)
    for beta, name in ((60, 'slant60'), (100, 'overhang100'), (110, 'overhang110')):
        y0 = next_lane()
        lane = f'i_{name}'
        b = math.radians(beta)
        u = (math.cos(b), 0.0, math.sin(b))
        n = (-math.sin(b), 0.0, math.cos(b))
        length = (START_HEIGHT + 300.0) / math.sin(b)   # reaches past the start point along the plate
        center = (u[0] * length / 2.0 - n[0] * PLATE_T / 2.0, y0 + LANE_WIDTH / 2.0, u[2] * length / 2.0 - n[2] * PLATE_T / 2.0)
        plan.box(lane, center, (PLATE_T, LANE_WIDTH, length), (beta - 90.0, 0.0, 0.0))
        s = START_HEIGHT / math.sin(b)
        start = (u[0] * s + n[0] * START_GAP, y0 + START_DY, u[2] * s + n[2] * START_GAP)
        marker(plan, lane, 'i', y0, loc=start, launch=(-n[0] * LAUNCH_INTO, 0.0, -n[2] * LAUNCH_INTO), stick=INTO_STICK)

    # j: surface-authored walls: climb-only (no WallSlide offered) and a wall that offers neither
    for behaviour, lane in (('WallClimb', 'j_climb_only'), ('Stand', 'j_no_slide')):
        y0 = next_lane()
        chain(plan, lane, y0, [(0, LANE_WIDTH)])
        plan.surfaces.append({'box': f'{lane}_seg0', 'behaviour': behaviour})
        marker(plan, lane, 'j', y0, stick=SIDE_STICK)

    plan.lane_count = k
    return plan


# ------------------------------------------------------------------------------------------------ editor side


def log(msg):
    unreal.log(f'[build_wall_slide_ab_map] {msg}')


def rotator_for(rot):
    return unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1])


def set_tags(actor, tags):
    actor.set_editor_property('tags', [unreal.Name(t) for t in tags])


def spawn(cls, loc, rot=(0.0, 0.0, 0.0)):
    return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(*loc), rotator_for(rot))


def existing_spec_tag():
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        for t in a.tags:
            if str(t).startswith('WallABSpec:'):
                return str(t)
    return None


def clear_generated():
    removed = 0
    for a in unreal.EditorLevelLibrary.get_all_level_actors():
        if TARGET_TAG in [str(t) for t in a.tags]:
            a.destroy_actor()
            removed += 1
    log(f'removed {removed} generated actors')


def add_surface_component(actor, behaviour):
    """UGSTN_SurfaceBehaviourComponent offering only Surface.Behaviour.<behaviour> (instance component, saved with the actor)."""
    subs = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subs.k2_gather_subobject_data_for_instance(actor)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.GSTN_SurfaceBehaviourComponent)
    handle, fail = subs.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f'adding the surface component failed: {fail}')
    comp = sdl.get_object(sdl.get_data(handle))
    if comp is None:
        raise RuntimeError('no component object for the new subobject')
    tags = unreal.GameplayTagContainer()
    tags.import_text(f'(GameplayTags=((TagName="Surface.Behaviour.{behaviour}")))')
    surface = unreal.GSTN_SurfaceBehaviour()
    surface.set_editor_property('behaviours', tags)
    comp.set_editor_property('default', surface)
    return comp


def generate(plan):
    cube = unreal.load_asset(CUBE)
    bounds = cube.get_bounds().box_extent
    unit = (bounds.x * 2.0, bounds.y * 2.0, bounds.z * 2.0)

    def scale_for(size):
        return unreal.Vector(size[0] / unit[0], size[1] / unit[1], size[2] / unit[2])

    actors = {}
    # Floor: one slab under every lane, from the first start to well past the last wall
    span = plan.lane_count * LANE_PITCH
    floor_size = (4000.0, span + 2000.0, FLOOR_THICK)
    floor = spawn(unreal.StaticMeshActor, (-1000.0, span / 2.0 - 500.0, -FLOOR_THICK / 2.0))
    floor.static_mesh_component.set_static_mesh(cube)
    floor.set_actor_scale3d(scale_for(floor_size))
    floor.set_actor_label('Floor')
    set_tags(floor, [TARGET_TAG])

    for b in plan.boxes:
        a = spawn(unreal.StaticMeshActor, b['center'], b['rot'])
        a.static_mesh_component.set_static_mesh(cube)
        a.set_actor_scale3d(scale_for(b['size']))
        a.set_actor_label(b['name'])
        set_tags(a, [TARGET_TAG])
        actors[b['name']] = a

    for m in plan.movers:
        a = spawn(unreal.GSTN_WallTestMover, m['center'])
        a.set_actor_scale3d(scale_for(m['size']))
        a.set_actor_label(m['name'])
        a.set_editor_property('travel_axis', unreal.Vector(*m['axis']))
        a.set_editor_property('translation_speed', m['speed'])
        a.set_editor_property('travel_distance', m['dist'])
        a.set_editor_property('yaw_rate', m['yaw_rate'])
        a.set_editor_property('yaw_range', m['yaw_range'])
        set_tags(a, [TARGET_TAG])

    for s in plan.surfaces:
        add_surface_component(actors[s['box']], s['behaviour'])

    for mk in plan.markers:
        a = spawn(unreal.TargetPoint, mk['loc'], (0.0, mk['yaw'], 0.0))
        a.set_actor_label('Start_' + mk['id'])
        set_tags(a, [TARGET_TAG, 'WallABLane', 'lane:' + mk['id'], 'group:' + mk['group'],
                     'launch:%.2f,%.2f,%.2f' % tuple(mk['launch']), 'stick:%.2f,%.2f' % tuple(mk['stick'])])

    first = plan.markers[0]
    ps = spawn(unreal.PlayerStart, first['loc'], (0.0, first['yaw'], 0.0))
    ps.set_actor_label('PlayerStart')
    set_tags(ps, [TARGET_TAG])

    sun = spawn(unreal.DirectionalLight, (0.0, 0.0, 1000.0), (-50.0, 30.0, 0.0))
    sun.set_actor_label('Sun')
    set_tags(sun, [TARGET_TAG])
    sky = spawn(unreal.SkyLight, (0.0, 0.0, 1200.0))
    sky.set_actor_label('Sky')
    set_tags(sky, [TARGET_TAG])


def run():
    plan = build_plan()
    digest = hashlib.sha1(plan.to_json().encode('utf-8')).hexdigest()[:16]
    wanted = 'WallABSpec:' + digest

    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.EditorLevelLibrary.load_level(MAP)
        have = existing_spec_tag()
        if have == wanted:
            log(f'{MAP} is up to date ({digest}), nothing to do')
            return False
        log(f'{MAP} has spec {have}, rebuilding as {wanted}')
        clear_generated()
    else:
        log(f'creating {MAP}')
        if not unreal.EditorLevelLibrary.new_level(MAP):
            raise RuntimeError('new_level failed')
        clear_generated()   # an empty template level may carry its own lights / floor: they are untagged and stay, tagged ones go

    generate(plan)

    spec = spawn(unreal.TargetPoint, (0.0, 0.0, -500.0))
    spec.set_actor_label('Spec')
    set_tags(spec, [TARGET_TAG, wanted])

    world = unreal.EditorLevelLibrary.get_editor_world()
    gm = unreal.load_asset(GAME_MODE)
    if gm:
        world.get_world_settings().set_editor_property('default_game_mode', gm.generated_class())
    else:
        log(f'WARNING: {GAME_MODE} not found, the game mode override is not set')

    unreal.EditorLevelLibrary.save_current_level()
    log(f'{MAP} saved: {len(plan.boxes)} boxes, {len(plan.movers)} moving walls, {len(plan.surfaces)} surface walls, {len(plan.markers)} start markers, spec {digest}')
    return True


if __name__ == '__main__':
    run()
