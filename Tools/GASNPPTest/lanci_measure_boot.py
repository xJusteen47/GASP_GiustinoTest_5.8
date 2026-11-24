# LANCI-1A2A (2026-11-24): launch measurements in Play. Runs pie_capture_move.py with extra sequences (nothing in that file changes):
#   lk_dropkick_p0 / _p40   dropkick (forward air) on a dummy at 0 % / 40 %
#   lk_dair_p0 / _p40       down air on a dummy at 0 % / 40 % (pressed rising, as dair_hit_rise)
#   lk_jab2_p0              jab 1 -> jab 2 on a dummy at 0 % (the dummy is put 95 cm ahead after jab 1, so jab 2 is the hit)
#   lk_jab2_air_<h>         the same jab 2 on a dummy put <h> cm above the floor just before the hit: airborne and descending when struck
# Every row also logs the dummy's velocity, and on frame 0 its attributes (JumpRiseGravityScale & co: does the victim fly the jump gravity?).
# LANCI_AB=1: the rules' KnockbackGravityScale and LaunchDeceleration are set to 0 in memory before Play (the old flight), never saved.
# Launch: Tools/CI/Run-Timed.ps1 -Preset Capture -Script <a boot without spaces that execs this file>, env PIE_CAPTURE_OUT / PIE_CAPTURE_SEQUENCES.
import os

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
os.environ.setdefault('PIE_CAPTURE_KIT', 'KIT_Test_Overlord')
os.environ.setdefault('PIE_CAPTURE_VIEWS', 'az90,game')

if os.environ.get('LANCI_AB') == '1':
    _rules = unreal.load_asset('/GSTNCombat/Rules/GSTN_CombatRules_Standard')
    _model = _rules.get_editor_property('knockback_model')
    _tun = _model.get_editor_property('tuning')
    _before = (_tun.get_editor_property('knockback_gravity_scale'), _tun.get_editor_property('launch_deceleration'))
    _tun.set_editor_property('knockback_gravity_scale', 0.0)
    _tun.set_editor_property('launch_deceleration', 0.0)
    _model.set_editor_property('tuning', _tun)
    unreal.log(f'[lanci] A/B old flight: KnockbackGravityScale, LaunchDeceleration {_before} -> (0, 0) in memory')

if os.environ.get('LANCI_NOBALLOON') == '1':   # A/B of the balloon, in memory only, never saved
    _rules = unreal.load_asset('/GSTNCombat/Rules/GSTN_CombatRules_Standard')
    _model = _rules.get_editor_property('knockback_model')
    _tun = _model.get_editor_property('tuning')
    _bal = _tun.get_editor_property('balloon')
    _bal.set_editor_property('enabled', False)
    _tun.set_editor_property('balloon', _bal)
    _model.set_editor_property('tuning', _tun)
    unreal.log('[lanci] A/B: balloon off in memory')

_DK = {'rise_h': 150.0, 'stick': True, 'press_delay': 5}
_DA = {'rise_h': float(os.environ.get('PIE_CAPTURE_DAIR_HIT_RISE', 100.0))}
LANCI_SEQUENCES = [
    {'name': 'lk_dropkick_p0', 'taps': [], 'frames': 200, 'dummy': 275.0, 'dummy_percent': 0.0, 'shoot': True, 'air': dict(_DK)},
    {'name': 'lk_dropkick_p40', 'taps': [], 'frames': 200, 'dummy': 275.0, 'dummy_percent': 40.0, 'shoot': True, 'air': dict(_DK)},
    {'name': 'lk_dair_p0', 'taps': [], 'frames': 200, 'dummy': 40.0, 'dummy_percent': 0.0, 'shoot': True, 'air': dict(_DA)},
    {'name': 'lk_dair_p40', 'taps': [], 'frames': 200, 'dummy': 40.0, 'dummy_percent': 40.0, 'shoot': True, 'air': dict(_DA)},
    {'name': 'lk_jab2_p0', 'taps': [0, 12], 'frames': 200, 'dummy': 600.0, 'dummy_percent': 0.0, 'shoot': True, 'lift': (13, 95.0, 0.0)},
]
for _h in [int(v) for v in os.environ.get('LANCI_AIR_H', '120,200,280').split(',') if v]:
    LANCI_SEQUENCES.append({'name': f'lk_jab2_air_{_h}', 'taps': [0, 12], 'frames': 200, 'dummy': 600.0, 'dummy_percent': 0.0, 'shoot': True,
                            'lift': (13, 95.0, float(_h))})
