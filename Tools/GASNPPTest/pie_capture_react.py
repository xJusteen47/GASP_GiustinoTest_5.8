# ANIM-REACT-1: a REAL Play capture of the victim reaction clips for the animation review. Same method as pie_capture_move.py (Play in
# L_Test_Hub: listen host + 2 clients through the hub's own automation, fixed review cameras + the player's camera, t.OverrideFPS 60 so one
# image = one 60 fps frame), but nothing is pressed and no kit is given: the host pawn plays the reaction clips straight on the anim instance
# the combat montages play on (the character mesh's anim instance, ability actor info; slot DefaultSlot, play rate 1.0). CAVEAT: the clips play
# IN PLACE, the pawn does not travel: the roll abilities' movement is NOT applied here, this only judges the pose animation and its blends.
#
# Launch (editor with a real renderer; the script path must have no space, so boot through a copy or a one-line exec):
#   UnrealEditor.exe <uproject> /Game/Levels/Test/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/pie_capture_react.py").read())
# Environment (all optional): PIE_CAPTURE_OUT (default Saved/AnimReact/capture_<date>_<time>), PIE_CAPTURE_SEQUENCES (comma separated subset of
# SEQUENCES: hitstand, tumble, splat, knockdown_getup, roll, rollback)
# Sequences: PRE idle frames, then the segments of the sequence (see SEQUENCES). 'loop' = a still clip looping as a dynamic montage; 'mont' =
# a moving clip (the montage asset from /Game/GASNPPTest/Combat/Reaction, preloaded before Play starts, or a dynamic montage of the clip when
# the asset is missing, the log says so); 'stop' = the current montage stops with a blend out and the sequence goes on for n frames. A moving clip starts on the frame after
# the previous segment ends; a still that follows a moving clip starts on the moving clip's LAST frame (the moving montage is cut with blend 0, and only then),
# so the pose never falls back to idle. The frames shot after a moving clip ends just keep shooting: the montage's own blend out (8/60 s from
# its last frame) runs, nothing stops it. 'splat' puts a collision-free wall 30 cm behind the pawn's root for the sequence (Play world only).
# The wall is hidden from the az180 camera, which stands behind it.
# Review stage, this Play session only: the other players are parked in Z1 and three shadowless directional fill lights shine on the pawn.
# Output: <out>/<sequence>/<view>/f<frame>.png, <out>/<sequence>/frames.jsonl (per frame: f, seg, segf = frame inside the segment, montage =
# [name, position in 60 fps frames] of what really plays, loc, pelvis_z, head_z), <out>/capture.log and <out>/done.txt ("ok" or the error).
# Nothing is saved. Then QUIT_EDITOR. The grids and the mp4 come from Tools/GASNPPTest/pie_capture_grid.py (it reads the views from the folders).
import json
import math
import os
import sys
import time
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g   # noqa: E402
import test_hub as h      # noqa: E402

OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimReact',
                                                        time.strftime('capture_%Y%m%d_%H%M%S'))
SIZE = 480
# (name, azimuth in degrees from the pawn's forward, as a yaw: 0 = in front of it, 90 = on its RIGHT, 180 = BEHIND it); 'game' = the player's camera
VIEWS = [('az180', 180.0), ('az90', 90.0), ('game', None)]
ELEVATION, DISTANCE, FOV = 10.0, 380.0, 45.0
FILL_LIGHTS = [(35.0, 5.0), (325.0, 5.0), (95.0, 3.0)]   # (azimuth the light comes from, as in VIEWS; intensity in lux)
PRE = 10
FPS = 60.0
SLOT = 'DefaultSlot'
REACT = '/Game/GASNPPTest/Combat/Reaction/'
STILL_IN = 3 / FPS
# The moving montages blend out AFTER their last frame: blend_out 8/60 s, blend_out_trigger_time 0.0 (0 = the blend starts at the sequence end;
# -1 would start it blend_out BEFORE the end and the game's motion matching would replace the clip's last frames). Assets and fallback agree.
BLEND_OUT_END = 8 / FPS
LOOPS = 1000
# segment: kind, asset (clip name without AS_React_, montage AM_React_<name> for 'mont'), n frames, blend in, blend out (seconds)
def _loop(clip, n, bin_=0.0):
    return {'kind': 'loop', 'clip': clip, 'n': n, 'bin': bin_, 'bout': 0.0}


