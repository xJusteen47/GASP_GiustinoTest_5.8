# Rollback plan F0 measurement (Plugins/GSTNWorldSim). Boot driver for Tools/CI/Run-Timed.ps1 -Preset Capture -Script <copy of this file>
# (copy it to a path without spaces: -ExecCmds does not take a quoted path).
#
# Starts PIE on the startup map as listen server + 2 clients, Run Under One Process (the PIE ini of the worktree must say so: the
# LevelEditorPlaySettings are also set here, best effort). gstn.WorldSim.Spawn 1 and gstn.WorldSim.LogEverySeconds are set before
# the world begins play; np.PrintReconciles 1 and np.LogReconcileField 1 so every reconcile is in the log with its model's states.
# After a warm-up with network emulation on (WSF0_LAG ms per direction, default 60), it runs every configuration of WSF0_RUNS
# ("label:entities:traces:parallel", comma separated). Each one: set the cvars, let the input reach the clients, ResetStats, a steady
# phase (WSF0_STEADY s, no forcing) closed by gstn.WorldSim.Report, then ResetStats and a forced phase (WSF0_FORCED s, np.ForceReconcile
# 1 every WSF0_FORCE_EVERY s) closed by another Report. Every phase is bracketed by "WSF0 BEGIN <label> <phase>" / "WSF0 END ..." lines,
# which Tools/GASNPPTest/worldsim_f0_parse.py reads. gstn.SimQuery.Hash 1 from the start; gstn.SimQuery.Dump 120 after each run
# (between "WSF0 DUMP BEGIN/END <label>"), to compare the per-frame query hashes of host and clients. Ends PIE, logs ESITO and quits the editor.
import os, runpy, time
import unreal

_here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools', 'GASNPPTest')
try:
    runpy.run_path(os.path.join(_here, 'vd2_mover.py'), run_name='vd2_mover')
except Exception as e:
    print('vd2_mover failed:', e)

LAG = int(os.environ.get('WSF0_LAG', '60'))
STEADY = float(os.environ.get('WSF0_STEADY', '20'))
FORCED = float(os.environ.get('WSF0_FORCED', '40'))
FORCE_EVERY = float(os.environ.get('WSF0_FORCE_EVERY', '0.5'))
RUNS = [r.split(':') for r in os.environ.get('WSF0_RUNS', 'a:0:0:0,b:8:8:0,c:40:40:0,d:40:40:1').split(',')]


def log(msg):
    unreal.log('WSF0 ' + msg)


def worlds():
    return list(unreal.EditorLevelLibrary.get_pie_worlds(True))


def cmd(c, world=None):
    unreal.SystemLibrary.execute_console_command(world, c)


s = unreal.get_default_object(unreal.LevelEditorPlaySettings)
for k, v in (('play_net_mode', unreal.PlayNetMode.PIE_LISTEN_SERVER), ('play_number_of_clients', 3),
             ('play_net_dedicated', False), ('run_under_one_process', True)):
    try:
        s.set_editor_property(k, v)
        log(f'set {k} {v}')
    except Exception as e:
        log(f'could not set {k}: {e}')
cmd('set /Script/UnrealEd.EditorPerformanceSettings bThrottleCPUWhenNotForeground 0')

# Before begin play: the subsystem spawns the entity at OnWorldBeginPlay. Global cvars, so every PIE world of the process sees them
for c in ('gstn.WorldSim.Spawn 1', 'gstn.WorldSim.LogEverySeconds 5', 'gstn.SimQuery.Hash 1', 'np.PrintReconciles 1', 'np.LogReconcileField 1',
          'np.ForceReconcileExtraFrames 0', 'gstn.WorldSim.Entities 0', 'gstn.WorldSim.Traces 0', 'gstn.WorldSim.Parallel 0'):
    cmd(c)
    log('pre-play ' + c)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
st = {'t0': time.time(), 'phase': 'boot', 'run': 0, 'last_force': 0.0, 'forced_sets': 0, 'esito': 'FAIL'}


