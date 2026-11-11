# Unattended batch of fresh-PIE scenarios of one module, then QUIT_EDITOR (M11 K2). Launch:
#   UnrealEditor.exe <uproject> -ExecCmds="py <this file>" -unattended -nullrhi   with the environment:
#   BATCH_MODULE (kit_scenarios), BATCH_NAMES (comma list, default the module's SUITE_KIT), BATCH_MODE (listen | dedicated),
#   BATCH_LOSS (percent, 0), BATCH_REPORT (path under Saved/Logs). The report's first line is the UTC time and the net settings.
import importlib, os, sys, time
import unreal
sys.path.insert(0, os.path.join(unreal.Paths.project_dir(), 'Tools', 'GASNPPTest'))
import gasnpp_test as g
import defence_scenarios as ds

MODULE = importlib.import_module(os.environ.get('BATCH_MODULE', 'kit_scenarios'))
NAMES = [n for n in os.environ.get('BATCH_NAMES', '').split(',') if n] or list(getattr(MODULE, 'SUITE_KIT', []))
REPORT = os.environ.get('BATCH_REPORT', 'k2/batch.log')
st = {'t': time.time(), 'phase': 'start'}


def tick(_dt):
    try:
        if st['phase'] == 'start' and time.time() - st['t'] > 5:
            ds.defence_batch(NAMES, loss=int(os.environ.get('BATCH_LOSS', '0')), report=REPORT, module=MODULE)
            st['phase'] = 'wait'
        elif st['phase'] == 'wait' and ds._batch['handle'] is None:
            st['phase'] = 'quit'
            unreal.unregister_slate_post_tick_callback(st['h'])
            g.setup_editor(False)
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception as e:
        path = os.path.join(unreal.Paths.project_saved_dir(), 'Logs', REPORT)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, 'a').write(f'driver error {e}\n')
        unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')


g.setup_editor(True, os.environ.get('BATCH_MODE', 'listen'), 2)
st['h'] = unreal.register_slate_post_tick_callback(tick)
