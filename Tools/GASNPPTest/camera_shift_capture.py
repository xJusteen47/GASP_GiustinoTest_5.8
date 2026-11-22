# camera-shift-1 (2026-11-21): one Play run that films the REAL game viewport (HighResShot), so the lens shift (off-axis projection, not seen
# by scene captures) shows. 1 player in L_Test_Hub, t.OverrideFPS 60 while filming, inputs by frame number: the A/B runs (SHIFTCAP_CVARS
# "gstn.Camera.Shift 0" vs "1") get the same frames. Plan: stand, run forward, stand, jump in place, one short step.
# Output SHIFTCAP_OUT/{run.log, done.txt, rows.csv, shots/f###.png}. SHIFTCAP_QUIT=1 quits the editor at the end (Run-Timed runs).
import os, sys, time, shutil, traceback
import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g   # noqa: E402

OUT = os.environ.get('SHIFTCAP_OUT') or os.path.join(_PROJ, 'Saved', 'CamShift', time.strftime('run_%H%M%S'))
CVARS = [c.strip() for c in os.environ.get('SHIFTCAP_CVARS', '').split(';') if c.strip()]
SHOTS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Screenshots', 'WindowsEditor')
TMPSHOTS = (os.environ.get('TEMP', 'C:/Temp') + '/gstn_camshift').replace('\\', '/')
shutil.rmtree(TMPSHOTS, ignore_errors=True)
os.makedirs(TMPSHOTS, exist_ok=True)
EVERY = 2          # one shot every 2 engine frames = 30 fps video
END = 400
RUN = (30, 150)    # stick forward
JUMP = (210, 234)  # jump pressed
STEP = (330, 345)  # short step
SHOOT = os.environ.get('SHIFTCAP_SHOOT', '1') == '1'
PLAN = os.environ.get('SHIFTCAP_PLAN', 'move')   # 'dropkick' = jump, forward air (dropkick) on the rise, landing: camera bob check
if PLAN == 'dropkick':
    END, RUN, JUMP, STEP = 300, (-1, -1), (30, 54), (-1, -1)
    DK_STICK, DK_TAP, DK_FREE = 52, 53, 70
LOGS = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_log_dir())
os.makedirs(OUT, exist_ok=True)
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'frame': 0, 'h': None, 'rows': None}