def _mont(name, n, bin_, bout):
    return {'kind': 'mont', 'clip': name, 'n': n, 'bin': bin_, 'bout': bout}


def _stop(n, bout=6 / FPS):
    return {'kind': 'stop', 'clip': None, 'n': n, 'bin': 0.0, 'bout': bout}


SEQUENCES = [
    {'name': 'hitstand', 'segs': [_loop('HitStand', 30, STILL_IN), _stop(15, 6 / FPS)]},
    {'name': 'tumble', 'segs': [_loop('Tumble', 30, STILL_IN), _stop(10)]},
    {'name': 'splat', 'wall': True, 'segs': [_loop('Splat', 30, STILL_IN), _stop(10)]},
    {'name': 'knockdown_getup', 'segs': [_loop('Tumble', 12, STILL_IN), _mont('GroundHit', 18, 4 / FPS, 0.0), _loop('LieUp', 15),
                                         _mont('GetUp', 26, 0.0, BLEND_OUT_END), _stop(25, 0.0)]},
    {'name': 'roll', 'segs': [_loop('LieUp', 15), _mont('Roll', 24, 0.0, BLEND_OUT_END), _stop(25, 0.0)]},
    {'name': 'rollback', 'segs': [_loop('LieUp', 15), _mont('RollBack', 24, 0.0, BLEND_OUT_END), _stop(25, 0.0)]},
]
if os.environ.get('PIE_CAPTURE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES if s['name'] in os.environ['PIE_CAPTURE_SEQUENCES'].split(',')]
WALL_MESH = '/Engine/BasicShapes/Cube'
WALL_SIZE = (20.0, 400.0, 300.0)    # thickness along the pawn's facing, width, height in cm
WALL_GAP = 30.0                     # cm from the pawn's root to the wall's face toward it (capsule radius is 30)
GROUND_BELOW_ROOT = 86.0            # capsule half height
TIMEOUT = 900.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'cams': [], 'rts': [], 'lights': [], 'handle': None,
      'montages': {}, 'rows': None, 'wall': None, 'cur': None, 'events': {}, 'total': 0}


def _log(msg):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'capture.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {msg}\n')


def _world():
    return g.worlds()[0]


def _pawn():
    return g._local_pawn(0)[1]


def _console(cmd):
    unreal.SystemLibrary.execute_console_command(_world(), cmd, g._local_pawn(0)[0])


def _ready():
    ws = g.worlds()
    if len(ws) < 3 or _pawn() is None:
        return False
    comp = g.local_comp(ws[0])
    return bool(comp and json.loads(comp.get_state_snapshot()).get('inputReady'))


def _mesh():
    """The skeletal mesh component whose anim instance the combat montages play on: the character's Mesh (ability actor info takes it), else the
    first skeletal mesh with an anim instance"""
    pawn = _pawn()
    try:
        mesh = pawn.get_editor_property('mesh')
        if mesh and mesh.get_anim_instance():
            return mesh
    except Exception:
        pass
    for comp in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_anim_instance():
            return comp
    return None


