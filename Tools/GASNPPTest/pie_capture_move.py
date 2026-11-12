# ANIM-JAB-3: a REAL Play capture of a kit's move for the animation review (project rule 10: someone watches the in-game video before the owner).
# Unlike Tools/AnimPipeline/ingame_judge.py (a montage scrubbed in L_PilotCapture, no game running), this starts Play in L_Test_Hub (listen host
# + 2 clients, the hub's own automation), gives the host the kit, presses the kit's attack button through the combat component's scripted
# buttons (resolved by name from the input config, no key is named here) and shoots every rendered frame from fixed review cameras plus the
# player's own camera. While a sequence runs the engine steps exactly 1/60 s per rendered frame (t.OverrideFPS 60), so a slow capture drops no
# frame: one image = one 60 fps frame of what a player sees. The simulation rate is not touched.
#
# Launch (editor with a real renderer; the script path must have no space, so boot through a copy or a one-line exec):
#   UnrealEditor.exe <uproject> /Game/Levels/Test/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/pie_capture_move.py").read())
# Environment (all optional): PIE_CAPTURE_KIT (KIT_Test_Overlord), PIE_CAPTURE_OUT (default Saved/AnimJab3/capture_<date>_<time>),
# PIE_CAPTURE_SEQUENCES (default: all of SEQUENCES, or all of DAIR_SEQUENCES when the kit's name contains DAir)
# Down-air captures (ANIM-DAIR-1): PIE_CAPTURE_KIT=KIT_Test_OverlordDAirV8 (or ...V7, one kit per Play session) and PIE_CAPTURE_OUT=<dir>; the
# sequences with an 'air' entry jump first and press crouch + attack on the way down, see DAIR_SEQUENCES
# Review stage, this Play session only: the other players are parked in Z1, the kit's placeholder cube is hidden and three shadowless
# directional fill lights shine on the pawn (the hub's sun is behind the player start, the character's front would be black).
# Output: <out>/<sequence>/<view>/f<frame>.png, <out>/<sequence>/frames.jsonl (the host's combat state per frame), <out>/capture.log and
# <out>/done.txt ("ok" or the error). Nothing is saved: the level and the kits stay as they are on disk. Then QUIT_EDITOR.
# The grids and the mp4 are made outside the editor by Tools/GASNPPTest/pie_capture_grid.py.
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

KIT = os.environ.get('PIE_CAPTURE_KIT', 'KIT_Test_Overlord')
OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimJab3',
                                                        time.strftime('capture_%Y%m%d_%H%M%S'))