# Desktop 2026-11-24: the hub's test pawn jumps ~9 m (gravity scale 1), so the dropkick and the dair strike far above a dummy on the floor.
# These put the dummy next to the attacker in the air just before the strike ('lift_rel'): an air hit, same launch velocity
for _n, _rel in (('a', (3, 130.0, -30.0)), ('b', (6, 170.0, -30.0))):
    LANCI_SEQUENCES.append({'name': f'lk_dk_air{_n}_p0', 'taps': [], 'frames': 200, 'dummy': 600.0, 'dummy_percent': 0.0, 'shoot': True,
                            'air': dict(_DK), 'lift_rel': _rel})
for _n, _rel in (('a', (6, 40.0, -140.0)), ('b', (10, 40.0, -160.0))):
    LANCI_SEQUENCES.append({'name': f'lk_da_air{_n}_p0', 'taps': [], 'frames': 200, 'dummy': 600.0, 'dummy_percent': 0.0, 'shoot': True,
                            'air': dict(_DA), 'lift_rel': _rel})
for _n, _pin in (('dk', (2, 30, 110.0, -20.0)), ('da', (4, 34, 35.0, -150.0))):
    LANCI_SEQUENCES.append({'name': f'lk_{_n}_pin_p0', 'taps': [], 'frames': 220, 'dummy': 600.0, 'dummy_percent': 0.0, 'shoot': True,
                            'air': dict(_DK if _n == 'dk' else _DA), 'pin_rel': _pin})
# Strong attack (run with PIE_CAPTURE_KIT=KIT_Test_Playtest PIE_CAPTURE_BUTTON=Strong): one tap on a dummy 95 cm ahead at these percents
for _p in [int(v) for v in os.environ.get('LANCI_STRONG_P', '0,40,80,100,120').split(',') if v]:
    LANCI_SEQUENCES.append({'name': f'lk_strong_p{_p}', 'taps': [0], 'frames': 330, 'dummy': 95.0, 'dummy_percent': float(_p), 'shoot': True})

# Per frame, the host's view of the dummy: <seq dir>/lanci_cam.jsonl (camera location and FOV, dummy on screen or not, its screen position).
# LANCI_UISHOT='<run>/<seq>:<from>:<to>:<step>;...' ('<seq>' alone when no runs): a real viewport shot WITH the UI (the off-screen arrow is a
# UMG widget, scene captures and HighResShot do not show it) on those frames: Saved/Screenshots/WindowsEditor/ScreenShot*.png, logged in order.
_UISHOT = {}
for _spec in [s for s in os.environ.get('LANCI_UISHOT', '').split(';') if s.strip()]:
    _k, _a, _b, _s = _spec.strip().split(':')
    _UISHOT[_k] = (int(_a), int(_b), int(_s))


def _lanci_cam(seq, frame):
    if not st.get('dummy_key'):
        return
    try:
        pc = g._local_pawn(0)[0]
        dummy = g.comps(g.worlds()[0])[st['dummy_key']].get_owner().get_actor_location()
        cam = pc.player_camera_manager
        cl = cam.get_camera_location()
        vx, vy = pc.get_viewport_size()
        r = unreal.GameplayStatics.project_world_to_screen(pc, dummy, False)
        ok, sp = (r[0], r[1]) if isinstance(r, tuple) else (r is not None, r)
        on = bool(ok and sp is not None and 0 <= sp.x <= vx and 0 <= sp.y <= vy)
        pawn = _pawn().get_actor_location()
        row = {'f': frame, 'cam': [round(cl.x), round(cl.y), round(cl.z)], 'fov': round(cam.get_fov_angle(), 2),
               'cam_pawn_cm': round((cl - pawn).length()), 'dummy': [round(dummy.x), round(dummy.y), round(dummy.z)],
               'on_screen': on, 'sx': round(sp.x / vx, 3) if ok and sp is not None and vx else None,
               'sy': round(sp.y / vy, 3) if ok and sp is not None and vy else None, 'vp': [vx, vy]}
        with open(os.path.join(_seq_dir(seq), 'lanci_cam.jsonl'), 'a') as f:
            f.write(json.dumps(row) + '\n')
    except Exception as e:  # noqa: BLE001
        if not st.get('lanci_cam_err'):
            st['lanci_cam_err'] = True
            _log(f'lanci_cam failed: {e!r}')
    key = f'{seq.get("run")}/{seq["name"]}' if seq.get('run') else seq['name']
    spec = _UISHOT.get(key)
    if spec and spec[0] <= frame <= spec[1] and (frame - spec[0]) % spec[2] == 0:
        _console('shot showui')
        _log(f'UISHOT {key} v{frame}')