def _make_cameras():
    """Scene captures cannot be spawned in a Play world from Python: the cheat manager's Summon does it (one per call, found on the next tick)"""
    if st['cams']:
        return True
    found = list(unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.SceneCapture2D))
    if len(found) < len(VIEWS):
        _console('Summon SceneCapture2D')
        return False
    st['cams'] = found[:len(VIEWS)]
    for cam in st['cams']:
        comp = cam.capture_component2d
        rt = unreal.RenderingLibrary.create_render_target2d(_world(), SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        st['rts'].append(rt)
        for prop, value in (('capture_every_frame', False), ('capture_on_movement', False), ('always_persist_rendering_state', True),
                            ('fov_angle', FOV), ('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR), ('texture_target', rt)):
            comp.set_editor_property(prop, value)
    return True


def _clear_stage():
    parked = [h.park(i, 'Z1') for i in h._players() if i != 0]
    _console('gstn.Combat.KitPlaceholderLook 0')    # the kit's placeholder box on the host pawn (yellow, over the head and upper back)
    hid = []
    pawn = _pawn()
    for comp in pawn.get_components_by_class(unreal.StaticMeshComponent):
        comp.set_visibility(False, True)
        hid.append(comp.get_name())
    for a in pawn.get_attached_actors():
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            comp.set_visibility(False, True)
            hid.append(a.get_name() + '/' + comp.get_name())
    _log(f'hidden static meshes on the host pawn: {hid}')
    me = _pawn().get_actor_location()
    others = [round(c.get_owner().get_actor_location().distance(me)) for c in g.comps(_world()).values() if c.get_owner() != _pawn()]
    return f'parked {parked}, other pawns at {sorted(others)[:4]} cm'


def _make_lights():
    """Shadowless directional fill lights (summoned like the cameras, aimed with them): the hub's only light is a sun behind the player start"""
    if st['lights']:
        return True
    if st.get('lights_before') is None:
        st['lights_before'] = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.DirectionalLight)}
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.DirectionalLight) if a.get_name() not in st['lights_before']]
    if len(new) < len(FILL_LIGHTS):
        _console('Summon DirectionalLight')
        return False
    st['lights'] = new[:len(FILL_LIGHTS)]
    for (_az, intensity), light in zip(FILL_LIGHTS, st['lights']):
        comp = light.get_component_by_class(unreal.DirectionalLightComponent)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_intensity(intensity)
        comp.set_cast_shadows(False)
    return True


def _place_cameras():
    pawn = _pawn()
    centre, yaw = pawn.get_actor_location(), pawn.get_actor_rotation().yaw
    st['cam_yaw'] = yaw
    for (az, _intensity), light in zip(FILL_LIGHTS, st['lights']):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-25.0, yaw=yaw + az + 180.0), False)
    for (name, az), cam in zip(VIEWS, st['cams']):
        if az is None:
            continue
        a, e = math.radians(yaw + az), math.radians(ELEVATION)
        pos = unreal.Vector(centre.x + DISTANCE * math.cos(e) * math.cos(a), centre.y + DISTANCE * math.cos(e) * math.sin(a),
                            centre.z + DISTANCE * math.sin(e))
        cam.set_actor_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, centre), False, False)
    return (round(centre.x), round(centre.y), round(centre.z)), round(yaw)


def _shoot(seq, frame):
    manager = g._local_pawn(0)[0].get_editor_property('player_camera_manager')
    for (name, az), cam, rt in zip(VIEWS, st['cams'], st['rts']):
        comp = cam.capture_component2d
        if az is None:
            cam.set_actor_location_and_rotation(manager.get_camera_location(), manager.get_camera_rotation(), False, False)
            comp.set_editor_property('fov_angle', manager.get_fov_angle())
        comp.capture_scene()
        folder = os.path.join(OUT, seq['name'], name)
        os.makedirs(folder, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(_world(), rt, folder, f'f{frame:03d}.png')


# ---- wall (splat) ----------------------------------------------------------------------------------------------------------------------
def _make_wall():
    """True once the wall stands. StaticMeshActors are summoned (Python cannot spawn in a Play world), then given the cube, size and place"""
    if st['wall'] is not None:
        return True
    if st.get('walls_before') is None:
        st['walls_before'] = {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.StaticMeshActor)}
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.StaticMeshActor) if a.get_name() not in st['walls_before']]
    if not new:
        _console('Summon StaticMeshActor')
        return False
    wall = new[0]
    comp = wall.get_component_by_class(unreal.StaticMeshComponent)
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(unreal.load_asset(WALL_MESH))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    pawn = _pawn()
    loc, yaw = pawn.get_actor_location(), pawn.get_actor_rotation().yaw
    back = math.radians(yaw + 180.0)
    dist = WALL_GAP + WALL_SIZE[0] / 2.0
    centre = unreal.Vector(loc.x + dist * math.cos(back), loc.y + dist * math.sin(back), loc.z - GROUND_BELOW_ROOT + WALL_SIZE[2] / 2.0)
    wall.set_actor_location_and_rotation(centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False, False)
    wall.set_actor_scale3d(unreal.Vector(WALL_SIZE[0] / 100.0, WALL_SIZE[1] / 100.0, WALL_SIZE[2] / 100.0))
    st['wall'] = wall
    try:   # the az180 camera stands behind the wall: it would film its back
        for (name, _az), cam in zip(VIEWS, st['cams']):
            if name == 'az180':
                cam.capture_component2d.set_editor_property('hidden_actors', [wall])
    except Exception as e:
        _log(f'wall: could not hide it from az180: {e!r}')
    _log(f'wall at ({centre.x:.0f}, {centre.y:.0f}, {centre.z:.0f}) yaw {yaw:.0f}, face {WALL_GAP:.0f} cm behind the root')
    return True


