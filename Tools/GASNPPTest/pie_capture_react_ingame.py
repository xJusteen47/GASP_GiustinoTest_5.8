# REACT-HOOK-1: a Play capture of the victim reactions IN GAME. Unlike pie_capture_react.py (clips played in place on the anim instance), nothing
# is played by this script: client 1's pawn really hits the host pawn with the test move, and what the host pawn shows comes from the game
# (UGSTN_ReactionAnimComponent reading Combat.State.*, the get-up abilities and their travel). Same stage as pie_capture_react.py: Play in L_Test_Hub,
# listen host + 2 clients, the third pawn parked in Z1, fixed fill lights, t.OverrideFPS 60 so one image = one 60 fps frame.
#
# Launch (editor with a real renderer; the script path must have no space, so boot through a one-line exec in a path without spaces):
#   UnrealEditor.exe <uproject> /Game/Levels/Test/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/pie_capture_react_ingame.py").read())
# Environment (optional): PIE_CAPTURE_OUT (default Saved/AnimReact/ingame_<date>_<time>), PIE_CAPTURE_SEQUENCES (subset of hitstand,
# knockdown_getup, roll, rollback).
# Content in memory only (nothing is saved): both pawns get KIT_Test_DefenceLanding (the landing bounce, the downed state with no automatic get-up,
# the get-up set with the roll back) through defence_scenarios.defence_setup(landing=True); hitstand uses the standard knockback thresholds at 0 %,
# the other sequences the thresholds at 0 (every hit launches into tumble).
# Sequences (frames at 60 fps, PRE idle frames first, then client 1 taps Attack):
#   hitstand         the hit and the hit stand, until the pawn is back in the game pose
#   knockdown_getup  launch -> tumble -> landing (bounce, ground hit chained into the lie), lying LIE frames, then the jump press: the neutral get-up
#   roll / rollback  the same landing, then (ROLL_LIE frames into the knockdown) the host holds the stick along / against its facing: the get-up roll toward the feet / the head
# Views: az180 (behind the pawn) and az90 (its right side), fixed azimuth from the pawn's yaw at the sequence start; the cameras follow the pawn
# only when it leaves a DEADZONE cm circle around their aim point (the launch travels several metres), so short travel (the rolls) stays visible.
# Output: <out>/<sequence>/<view>/f<frame>.png, <out>/<sequence>/frames.jsonl (f, seg = the host's combat state, segf = frames in it, montage,
# loc, tap), <out>/capture.log, <out>/done.txt. The mp4 and the sheets come from pie_capture_grid.py.
import json
import math
import os
import sys
import time
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g          # noqa: E402
import test_hub as h             # noqa: E402
import defence_scenarios as ds   # noqa: E402

OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimReact',
                                                        time.strftime('ingame_%Y%m%d_%H%M%S'))
SIZE = 480
# react-hook-2: the victim faces the attacker (it was hit from behind in react-hook-1). az45 = in front of the victim, a quarter off the attacker's
# line (the attacker is hidden in it), az90 = its right side with both pawns in profile
VIEWS = [('az45', 45.0), ('az90', 90.0)]
HIDE_ATTACKER = {'az45'}
ELEVATION, DISTANCE, FOV = 10.0, 320.0, 45.0
DEADZONE = 40.0   # the pawn never leaves the middle of the frame (react-hook-1: cut at the edge)
FILL_LIGHTS = [(35.0, 5.0), (325.0, 5.0), (95.0, 3.0)]
PRE = 10
LIE = 50          # frames lying after the knockdown starts before the get-up input (the ground hit and the lie must read)
AFTER = 70        # frames shot after the get-up ends (the held pose blending out, the game pose settling)
MAX_FRAMES = 420
STATES = ['Hitstun', 'Tumble', 'Knockdown', 'Splat']
SEQUENCES = [
    {'name': 'hitstand', 'launch': False, 'getup': None},
    {'name': 'knockdown_getup', 'launch': True, 'getup': 'jump'},
    {'name': 'roll', 'launch': True, 'getup': 'facing'},
    {'name': 'rollback', 'launch': True, 'getup': 'against'},
    {'name': 'roll_late', 'launch': True, 'getup': 'facing', 'lie': 50},   # the stick only after lying 50 frames: a roll on any frame while down
]
if os.environ.get('PIE_CAPTURE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES if s['name'] in os.environ['PIE_CAPTURE_SEQUENCES'].split(',')]
ATTACKER = 1
KIT = 'KIT_Test_DefenceLanding'
ROLL_LIE = 4      # roll sequences: the stick is held from this many frames after the knockdown starts; the roll must wait for the landing
FACE_SECONDS = float(os.environ.get('PIE_CAPTURE_FACE_SECONDS') or 0.3)  # the victim walks toward the attacker this long to face it, from PLACE cm away
PLACE = 245.0
TIMEOUT = 1500.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'cams': [], 'rts': [], 'lights': [], 'handle': None,
      'rows': None, 'aim': None, 'yaw': 0.0}


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


