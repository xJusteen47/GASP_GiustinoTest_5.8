# POLISH-M2: in-game capture of the presentation VFX kit v1 for the rule 10 review (VFX off, then the same frames with VFX on).
# A REAL Play session in L_Test_Hub (listen host + 2 clients, the hub's automation): the host runs, skids, sprints, jumps and hits a dummy
# at three damage percents (three hit sizes; the last one launches it: tumble and ground bounce). While a sequence runs the engine steps
# exactly 1/60 s per rendered frame (t.OverrideFPS 60): one image = one 60 fps frame. The simulation is deterministic and every session
# starts from the same spawn with the same inputs, so the OFF and ON sessions show the same frames: run this script twice, with
# PIE_VFX=0 and PIE_VFX=1 (console GSTN.Presentation.Effects; the events still fire and are logged either way).
#
# Launch (editor with a real renderer; the -ExecCmds script path must have no space: boot through a copy, see Run-Timed -Kind Capture):
#   UnrealEditor.exe <uproject> /Game/Levels/Test/L_Test_Hub -ExecCmds="py <boot.py>" -windowed -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/pie_capture_vfx.py").read())
# Environment: PIE_VFX (1), PIE_CAPTURE_OUT (Saved/PolishM2/vfx<0|1>), PIE_CAPTURE_SEQUENCES (comma list, default all), PIE_CAPTURE_KIT
# (KIT_Test_Hits). Output: <out>/<sequence>/<view>/f<frame>.png, <out>/capture.log, <out>/done.txt; the presentation events of every
# world go to the editor log (GSTN.Presentation.Log 1, lines "PRES ..."). Nothing is saved. Then QUIT_EDITOR.
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

