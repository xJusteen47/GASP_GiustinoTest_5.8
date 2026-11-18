"""Playtest bugs 2026-11-18 diagnostics (rows per frame, no video). Launched by Run-Timed.ps1 -Preset Capture; quits the editor at the end.
Seq 'hold' (KIT_Test_Playtest): crouch held on the ground -> jump with it held -> land with it held; then crouch pressed in the air and held
through the landing; then dodge held 3 s. Seq 'stomp' (KIT_Test_Overlord): jump, crouch+attack (dair), land. Rows -> Saved/Diag1118/*.jsonl"""
import json, os, sys, time, traceback
import unreal
_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import importlib
import gasnpp_test as g
import test_hub as h
importlib.reload(g); importlib.reload(h)
OUT = os.path.join(_PROJ, 'Saved', 'Diag1118')
os.makedirs(OUT, exist_ok=True)
try:
    import runpy
    runpy.run_path(os.path.join(_PROJ, 'Tools', 'GASNPPTest', 'vd2_mover.py'), run_name='vd2_mover')
except Exception as _e:
    print('vd2_mover not run', _e)
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'frame': 0}

HOLD_SEQ = [(20, 'crouch'), (90, 'jump'), (200, 'clear'), (240, 'jump'), (262, 'crouch'), (380, 'clear'), (420, 'dodge'), (610, 'clear'), (640, 'stick'), (650, 'dodge'), (840, 'clear'), (841, 'unstick'), (900, 'end')]
STOMP_SEQ = [(20, 'jumphold'), (45, 'clear'), (60, 'crouch'), (62, 'tap'), (80, 'clear'), (260, 'end')]
DROPKICK_SEQ = [(20, 'jumphold'), (45, 'clear'), (50, 'stick'), (52, 'tap'), (80, 'unstick'), (260, 'end')]
SEQS = [('dropkick', 'KIT_Test_Playtest', DROPKICK_SEQ), ('stomp', 'KIT_Test_Overlord', STOMP_SEQ)]


