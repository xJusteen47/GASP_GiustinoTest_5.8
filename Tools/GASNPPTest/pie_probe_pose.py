# MONTAGE-EXIT probe (thread n216qs): numbers, no pictures, for "what the game pose under a combat montage does when the montage leaves".
# Same Play setup as pie_capture_move.py (L_Test_Hub, listen host + 2 clients, the kit's button pressed through the combat component) but it
# shoots nothing: per rendered frame it writes the host pawn's bone heights above the capsule centre (pelvis, head, feet), the Mover mode, the
# montage and its position, the combat state and tags; on chosen frames it runs `obj dump` on the main anim instance (the GASP graph's own
# variables, which Python cannot list) between two log markers, so a later diff shows which graph variable changes with the squat or the legs.
#
# Launch: UnrealEditor.exe <uproject> /Game/Levels/Test/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py: exec(open(r"<project>/Tools/GASNPPTest/pie_probe_pose.py").read())   (the boot path must have no space)
# Environment: PIE_PROBE_OUT (default Saved/MontageExit/probe_<time>), PIE_PROBE_SEQUENCES (comma list of the names below).
# Output: <out>/<sequence>.jsonl, <out>/probe.log, <out>/done.txt; the dumps are in the editor log between "[pose_probe] dump <seq> <frame>" lines.
# Nothing is saved: the montage blend changed by a sequence is put back after it, then QUIT_EDITOR.
import json
import os
import sys
import time
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g   # noqa: E402
import test_hub as h      # noqa: E402

OUT = os.environ.get('PIE_PROBE_OUT') or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'MontageExit',
                                                      time.strftime('probe_%Y%m%d_%H%M%S'))
PRE = 20
BONES = ('pelvis', 'head', 'foot_l', 'foot_r')
JAB1 = '/Game/GASNPPTest/Combat/Overlord/AM_OverlordJab1V3'
# kit: the kit for the sequence (changed only when it differs from the last one). blend_in: seconds put on `montage` for this sequence only.
# taps: frames after PRE the Attack button is pressed. air: jump on PRE, crouch + Attack once rising past rise_h (pie_capture_move's dair_whiff).
# dumps: frames after the press (taps[0] or the air press) that get an `obj dump`
SEQUENCES = [
    {'name': 'jab1_in0', 'kit': 'KIT_Test_Overlord', 'taps': [0], 'frames': 100, 'dumps': [12, 30, 40, 95]},
    {'name': 'jab1_in2', 'kit': 'KIT_Test_Overlord', 'taps': [0], 'frames': 100, 'dumps': [12, 30, 40, 95],
     'montage': JAB1, 'blend_in': 2.0 / 60.0},
    {'name': 'jab1_trig0', 'kit': 'KIT_Test_Overlord', 'taps': [0], 'frames': 100, 'dumps': list(range(22, 34)) + [40],
     'montage': JAB1, 'trigger': 0.0},
    {'name': 'jab1_each', 'kit': 'KIT_Test_Overlord', 'taps': [0], 'frames': 100, 'dumps': list(range(20, 34))},
    {'name': 'idle', 'kit': 'KIT_Test_Overlord', 'taps': [], 'frames': 30, 'dumps': [10]},
    {'name': 'dair_whiff', 'kit': 'KIT_Test_OverlordDAirV9', 'taps': [], 'frames': 130, 'dumps': [20] + list(range(38, 50)) + [60], 'air': {'rise_h': 150.0}},
]
if os.environ.get('PIE_PROBE_SEQUENCES'):
    SEQUENCES = [s for s in SEQUENCES if s['name'] in os.environ['PIE_PROBE_SEQUENCES'].split(',')]
TIMEOUT = 900.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'seq': 0, 'frame': 0, 'handle': None, 'rows': None, 'kit': None}


def _log(msg):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'probe.log'), 'a') as f:
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


def _main_anim():
    """(mesh, anim instance) of the first skeletal mesh with an anim instance: the GASP graph"""
    for comp in _pawn().get_components_by_class(unreal.SkeletalMeshComponent):
        anim = comp.get_anim_instance()
        if anim:
            return comp, anim
    return None, None


def _pose():
    mesh, anim = _main_anim()
    pawn = _pawn()
    base = pawn.get_actor_location()
    out = {'z': round(base.z, 1)}
    if mesh:
        for bone in BONES:
            loc = mesh.get_socket_location(bone)
            out[bone] = round(loc.z - base.z, 1)
        for bone in ('foot_l', 'foot_r', 'calf_l', 'calf_r'):
            t = mesh.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT).translation
            out['c_' + bone] = [round(t.x, 1), round(t.y, 1), round(t.z, 1)]
    if anim:
        montage = anim.get_current_active_montage()
        out['montage'] = [montage.get_name(), round(anim.montage_get_position(montage) * 60.0, 2), bool(anim.montage_is_active(montage))] if montage else None
        out['any_montage'] = bool(anim.is_any_montage_playing())
    try:
        mover = pawn.get_component_by_class(unreal.MoverComponent)
        out['mode'] = str(mover.get_movement_mode_name()) if mover else None
    except Exception as e:
        out['mode'] = repr(e)[:60]
    return out