VFX = os.environ.get('PIE_VFX', '1')
KIT = os.environ.get('PIE_CAPTURE_KIT', 'KIT_Test_Hits')
OUT = os.environ.get('PIE_CAPTURE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                                                        'PolishM2', f'vfx{VFX}')
SIZE = 480
# 'side': on the pawn's right at the sequence start, follows the pawn (movement), the victim (follow_victim) or frames pawn + dummy (hits);
# 'game': the player's camera; 'close': the player's camera at CLOSE x its field of view (review r1: the fighter was ~45 px high in 'game')
VIEWS = ['side', 'game', 'close']
CLOSE = 0.5
CUBE = '/Engine/BasicShapes/Cube'
ELEVATION, FOV = 12.0, 50.0
PRE = 20
FWD, BACK = (0, 1, 0), (0, -1, 0)   # stick, relative to the camera (as in scenario_combat_main_inputs)
# stick: [(from frame, to frame, direction)] after PRE; jump: frames after PRE pressing Jump; tap: frames pressing Attack;
# dummy: (cm ahead, damage percent) or None; dist: camera distance; follow: the side camera moves with the pawn; follow_victim: with the
# dummy; kit: the host's kit for this sequence (default KIT); strong: frames pressing Strong; blocks: boxes summoned for the sequence and
# removed after it, (cm ahead, cm to the side, cm above the floor, size along, across, up), placed along the host's facing
SEQUENCES = [
    {'name': 'jump', 'frames': 100, 'jump': [0], 'dist': 520.0, 'follow': False},
    {'name': 'hit_small', 'frames': 60, 'tap': [0], 'dummy': (95.0, 0.0), 'dist': 480.0, 'follow': False},
    {'name': 'hit_medium', 'frames': 80, 'tap': [0], 'dummy': (95.0, 90.0), 'dist': 480.0, 'follow': False},
    # The launch, followed until the downed landing (~190 frames after the hit) and a little after
    {'name': 'hit_large', 'frames': 260, 'tap': [0], 'dummy': (95.0, 170.0), 'dist': 650.0, 'follow': False, 'follow_victim': True},
    # Wall and ceiling contacts of a launched victim (NS_GSTN_Impact): a wall across the launch and a low ceiling over it
    {'name': 'wall', 'frames': 120, 'tap': [0], 'dummy': (95.0, 170.0), 'dist': 650.0, 'follow': False, 'follow_victim': True,
     'blocks': [(420.0, 0.0, 0.0, 60.0, 700.0, 500.0)]},
    {'name': 'ceiling', 'frames': 120, 'tap': [0], 'dummy': (95.0, 170.0), 'dist': 650.0, 'follow': False, 'follow_victim': True,
     'blocks': [(650.0, 0.0, 300.0, 1200.0, 700.0, 40.0)]},
    # The small default bump (combat rules WallImpact, under 1200 cm/s): the medium launch (90 %) into a nearer wall. 200 frames: the
    # Bump.Wall event comes ~176 frames in (r3 missed it at 90)
    # side_az 135 (review r4): from 90 the camera saw the wall edge-on (impact ring on its plane not judgeable); 135 = behind-right,
    # 45 would put the camera behind the wall
    {'name': 'wall_bump', 'frames': 200, 'side_az': 135.0, 'tap': [0], 'dummy': (95.0, 90.0), 'dist': 520.0, 'follow': False, 'follow_victim': True,
     'blocks': [(260.0, 0.0, 0.0, 60.0, 700.0, 500.0)]},
    # KIT_Example_Impacts: strong = wall splat (a wall behind the victim), attack = spike with a ground bounce
    {'name': 'splat', 'frames': 120, 'strong': [0], 'kit': 'KIT_Example_Impacts', 'dummy': (95.0, 100.0), 'dist': 650.0, 'follow': False,
     'follow_victim': True, 'blocks': [(330.0, 0.0, 0.0, 60.0, 700.0, 500.0)]},
    {'name': 'ground_bounce', 'frames': 120, 'tap': [0], 'kit': 'KIT_Example_Impacts', 'dummy': (95.0, 100.0), 'dist': 650.0,
     'follow': False, 'follow_victim': True},
    # Movement last: the sprint ends ~37 m away, near where the other players are parked
    {'name': 'run_skid', 'frames': 200, 'stick': [(0, 110, FWD), (110, 150, BACK)], 'dist': 650.0, 'follow': True},
    {'name': 'sprint', 'frames': 300, 'stick': [(0, 250, FWD)], 'dist': 700.0, 'follow': True},
]
if os.environ.get('PIE_CAPTURE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES if s['name'] in os.environ['PIE_CAPTURE_SEQUENCES'].split(',')]
FILL_LIGHTS = [(35.0, 5.0), (325.0, 5.0), (95.0, 3.0)]
WARMUP = ['NS_GSTN_RunStartDust', 'NS_GSTN_SkidDust', 'NS_GSTN_JumpPuff', 'NS_GSTN_LandRing', 'NS_GSTN_SpeedLines', 'NS_GSTN_HitSpark',
          'NS_GSTN_Impact', 'NS_GSTN_TumbleTrail']
# PIE_SWATCH=1: colour calibration run. NS_GSTN_Swatch (setup_presentation_vfx.swatch) is placed between the side camera and the scene at
# every sequence start, facing the camera; read the side frames with Tools/GASNPPTest/vfx_swatch.py
SWATCH = os.environ.get('PIE_SWATCH') == '1'
if SWATCH:
    WARMUP.append('NS_GSTN_Swatch')
# PIE_PROBE=1: debug run. At the first sequence frame the kit's burst systems are spawned 6 m above the pawn (unscaled, hit at intensity
# 0.56) and their particles (position / velocity) are written to capture.log for 12 frames (UGSTN_VfxAuthoringLibrary.DescribeParticles)
PROBE = [('NS_GSTN_HitSpark', ['Stars', 'Accent']), ('NS_GSTN_RunStartDust', ['Big', 'Small']), ('NS_GSTN_LandRing', ['Puffs'])]     if os.environ.get('PIE_PROBE') == '1' else []
TIMEOUT = 840.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'cams': [], 'rts': [], 'lights': [], 'handle': None,
      'blocks': [], 'kit': KIT}


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


def _victim():
    keys = h.dummies()
    comp = g.comps(_world()).get(keys[0]) if keys else None
    return comp.get_owner() if comp else None


def _blocks(seq):
    """Summons the sequence's boxes (one per call, like the scene captures); True when all are placed"""
    want = seq.get('blocks', [])
    if len(st['blocks']) >= len(want):
        return True
    known = st.setdefault('mesh_before', {a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.StaticMeshActor)})
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.StaticMeshActor) if a.get_name() not in known]
    if not new:
        _console('Summon StaticMeshActor')
        return False
    actor = new[0]
    known.add(actor.get_name())
    ahead, side, up, along, across, height = want[len(st['blocks'])]
    pawn = _pawn()
    p, yaw = pawn.get_actor_location(), pawn.get_actor_rotation().yaw
    fx, fy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    floor = p.z - 92.0  # capsule half height of the test character
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(unreal.load_asset(CUBE))
    comp.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector(along / 100.0, across / 100.0, height / 100.0))
    centre = unreal.Vector(p.x + fx * ahead - fy * side, p.y + fy * ahead + fx * side, floor + up + height * 0.5)
    actor.set_actor_location_and_rotation(centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False, True)
    st['blocks'].append(actor)
    _log(f'{seq["name"]}: block at ({centre.x:.0f}, {centre.y:.0f}, {centre.z:.0f}) size ({along:.0f}, {across:.0f}, {height:.0f})')
    return len(st['blocks']) >= len(want)


