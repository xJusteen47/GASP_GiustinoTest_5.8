# R3-1: M3 suite once (listen host + 2 clients, PIE restarted by the driver). Report: Saved/R31/report-<TAG>.txt
import os, sys, time, json
import unreal
sys.path.insert(0, os.path.join(unreal.Paths.project_dir(), 'Tools', 'GASNPPTest'))
import gasnpp_test as g
TAG = os.environ.get('R31_TAG', 'x')
NAMES = g.SUITE_DEFAULT[:g.SUITE_DEFAULT.index('slide_jump') + 1] + ['loadout_change']
# R31_ALL=1: the whole SUITE_DEFAULT in one PIE (the closing check of a slice)
if os.environ.get('R31_ALL'):
    NAMES = list(g.SUITE_DEFAULT)
if os.environ.get('R31_ONLY'):
    NAMES = os.environ['R31_ONLY'].split(',')
# R31_IMPORT: scenario modules to import first (comma list); a module that registers its scenarios on gasnpp_test (example_scenarios) can then
# run in this one-PIE suite
for _m in filter(None, os.environ.get('R31_IMPORT', '').split(',')):
    __import__(_m)
OUT = os.path.join(unreal.Paths.project_dir(), 'Saved', 'R31', f'report-{TAG}.txt')
# Without the folder the report write raised every tick and the editor never quit
os.makedirs(os.path.dirname(OUT), exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# The editor now starts on L_Test_Hub (owner, 2026-11-13); the suite is written for the harness level, so open it first.
# R31_MAP picks another level. Loading a level never saves the one that was open.
HARNESS_MAP = os.environ.get('R31_MAP', '/Game/Levels/DefaultLevel')
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0] != HARNESS_MAP:
    les.load_level(HARNESS_MAP)
st = {'t': time.time(), 'phase': 'start'}
def tick(_dt):
    try:
        el = time.time() - st['t']
        if st['phase'] == 'start' and el > 5:
            les.editor_request_begin_play(); st.update(phase='ready', t=time.time())
        elif st['phase'] == 'ready':
            ws = g.worlds()
            if len(ws) >= 3 and all(len(g.comps(w)) >= 3 for w in ws) and el > 8:
                st.update(phase='go', t=time.time())
            elif el > 90:
                open(OUT, 'w').write('pawns not ready'); unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR'); st['phase'] = 'dead'
        elif st['phase'] == 'go':
            g.net(60, 15, 0)
            r = g.run_suite(NAMES, timeout=240.0 if os.environ.get('R31_ALL') else 120.0)
            st['phase'] = 'wait'
            if not str(r).startswith('suite'): open(OUT, 'w').write(str(r)); st['phase'] = 'quit'
        elif st['phase'] == 'wait':
            if g._suite['handle'] is not None:
                return
            open(OUT, 'w').write(g.suite_report()); st['phase'] = 'quit'
        elif st['phase'] == 'quit':
            unreal.unregister_slate_post_tick_callback(st['h'])
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR'); st['phase'] = 'dead'
    except Exception as e:
        open(OUT, 'a').write(f'driver error {e}'); unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
g.setup_editor(True, os.environ.get('R31_MODE','listen'), 2)
st['h'] = unreal.register_slate_post_tick_callback(tick)