def _dump(seq, rel):
    _, anim = _main_anim()
    if not anim:
        return
    unreal.log(f'[pose_probe] dump {seq} {rel} begin {anim.get_path_name()}')
    _console(f'obj dump {anim.get_path_name()}')
    unreal.log(f'[pose_probe] dump {seq} {rel} end')


def _row(seq, frame, tapped, extra):
    view = g._view(0, h._key(0)) or {}
    row = {'f': frame, 'tap': tapped, 'combat': view.get('combat'), 'tags': view.get('tags')}
    row.update(_pose())
    row.update(extra)
    st['rows'].write(json.dumps(row) + '\n')


def _set_trigger(path, seconds):
    montage = unreal.load_asset(path)
    old = montage.get_editor_property('blend_out_trigger_time')
    montage.set_editor_property('blend_out_trigger_time', seconds)
    return old


def _set_blend_in(path, seconds):
    montage = unreal.load_asset(path)
    blend = montage.get_editor_property('blend_in')
    old = blend.get_editor_property('blend_time')
    blend.set_editor_property('blend_time', seconds)
    montage.set_editor_property('blend_in', blend)
    return old


def _finish(status):
    _log(f'finish: {status}')
    st.update(phase='ending', status=status)
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
        elif phase == 'settle' and now - st['t'] > 12:
            _log(f'parked {[h.park(i, "Z1") for i in h._players() if i != 0]}')
            st.update(phase='seq_begin', t=now)
        elif phase == 'seq_begin' and now - st['t'] > 3:
            if st['seq'] >= len(SEQUENCES):
                return _finish('ok')
            seq = SEQUENCES[st['seq']]
            if seq['kit'] != st['kit']:
                _log(f'kit {seq["kit"]}: {h.kit(0, seq["kit"])}')
                st.update(kit=seq['kit'], t=now)
                return None
            z = _pawn().get_actor_location().z
            if (st.get('rest_z') is None or abs(z - st['rest_z']) > 0.05) and now - st['t'] < 10:
                st['rest_z'] = z
                return None
            st['ground_z'] = z
            if seq.get('trigger') is not None:
                st['restore_trig'] = (seq['montage'], _set_trigger(seq['montage'], seq['trigger']))
                _log(f'{seq["name"]}: blend out trigger {st["restore_trig"][1]} -> {seq["trigger"]}')
            elif seq.get('montage'):
                st['restore'] = (seq['montage'], _set_blend_in(seq['montage'], seq['blend_in']))
                _log(f'{seq["name"]}: blend in of {seq["montage"]} {st["restore"][1]:.4f} -> {seq["blend_in"]:.4f} s')
            _log(f'sequence {seq["name"]}: z {z:.1f}')
            st['rows'] = open(os.path.join(OUT, f'{seq["name"]}.jsonl'), 'w')
            _console('t.OverrideFPS 60')
            st.update(phase='seq_run', frame=0, mask=g.button_mask('Attack'), crouch=g.button_mask('Crouch'), fired=None, prev_z=None)
        elif phase == 'seq_run':
            seq, frame = SEQUENCES[st['seq']], st['frame']
            air = seq.get('air')
            extra, hold_crouch = {}, False
            if air is not None:
                z = _pawn().get_actor_location().z
                height = z - st['ground_z']
                extra['h'] = round(height, 1)
                rising = st['prev_z'] is not None and z > st['prev_z']
                if frame == PRE:
                    g.press(0, 'jump')
                elif st['fired'] is None and frame > PRE + 3 and ((rising and height >= air['rise_h']) or frame - PRE >= 100):
                    st['fired'] = frame + 1
                    hold_crouch = True
                    _log(f'{seq["name"]}: crouch on v{frame}, attack on v{frame + 1}')
                tapped = st['fired'] is not None and frame == st['fired']
                st['prev_z'] = z
                press = st['fired']
            else:
                tapped = (frame - PRE) in seq['taps']
                press = PRE + seq['taps'][0] if seq['taps'] else PRE
            _row(seq['name'], frame, tapped, extra)
            if press is not None and (frame - press) in seq['dumps']:
                _dump(seq['name'], frame - press)
            if hold_crouch:
                g.combat_hold(0, st['crouch'])
            if tapped:
                g.combat_tap(0, st['mask'])
            if air is not None and st['fired'] is not None and frame == st['fired'] + 4:
                g.combat_clear(0)
            st['frame'] = frame + 1
            if st['frame'] >= PRE + seq['frames']:
                st['rows'].close()
                g.combat_clear(0)
                _console('t.OverrideFPS 0')
                if st.get('restore_trig'):
                    _set_trigger(*st['restore_trig'])
                    st['restore_trig'] = None
                if st.get('restore'):
                    _set_blend_in(*st['restore'])
                    st['restore'] = None
                _log(f'sequence {seq["name"]} done')
                st.update(phase='seq_begin', seq=st['seq'] + 1, t=now, rest_z=None)
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
            _finish('error (see probe.log)')
        else:
            unreal.unregister_slate_post_tick_callback(st['handle'])
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write('error while ending\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    return None


_log(f'probe loaded, out {OUT}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