def _aim(seq, follow):
    pawn = _pawn()
    centre = pawn.get_actor_location()
    if follow and seq.get('follow_victim') and st.get('victim'):
        v = st['victim'].get_actor_location()
        centre = unreal.Vector(v.x, v.y, max(v.z, st['centre'].z))
    if not follow:
        st['yaw'] = pawn.get_actor_rotation().yaw
        if seq.get('dummy'):   # frame attacker and victim: look at the middle
            ahead = seq['dummy'][0] * 0.5
            centre = unreal.Vector(centre.x + ahead * math.cos(math.radians(st['yaw'])), centre.y + ahead * math.sin(math.radians(st['yaw'])), centre.z)
        st['centre'] = centre
    elif seq.get('follow_victim') and st.get('victim'):
        pass
    elif seq.get('follow'):
        centre = unreal.Vector(centre.x, centre.y, st['centre'].z)
    else:
        centre = st['centre']
    yaw = st['yaw']
    for (az, _i), light in zip(FILL_LIGHTS, st['lights']):
        light.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-25.0, yaw=yaw + az + 180.0), False)
    a, e = math.radians(yaw + seq.get('side_az', 90.0)), math.radians(ELEVATION)   # side_az: 90 = on the pawn's right
    d = seq['dist']
    pos = unreal.Vector(centre.x + d * math.cos(e) * math.cos(a), centre.y + d * math.cos(e) * math.sin(a), centre.z + d * math.sin(e))
    st['cams'][0].set_actor_location_and_rotation(pos, unreal.MathLibrary.find_look_at_rotation(pos, centre), False, False)
    return (round(centre.x), round(centre.y), round(centre.z)), round(yaw)


def _shoot(seq, frame):
    _aim(seq, True)
    manager = g._local_pawn(0)[0].get_editor_property('player_camera_manager')
    for cam, scale in ((st['cams'][1], 1.0), (st['cams'][2], CLOSE)):
        cam.set_actor_location_and_rotation(manager.get_camera_location(), manager.get_camera_rotation(), False, False)
        cam.capture_component2d.set_editor_property('fov_angle', manager.get_fov_angle() * scale)
    for name, cam, rt in zip(VIEWS, st['cams'], st['rts']):
        cam.capture_component2d.capture_scene()
        folder = os.path.join(OUT, seq['name'], name)
        os.makedirs(folder, exist_ok=True)
        unreal.RenderingLibrary.export_render_target(_world(), rt, folder, f'f{frame:03d}.png')


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