def _drop_wall():
    if st['wall'] is not None:
        try:
            st['wall'].destroy_actor()
        except Exception as e:
            _log(f'wall destroy failed: {e!r}')
        st['wall'] = None
        st['walls_before'] = None
        for cam in st['cams']:
            try:
                cam.capture_component2d.set_editor_property('hidden_actors', [])
            except Exception:
                pass


# ---- clips -----------------------------------------------------------------------------------------------------------------------------
def _schedule(seq):
    """{frame: segment index} where each segment starts, the total frame count. A segment starts the frame after the previous one ends, except a
    still after a moving clip: it starts on that clip's last frame"""
    events, cursor, prev = {}, PRE, None
    for i, s in enumerate(seq['segs']):
        start = cursor - 1 if (s['kind'] == 'loop' and prev is not None and prev['kind'] == 'mont') else cursor
        events[start] = i
        cursor = start + s['n']
        prev = s
    return events, cursor


def _preload():
    """Load the montage assets before Play starts (loading them from the Play tick logged MISSING in r1) and keep them referenced"""
    for name in {s['clip'] for q in SEQUENCES for s in q['segs'] if s['kind'] == 'mont'}:
        path = REACT + 'AM_React_' + name
        asset = unreal.load_asset(path)
        st['montages'][name] = asset
        _log(f'preload {path}: {"ok, blend out " + format(asset.get_editor_property("blend_out").get_editor_property("blend_time") * FPS, ".1f") + "/60, trigger " + str(asset.get_editor_property("blend_out_trigger_time")) if asset else "FAILED"}')


def _asset(name):
    return st['montages'].get(name)


def _start(seg, prev):
    """Begin one segment on the mesh's anim instance"""
    mesh = _mesh()
    anim = mesh.get_anim_instance() if mesh else None
    if anim is None:
        raise RuntimeError('no anim instance on the pawn')
    cur = st['cur']
    if seg['kind'] == 'stop':
        if seg['bout'] > 0.0 and cur is not None and anim.montage_is_playing(cur):   # 'hold' (bout 0) after a moving clip: its own blend out runs
            anim.montage_stop(seg['bout'], cur)
        return
    if prev is not None and prev['kind'] == 'mont' and cur is not None and anim.montage_is_playing(cur):
        anim.montage_stop(0.0, cur)               # a still follows on the moving clip's last frame: no blend out from it
    if seg['kind'] == 'loop':
        clip = unreal.load_asset(REACT + 'AS_React_' + seg['clip'])
        st['cur'] = anim.play_slot_animation_as_dynamic_montage(clip, SLOT, seg['bin'], 0.0, 1.0, LOOPS, -1.0, 0.0)
        return
    montage = _asset(seg['clip'])
    if montage is not None:
        anim.montage_play(montage, 1.0)
        st['cur'] = montage
    else:
        _log(f'MISSING montage AM_React_{seg["clip"]}: dynamic montage of AS_React_{seg["clip"]}, blend in {seg["bin"] * FPS:.0f}/60 out {seg["bout"] * FPS:.0f}/60')
        clip = unreal.load_asset(REACT + 'AS_React_' + seg['clip'])
        st['cur'] = anim.play_slot_animation_as_dynamic_montage(clip, SLOT, seg['bin'], seg['bout'], 1.0, 1, -1.0, 0.0)


def _montage():
    try:
        mesh = _mesh()
        anim = mesh.get_anim_instance() if mesh else None
        montage = anim.get_current_active_montage() if anim else None
        if montage:
            return [montage.get_name(), round(anim.montage_get_position(montage) * FPS, 2)]
        cur = st.get('cur')     # no active montage: the one that blends out (not playing, but its instance may still be there)
        if cur is not None and anim:
            pos = anim.montage_get_position(cur)
            if pos > 0.0 and anim.montage_is_active(cur):
                return [cur.get_name(), round(pos * FPS, 2)]
    except Exception as e:
        return ['error', repr(e)[:80]]
    return None