def worldsim_counts():
    out = []
    cls = getattr(unreal, 'GSTN_WorldSimActor', None)
    for w in worlds():
        n = len(unreal.GameplayStatics.get_all_actors_of_class(w, cls)) if cls else -1
        out.append(f'{w.get_name()}={n}')
    return out


def enter(phase):
    st['phase'] = phase
    st['tp'] = time.time()


def elapsed():
    return time.time() - st['tp']


def finish(esito):
    st['esito'] = esito
    log('ESITO pending ' + esito)
    try:
        les.editor_request_end_play()
    except Exception as e:
        log(f'end play failed: {e}')
    enter('quit')


def tick(dt):
    try:
        _tick()
    except Exception as e:
        log(f'driver error in {st["phase"]}: {e}')
        finish('FAIL')


def _tick():
    t = time.time() - st['t0']
    ph = st['phase']
    if ph == 'boot' and t > 8:
        les.editor_request_begin_play()
        log('PIE requested')
        enter('wait_pie')
    elif ph == 'wait_pie':
        ws = worlds()
        if len(ws) >= 3 and elapsed() > 6:
            counts = worldsim_counts()
            log('PIE worlds ' + ' '.join(counts))
            for w in ws:
                for c in (f'NetEmulation.PktLag {LAG}', 'NetEmulation.PktLagVariance 0', 'NetEmulation.PktLoss 0', 't.IdleWhenNotForeground 0'):
                    cmd(c, w)
            log(f'net emulation PktLag {LAG} on {len(ws)} worlds (per direction)')
            enter('warm')
        elif elapsed() > 60:
            log(f'PIE not ready: {len(ws)} worlds')
            finish('FAIL')
    elif ph == 'warm' and elapsed() > 12:
        start_run()
    elif ph == 'settle' and elapsed() > 3:
        label = RUNS[st['run']][0]
        cmd('gstn.WorldSim.ResetStats')
        log(f'BEGIN {label} steady')
        enter('steady')
    elif ph == 'steady' and elapsed() > STEADY:
        label = RUNS[st['run']][0]
        cmd('gstn.WorldSim.Report')
        log(f'END {label} steady')
        cmd('gstn.WorldSim.ResetStats')
        st['forced_sets'] = 0
        log(f'BEGIN {label} forced')
        enter('forced')
    elif ph == 'forced':
        now = time.time()
        if now - st['last_force'] >= FORCE_EVERY:
            cmd('np.ForceReconcile 1')
            st['last_force'] = now
            st['forced_sets'] += 1
        if elapsed() > FORCED:
            label = RUNS[st['run']][0]
            cmd('np.ForceReconcile 0')
            cmd('gstn.WorldSim.Report')
            log(f'END {label} forced (np.ForceReconcile set {st["forced_sets"]} times)')
            log(f'DUMP BEGIN {label}')
            cmd('gstn.SimQuery.Dump 120')
            log(f'DUMP END {label}')
            st['run'] += 1
            if st['run'] < len(RUNS):
                start_run()
            else:
                log('all runs done')
                finish('OK')
    elif ph == 'quit' and elapsed() > 4:
        for c in ('gstn.WorldSim.Spawn 0', 'gstn.WorldSim.LogEverySeconds 0',
                  'set /Script/UnrealEd.EditorPerformanceSettings bThrottleCPUWhenNotForeground 1'):
            cmd(c)
        unreal.log('ESITO: ' + st['esito'])
        unreal.unregister_slate_post_tick_callback(_h)
        cmd('QUIT_EDITOR')


def start_run():
    label, ents, traces, par = RUNS[st['run']]
    for c in (f'gstn.WorldSim.Entities {ents}', f'gstn.WorldSim.Traces {traces}', f'gstn.WorldSim.Parallel {par}'):
        cmd(c)
    log(f'config {label}: Entities {ents} Traces {traces} Parallel {par}')
    enter('settle')


_h = unreal.register_slate_post_tick_callback(tick)
log(f'driver loaded: lag {LAG} steady {STEADY}s forced {FORCED}s every {FORCE_EVERY}s runs {RUNS}')