def _make_lights():
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


def _clear_stage():
    """Parks every player but the host (victim) and client 1 (attacker); hides the static meshes on the host pawn (kit placeholder)"""
    parked = [h.park(i, 'Z1') for i in h._players() if i not in (0, ATTACKER)]
    _console('gstn.Combat.KitPlaceholderLook 0')
    for comp in _pawn().get_components_by_class(unreal.StaticMeshComponent):
        comp.set_visibility(False, True)
    for a in _pawn().get_attached_actors():
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            comp.set_visibility(False, True)
    return f'parked {parked}'


def _hide_from(cam, view):
    """Every pawn but the victim (and the attacker, except in the views of HIDE_ATTACKER) is hidden from this capture"""
    victim = _pawn()
    attacker = g._local_pawn(ATTACKER)[1] if len(g.worlds()) > ATTACKER else None
    hidden = []
    for p in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Pawn):
        if p == victim:
            continue
        if attacker is not None and _same_player(p, attacker) and view not in HIDE_ATTACKER:
            continue
        hidden.append(p)
    cam.capture_component2d.set_editor_property('hidden_actors', hidden)


def _same_player(server_pawn, client_pawn):
    """The server world's copy of a client's pawn: the attacker's location read in the server snapshot"""
    try:
        alo = g.snap(_world())[st['akey']]['location']
        loc = server_pawn.get_actor_location()
        return math.dist((loc.x, loc.y, loc.z), alo) < 5.0
    except Exception:
        return False


def _aim_cameras():
    """Moves the aim point toward the pawn only past DEADZONE, then places the cameras around it at the sequence's fixed azimuths"""
    loc = _pawn().get_actor_location()
    if st['aim'] is None:
        st['aim'] = [loc.x, loc.y, loc.z]
    ax, ay, az_ = st['aim']
    dx, dy = loc.x - ax, loc.y - ay
    d = math.hypot(dx, dy)
    if d > DEADZONE:
        k = (d - DEADZONE) / d
        ax, ay = ax + dx * k, ay + dy * k
    az_ = az_ + (loc.z - az_) * 0.3 if abs(loc.z - az_) > 30.0 else az_
    st['aim'] = [ax, ay, az_]
    centre = unreal.Vector(ax, ay, az_)
    for (name, azim), cam in zip(VIEWS, st['cams']):
        a, e = math.radians(st['yaw'] + azim), math.radians(ELEVATION)
        pos = unreal.Vector(centre.x + DISTANCE * math.cos(e) * math.cos(a), centre.y + DISTANCE * math.cos(e) * math.sin(a),
                            centre.z + DISTANCE * math.sin(e))
        cam.set_actor_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, centre), False, False)


def _place_lights():
    for (az, _intensity), light in zip(FILL_LIGHTS, st['lights']):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-25.0, yaw=st['yaw'] + az + 180.0), False)


