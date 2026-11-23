# crouch-mesh-pop-1 (2026-11-21, owner: "the mesh flashes / jitters when I crouch / stand with ZL"). One Play session, 1 player in L_Test_Hub,
# rendering capped at 144 fps (the owner's rate; no t.OverrideFPS, the real uneven frame time matters) with CPU throttling off.
# Plan: stand 2 s, crouch spam with gstn.Mesh.CapsuleShiftBeforeAnim 1 (fix) for 6 s, stand 1 s, the same with 0 (first version) for 6 s.
# Crouch held 0.25 s / released 0.25 s (real crouches: 12 each half). A slate post-tick sampler (after the world tick: the pose and the mesh
# this frame draws) writes rows.csv: frame, dt, actor z, capsule half height, mesh component z, root / pelvis / head bone world z, phase.
# Output: CROUCHPOP_OUT/{run.log,done.txt,rows.csv}; crouch_pop_check.py reads it. CROUCHPOP_QUIT=1 quits the editor (Run-Timed runs).
import os, sys, time, traceback
import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g   # noqa: E402

OUT = os.environ.get('CROUCHPOP_OUT') or os.path.join(_PROJ, 'Saved', 'CrouchPop', time.strftime('run_%H%M%S'))
FPS = os.environ.get('CROUCHPOP_FPS', '144')
os.makedirs(OUT, exist_ok=True)
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'rows': None, 'h': None}
PLAN = [('stand', 2.0, None), ('spam_fix', 6.0, 'gstn.Mesh.CapsuleShiftBeforeAnim 1'), ('rest', 1.0, None),
        ('spam_old', 6.0, 'gstn.Mesh.CapsuleShiftBeforeAnim 0'), ('rest2', 1.0, None)]
BONES = ['root', 'pelvis', 'head']


def _log(m):
    with open(os.path.join(OUT, 'run.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {m}\n')


def _console(cmd):
    for w in g.worlds():
        unreal.SystemLibrary.execute_console_command(w, cmd)


def _finish(msg):
    try:
        g.combat_clear(0)
        _console('gstn.Mesh.CapsuleShiftBeforeAnim 1')
    except Exception:
        pass
    st.update(phase='ending', end_msg=msg, t=time.time())


def _sample(dt):
    pc, pawn = g._local_pawn(0)
    if not pawn:
        return
    mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    cap = pawn.get_component_by_class(unreal.CapsuleComponent)
    a = pawn.get_actor_location()
    hh = cap.get_scaled_capsule_half_height() if cap else 0.0
    mz = mesh.get_world_location().z if mesh else a.z
    bz = [(mesh.get_socket_location(b).z if mesh else 0.0) for b in BONES]
    st['rows'].write(f'{unreal.SystemLibrary.get_frame_count()},{dt:.5f},{a.z:.3f},{hh:.3f},{mz:.3f},'
                     + ','.join(f'{z:.3f}' for z in bz) + f',{st["phase"]}\n')


def _tick(dt):
    try:
        now, ph = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ph != 'ending' and now - st['start'] > 600:
            return _finish('timeout')
        if ph == 'boot' and now - st['t'] > 5:
            try:
                perf = unreal.get_default_object(unreal.EditorPerformanceSettings)
                perf.set_editor_property('throttle_cpu_when_not_foreground', False)
            except Exception as e:
                _log(f'throttle setting: {e}')
            _log(f'setup_editor: {g.setup_editor(True, "listen", 0)}')
            les.editor_request_begin_play()
            st.update(phase='wait', t=now)
        elif ph == 'wait':
            if les.is_in_play_in_editor() and g._local_pawn(0)[1] is not None:
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawns not ready')
        elif ph == 'settle' and now - st['t'] > 12:
            for c in ['gstn.Loco.Base 1', f't.MaxFPS {FPS}', 'gstn.Mesh.CapsuleShiftBeforeAnim 1']:
                _console(c)
                _log(f'cvar {c}')
            st['rows'] = open(os.path.join(OUT, 'rows.csv'), 'w')
            st['rows'].write('frame,dt,actor_z,half_height,mesh_z,' + ','.join(f'{b}_z' for b in BONES) + ',phase\n')
            st.update(phase=PLAN[0][0], t=now, i=0)
            _log(PLAN[0][0])
        elif ph in [p for p, _, _ in PLAN]:
            _sample(dt)
            i = st['i']
            if ph.startswith('spam') and now - st.get('ct', 0) > 0.25:
                st['crouch'] = not st.get('crouch', False)
                if st['crouch']:
                    g.combat_hold(0, g.button_mask('Crouch'))
                else:
                    g.combat_clear(0)
                st['ct'] = now
            if now - st['t'] > PLAN[i][1]:
                if ph.startswith('spam'):
                    g.combat_clear(0)
                    st['crouch'] = False
                if i + 1 >= len(PLAN):
                    return _finish('ok')
                nxt, _, cvar = PLAN[i + 1]
                if cvar:
                    _console(cvar)
                    _log(f'cvar {cvar}')
                st.update(phase=nxt, t=now, i=i + 1)
                _log(nxt)
        elif ph == 'ending':
            if st['rows']:
                st['rows'].close()
                st['rows'] = None
            if les.is_in_play_in_editor():
                les.editor_request_end_play()
                return None
            if now - st['t'] > 3:
                with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                    f.write(st['end_msg'] + '\n')
                _log('done: ' + st['end_msg'])
                unreal.unregister_slate_post_tick_callback(st['h'])
                if os.environ.get('CROUCHPOP_QUIT') == '1':
                    unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:
        _log(traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error')
        else:
            unreal.unregister_slate_post_tick_callback(st['h'])


st['h'] = unreal.register_slate_post_tick_callback(_tick)
_log(f'start out={OUT} fps={FPS}')
