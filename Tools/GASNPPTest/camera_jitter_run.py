# camera-jitter-2 (2026-11-20): one Play session that reproduces the owner's two-height camera bounce and records it.
# Starts PIE in L_Test_Hub (CAMTRACE_CLIENTS=0 -> 1 player, 2 -> listen host + 2 clients), waits for the hub setup, sets gstn.Loco.Base 1 and
# CAMTRACE_CVARS (';' separated), then with gstn.Camera.Trace 1: stand 5 s, walk in place 4 s (stick flips forward/back every 0.4 s), stand 1.5 s,
# full jump, 2.5 s rest; Trace 0. No t.OverrideFPS: the jitter may depend on the real, uneven frame time.
# Besides the C++ trace (Saved/Logs/gstn_camera_trace*.csv, copied to CAMTRACE_OUT) a Python sampler writes py_rows.csv from a slate post-tick
# (after the world tick, so the camera cache is the view this frame draws): dt, host camera POV, actor, mesh, GASP camera component.
# Ends PIE; CAMTRACE_QUIT=1 also quits the editor (Run-Timed runs). Output: CAMTRACE_OUT/{run.log,done.txt,py_rows.csv,trace_*.csv}
import os, sys, time, json, shutil, traceback
import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g   # noqa: E402

CLIENTS = int(os.environ.get('CAMTRACE_CLIENTS', '0'))
OUT = os.environ.get('CAMTRACE_OUT') or os.path.join(_PROJ, 'Saved', 'CamJitter', time.strftime('run_%H%M%S'))
CVARS = [c.strip() for c in os.environ.get('CAMTRACE_CVARS', '').split(';') if c.strip()]
LOGS = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())
os.makedirs(OUT, exist_ok=True)
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'rows': None, 'h': None, 'flip': 0}
# (phase, seconds): what happens while the trace is on
# camera-jitter-3 (owner 22:14: the bounce was crouch spam; also FAir and landings): crouch toggled every 0.15 s for 3 s, then a jump with a
# forward air attack (FAir: aerial capsule) and its landing. The checker's capsule line reads these
PLAN = [('stand', 5.0), ('walk', 4.0), ('stand2', 1.5), ('jump', 2.5), ('crouchspam', 3.0), ('fair', 3.0)]