def _shoot(seq, frame):
    _aim_cameras()
    for (name, _az), cam, rt in zip(VIEWS, st['cams'], st['rts']):
        _hide_from(cam, name)
        cam.capture_component2d.capture_scene()
        folder = os.path.join(OUT, seq['name'], name)
        os.makedirs(folder, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(_world(), rt, folder, f'f{frame:03d}.png')


def _states():
    return [s for s in STATES if ds.has_tag(0, st['hkey'], 'Combat.State.' + s)]


def _montage():
    try:
        anim = _mesh().get_anim_instance()
        m = anim.get_current_active_montage()
        return [m.get_name(), round(anim.montage_get_position(m) * 60.0, 2)] if m else None
    except Exception as e:
        return ['error', repr(e)[:80]]


# ---- the sequence: frame-driven, one tick = one 60 fps frame -------------------------------------------------------------------------------
def _seq_prep(seq):
    if _states():   # still downed from the sequence before: get up first (jump), the caller waits
        g.release(0, 'move')
        g.combat_tap(0, g.button_mask('Jump'))
        return False
    g.hit_rules_setup()
    if seq['launch']:
        g.set_tuning(push_back_threshold=0.0, launch_threshold=0.0, tumble_threshold=0.0)
    g.set_damage(st['hkey'], 0)
    g.release(0, 'move')
    # In FRONT of the attacker and facing it (react-hook-1 placed it with its back to the attacker): a short walk toward the attacker turns it,
    # then the pawn stops about 205 cm away (the test move's reach)
    ax, ay = g._place_near(ATTACKER, st['hkey'], (PLACE, 0.0))
    alo = g.snap(_world())[st['akey']]['location']
    g.hold(0, 'move', g._steer_value(0, (alo[0], alo[1])))
    return True


def _stick(direction):
    loc, yaw = _pawn().get_actor_location(), math.radians(_pawn().get_actor_rotation().yaw + (0.0 if direction == 'facing' else 180.0))
    target = (loc.x + 300.0 * math.cos(yaw), loc.y + 300.0 * math.sin(yaw))
    g.hold(0, 'move', g._steer_value(0, target))
    return round(math.degrees(yaw))


def _step(seq, frame, states):
    """Inputs of this frame; returns (tap text or None, done)"""
    s = st['run']
    down = 'Knockdown' in states
    if frame == PRE:
        g.combat_tap(ATTACKER, g.button_mask('Attack'))
        return 'attack c1', False
    if frame <= PRE + 5:
        return None, False
    if not seq['launch']:
        if not states and s.get('hit') and s.setdefault('free_at', frame) + AFTER <= frame:
            return None, True
        if states:
            s['hit'] = True
            s.pop('free_at', None)
        return None, frame >= MAX_FRAMES
    if down and 'down_at' not in s:
        s['down_at'] = frame
    tap = None
    if 'down_at' in s and 'input_at' not in s and frame >= s['down_at'] + seq.get('lie', LIE if seq['getup'] == 'jump' else ROLL_LIE):
        s['input_at'] = frame
        if seq['getup'] == 'jump':
            g.combat_tap(0, g.button_mask('Jump'))
            tap = 'jump'
        else:
            tap = f'stick {seq["getup"]} yaw {_stick(seq["getup"])}'
    if 'input_at' in s and not down and 'up_at' not in s:
        s['up_at'] = frame
    if 'up_at' in s and seq['getup'] != 'jump' and 'released' not in s and not states and frame >= s['up_at'] + 26:
        g.release(0, 'move')   # the stick held past the roll would walk the pawn: let go once the roll is over (24 frames)
        s['released'] = frame
        tap = 'stick released'
    done = ('up_at' in s and frame >= s['up_at'] + 26 + AFTER) or frame >= MAX_FRAMES
    return tap, done


def _row(frame, states, tap):
    s = st['run']
    key = '+'.join(states) if states else ('getup' if 'input_at' in s and 'up_at' in s and frame < s['up_at'] + 26 else 'game')
    if key != s.get('seg'):
        s['seg'], s['seg0'] = key, frame
    loc = _pawn().get_actor_location()
    row = {'f': frame, 'seg': key, 'segf': frame - s['seg0'], 'montage': _montage(), 'loc': [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
           'yaw': round(_pawn().get_actor_rotation().yaw, 1)}
    if tap:
        row['tap'] = tap
    st['rows'].write(json.dumps(row) + '\n')


def _finish(status):
    _log(f'finish: {status}')
    st['phase'] = 'ending'
    st['status'] = status
    try:
        _console('t.OverrideFPS 0')
        g.release(0, 'move')
        ds.defence_reset()
        g.hit_rules_setup()
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
        elif phase == 'settle' and now - st['t'] > 12:
            st['hkey'], st['akey'] = g.key_of_player(0), g.key_of_player(ATTACKER)
            _log(f'review stage: {_clear_stage()}; host {st["hkey"]}, attacker {st["akey"]}')
            g.hit_rules_setup()
            # The saved content kit, applied directly: defence_setup() fell back to the runtime path on KIT_Test_Hits in the first run (no roll back)
            kit = KIT
            if not ds._has_steps_saved():
                ds.Lib.add_defence_steps_to_rules(g.hit_rules())
                ds._saved['steps'] = True
            ds._saved['kit'] = kit
            _log(f'defence steps saved in the rules: {ds._has_steps_saved()}')
            _log(f'kits {kit}: host {g._apply_loadout(st["hkey"], kit)}, attacker {g._apply_loadout(st["akey"], kit)}')
            mesh = _mesh()
            comps = [c.get_class().get_name() for c in _pawn().get_components_by_class(unreal.ActorComponent) if 'Reaction' in c.get_class().get_name()]
            _log(f'anim mesh {mesh.get_name() if mesh else None}, reaction components {comps}')
            st.update(phase='cameras', t=now)
        elif phase == 'cameras':
            if _make_cameras() and _make_lights():
                st.update(phase='seq_prep', t=now)
            elif now - st['t'] > 20:
                _finish('could not summon the scene captures')
        elif phase == 'seq_prep' and now - st['t'] > 1.5:
            if st['seq'] >= len(SEQUENCES):
                return _finish('ok')
            if _seq_prep(SEQUENCES[st['seq']]):
                st.update(phase='seq_face', t=now)
            else:
                _log('host still downed: jump to get up before the next sequence')
                st.update(t=now)
        elif phase == 'seq_face' and now - st['t'] > FACE_SECONDS:
            g.release(0, 'move')
            st.update(phase='seq_begin', t=now)
        elif phase == 'seq_begin' and now - st['t'] > 2.5:
            seq = SEQUENCES[st['seq']]
            st['yaw'], st['aim'], st['run'] = _pawn().get_actor_rotation().yaw, None, {}
            _place_lights()
            loc = _pawn().get_actor_location()
            alo = g.snap(_world())[st['akey']]['location']
            a = g.snap(_world())[st['akey']]
            dist = math.hypot(loc.x - alo[0], loc.y - alo[1])
            facing_err = abs((st['yaw'] - (a['yaw'] + 180.0) + 180.0) % 360.0 - 180.0)
            _log(f'sequence {seq["name"]}: host at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}) yaw {st["yaw"]:.0f}, attacker at {[round(v) for v in alo]} '
                 f'yaw {a["yaw"]:.0f}, distance {dist:.0f} cm, facing error {facing_err:.0f} deg (0 = face to face), states {_states()}')
            os.makedirs(os.path.join(OUT, seq['name']), exist_ok=True)
            st['rows'] = open(os.path.join(OUT, seq['name'], 'frames.jsonl'), 'w')
            _console('t.OverrideFPS 60')
            st.update(phase='seq_run', frame=0)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            states = _states()
            _shoot(seq, frame)
            tap, done = _step(seq, frame, states)
            _row(frame, states, tap)
            if tap:
                _log(f'{seq["name"]}: v{frame} {tap} (states {states})')
            st['frame'] = frame + 1
            if done:
                st['rows'].close()
                _console('t.OverrideFPS 0')
                g.release(0, 'move')
                _log(f'sequence {seq["name"]} done: {st["frame"]} frames, run {st["run"]}')
                st.update(phase='seq_prep', seq=st['seq'] + 1, t=now)
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
