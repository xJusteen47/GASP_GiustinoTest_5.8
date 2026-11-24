# Unattended editor session for animpipe (dropkick-tb-4, 2026-11-21; job queue import-veloce-1, 2026-11-24): the laptop threads have
# no owner at the editor, and animpipe's build, check and render talk to a RUNNING editor through UE_MCP_Bridge (ue.py). This script
# is the editor's -ExecCmds boot: it writes Saved/AnimPipeSession/ready.txt once the editor ticks, then waits; creating
# Saved/AnimPipeSession/quit.txt closes the editor (no save).
# Start it under the time limit (project rule 5), e.g.:
#   Run-Timed.ps1 -Preset Capture -Script Tools\AnimPipeline\editor_session.py -Log Saved\Logs\ci\animpipe_session.log -MaxMinutes 90 -SilentMinutes 30
# then run `python -m animpipe <verb> <move>` from Tools/AnimPipeline as usual, then: echo > Saved/AnimPipeSession/quit.txt
# Tools/AnimPipeline/round_runner.py does all of this, reuses a live session and only boots one when none answers.
#
# Job queue (why: one editor boot + map load per round instead of one per step; speed-up.md TODO "one editor session for many
# captures"). A client drops Saved/AnimPipeSession/jobs/<id>.json = {"script": <repo-relative .py>, "env": {...}, "mode": "sync"|"boot",
# "entry": <function, sync only>, "kwargs": {...}} and waits for jobs/<id>.done.json = {"ok", "result", "sec", "mode"}.
#  - sync: runpy (or the named entry) runs inside one tick, like the bridge's run_python_file.
#  - boot: a script written as an -ExecCmds boot (pie_capture_move.py: registers its own tick callbacks, plays, then sends
#    QUIT_EDITOR). It runs against a proxy of the `unreal` module: QUIT_EDITOR / quit_editor end the JOB instead of the editor, and
#    every tick callback the job registered is unregistered when it ends, so the next job starts from a clean editor tick list.
#    PIE is stopped if the job left it running. Nothing is saved (as in a fresh boot); the level on disk is never touched.
# Env set by a job is restored after it, so the next job reads the same environment as a fresh editor would.
import json
import os
import runpy
import sys
import time
import traceback
import types

import unreal

_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimPipeSession')
_JOBS = os.path.join(_DIR, 'jobs')
os.makedirs(_JOBS, exist_ok=True)
_QUIT = os.path.join(_DIR, 'quit.txt')
_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
for _stale in (_QUIT, os.path.join(_DIR, 'ready.txt')):
    if os.path.exists(_stale):
        os.remove(_stale)
_state = {'ticks': 0, 'handle': None, 'job': None, 'jobs_done': 0, 'boot': time.time()}


def _write(path, data):
    tmp = path + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as fh:
        json.dump(data, fh)
    os.replace(tmp, path)


class _JobUnreal(types.ModuleType):
    """`unreal` as a boot-style job sees it: everything forwards to the real module, except quitting and tick callbacks."""

    def __init__(self, job):
        super().__init__('unreal')
        self._job = job
        real = unreal

        class SystemLibrary:
            def __getattr__(self, name):
                return getattr(real.SystemLibrary, name)

            @staticmethod
            def execute_console_command(world, cmd, *a, **kw):
                if str(cmd).strip().upper() in ('QUIT_EDITOR', 'QUIT', 'EXIT'):
                    job['quit'] = True
                    return None
                return real.SystemLibrary.execute_console_command(world, cmd, *a, **kw)

            @staticmethod
            def quit_editor():
                job['quit'] = True

        self.SystemLibrary = SystemLibrary()

    def __getattr__(self, name):
        return getattr(unreal, name)

    def register_slate_post_tick_callback(self, fn):
        h = unreal.register_slate_post_tick_callback(fn)
        self._job['handles'].append(h)
        return h

    def register_slate_pre_tick_callback(self, fn):
        h = unreal.register_slate_pre_tick_callback(fn)
        self._job['pre_handles'].append(h)
        return h

    def unregister_slate_post_tick_callback(self, h):
        if h in self._job['handles']:
            self._job['handles'].remove(h)
        unreal.unregister_slate_post_tick_callback(h)

    def unregister_slate_pre_tick_callback(self, h):
        if h in self._job['pre_handles']:
            self._job['pre_handles'].remove(h)
        unreal.unregister_slate_pre_tick_callback(h)