SIZE = 480
# Review cameras around the pawn: (name, azimuth in degrees from the pawn's forward, as a yaw: 0 = in front of it, 90 = on its RIGHT, 270 = on
# its left, 45 = front right, 315 = front left). 'game' = the player's camera
VIEWS = [('az0', 0.0), ('az45', 45.0), ('az90', 90.0), ('az180', 180.0), ('az270', 270.0), ('az315', 315.0), ('game', None)]
ELEVATION, DISTANCE, FOV = 10.0, 380.0, 45.0
FILL_LIGHTS = [(35.0, 5.0), (325.0, 5.0), (95.0, 3.0)]   # (azimuth the light comes from, as in VIEWS; intensity in lux)
PRE = 20         # idle frames shot before the first press (the captures' temporal history settles)
BUTTON = 'Attack'
# taps: rendered frames after PRE on which the button is pressed for one frame. dummy: a dummy this many cm ahead (None = whiff)
SEQUENCES = [
    {'name': 'jab1', 'taps': [0], 'frames': 75, 'dummy': None, 'shoot': True},
    {'name': 'chain', 'taps': [0, 12], 'frames': 120, 'dummy': None, 'shoot': True},
    {'name': 'chain_buffered', 'taps': [0, 4], 'frames': 120, 'dummy': None, 'shoot': False},
    {'name': 'chain_hit', 'taps': [0, 12], 'frames': 140, 'dummy': 95.0, 'shoot': True},
]
# PIE_CAPTURE_SEQUENCES: a comma separated subset of the names above (a recheck of one case does not pay for the other captures)
# Down-air (aerial) sequences. 'air': the pawn jumps on frame PRE (a press of Jump) and, on the way down (z falling) once its height above the
# standing height is at most trigger_h cm, crouch is held and, the next rendered frame, Attack is tapped (command CrouchNeutral, bound while
# Airborne by setup_overlord_dair_kit.py). The press frame is the same for the V8 and the V7 kit (the simulation is deterministic), so frame
# numbers compare; if the height is never reached the press is made at PRE + AIR_GIVE_UP and the log says so. trigger_h is a first guess for the
# jump height of the test character (not measured, the rows hold 'h' per frame to retune it): high = the move finishes in the air, low = the
# character lands in the middle of it. Order matters: the hit case comes last because its dummy stays where it was put.
# Measured on the first run (2026-11-12, capture_v8_r2): the test character's jump peaks 233 cm above the standing height 42 frames after the press
# and lands 37 frames after the peak, so a 43 frame move pressed on the way down always lands before its end (pressed at 136 cm it landed on
# animation frame 12, before the strike). Hence: the whiff is pressed on the way UP ('rise_h': the first rising frame at or above that height) and
# ends in the air; the landing case is pressed at 184 cm falling (lands about frame 20, in the hold); the hit case at 225 cm falling, so the feet are
# at about 150 cm (the dummy's chest and head) on frame 15, with the dummy 62 cm ahead (the hitbox is a 40 cm sphere on the foot, a pawn is about
# 30 cm wide: at 80 cm it did not reach). 'jump_only' never presses: it records where a plain jump lands (the standing height after a landing)
DAIR_SEQUENCES = [
    {'name': 'dair_whiff', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'air': {'rise_h': 150.0}},
    {'name': 'dair_land', 'taps': [], 'frames': 130, 'dummy': None, 'shoot': True, 'air': {'trigger_h': 184.0}},
    {'name': 'jump_only', 'taps': [], 'frames': 110, 'dummy': None, 'shoot': False, 'air': {}},
    {'name': 'dair_hit', 'taps': [], 'frames': 130, 'dummy': 62.0, 'shoot': True, 'air': {'trigger_h': 225.0}},
]
AIR_GIVE_UP, AIR_CROUCH_RELEASE = 100, 4   # frames after the jump press / after the attack press
if os.environ.get('PIE_CAPTURE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES + DAIR_SEQUENCES if s['name'] in os.environ['PIE_CAPTURE_SEQUENCES'].split(',')]
elif 'DAir' in KIT:
    SEQUENCES = DAIR_SEQUENCES
TIMEOUT = 900.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'cams': [], 'rts': [], 'lights': [], 'handle': None, 'log': None, 'rows': None}


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
    """Review only, nothing saved: the other players leave the piazza (they spawn a step away and hide the move) and the kit's placeholder cube
    on the back is hidden (it covers the trunk and the arms from behind)"""
    parked = [h.park(i, 'Z1') for i in h._players() if i != 0]
    _console('gstn.Combat.KitPlaceholderLook 0')
    me = _pawn().get_actor_location()
    others = [round(c.get_owner().get_actor_location().distance(me)) for c in g.comps(_world()).values() if c.get_owner() != _pawn()]
    return f'parked {parked}, other pawns at {sorted(others)[:4]} cm'


def _make_lights():
    """The hub's only light is a sun behind the player start and it has no sky: the front of the character is black (point lights do not help,
    the mannequin is metallic). Shadowless directional fill lights (summoned like the cameras, aimed with them) make every side readable;
    they exist only in this Play session"""
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


