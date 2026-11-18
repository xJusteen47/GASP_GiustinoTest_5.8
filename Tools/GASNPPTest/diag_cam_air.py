"""CamAir: camera vs body per frame on jump/dair/fair. Was: PF1117b diagnostics in the OPEN editor (never quits it): jab, crouch->stand->jab, strong on a dummy. Rows -> Saved/PF1117b/diag/*.jsonl"""
import json, os, sys, time, traceback
import unreal
_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import importlib
import gasnpp_test as g
import test_hub as h
importlib.reload(g); importlib.reload(h)
OUT = os.path.join(_PROJ, 'Saved', 'CamAir', os.environ.get('DIAG_OUT', 'diag'))
os.makedirs(OUT, exist_ok=True)
KIT = os.environ.get('DIAG_KIT', 'KIT_Test_OverlordDAirV9')
DUMMY_PCT = float(os.environ.get('DIAG_PCT', '40'))
STRONG = os.environ.get('DIAG_STRONG', 'Strong')
PCTS = []
ST_MODE = os.environ.get('DIAG_MODE', 'strong')
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'frame': 0}


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
                nm = m.get_name()
                try:
                    tr = m.get_editor_property('slot_anim_tracks')[0]
                    seg = tr.get_editor_property('anim_track').get_editor_property('anim_segments')[0]
                    nm = seg.get_editor_property('anim_reference').get_name()
                except Exception as e:
                    nm += '?' + repr(e)[:40]
                return [nm, round(ai.montage_get_position(m) * 60.0, 1)]
    except Exception as e:
        return ['err', repr(e)[:60]]
    return None


def _heads(p):
    try:
        m = p.get_component_by_class(unreal.SkeletalMeshComponent)
        z0 = p.get_actor_location().z
        return {b: round(m.get_socket_location(b).z - z0, 1) for b in ('root', 'pelvis', 'head')} | {'az': round(z0, 1), 'vz': round(p.get_velocity().z)}
    except Exception as e:
        return repr(e)[:60]


def _row(f, ev):
    v = g._view(0, h._key(0)) or {}
    pc = g._local_pawn(0)[0]
    cm = pc.get_editor_property('player_camera_manager')
    cam = round(cm.get_camera_location().distance(_pawn().get_actor_location())) if cm else None
    r = {'f': f, 'ev': ev, 'yaw': round(_pawn().get_actor_rotation().yaw), 'cam': cam, 'combat': v.get('combat'), 'tags': v.get('tags'), 'loc': v.get('location'), 'm': _montage(_pawn()), 'hz': _heads(_pawn())}
    if st.get('dk') is not None:
        try:
            dv = g._view(0, st['dk']) or {}
            comp = g.comps(g.worlds()[0])[st['dk']]
            se = json.loads(comp.read_snapshot_entry(-1) or '{}')
            dp = comp.get_owner()
            mv = dp.get_component_by_class(unreal.CharacterMoverComponent) if hasattr(unreal, 'CharacterMoverComponent') else None
            r['d'] = {'tags': dv.get('tags'), 'loc': dv.get('location'), 'combat': dv.get('combat'), 'm': _montage(dp),
                      'snap': {k: se.get(k) for k in ('actionState', 'reactionFlags', 'damagePercent', 'stateTags') if k in se},
                      'mode': str(mv.get_movement_mode_name()) if mv else None,
                      'vel': [round(x) for x in (lambda q: (q.x, q.y, q.z))(dp.get_velocity())]}
        except Exception as e:
            r['d_err'] = repr(e)[:100]
    st['rows'].write(json.dumps(r) + '\n')



KITS = os.environ.get('DIAG_KITS', 'KIT_Test_Overlord,KIT_Test_Playtest').split(',')
SEQS = {
  'dair': [(0, 'jump'), (14, 'crouch'), (16, 'tap'), (24, 'clear'), (130, 'end')],
  'fair': [(0, 'jump'), (14, 'stick'), (16, 'tap'), (24, 'unstick'), (130, 'end')],
  'jump': [(0, 'jump'), (130, 'end')],
}
PLAN = [(k, s) for k in KITS for s in ('jump', 'dair', 'fair')]


def _v(x):
    return [round(x.x, 2), round(x.y, 2), round(x.z, 2)]


