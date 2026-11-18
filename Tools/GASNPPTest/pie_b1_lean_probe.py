# Bug pass B1 check in Play: the run start scramble's extra lean at low frame rates. For each cap in CAPS (t.MaxFPS), the host stands still,
# then holds the stick forward for HOLD_S seconds and stops; every rendered frame logs the scramble lean (degrees) and alpha
# (UGSTN_RunPresentationComponent::GetScrambleLeanAndAlpha). Before the B1 fix the lean diverged at 30 fps and below (inf/NaN, torso locked bent).
# PASS per cap: every value finite, |lean| under LEAN_LIMIT, and the lean back near 0 (under SETTLE_DEG) by the end of the rest after the stop.
# Output: Saved/B1Lean/<date_time>/samples.jsonl, b1_summary.json, capture.log, done.txt. Boot: vd2_mover first, then this file.
import json
import math
import os
import runpy
import sys
import time
import traceback

_here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(_here, 'vd2_mover.py'), run_name='vd2_mover')
except Exception as e:  # noqa: BLE001
    print('vd2_mover failed:', e)

import unreal  # noqa: E402

sys.path.insert(0, _here)
import gasnpp_test as g  # noqa: E402

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'B1Lean', time.strftime('%Y%m%d_%H%M%S'))
MAP = '/Game/Levels/Test/L_Test_Hub'
CAPS = [60, 30, 20]
REST_S, HOLD_S, AFTER_S = 1.5, 2.5, 2.5
LEAN_LIMIT, SETTLE_DEG = 45.0, 3.0
TIMEOUT = 600.0
st = {'phase': 'boot', 't': time.time(), 'start': time.time(), 'ci': 0, 'samples': {}, 'handle': None}


def _log(msg):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'capture.log'), 'a') as f:
        f.write(f'{time.time() - st["start"]:7.1f}s {msg}\n')


def _world():
    return g.worlds()[0]


def _console(cmd):
    unreal.SystemLibrary.execute_console_command(_world(), cmd)


def _pres():
    pawn = unreal.GameplayStatics.get_player_pawn(_world(), 0)
    return pawn.get_component_by_class(unreal.GSTN_RunPresentationComponent) if pawn else None


def _finish(status):
    _log(f'finish: {status}')
    st['status'] = status
    st['phase'] = 'ending'
    try:
        g.release(0, 'move')
        _console('t.MaxFPS 0')
    except Exception:  # noqa: BLE001
        pass
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les.is_in_play_in_editor():
        les.editor_request_end_play()


def _summarise():
    out = {}
    for cap, rows in st['samples'].items():
        leans = [r['lean'] for r in rows]
        finite = all(math.isfinite(x) for x in leans) and all(math.isfinite(r['alpha']) for r in rows)
        peak = max((abs(x) for x in leans if math.isfinite(x)), default=None)
        tail = [abs(r['lean']) for r in rows if r['t'] >= REST_S + HOLD_S + AFTER_S - 0.3]
        hold = [r for r in rows if REST_S <= r['t'] < REST_S + HOLD_S]
        dts = [b['t'] - a['t'] for a, b in zip(rows, rows[1:])]
        out[cap] = {'frames': len(rows), 'mean_dt_ms': round(1000 * sum(dts) / len(dts), 1) if dts else None, 'finite': finite,
                    'peak_lean': round(peak, 2) if peak is not None else None, 'end_lean': round(max(tail), 2) if tail else None,
                    'peak_alpha': round(max((r['alpha'] for r in hold), default=0.0), 3),
                    'pass': bool(finite and peak is not None and peak < LEAN_LIMIT and tail and max(tail) < SETTLE_DEG)}
    with open(os.path.join(OUT, 'b1_summary.json'), 'w') as f:
        json.dump(out, f, indent=1)
    _log('summary ' + json.dumps(out))


def _tick(dt):
    try:
        now, phase = time.time(), st['phase']
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase != 'ending' and now - st['start'] > TIMEOUT:
            return _finish(f'timeout in phase {phase}')
        if phase == 'boot' and now - st['t'] > 8:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            if world is None or world.get_name() != 'L_Test_Hub':
                _log(f'loading {MAP}: {les.load_level(MAP)}')
            st.update(phase='play', t=now)
        elif phase == 'play' and now - st['t'] > 4:
            _log(f'setup_editor: {g.setup_editor(True, "listen", 1)}')
            les.editor_request_begin_play()
            st.update(phase='wait', t=now)
        elif phase == 'wait':
            if les.is_in_play_in_editor() and _pres() is not None:
                st.update(phase='settle', t=now)
            elif now - st['t'] > 180:
                _finish('no host pawn with a run presentation component after 180 s')
        elif phase == 'settle' and now - st['t'] > 10:
            st.update(phase='cap', t=now)
        elif phase == 'cap':
            if st['ci'] >= len(CAPS):
                _summarise()
                return _finish('ok')
            cap = CAPS[st['ci']]
            _console(f't.MaxFPS {cap}')
            g.release(0, 'move')
            st['samples'][cap] = []
            st.update(phase='run', t=now, run_t=now, held=False, released=False)
            _log(f'cap {cap}')
        elif phase == 'run':
            cap = CAPS[st['ci']]
            t = now - st['run_t']
            if not st['held'] and t >= REST_S:
                g.hold(0, 'move', (1.0, 0.0, 0.0))
                st['held'] = True
            if st['held'] and not st['released'] and t >= REST_S + HOLD_S:
                g.release(0, 'move')
                st['released'] = True
            v = _pres().get_scramble_lean_and_alpha()
            row = {'t': round(t, 4), 'lean': float(v.x), 'alpha': float(v.y), 'dt': round(dt, 4)}
            st['samples'][cap].append(row)
            with open(os.path.join(OUT, 'samples.jsonl'), 'a') as f:
                f.write(json.dumps({'cap': cap, **{k: (x if not isinstance(x, float) or math.isfinite(x) else str(x)) for k, x in row.items()}}) + '\n')
            if t >= REST_S + HOLD_S + AFTER_S:
                st['ci'] += 1
                st.update(phase='cap', t=now)
        elif phase == 'ending' and not les.is_in_play_in_editor():
            unreal.unregister_slate_post_tick_callback(st['handle'])
            try:
                g.setup_editor(False)
            except Exception as e:  # noqa: BLE001
                _log(f'setup_editor(False) failed: {e!r}')
            with open(os.path.join(OUT, 'done.txt'), 'w') as f:
                f.write(st.get('status', '?') + '\n')
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:  # noqa: BLE001
        _log('ERROR ' + traceback.format_exc())
        if st['phase'] != 'ending':
            _finish('error (see capture.log)')
        else:
            unreal.unregister_slate_post_tick_callback(st['handle'])
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    return None


_log(f'driver loaded: out {OUT}')
st['handle'] = unreal.register_slate_post_tick_callback(_tick)