def _log(m):
    with open(os.path.join(OUT, 'run.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {m}\n')


def _console(cmd):
    for w in g.worlds():
        unreal.SystemLibrary.execute_console_command(w, cmd)


def _finish(msg):
    try:
        _console('gstn.Camera.Trace 0')
    except Exception:
        pass
    st['phase'] = 'ending'
    st['end_msg'] = msg
    st['t'] = time.time()


def _sample(dt):
    pc, pawn = g._local_pawn(0)
    if not pawn or not pc:
        return
    cam = pc.player_camera_manager
    pov = cam.get_camera_location()
    rot = cam.get_camera_rotation()
    mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    gcc = None
    for c in pawn.get_components_by_class(unreal.SceneComponent):
        if 'GameplayCamera' in c.get_class().get_name():
            gcc = c
            break
    a = pawn.get_actor_location()
    m = mesh.get_world_location() if mesh else a
    k = gcc.get_world_location() if gcc else a
    st['rows'].write(f'{unreal.SystemLibrary.get_frame_count()},{dt:.4f},{pov.x:.2f},{pov.y:.2f},{pov.z:.2f},{rot.pitch:.3f},'
                     f'{a.x:.2f},{a.y:.2f},{a.z:.2f},{m.z:.2f},{k.z:.2f},{st["phase"]}\n')


def _tick(dt):
    try:
        now, ph = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ph != 'ending' and now - st['start'] > 600:
            return _finish('timeout')
        if ph == 'boot' and now - st['t'] > 5:
            _log(f'setup_editor: {g.setup_editor(True, "listen", CLIENTS)}')
            les.editor_request_begin_play()
            st.update(phase='wait', t=now)
        elif ph == 'wait':
            if les.is_in_play_in_editor() and len(g.worlds()) >= CLIENTS + 1 and g._local_pawn(0)[1] is not None:
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready')
        elif ph == 'settle' and now - st['t'] > 14:
            for f in os.listdir(LOGS):
                if f.startswith('gstn_camera_trace'):
                    os.remove(os.path.join(LOGS, f))
            for c in ['gstn.Loco.Base 1'] + CVARS:
                _console(c)
                _log(f'cvar {c}')
            st['rows'] = open(os.path.join(OUT, 'py_rows.csv'), 'w')
            st['rows'].write('frame,dt,pov_x,pov_y,pov_z,pov_pitch,root_x,root_y,root_z,mesh_z,gcc_z,phase\n')
            _console('gstn.Camera.Trace 1')
            st.update(phase='stand', t=now, i=0)
            _log('trace on, stand')
        elif ph in [p for p, _ in PLAN]:
            _sample(dt)
            i = st['i']
            if ph == 'walk' and now - st.get('ft', 0) > 0.4:
                st['flip'] ^= 1
                g.hold(0, 'move', (0, 1 if st['flip'] else -1, 0))
                st['ft'] = now
            if now - st['t'] > PLAN[i][1]:
                if ph == 'walk':
                    g.release(0, 'move')
                if i + 1 >= len(PLAN):
                    _console('gstn.Camera.Trace 0')
                    st.update(phase='flush', t=now)
                    return None
                nxt = PLAN[i + 1][0]
                if nxt == 'jump':
                    m = g.button_mask('Jump')
                    g.combat_tap(0, m)
                    g.combat_hold(0, m)
                    st['jt'] = now
                if ph == 'crouchspam':
                    g.combat_clear(0)
                if nxt == 'fair':
                    st['fs'] = 0
                st.update(phase=nxt, t=now, i=i + 1)
                _log(nxt)
            if ph == 'jump' and st.get('jt') and now - st['jt'] > 0.4:
                g.combat_clear(0)
                st['jt'] = None
            if ph == 'crouchspam' and now - st.get('ct', 0) > 0.15:
                st['crouch'] = not st.get('crouch', False)
                if st['crouch']:
                    g.combat_hold(0, g.button_mask('Crouch'))
                else:
                    g.combat_clear(0)
                st['ct'] = now
            if ph == 'fair':
                age = now - st['t']
                if st.get('fs') == 0:                       # take off
                    g.combat_clear(0)
                    m = g.button_mask('Jump')
                    g.combat_tap(0, m)
                    g.combat_hold(0, m)
                    st['fs'] = 1
                elif st.get('fs') == 1 and age > 0.3:       # near the apex: stick forward + Attack = FAir
                    g.combat_clear(0)
                    g.hold(0, 'move', (0, 1, 0))
                    g.combat_tap(0, g.button_mask('Attack'))
                    st['fs'] = 2
                elif st.get('fs') == 2 and age > 0.6:
                    g.release(0, 'move')
                    st['fs'] = 3
        elif ph == 'flush':
            _sample(dt)                                    # one more frame: the C++ side flushes its tail when Trace is 0
            if now - st['t'] > 1.0:
                return _finish('ok')
        elif ph == 'ending':
            if st['rows']:
                st['rows'].close()
                st['rows'] = None
                n = 0
                for f in os.listdir(LOGS):
                    if f.startswith('gstn_camera_trace'):
                        shutil.copy(os.path.join(LOGS, f), os.path.join(OUT, 'trace' + f[len('gstn_camera_trace'):]))
                        n += 1
                _log(f'copied {n} trace files')
            if les.is_in_play_in_editor():
                les.editor_request_end_play()
                return None
            if now - st['t'] > 3:
                with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                    f.write(st['end_msg'] + '\n')
                _log('done: ' + st['end_msg'])
                unreal.unregister_slate_post_tick_callback(st['h'])
                if os.environ.get('CAMTRACE_QUIT') == '1':
                    unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:
        _log(traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error')
        else:
            unreal.unregister_slate_post_tick_callback(st['h'])


st['h'] = unreal.register_slate_post_tick_callback(_tick)
_log(f'start clients={CLIENTS} cvars={CVARS} out={OUT}')