def _lanci_hook(seq, frame):
    """'lift': (frame after PRE, cm ahead of the host, cm above the floor): server teleport of the dummy there on that frame"""
    _lanci_cam(seq, frame)
    # 'pin_rel': (from, to frames after the air attack press, cm ahead, cm above the host's centre): the dummy is held there every frame
    # (server teleport) until it is hit (its action state leaves 0), so the strike finds it wherever the jump has taken the attacker
    pin = seq.get('pin_rel')
    if pin and st.get('dummy_key') and st.get('fired') is not None and pin[0] <= frame - st['fired'] <= pin[1] and not st.get('pin_done'):
        comp = g.comps(g.worlds()[0])[st['dummy_key']]
        se = json.loads(comp.read_snapshot_entry(-1) or '{}')
        if se.get('actionState', 0) != 0 or (se.get('damagePercent') or 0) > 0.01:
            st['pin_done'] = frame
            _log(f'{seq["name"]}: dummy hit, pin released on v{frame} ({frame - st["fired"]} after the press)')
        else:
            import math as _m
            p = _pawn()
            loc, yaw = p.get_actor_location(), _m.radians(p.get_actor_rotation().yaw)
            comp.server_teleport_exact(unreal.Vector(loc.x + pin[2] * _m.cos(yaw), loc.y + pin[2] * _m.sin(yaw), loc.z + pin[3]))
    if frame == 0:
        st['pin_done'] = None
    rel = seq.get('lift_rel')   # (frames after the air attack press, cm ahead of the host, cm above the host's centre): air-hit version
    if rel and st.get('dummy_key') and st.get('fired') is not None and frame == st['fired'] + rel[0]:
        import math as _m
        p = _pawn()
        loc, yaw = p.get_actor_location(), _m.radians(p.get_actor_rotation().yaw)
        x, y, z = loc.x + rel[1] * _m.cos(yaw), loc.y + rel[1] * _m.sin(yaw), loc.z + rel[2]
        g.comps(g.worlds()[0])[st['dummy_key']].server_teleport_exact(unreal.Vector(x, y, z))
        _log(f'{seq["name"]}: dummy put at ({x:.0f}, {y:.0f}, {z:.0f}) on v{frame}, host z {loc.z:.0f}')
    lift = seq.get('lift')
    if not lift or frame - PRE != lift[0] or not st.get('dummy_key'):
        return
    v = g.snap(g.worlds()[0])[h._key(0)]
    import math as _m
    yaw = _m.radians(v['yaw'])
    x, y = v['location'][0] + lift[1] * _m.cos(yaw), v['location'][1] + lift[1] * _m.sin(yaw)
    z = st.get('lanci_ground', v['location'][2]) + 2.0 + lift[2]
    g.comps(g.worlds()[0])[st['dummy_key']].server_teleport_exact(unreal.Vector(x, y, z))
    _log(f'{seq["name"]}: dummy lifted to ({x:.0f}, {y:.0f}, {z:.0f}) on v{frame}')


_src = open(os.path.join(_PROJ, 'Tools', 'GASNPPTest', 'pie_capture_move.py'), encoding='utf-8').read()
_patches = [
    ('DROPKICK_SEQUENCES = [\n', 'DROPKICK_SEQUENCES = LANCI_SEQUENCES + [\n'),
    ("            row['dummy'] = {'tags': dv.get('tags'), 'loc': dv.get('location'), 'combat': dv.get('combat')}\n",
     "            row['dummy'] = {'tags': dv.get('tags'), 'loc': dv.get('location'), 'combat': dv.get('combat'), 'vel': dv.get('velocity')}\n"
     "            if frame == 0:\n"
     "                row['dummy']['attr'] = dv.get('attributes')\n"
     "                st['lanci_ground'] = _pawn().get_actor_location().z\n"),
    ('            _row(frame, tapped, extra)\n', '            _row(frame, tapped, extra)\n            _lanci_hook(seq, frame)\n'),
]
for _a, _b in _patches:
    assert _src.count(_a) == 1, f'lanci patch anchor not found once: {_a[:60]!r}'
    _src = _src.replace(_a, _b)
exec(compile(_src, 'pie_capture_move.py', 'exec'))