def _bone_z(mesh, name):
    try:
        return round(mesh.get_socket_location(name).z, 1)
    except Exception:
        return None


def _row(seq, frame):
    segs, events = seq['segs'], st['events']
    idx = max((f for f in events if f <= frame), default=None)
    seg = segs[events[idx]] if idx is not None else None
    loc = _pawn().get_actor_location()
    mesh = _mesh()
    row = {'f': frame, 'seg': (seg['kind'] + ':' + (seg['clip'] or '')) if seg else 'idle', 'segf': frame - idx if idx is not None else frame,
           'montage': _montage(), 'loc': [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
           'pelvis_z': _bone_z(mesh, 'pelvis') if mesh else None, 'head_z': _bone_z(mesh, 'head') if mesh else None}
    st['rows'].write(json.dumps(row) + '\n')


def _finish(status):
    _log(f'finish: {status}')
    st['phase'] = 'ending'
    st['status'] = status
    try:
        _console('t.OverrideFPS 0')
    except Exception:
        pass
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()


def _tick(_dt):
    try:
        now, phase = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase != 'ending' and now - st['start'] > TIMEOUT:
            return _finish(f'timeout in phase {phase}')
        if phase == 'boot' and now - st['t'] > 8:
            _preload()
            _log(f'setup_editor: {g.setup_editor(True, "listen", 2)}')
            les.editor_request_begin_play()
            st.update(phase='wait_ready', t=now)
        elif phase == 'wait_ready':
            if les.is_in_play_in_editor() and _ready():
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready after 180 s')
        elif phase == 'settle' and now - st['t'] > 12:     # the hub's watcher runs play_setup 4 s after Play
            _log(f'review stage: {_clear_stage()}')
            mesh = _mesh()
            _log(f'anim mesh: {mesh.get_name() if mesh else None}, anim instance: {mesh.get_anim_instance().get_class().get_name() if mesh else None}')
            st.update(phase='cameras', t=now)
        elif phase == 'cameras':
            if _make_cameras() and _make_lights():
                _log(f'{len(st["cams"])} cameras, {len(st["lights"])} fill lights')
                st.update(phase='seq_begin', t=now)
            elif now - st['t'] > 20:
                _finish('could not summon the scene captures')
        elif phase == 'seq_begin' and now - st['t'] > 2:
            if st['seq'] >= len(SEQUENCES):
                return _finish('ok')
            seq = SEQUENCES[st['seq']]
            if seq.get('wall') and not _make_wall():
                return None
            _log(f'sequence {seq["name"]}: pawn at {_place_cameras()}')
            os.makedirs(os.path.join(OUT, seq['name']), exist_ok=True)
            st['rows'] = open(os.path.join(OUT, seq['name'], 'frames.jsonl'), 'w')
            st['events'], st['total'] = _schedule(seq)
            st['cur'] = None
            _console('t.OverrideFPS 60')
            st.update(phase='seq_run', frame=0)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            _shoot(seq, frame)
            _row(seq, frame)
            nxt = st['events'].get(frame + 1)    # a segment is started after this frame's shot, so the next shot is its first frame
            if nxt is not None:
                prev = seq['segs'][nxt - 1] if nxt > 0 else None
                _start(seq['segs'][nxt], prev)
                _log(f'{seq["name"]}: v{frame + 1} start {seq["segs"][nxt]["kind"]} {seq["segs"][nxt]["clip"]}')
            st['frame'] = frame + 1
            if st['frame'] >= st['total']:
                st['rows'].close()
                _console('t.OverrideFPS 0')
                _drop_wall()
                _log(f'sequence {seq["name"]} done: {st["frame"]} frames')
                st.update(phase='seq_begin', seq=st['seq'] + 1, t=now)
        elif phase == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:
                _log(f'setup_editor(False) failed: {e!r}')
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write(st.get('status', '?') + '\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:
        _log('ERROR ' + traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error (see capture.log)')
        else:
            unreal.unregister_slate_post_tick_callback(st['handle'])
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write('error while ending\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    return None


_log(f'driver loaded: out {OUT}, sequences {[s["name"] for s in SEQUENCES]}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