def _cam_row(f, ev):
    p = _pawn()
    pc = g._local_pawn(0)[0]
    cm = pc.get_editor_property('player_camera_manager')
    r = {'f': f, 'ev': ev, 'a': _v(p.get_actor_location()), 'yaw': round(p.get_actor_rotation().yaw, 1), 'vel': _v(p.get_velocity()), 'm': _montage(p)}
    try:
        mesh = p.get_component_by_class(unreal.SkeletalMeshComponent)
        r['mesh'] = _v(mesh.get_world_location())
        r['meshrel'] = _v(mesh.get_editor_property('relative_location'))
        r['pelvis'] = _v(mesh.get_socket_location('pelvis'))
    except Exception as e:
        r['mesh_err'] = repr(e)[:80]
    try:
        for c in p.get_components_by_class(unreal.SceneComponent):
            cn = c.get_class().get_name()
            if cn in ('GameplayCameraComponent', 'GSTN_CameraAnchorComponent') or 'Camera' in cn:
                if hasattr(c, 'get_world_location'):
                    r.setdefault('cc', {})[cn] = _v(c.get_world_location())
        cap = p.get_component_by_class(unreal.CapsuleComponent)
        r['hh'] = round(cap.get_unscaled_capsule_half_height(), 2)
    except Exception as e:
        r['cc_err'] = repr(e)[:80]
    try:
        r['pov'] = _v(cm.get_camera_location())
        rot = cm.get_camera_rotation()
        r['povr'] = [round(rot.pitch, 2), round(rot.yaw, 2)]
    except Exception as e:
        r['pov_err'] = repr(e)[:80]
    try:
        mv = p.get_component_by_class(unreal.CharacterMoverComponent)
        r['mode'] = str(mv.get_movement_mode_name())
    except Exception:
        pass
    try:
        t = unreal.GSTN_PropertyTextLibrary.get_property_text(p, 'PlayerInputState')
        r['pis'] = t.replace(' ', '')[:240]
    except Exception as e:
        r['pis_err'] = repr(e)[:60]
    st['rows'].write(json.dumps(r) + '\n')


def _start_item(now):
    if st['pi'] >= len(PLAN):
        return _finish('ok')
    k, s = PLAN[st['pi']]
    if st.get('curkit') != k:
        _log(f'kit {k} {h.kit(0, k)}')
        st['curkit'] = k
        st.update(phase='kitwait2', t=now)
        return None
    st.update(phase='run', seq=s, frame=0, rows=open(os.path.join(OUT, f'{k}_{s}.jsonl'), 'w'))


def _tick(_dt):
    try:
        now, ph = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ph != 'ending' and now - st['start'] > 600:
            return _finish('timeout ' + ph)
        if ph == 'boot' and now - st['t'] > 3:
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
            _console('t.OverrideFPS 60')
            st.update(atk=g.button_mask('Attack'), cr=g.button_mask('Crouch'), pi=0)
            for cmd in [c for c in os.environ.get('DIAG_CVARS', '').split(';') if c]:
                _console(cmd)
                _log('console ' + cmd)
            if os.environ.get('DIAG_ABS'):
                for c in _pawn().get_components_by_class(unreal.SceneComponent):
                    if c.get_class().get_name() == 'GameplayCameraComponent':
                        c.set_absolute(True, False, False)
                        _log('abs set on ' + c.get_name())
            _start_item(now)
        elif ph == 'kitwait2' and now - st['t'] > 3:
            st.update(phase='gap', t=now - 1.5)
        elif ph == 'gap' and now - st['t'] > 1.5:
            try:
                grounded = str(_pawn().get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name()) == 'Walking'
            except Exception:
                grounded = True
            if grounded or now - st['t'] > 12:
                if not st.get('gt'):
                    st['gt'] = now
                elif now - st['gt'] > 1.0:
                    st['gt'] = None
                    _start_item(now)
            else:
                st['gt'] = None
        elif ph == 'run':
            f = st['frame']
            ev = [a for (fr, a) in SEQS[st['seq']] if fr == f]
            _cam_row(f, ev)
            for a in ev:
                if a == 'tap':
                    g.combat_tap(0, st['atk'])
                elif a == 'jump':
                    g.press(0, 'jump')
                elif a == 'stick':
                    g.hold(0, 'move', (0, 1, 0))
                elif a == 'unstick':
                    g.release(0, 'move')
                elif a == 'crouch':
                    g.combat_hold(0, st['cr'])
                elif a == 'clear':
                    g.combat_clear(0)
            st['frame'] = f + 1
            if 'end' in ev:
                st['rows'].close()
                g.combat_clear(0)
                g.release(0, 'move')
                st['pi'] += 1
                st.update(phase='gap', t=now)
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