def _log(m):
    with open(os.path.join(OUT, 'diag.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {m}\n')


def _pawn():
    return g._local_pawn(0)[1]


def _console(c):
    unreal.SystemLibrary.execute_console_command(g.worlds()[0], c, g._local_pawn(0)[0])


def _montage(pawn):
    try:
        for comp in pawn.get_components_by_class(unreal.SkeletalMeshComponent):
            ai = comp.get_anim_instance()
            m = ai.get_current_active_montage() if ai else None
            if m:
                return [m.get_name(), round(ai.montage_get_position(m) * 60.0, 1), round(ai.montage_get_blend_time(m), 3) if hasattr(ai, 'montage_get_blend_time') else None]
    except Exception as e:
        return ['err', repr(e)[:60]]
    return None


def _row(f, ev):
    p = _pawn()
    v = g._view(0, h._key(0)) or {}
    pc = g._local_pawn(0)[0]
    cm = pc.get_editor_property('player_camera_manager')
    cap = p.get_component_by_class(unreal.CapsuleComponent)
    mesh = p.get_component_by_class(unreal.SkeletalMeshComponent)
    c = (v.get('combat') or {})
    r = {'f': f, 'ev': ev, 'mode': v.get('mode'), 'intl': c.get('inTimeline'), 'req': c.get('request'),
         'tags': [t for t in (v.get('tags') or []) if any(k in t for k in ('Crouch', 'Dodge', 'Dash', 'Land', 'Attack', 'Timeline', 'Ground', 'Air'))],
         'loc': [round(x, 1) for x in (v.get('location') or [0, 0, 0])], 'vel': [round(x) for x in (v.get('velocity') or [0, 0, 0])],
         'hh': round(cap.get_scaled_capsule_half_height(), 1) if cap else None,
         'meshz': round(mesh.get_world_location().z, 1) if mesh else None,
         'cam': [round(x, 1) for x in (lambda q: (q.x, q.y, q.z))(cm.get_camera_location())] if cm else None,
         'm': _montage(p), 'gt': [t.split('.')[-1] for t in ('Combat.State.Crouch', 'Combat.State.AirCrouch', 'Combat.State.Dodge.EndLag') if g._has_tag(p, t)]}
    st['rows'].write(json.dumps(r) + '\n')


def _components():
    try:
        return [f'{c.get_name()}:{c.get_class().get_name()}' for c in _pawn().get_components_by_class(unreal.ActorComponent)
                if any(k in c.get_class().get_name() for k in ('Anchor', 'Camera', 'SpringArm'))]
    except Exception as e:
        return repr(e)[:100]


def _start_seq(now):
    name, kit, seq = SEQS[st['si']]
    _log(f'kit {kit}: {h.kit(0, kit)}')
    st.update(phase='kitwait', t=now, name=name, seq=seq)


def _tick(_dt):
    try:
        now, ph = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ph != 'ending' and now - st['start'] > 600:
            return _finish('timeout ' + ph)
        if ph == 'boot' and now - st['t'] > 3:
            da = unreal.load_asset('/Game/Blueprints/Data/DA_CameraAnchor_Default')
            s = da.get_editor_property('settings')
            s.set_editor_property('mode', getattr(unreal.GSTN_CameraAnchorMode, os.environ.get('DIAG_ANCHOR_MODE', 'SHIFT_VIEW')))
            da.set_editor_property('settings', s)
            unreal.EditorAssetLibrary.save_loaded_asset(da, False)
            _log(f'anchor settings {da.get_editor_property("settings")}')
            _log(f'setup {g.setup_editor(True, "listen", 2)}')
            les.editor_request_begin_play()
            st.update(phase='wait', t=now)
        elif ph == 'wait':
            ws = g.worlds()
            if les.is_in_play_in_editor() and len(ws) >= 3 and _pawn() is not None:
                c = g.local_comp(ws[0])
                if c and json.loads(c.get_state_snapshot()).get('inputReady'):
                    st.update(phase='settle', t=now)
            elif now - st['t'] > 240:
                _finish('not ready')
        elif ph == 'settle' and now - st['t'] > 12:
            _log('parked ' + str([h.park(i, 'Z1') for i in h._players() if i != 0]))
            _log('camera comps ' + str(_components()))
            st['si'] = 0
            _start_seq(now)
        elif ph == 'kitwait' and now - st['t'] > 4:
            _log('camera comps after kit ' + str(_components()))
            _console('t.OverrideFPS 60')
            st.update(phase='run', frame=0, rows=open(os.path.join(OUT, st['name'] + '.jsonl'), 'w'),
                      atk=g.button_mask('Attack'), cr=g.button_mask('Crouch'), dg=g.button_mask('Dodge'), jp=g.button_mask('Jump'))
        elif ph == 'run':
            f = st['frame']
            ev = [a for (fr, a) in st['seq'] if fr == f]
            _row(f, ev)
            for a in ev:
                if a == 'tap':
                    g.combat_tap(0, st['atk'])
                elif a == 'jump':
                    g.combat_tap(0, st['jp'])
                elif a == 'jumphold':
                    g.combat_tap(0, st['jp'])
                    g.combat_hold(0, st['jp'])
                elif a == 'crouch':
                    g.combat_hold(0, st['cr'])
                elif a == 'dodge':
                    g.combat_hold(0, st['dg'])
                elif a == 'clear':
                    g.combat_clear(0)
                elif a == 'stick':
                    g.hold(0, 'move', (0, 1, 0))
                elif a == 'unstick':
                    g.release(0, 'move')
            st['frame'] = f + 1
            if 'end' in ev:
                st['rows'].close()
                g.combat_clear(0)
                st['si'] += 1
                if st['si'] < len(SEQS):
                    _console('t.OverrideFPS 0')
                    _start_seq(now)
                else:
                    _finish('ok')
        elif ph == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:
                _log(f'restore failed {e!r}')
            open(os.path.join(OUT, 'done.txt'), 'w').write(st.get('status', '?') + '\n')
            unreal.SystemLibrary.quit_editor()
    except Exception:
        _log('ERROR ' + traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error')
    return None


def _finish(s):
    _log('finish ' + s)
    st.update(phase='ending', status=s)
    try:
        _console('t.OverrideFPS 0')
    except Exception:
        pass
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()


for fn in ('diag.log', 'done.txt'):
    p = os.path.join(OUT, fn)
    if os.path.exists(p):
        os.remove(p)
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
print('started')