def _stick_at(seq, k):
    for a, b, direction in seq.get('stick', []):
        if a <= k < b:
            return direction
    return None


def _tick(_dt):
    try:
        now, phase = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase != 'ending' and now - st['start'] > TIMEOUT:
            return _finish(f'timeout in phase {phase}')
        if phase == 'boot' and now - st['t'] > 8:
            _log(f'setup_editor: {g.setup_editor(True, "listen", 2)}')
            # ProxyCreationWhenPSOReady 0: in the editor an effect's first use otherwise draws the default material (a dark quad) while its
            # pipeline compiles; a packaged game ships its PSO cache. Capture only
            for cmd in (f'GSTN.Presentation.Effects {VFX}', 'GSTN.Presentation.Log 1', 'r.PSOPrecache.ProxyCreationWhenPSOReady 0'):
                unreal.SystemLibrary.execute_console_command(None, cmd)
            les.editor_request_begin_play()
            st.update(phase='wait_ready', t=now)
        elif phase == 'wait_ready':
            if les.is_in_play_in_editor() and _ready():
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready after 180 s')
        elif phase == 'settle' and now - st['t'] > 12:
            _log(f'kit {KIT}: {h.kit(0, KIT)}')
            st.update(phase='kit_wait', t=now)
        elif phase == 'kit_wait' and now - st['t'] > 3:
            parked = [h.park(i, 'Z1') for i in h._players() if i != 0]
            _console('gstn.Combat.KitPlaceholderLook 0')
            _log(f'parked {parked}; effects cvar {VFX}')
            st.update(phase='cameras', t=now)
        elif phase == 'cameras':
            if _make_cameras() and _make_lights():
                # Warm-up, before any captured frame: every system once in view, so its shaders are compiled (in the editor a first use draws
                # the default material for a few frames: a grey square that is not the effect)
                p = _pawn().get_actor_location()
                st['warm'] = []
                for name in WARMUP:
                    system = unreal.load_asset(f'/GSTNPresentation/VFX/{name}')
                    st['warm'].append(unreal.NiagaraFunctionLibrary.spawn_system_at_location(_world(), system, unreal.Vector(p.x + 250, p.y, p.z), unreal.Rotator(),
                                                                          unreal.Vector(1, 1, 1), True, True, unreal.NCPoolMethod.NONE, False))
                _log(f'warm-up: {len(WARMUP)} systems')
                st.update(phase='warmup', t=now)
            elif now - st['t'] > 20:
                _finish('could not summon the scene captures')
        elif phase == 'warmup':
            if now - st['t'] > 2 and st.get('warm'):
                for comp in st['warm']:   # the looping ones (skid, sprint lines, tumble) would run for the whole capture
                    if comp:
                        comp.deactivate()
                st['warm'] = []
            if now - st['t'] > 8:
                st.update(phase='seq_begin', t=now)
        elif phase == 'seq_begin' and now - st['t'] > 2.5:
            if st['seq'] >= len(SEQUENCES):
                return _finish('ok')
            seq = SEQUENCES[st['seq']]
            if seq.get('kit', KIT) != st['kit']:
                st['kit'] = seq.get('kit', KIT)
                _log(f'{seq["name"]}: kit {st["kit"]}: {h.kit(0, st["kit"])}')
                st.update(t=now + 2.0)
                return None
            if seq.get('dummy') and not st.get('dummy_placed'):
                keys = h.dummies()
                ahead, percent = seq['dummy']
                placed = h._place_ahead(0, keys[0], ahead) if keys else 'NO DUMMY'
                if keys:
                    g.comps(g.worlds()[0])[keys[0]].server_set_damage_percent(percent)
                _log(f'{seq["name"]}: dummy {keys[:1]} at {placed}, percent {percent}')
                st.update(dummy_placed=True, t=now, victim=_victim())
                return None
            if not _blocks(seq):
                return None
            _log(f'sequence {seq["name"]}: {_aim(seq, False)}')
            if SWATCH:
                c, cam = st['centre'], st['cams'][0].get_actor_location()
                mid = unreal.Vector(c.x + (cam.x - c.x) * 0.45, c.y + (cam.y - c.y) * 0.45, c.z + (cam.z - c.z) * 0.45)
                rot = unreal.MathLibrary.find_look_at_rotation(mid, cam)
                system = unreal.load_asset('/GSTNPresentation/VFX/NS_GSTN_Swatch')
                unreal.NiagaraFunctionLibrary.spawn_system_at_location(_world(), system, mid, rot, unreal.Vector(1, 1, 1), True, True,
                                                                      unreal.NCPoolMethod.NONE, False)
                _log(f'swatch at {mid}')
            _console('t.OverrideFPS 60')
            st.update(phase='seq_run', frame=0, mask=g.button_mask('Attack'), strong_mask=g.button_mask('Strong'), stick=None)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            if PROBE and frame == 0:
                p = _pawn().get_actor_location()
                st['probe'] = []
                for name, emitters in PROBE:
                    comp = unreal.NiagaraFunctionLibrary.spawn_system_at_location(
                        _world(), unreal.load_asset(f'/GSTNPresentation/VFX/{name}'), unreal.Vector(p.x, p.y, p.z + 600), unreal.Rotator(),
                        unreal.Vector(1, 1, 1), False, True, unreal.NCPoolMethod.NONE, False)
                    comp.set_variable_float('User.Intensity', 0.56)
                    comp.set_variable_float('User.Strong', 0.0)
                    comp.set_variable_int('User.Count', 4)
                    comp.set_variable_int('User.StrongCount', 0)
                    st['probe'].append((name, emitters, comp))
            if PROBE and 0 < frame <= 12:
                for name, emitters, comp in st['probe']:
                    for em in emitters:
                        _log(f'probe f{frame} {name}.{em}: {unreal.GSTN_VfxAuthoringLibrary.describe_particles(comp, em, 6)}')
            _shoot(seq, frame)
            k = frame - PRE
            want = _stick_at(seq, k) if k >= 0 else None
            if want != st['stick']:
                if want is None:
                    g.release(0, 'move')
                else:
                    g.hold(0, 'move', want)
                _log(f'{seq["name"]}: stick {want} from v{frame}')
                st['stick'] = want
            if k in seq.get('jump', []):
                g.press(0, 'jump')
                _log(f'{seq["name"]}: jump on v{frame}')
            if k in seq.get('tap', []):
                g.combat_tap(0, st['mask'])
                _log(f'{seq["name"]}: attack on v{frame}')
            if k in seq.get('strong', []):
                g.combat_tap(0, st['strong_mask'])
                _log(f'{seq["name"]}: strong on v{frame}')
            st['frame'] = frame + 1
            if st['frame'] >= PRE + seq['frames']:
                g.release(0, 'move')
                g.combat_clear(0)
                _console('t.OverrideFPS 0')
                p = _pawn().get_actor_location()
                _log(f'sequence {seq["name"]} done: {st["frame"]} frames, pawn at ({p.x:.0f}, {p.y:.0f}, {p.z:.0f})')
                for block in st['blocks']:
                    block.destroy_actor()
                st.update(phase='seq_begin', seq=st['seq'] + 1, t=now, dummy_placed=False, blocks=[], victim=None)
        elif phase == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:  # noqa: BLE001
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


_log(f'driver loaded: vfx {VFX}, kit {KIT}, out {OUT}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
