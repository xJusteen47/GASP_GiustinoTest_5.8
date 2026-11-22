"""Editor boot script: ask the Unreal Assistant (AIAssistantInstance, free) a list of questions, write the replies, quit.

For a thread with no editor open: run it through Run-Timed (Capture preset, windowed editor, moved to virtual desktop 2):
  & .\\Tools\\CI\\Run-Timed.ps1 -Preset Capture -Log Saved\\Logs\\ci\\ask.log -Script Tools\\UE59\\ask_assistant_boot.py
Questions: Saved/Assistant/questions.json ({"context": "...", "questions": ["...", ...]}), one new conversation for all of them.
Replies: Saved/Assistant/replies.json (written after each reply; "done": true at the end). The call is async: a slate post-tick
callback polls it, so the editor must keep ticking. Timeout ASSIST_TIMEOUT_S (default 900 s) per question.
"""
import json, os, runpy, time, uuid

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
VD2 = os.path.join(ROOT, 'Tools', 'GASNPPTest', 'vd2_mover.py')
if os.path.exists(VD2):
    try:
        runpy.run_path(VD2, run_name='vd2_mover')
    except Exception as e:  # noqa: BLE001  (the desktop move is a courtesy, never a reason to stop)
        unreal.log_warning(f'vd2_mover: {e}')

D = os.path.join(ROOT, 'Saved', 'Assistant')
Q = json.load(open(os.path.join(D, 'questions.json'), encoding='utf-8'))
OUT = os.path.join(D, 'replies.json')
T_START = time.time()
WARM_S = float(os.environ.get('ASSIST_WARM_S', '60'))
TIMEOUT = float(os.environ.get('ASSIST_TIMEOUT_S', '900'))
_st = {'i': 0, 'pending': None, 'conv': '', 'res': [], 'h': None, 't0': 0.0, 'warm': 0}


def _save(done=False):
    json.dump({'done': done, 'conversation_id': _st['conv'], 'replies': _st['res']}, open(OUT, 'w', encoding='utf-8'), indent=1)


def _finish():
    _save(True)
    unreal.unregister_slate_post_tick_callback(_st['h'])
    unreal.SystemLibrary.quit_editor()


def _tick(_dt):
    s = _st
    try:
        if time.time() - T_START < WARM_S:   # the assistant's web backend needs time after boot (try 1: 120 ticks gave empty replies)
            return
        if s['pending'] is None:
            if time.time() < s.get('wait_until', 0):
                return
            if s['i'] >= len(Q['questions']):
                _finish(); return
            s['pending'] = unreal.AIAssistantInstance.get().send_text_message_and_get_reply(s['conv'], Q['questions'][s['i']], Q.get('context', ''))
            s['t0'] = time.time()
            return
        r = s['pending']
        if r.is_complete and not str(r.value).strip() and s.get('retry', 0) < 2:   # empty reply: backend not ready, ask again
            s['retry'] = s.get('retry', 0) + 1; s['pending'] = None; s['wait_until'] = time.time() + 20; return
        if r.is_complete:
            rec = {'q': Q['questions'][s['i']], 'a': str(r.value)}
            if not rec['a'].strip():   # diagnosis: what the result object carries
                rec['debug'] = {k: str(getattr(r, k, ''))[:300] for k in dir(r) if not k.startswith('_') and not callable(getattr(r, k, None))}
            s['res'].append(rec); s['retry'] = 0
        elif time.time() - s['t0'] > TIMEOUT:
            s['res'].append({'q': Q['questions'][s['i']], 'a': 'TIMEOUT'})
        else:
            return
        s['pending'] = None; s['i'] += 1; _save(False)
    except Exception as e:  # noqa: BLE001
        s['res'].append({'q': Q['questions'][min(s['i'], len(Q['questions']) - 1)], 'a': f'ERROR {e!r}'})
        _finish()


_st['h'] = unreal.register_slate_post_tick_callback(_tick)