def _place_cameras(follow=False):
    """follow=True (aerial sequences, every frame): the cameras keep the yaw of the sequence start and move with the pawn, which stays in the
    centre of the picture (the jump is 233 cm high, a fixed camera at 380 cm loses it); the height is in the rows ('h'), not in these pictures"""
    pawn = _pawn()
    centre, yaw = pawn.get_actor_location(), pawn.get_actor_rotation().yaw
    if follow:
        yaw = st.get('cam_yaw', yaw)
    else:
        st['cam_yaw'] = yaw
    for (az, _intensity), light in zip(FILL_LIGHTS, st['lights']):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-25.0, yaw=yaw + az + 180.0), False)   # shines from azimuth az toward the pawn
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
    if seq.get('air') is not None:
        _place_cameras(follow=True)
    for (name, az), cam, rt in zip(VIEWS, st['cams'], st['rts']):
        comp = cam.capture_component2d
        if az is None:
            cam.set_actor_location_and_rotation(manager.get_camera_location(), manager.get_camera_rotation(), False, False)
            comp.set_editor_property('fov_angle', manager.get_fov_angle())
        comp.capture_scene()
        folder = os.path.join(OUT, seq['name'], name)
        os.makedirs(folder, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(_world(), rt, folder, f'f{frame:03d}.png')


def _montage():
    """[montage name, position in 60 fps animation frames] of the first skeletal mesh that plays one, or None: what the picture really shows"""
    try:
        for comp in _pawn().get_components_by_class(unreal.SkeletalMeshComponent):
            anim = comp.get_anim_instance()
            montage = anim.get_current_active_montage() if anim else None
            if montage:
                return [montage.get_name(), round(anim.montage_get_position(montage) * 60.0, 2)]
    except Exception as e:
        return ['error', repr(e)[:80]]
    return None


def _row(frame, tapped, extra=None):
    view = g._view(0, h._key(0)) or {}
    row = {'f': frame, 'tap': tapped, 'combat': view.get('combat'), 'tags': view.get('tags'), 'loc': view.get('location'), 'montage': _montage()}
    row.update(extra or {})
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
            _log(f'setup_editor: {g.setup_editor(True, "listen", 2)}')
            les.editor_request_begin_play()
            st.update(phase='wait_ready', t=now)
        elif phase == 'wait_ready':
            if les.is_in_play_in_editor() and _ready():
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready after 180 s')
        elif phase == 'settle' and now - st['t'] > 12:     # the hub's watcher runs play_setup 4 s after Play (dummies, kits, hit rules)
            _log(f'kit {KIT}: {h.kit(0, KIT)}')
            st.update(phase='kit_wait', t=now)
        elif phase == 'kit_wait' and now - st['t'] > 3:
            view = g._view(0, h._key(0)) or {}
            _log(f'loadout: {json.dumps(view.get("loadout"))[:300]}')
            _log(f'review stage: {_clear_stage()}')
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
            if seq['dummy'] and not st.get('dummy_placed'):
                keys = h.dummies()
                _log(f'dummy {keys[:1]} ahead: {h._place_ahead(0, keys[0], seq["dummy"]) if keys else "NO DUMMY"}')
                st.update(dummy_placed=True, t=now)         # one more wait for the teleport to settle
                return None
            if seq.get('air') is not None:
                z = _pawn().get_actor_location().z
                if (st.get('rest_z') is None or abs(z - st['rest_z']) > 0.05) and now - st['t'] < 10:
                    st['rest_z'] = z
                    return None                              # still falling or landing from the last sequence
                # Per sequence: the standing height is not the same before and after the first landing (100 then 88 cm on the first run)
                st['ground_z'] = z
                _log(f'{seq["name"]}: standing height z {z:.1f}')
            _log(f'sequence {seq["name"]}: pawn at {_place_cameras()}')
            os.makedirs(os.path.join(OUT, seq['name']), exist_ok=True)
            st['rows'] = open(os.path.join(OUT, seq['name'], 'frames.jsonl'), 'w')
            _console('t.OverrideFPS 60')
            st.update(phase='seq_run', frame=0, mask=g.button_mask(BUTTON), crouch=g.button_mask('Crouch'), jumped=None, fired=None, prev_z=None)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            if seq['shoot']:
                _shoot(seq, frame)
            tapped = (frame - PRE) in seq['taps']
            extra, hold_crouch = None, False
            air = seq.get('air')
            if air is not None:
                z = _pawn().get_actor_location().z
                height = z - st.get('ground_z', z)
                extra = {'h': round(height, 1)}
                rising = st['prev_z'] is not None and z > st['prev_z']
                falling = st['prev_z'] is not None and z < st['prev_z']
                if 'rise_h' in air:
                    reached = rising and height >= air['rise_h']
                elif 'trigger_h' in air:
                    reached = falling and height <= air['trigger_h']
                else:
                    reached = None                           # a plain jump: nothing is pressed
                if frame == PRE:
                    st['jumped'] = frame
                    g.press(0, 'jump')
                elif reached is not None and st['jumped'] is not None and st['fired'] is None and frame > PRE + 3 and (
                        reached or frame - PRE >= AIR_GIVE_UP):
                    st['fired'] = frame + 1                  # crouch now, Attack on the next frame
                    hold_crouch = True
                    _log(f'{seq["name"]}: crouch on v{frame} (height {height:.0f} cm, {"rising" if rising else "falling"}), attack on v{frame + 1}')
                tapped = st['fired'] is not None and frame == st['fired']
                st['prev_z'] = z
            _row(frame, tapped, extra)
            if hold_crouch:
                g.combat_hold(0, st['crouch'])
            if tapped:
                g.combat_tap(0, st['mask'])                 # read by the next simulation tick
            if air is not None and st['fired'] is not None and frame == st['fired'] + AIR_CROUCH_RELEASE:
                g.combat_clear(0)
            st['frame'] = frame + 1
            if st['frame'] >= PRE + seq['frames']:
                st['rows'].close()
                g.combat_clear(0)
                _console('t.OverrideFPS 0')
                _log(f'sequence {seq["name"]} done: {st["frame"]} frames, pawn z {_pawn().get_actor_location().z:.1f}')
                st.update(phase='seq_begin', seq=st['seq'] + 1, t=now, dummy_placed=False)
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


_log(f'driver loaded: kit {KIT}, out {OUT}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