def _set_env(env):
    saved = {k: os.environ.get(k) for k in env}
    for k, v in env.items():
        os.environ[k] = str(v)
    return saved


def _restore_env(saved):
    for k, v in saved.items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = v


def _end_play():
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if les.is_in_play_in_editor():
            les.editor_request_end_play()
            return True
    except Exception:
        pass
    return False


def _finish(job, ok, result):
    for h in list(job['handles']):
        try:
            unreal.unregister_slate_post_tick_callback(h)
        except Exception:
            pass
    for h in list(job['pre_handles']):
        try:
            unreal.unregister_slate_pre_tick_callback(h)
        except Exception:
            pass
    pie = _end_play()
    _restore_env(job['saved_env'])
    sec = round(time.time() - job['t0'], 2)
    _write(job['done'], {'ok': ok, 'result': str(result)[:4000], 'sec': sec, 'mode': job['mode'], 'pie_stopped': pie,
                         'session_jobs': _state['jobs_done'] + 1, 'session_age_s': round(time.time() - _state['boot'], 1)})
    try:
        os.remove(job['running'])
    except OSError:
        pass
    unreal.log(f"[animpipe_session] job {job['id']} {'ok' if ok else 'FAILED'} in {sec} s")
    _state['job'] = None
    _state['jobs_done'] += 1


def _start(path):
    jid = os.path.basename(path)[:-5]
    running = path[:-5] + '.running'
    os.replace(path, running)
    with open(running, encoding='utf-8') as fh:
        spec = json.load(fh)
    mode = spec.get('mode', 'sync')
    job = {'id': jid, 'mode': mode, 'running': running, 'done': os.path.join(_JOBS, jid + '.done.json'), 't0': time.time(),
           'handles': [], 'pre_handles': [], 'quit': False, 'saved_env': _set_env(spec.get('env') or {})}
    script = spec['script'] if os.path.isabs(spec['script']) else os.path.join(_PROJ, spec['script'])
    unreal.log(f"[animpipe_session] job {jid} {mode} {spec['script']}")
    try:
        if mode == 'boot':
            proxy = _JobUnreal(job)
            real = sys.modules['unreal']
            sys.modules['unreal'] = proxy
            try:
                runpy.run_path(script, run_name='__main__')
            finally:
                sys.modules['unreal'] = real
            _state['job'] = job
            if job['quit']:                 # finished inside the boot itself
                _finish(job, True, 'quit during boot')
            return
        g = runpy.run_path(script, run_name='__main__' if not spec.get('entry') else 'animpipe_job')
        res = g[spec['entry']](**(spec.get('kwargs') or {})) if spec.get('entry') else 'OK ' + spec['script']
        _state['job'] = job
        _finish(job, True, res)
    except Exception:
        _state['job'] = job
        _finish(job, False, traceback.format_exc()[-3000:])


def _tick(dt):
    _state['ticks'] += 1
    if _state['ticks'] == 30:
        with open(os.path.join(_DIR, 'ready.txt'), 'w') as fh:
            fh.write(f'ready pid={os.getpid()} t={time.time():.0f}\n')
        unreal.log('[animpipe_session] ready')
    if _state['ticks'] % 600 == 0:
        unreal.log(f"[animpipe_session] alive, tick {_state['ticks']}")   # keeps the watched log growing (Run-Timed silent-log limit)
    if os.path.exists(_QUIT) and _state['job'] is None:
        unreal.log('[animpipe_session] quit requested')
        unreal.unregister_slate_post_tick_callback(_state['handle'])
        unreal.SystemLibrary.quit_editor()
        return
    job = _state['job']
    if job is not None:
        if job['quit']:
            _finish(job, True, 'QUIT_EDITOR intercepted: job ended, editor kept')
        return
    if _state['ticks'] > 30:
        pending = sorted(p for p in os.listdir(_JOBS) if p.endswith('.json') and not p.endswith('.done.json'))
        if pending:
            _start(os.path.join(_JOBS, pending[0]))


_state['handle'] = unreal.register_slate_post_tick_callback(_tick)
