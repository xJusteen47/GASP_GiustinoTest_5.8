# Unattended editor session for animpipe (dropkick-tb-4, 2026-11-21): the laptop threads have no owner at the editor, and animpipe's build,
# check and render talk to a RUNNING editor through UE_MCP_Bridge (ue.py). This script is the editor's -ExecCmds boot: it writes
# Saved/AnimPipeSession/ready.txt once the editor ticks, then waits; creating Saved/AnimPipeSession/quit.txt closes the editor (no save).
# Start it under the time limit (project rule 5), e.g.:
#   Run-Timed.ps1 -Preset Capture -Script Tools\AnimPipeline\editor_session.py -Log Saved\Logs\ci\animpipe_session.log -MaxMinutes 90 -SilentMinutes 30
# then run `python -m animpipe <verb> <move>` from Tools/AnimPipeline as usual, then: echo > Saved/AnimPipeSession/quit.txt
import os

import unreal

_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'AnimPipeSession')
os.makedirs(_DIR, exist_ok=True)
_QUIT = os.path.join(_DIR, 'quit.txt')
if os.path.exists(_QUIT):
    os.remove(_QUIT)
_state = {'ticks': 0, 'handle': None}


def _tick(dt):
    _state['ticks'] += 1
    if _state['ticks'] == 30:
        with open(os.path.join(_DIR, 'ready.txt'), 'w') as fh:
            fh.write('ready\n')
        unreal.log('[animpipe_session] ready')
    if _state['ticks'] % 600 == 0:
        unreal.log(f"[animpipe_session] alive, tick {_state['ticks']}")   # keeps the watched log growing (Run-Timed silent-log limit)
    if os.path.exists(_QUIT):
        unreal.log('[animpipe_session] quit requested')
        unreal.unregister_slate_post_tick_callback(_state['handle'])
        unreal.SystemLibrary.quit_editor()


_state['handle'] = unreal.register_slate_post_tick_callback(_tick)