def _log(m):
    with open(os.path.join(OUT, 'run.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {m}\n')


def _console(cmd):
    for w in g.worlds():
        unreal.SystemLibrary.execute_console_command(w, cmd)


def _finish(msg):
    try:
        _console('t.OverrideFPS 0')
        g.release(0, 'move')
        g.combat_clear(0)
    except Exception:
        pass
    st.update(phase='ending', end_msg=msg, t=time.time())


def _mesh_z(pawn):
    m = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    return m.get_world_location().z if m else 0.0


def _cap(pawn):
    c = pawn.get_component_by_class(unreal.CapsuleComponent)
    return c.get_scaled_capsule_half_height() if c else 0.0


def _phase(f):
    if RUN[0] <= f < RUN[1]:
        return 'run'
    if JUMP[0] <= f < STEP[0]:
        return 'jump'
    if STEP[0] <= f < STEP[1] + 30:
        return 'step'
    return 'stand'


def _tick(dt):
    try:
        now, ph = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ph != 'ending' and now - st['start'] > 600:
            return _finish('timeout')
        if ph == 'boot' and now - st['t'] > 5:
            _log(f'setup_editor: {g.setup_editor(True, "listen", 0)}')
            les.editor_request_begin_play()
            st.update(phase='wait', t=now)
        elif ph == 'wait':
            if les.is_in_play_in_editor() and g._local_pawn(0)[1] is not None:
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('pawn not ready')
        elif ph == 'settle' and now - st['t'] > 14:
            for c in ['gstn.Loco.Base 1'] + CVARS:
                _console(c)
                _log(f'cvar {c}')
            if os.path.isdir(SHOTS):
                for f in os.listdir(SHOTS):
                    if f.lower().startswith('highresscreenshot'):
                        os.remove(os.path.join(SHOTS, f))
            st['rows'] = open(os.path.join(OUT, 'rows.csv'), 'w')
            st['rows'].write('frame,phase,pov_x,pov_y,pov_z,pov_pitch,pov_yaw,fov,root_x,root_y,root_z,mesh_z,cap_half\n')
            for lf in os.listdir(LOGS):
                if lf.startswith('gstn_camera_trace'):
                    os.remove(os.path.join(LOGS, lf))
            _console('gstn.Camera.Trace 1')
            _console('t.OverrideFPS 60')
            st.update(phase='film', t=now, frame=0)
            _log('filming')
        elif ph == 'film':
            # Game frames, not editor ticks: in the background (virtual desktop 2) the PIE world ticks only every few editor ticks
            pc, pawn = g._local_pawn(0)
            gt = unreal.GameplayStatics.get_time_seconds(pawn.get_world())
            if st.get('t0') is None:
                st['t0'] = gt
            f = int(round((gt - st['t0']) * 60.0))
            if f == st.get('last_f'):
                return None
            st['last_f'] = f
            cam = pc.player_camera_manager
            pov, rot, a = cam.get_camera_location(), cam.get_camera_rotation(), pawn.get_actor_location()
            st['rows'].write(f'{f},{_phase(f)},{pov.x:.1f},{pov.y:.1f},{pov.z:.1f},{rot.pitch:.2f},{rot.yaw:.2f},{cam.get_fov_angle():.1f},'
                             f'{a.x:.1f},{a.y:.1f},{a.z:.1f},{_mesh_z(pawn):.1f},{_cap(pawn):.1f}\n')
            done = st.setdefault('done', set())

            def due(key, at):
                if at >= 0 and f >= at and key not in done:
                    done.add(key)
                    return True
                return False
            if due('run', RUN[0]) or due('step', STEP[0]):
                g.hold(0, 'move', (0, 1, 0))
            if due('run_end', RUN[1]) or due('step_end', STEP[1]):
                g.release(0, 'move')
            if due('jump', JUMP[0]):
                m = g.button_mask('Jump')
                g.combat_tap(0, m)
                g.combat_hold(0, m)
            if due('jump_end', JUMP[1]):
                g.combat_clear(0)
            if PLAN == 'dropkick':
                if due('dk_stick', DK_STICK):
                    g.hold(0, 'move', (0, 1, 0))
                if due('dk_tap', DK_TAP):
                    g.combat_tap(0, g.button_mask('Attack'))
                if due('dk_free', DK_FREE):
                    g.release(0, 'move')
            if SHOOT and f % EVERY == 0:
                _console(f'HighResShot 1 filename={TMPSHOTS}/f{f:03d}')   # no spaces allowed in the console argument
            if f >= END:
                _console('t.OverrideFPS 0')
                _console('gstn.Camera.Trace 0')
                st.update(phase='collect', t=now)
        elif ph == 'collect' and now - st['t'] > 4:
            dst = os.path.join(OUT, 'shots')
            os.makedirs(dst, exist_ok=True)
            for f in os.listdir(TMPSHOTS):
                shutil.move(os.path.join(TMPSHOTS, f), os.path.join(dst, f))
            shots = [f for f in os.listdir(dst) if f.endswith('.png')]
            for lf in os.listdir(LOGS):
                if lf.startswith('gstn_camera_trace'):
                    shutil.copy(os.path.join(LOGS, lf), os.path.join(OUT, 'trace' + lf[len('gstn_camera_trace'):]))
            _log(f'shots {len(shots)} (expected {END // EVERY})')
            _finish('ok' if (not SHOOT or len(shots) >= END // EVERY) else f'shots {len(shots)}/{END // EVERY}')
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
                if os.environ.get('SHIFTCAP_QUIT') == '1':
                    unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:
        _log(traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error')
        else:
            unreal.unregister_slate_post_tick_callback(st['h'])


st['h'] = unreal.register_slate_post_tick_callback(_tick)
_log(f'start cvars={CVARS} out={OUT}')
