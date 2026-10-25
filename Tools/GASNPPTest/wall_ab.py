# Wall slide A/B driver (wall-slide-v2.md section 4, W6): runs every lane of the map /Game/Levels/L_WallSlideAB with the legacy slide
# ("WallSliding") and the v2 slide ("WallSlidingV2") through the PIE network harness (gasnpp_test.py) and writes one summary table,
# Saved/Metrics/wallslide-ab-summary.md, plus the per-run CSVs of gstn.Wall.Metrics next to it.
#
# Two ways to run it:
#
#  1. Headless, from a shell (editor closed). This script starts the editor, drives it and restores the ini file it patched:
#       python Tools/GASNPPTest/wall_ab.py                       all lanes, both modes
#       python Tools/GASNPPTest/wall_ab.py --lanes "^(a|b)_"     only lanes whose id matches the regex (ids: a_flat, b_convex90, d_bump5_down, ...)
#       python Tools/GASNPPTest/wall_ab.py --modes WallSlidingV2 --seconds 4 --lag 60 --var 15 --loss 0
#     The editor is started as in wall-slide-v2.md 6.10: listen host + 1 client in one process, PlayNumberOfClients patched in
#     Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini for the run (a backup is restored at the end, also on errors).
#
#  2. In an open editor with the map loaded (Python console):
#       import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import wall_ab; wall_ab.start(lanes='^b_', modes=('WallSlidingV2',))
#     Start PIE yourself first as listen server with 2 players (host + 1 client) and the test game mode (gasnpp_test.setup_editor(True,
#     'listen', 1, pawn='SandboxCharacter_Mover')); start() then only drives the runs. Read the table when it logs "wall_ab done".
#
# What a run does (per lane and mode): sets the entry mode of the pawn's Falling transitions in memory (nothing is saved, assets are
# not touched), teleports the client's pawn to the lane's start marker, launches it into the wall with the marker's gstn.Wall.Launch
# velocity and its stick (gstn.Wall.DebugInput), records gstn.Wall.Metrics for --seconds, then reads the CSV and the reconcile lines of
# the editor log. The host's own pawn is parked away from the lanes. The legacy modes are not query-instrumented: their query columns
# read "-".
import os, sys, re, json, time, glob, subprocess, shutil, statistics, datetime

try:
    import unreal
except ImportError:
    unreal = None

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(HERE, '..', '..'))
UPROJECT = os.path.join(PROJECT_DIR, 'GASP_Giustino2.uproject')
MAP = '/Game/Levels/L_WallSlideAB'
METRICS_DIR = os.path.join(PROJECT_DIR, 'Saved', 'Metrics')
SUMMARY = os.path.join(METRICS_DIR, 'wallslide-ab-summary.md')
INI = os.path.join(PROJECT_DIR, 'Saved', 'Config', 'WindowsEditor', 'EditorPerProjectUserSettings.ini')
ENGINE = r'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
MODES = ('WallSliding', 'WallSlidingV2')
# Distance from the start marker (along the lane, cm) to the feature of lanes with an edge, as built by build_wall_slide_ab_map.py
FEATURE_DY = 300.0
FEATURE_GROUPS = ('b', 'c', 'd', 'e', 'f', 'g')

# ------------------------------------------------------------------------------------------------ analysis (pure Python)


def parse_csv(path):
    """Rows of a wallslide-<mode>-<stamp>.csv as dicts (numbers converted)."""
    rows = []
    with open(path, encoding='utf-8') as f:
        header = f.readline().strip().split(',')
        for line in f:
            parts = line.rstrip('\n').split(',')
            if len(parts) < len(header):
                continue
            r = dict(zip(header, parts))
            for k in ('time', 'speed', 'hspeed', 'vspeed', 'dnormal_deg', 'x', 'y', 'z'):
                r[k] = float(r[k])
            r['queries'] = int(r['queries'])
            r['events'] = set(e for e in r['events'].split('|') if e)
            rows.append(r)
    return rows


def dedupe(rows):
    """One row per (source, time): a re-simulated tick replaces the first simulation of it."""
    by = {}
    for r in rows:
        by[(r['source'], round(r['time'], 3))] = r
    return sorted(by.values(), key=lambda r: r['time'])


def _mean(vals):
    return statistics.fmean(vals) if vals else None


def summarize(rows, marker_y=None, has_feature=False, instrumented=True):
    """The numbers of one run (rows of one source, in time order)."""
    out = {'ticks': len(rows), 'grab': False}
    if not rows:
        return out
    out['grab'] = any('grab' in r['events'] for r in rows)
    out['wall_lost'] = sum(1 for r in rows if 'wall-lost' in r['events'])
    out['exits'] = sum(1 for r in rows if 'exit' in r['events'])
    out['switches'] = sum(1 for r in rows if 'mode-switch' in r['events'])
    out['duration'] = rows[-1]['time'] - rows[0]['time']
    out['speed_first'] = rows[0]['speed']
    out['speed_min'] = min(r['speed'] for r in rows)
    out['speed_last'] = rows[-1]['speed']
    out['max_dnormal'] = max(r['dnormal_deg'] for r in rows)
    if instrumented:
        q = [r['queries'] for r in rows]
        out['q_avg'] = sum(q) / len(q)
        out['q_max'] = max(q)
    if has_feature and marker_y is not None:
        fy = marker_y + FEATURE_DY
        win = lambda lo, hi: [r['speed'] for r in rows if lo <= r['y'] <= hi]
        out['before'] = _mean(win(fy - 200, fy - 80))
        at = win(fy - 40, fy + 40)
        out['at'] = min(at) if at else None
        out['after'] = _mean(win(fy + 80, fy + 200))
        t_in = next((r['time'] for r in rows if r['y'] >= fy - 100), None)
        t_out = next((r['time'] for r in rows if r['y'] >= fy + 100), None)
        out['cross'] = (t_out - t_in) if (t_in is not None and t_out is not None) else None
    return out


def fmt(v, digits=0, dash='-'):
    return dash if v is None else f'{v:.{digits}f}'


def table(results):
    """results: list of dicts {lane, mode, summary, recon}. Markdown, one row per lane and mode."""
    lines = ['| lane | mode | grab | speed before / at / after (cm/s) | cross (s) | queries avg / max | wall lost | exits | switches | reconciles (all / unexplained / after 0.6 s) |',
             '|---|---|---|---|---|---|---|---|---|---|']
    for r in results:
        s, rec = r['summary'], r['recon']
        if not s.get('grab'):
            edge = '-'
        elif 'before' in s:
            edge = f"{fmt(s['before'])} / {fmt(s['at'])} / {fmt(s['after'])}"
        else:
            edge = f"{fmt(s['speed_first'])} first, {fmt(s['speed_min'])} min, {fmt(s['speed_last'])} last"
        q = f"{fmt(s.get('q_avg'), 2)} / {fmt(s.get('q_max'))}" if 'q_avg' in s else '-'
        lines.append(f"| {r['lane']} | {r['mode']} | {'yes' if s.get('grab') else 'NO'} | {edge} | {fmt(s.get('cross'), 2)} | {q} | "
                     f"{s.get('wall_lost', '-')} | {s.get('exits', '-')} | {s.get('switches', '-')} | {rec.get('mismatches', '-')} / {rec.get('unexplained', '-')} / {rec.get('late', '-')} |")
    return '\n'.join(lines)


RESULTS_JSON = os.path.join(METRICS_DIR, 'wallslide-ab-results.json')


def merge_results(results):
    """Keeps the rows of earlier runs for the (lane, mode) pairs this run did not repeat, so a partial rerun refreshes the full table."""
    merged = {}
    try:
        with open(RESULTS_JSON, encoding='utf-8') as f:
            for r in json.load(f):
                merged[(r['lane'], r['mode'])] = r
    except (OSError, ValueError):
        pass
    for r in results:
        merged[(r['lane'], r['mode'])] = {k: r[k] for k in ('lane', 'mode', 'summary', 'recon')}
    with open(RESULTS_JSON, 'w', encoding='utf-8') as f:
        json.dump(list(merged.values()), f, indent=1, default=list)
    return list(merged.values())


def write_summary(results, meta):
    os.makedirs(METRICS_DIR, exist_ok=True)
    results = merge_results(results)
    head = [f"# Wall slide A/B summary ({meta.get('when', '')})", '',
            f"Net: {meta.get('net', '')}. Seconds per run: {meta.get('seconds', '')}. Subject: the client's pawn (prediction side); rows de-duplicated per tick.",
            'Speeds are the total speed along the lane run; "before" = 200..80 cm before the feature, "at" = minimum within 40 cm of it, "after" = 80..200 cm past it',
            '(lanes with a feature only; otherwise first / minimum / last speed of the run). Legacy modes are not query-instrumented. Reconciles are the',
            'Network Prediction "Sync/Aux mismatch" lines of the client during the run (unexplained = not on a time-offset resync; "after 0.6 s" leaves out the',
            'first 0.6 s, where the teleport and the launch, queued on both the host and the client copy, make their own correction).', '']
    with open(SUMMARY, 'w', encoding='utf-8') as f:
        f.write('\n'.join(head) + table(results) + '\n')
    return SUMMARY


# ------------------------------------------------------------------------------------------------ in-editor driver


def _log(msg):
    unreal.log(f'wall_ab: {msg}')


class Driver:
    def __init__(self, opts):
        self.opts = opts
        self.results = []
        self.handle = None
        self.gen = None
        self.wake = 0.0
        self.done = False
        self.error = None

    # -- helpers
    def worlds(self):
        return list(unreal.EditorLevelLibrary.get_pie_worlds(True))

    def cmd(self, text, world=None):
        w = world if world is not None else self.worlds()[0]
        unreal.SystemLibrary.execute_console_command(w, text)

    def pawn_of(self, world_index):
        pc = unreal.GameplayStatics.get_player_controller(self.worlds()[world_index], 0)
        return pc, (pc.get_controlled_pawn() if pc else None)

    def player_id(self, world_index):
        _, pawn = self.pawn_of(world_index)
        ps = pawn.get_editor_property('player_state')
        return ps.get_editor_property('player_id')

    def markers(self):
        out = {}
        for a in unreal.GameplayStatics.get_all_actors_with_tag(self.worlds()[0], 'WallABLane'):
            tags = [str(t) for t in a.tags]
            info = {'loc': a.get_actor_location(), 'yaw': a.get_actor_rotation().yaw}
            for t in tags:
                if t.startswith('lane:'):
                    info['id'] = t[5:]
                elif t.startswith('group:'):
                    info['group'] = t[6:]
                elif t.startswith('launch:'):
                    info['launch'] = [float(v) for v in t[7:].split(',')]
                elif t.startswith('stick:'):
                    info['stick'] = [float(v) for v in t[6:].split(',')]
            out[info['id']] = info
        return out

    def set_entry_mode(self, mode):
        """Points the Falling mode's wall entry transition(s) at `mode`, in memory, on every copy of the pawn (every PIE world)."""
        n = 0
        for w in self.worlds():
            for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Pawn):
                mover = a.get_component_by_class(unreal.MoverComponent)
                falling = mover.get_editor_property('movement_modes').get('Falling') if mover else None
                if not falling:
                    continue
                for tr in falling.get_editor_property('transitions'):
                    if isinstance(tr, (unreal.GSTN_WallSurfaceModeTransition, unreal.GSTN_AbilityWallSurfaceModeTransition)):
                        lst = tr.get_editor_property('surface_modes')
                        lst.set_editor_property('unknown_surface_mode_name', mode)
                        tr.set_editor_property('surface_modes', lst)
                        n += 1
                    elif isinstance(tr, unreal.GSTN_WallEntryTransition):
                        tr.set_editor_property('target_mode_name', mode)
                        n += 1
        return n

    def newest_csv(self, mode, since):
        files = [f for f in glob.glob(os.path.join(METRICS_DIR, f'wallslide-{mode}-*.csv')) if os.path.getmtime(f) >= since]
        return max(files, key=os.path.getmtime) if files else None

    # -- the run, as a coroutine: every `yield seconds` waits that long on the editor tick
    def script(self):
        o = self.opts
        yield 2.0
        if o.get('start_pie', True):
            import gasnpp_test as t
            t.setup_editor(True, 'listen', 1, pawn='SandboxCharacter_Mover')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            deadline = time.time() + 180
            while True:
                yield 1.0
                ws = self.worlds()
                if len(ws) >= 2 and all(unreal.GameplayStatics.get_player_controller(w, 0) and unreal.GameplayStatics.get_player_controller(w, 0).get_controlled_pawn() for w in ws):
                    break
                if time.time() > deadline:
                    raise RuntimeError('PIE did not come up with two players with pawns')
            yield 3.0
        import gasnpp_test as t
        t.net(o['lag'], o['var'], o['loss'])
        self.cmd('np.PrintReconciles 1')
        markers = self.markers()
        lanes = [l for l in sorted(markers, key=lambda k: (markers[k]['loc'].y, k)) if re.search(o['lanes'], l)]
        _log(f'{len(markers)} markers, {len(lanes)} lanes selected, modes {o["modes"]}')
        pid = self.player_id(1)
        host_pid = self.player_id(0)
        self.cmd(f'gstn.Wall.Teleport -1500 -1000 100 0 {host_pid}')
        for lane in lanes:
            m = markers[lane]
            for mode in o['modes']:
                if mode != 'WallSliding' and m['group'] == 'x':
                    continue
                if mode == 'WallSliding' and m['group'] in ('j',):
                    continue   # surface-authored lanes only mean something to the surface-driven v2 modes
                changed = self.set_entry_mode(mode)
                yield 0.3
                stick = m.get('stick', [0.0, 0.0])
                self.cmd('gstn.Wall.DebugInput ""')
                self.cmd('gstn.Wall.ResetMemory')
                self.cmd('gstn.Wall.QueryStats reset')
                self.cmd('gstn.Wall.CountQueries 1')
                t._set_yaw(1, m['yaw'])
                # Park on the floor in front of the lane first, so the previous run's wall mode (and its timers) is left before the grab
                self.cmd(f'gstn.Wall.Teleport {m["loc"].x - 150} {m["loc"].y} 120 {m["yaw"]} {pid}')
                yield 1.2
                mark = t.mark_log()
                started = time.time() - 1.0
                self.cmd('gstn.Wall.Metrics 0')
                self.cmd('gstn.Wall.Metrics 1')
                self.cmd(f'gstn.Wall.DebugInput "stick {stick[0]} {stick[1]}"')
                L = m['launch']
                self.cmd(f'gstn.Wall.Teleport {m["loc"].x} {m["loc"].y} {m["loc"].z} {m["yaw"]} {pid}')
                self.cmd(f'gstn.Wall.Launch {L[0]} {L[1]} {L[2]} 0 {pid}')
                yield 0.6
                late_mark = t.mark_log()
                yield max(o['seconds'] - 0.6, 0.5)
                _, cpawn = self.pawn_of(1)
                end_mode = str(cpawn.get_component_by_class(unreal.MoverComponent).get_movement_mode_name())
                end_loc = cpawn.get_actor_location()
                self.cmd('gstn.Wall.Metrics 0')
                recon = t.report_reconciles(mark)
                recon['late'] = t.report_reconciles(late_mark)['mismatches']
                self.cmd('gstn.Wall.DebugInput ""')
                self.cmd('gstn.Wall.CountQueries 0')
                rows_all, csv_path = [], None
                for _ in range(10):
                    csv_path = self.newest_csv(mode, started)
                    if csv_path:
                        break
                    yield 0.3
                if csv_path:
                    rows_all = parse_csv(csv_path)
                if mode == 'WallSlidingV2':
                    # The surface-driven entry may hand the pawn to the climb mode (its own file): same run, same subject
                    climb_csv = self.newest_csv('WallClimbingV2', started)
                    if climb_csv:
                        rows_all += parse_csv(climb_csv)
                client_rows = dedupe([r for r in rows_all if r['source'].endswith('@client')])
                summary = summarize(client_rows, marker_y=m['loc'].y, has_feature=m['group'] in FEATURE_GROUPS and not lane.endswith('_down'),
                                    instrumented=(mode != 'WallSliding'))
                _log(f'{lane} {mode}: ' + json.dumps({k: (round(v, 2) if isinstance(v, float) else v) for k, v in summary.items()}) + f' recon={recon.get("mismatches")}/{recon.get("unexplained")}/{recon.get("late")} entry_transitions={changed} end_mode={end_mode} end_loc=({end_loc.x:.0f},{end_loc.y:.0f},{end_loc.z:.0f})')
                self.results.append({'lane': lane, 'mode': mode, 'summary': summary, 'recon': recon, 'csv': csv_path})
        self.cmd(f'gstn.Wall.Teleport -1500 -1000 100 0 {pid}')

    # -- plumbing
    def tick(self, _dt):
        if self.done or time.time() < self.wake:
            return
        try:
            self.wake = time.time() + next(self.gen)
        except StopIteration:
            self.finish()
        except Exception as e:
            import traceback
            self.error = f'{e}\n{traceback.format_exc()}'
            _log('ERROR ' + self.error)
            self.finish()

    def finish(self):
        self.done = True
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        meta = {'when': datetime.datetime.now().strftime('%Y-%m-%d %H:%M'), 'seconds': self.opts['seconds'],
                'net': f"{self.opts['lag']} ms lag, {self.opts['var']} ms jitter, {self.opts['loss']} % loss, listen host + 1 client"}
        if self.results:
            _log('wall_ab done: ' + write_summary(self.results, meta))
        else:
            _log('wall_ab done: no results' + (f' ({self.error})' if self.error else ''))
        try:
            with open(os.path.join(METRICS_DIR, 'wall_ab.done'), 'w') as f:
                f.write(self.error or 'ok')
        except OSError:
            pass
        if self.opts.get('quit', False):
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')

    def run(self):
        os.makedirs(METRICS_DIR, exist_ok=True)
        done = os.path.join(METRICS_DIR, 'wall_ab.done')
        if os.path.exists(done):
            os.remove(done)
        self.gen = self.script()
        self.handle = unreal.register_slate_post_tick_callback(self.tick)


_driver = None


def start(lanes='.', modes=MODES, seconds=3.5, lag=60, var=15, loss=0, start_pie=False, quit=False):
    """Starts the run on the editor tick and returns at once; the table is written when it finishes (log line 'wall_ab done')."""
    global _driver
    _driver = Driver({'lanes': lanes, 'modes': tuple(modes), 'seconds': seconds, 'lag': lag, 'var': var, 'loss': loss, 'start_pie': start_pie, 'quit': quit})
    _driver.run()
    return _driver


# ------------------------------------------------------------------------------------------------ launcher (plain Python)


def launch(opts):
    """Starts the editor with the driver and waits for it. Patches PlayNumberOfClients in the user's PIE settings for the run only."""
    backup = INI + '.wall_ab.bak'
    original = open(INI, encoding='utf-8').read() if os.path.exists(INI) else None
    if original is not None:
        shutil.copy2(INI, backup)
    try:
        text = original or ''
        patch = {'PlayNetMode': 'PIE_ListenServer', 'RunUnderOneProcess': 'True', 'PlayNumberOfClients': '2'}
        if '[/Script/UnrealEd.LevelEditorPlaySettings]' not in text:
            text += '\n[/Script/UnrealEd.LevelEditorPlaySettings]\n'
        for k, v in patch.items():
            if re.search(rf'^{k}=', text, re.M):
                text = re.sub(rf'^{k}=.*$', f'{k}={v}', text, flags=re.M)
            else:
                text = text.replace('[/Script/UnrealEd.LevelEditorPlaySettings]', f'[/Script/UnrealEd.LevelEditorPlaySettings]\n{k}={v}', 1)
        os.makedirs(os.path.dirname(INI), exist_ok=True)
        open(INI, 'w', encoding='utf-8').write(text)

        os.makedirs(METRICS_DIR, exist_ok=True)
        done = os.path.join(METRICS_DIR, 'wall_ab.done')
        if os.path.exists(done):
            os.remove(done)
        log = os.path.join(METRICS_DIR, 'wall_ab-editor.log')
        driver_script = os.path.join(METRICS_DIR, '_wall_ab_run.py')
        with open(driver_script, 'w', encoding='utf-8') as f:
            f.write("import sys\nsys.path.insert(0, r'%s')\nimport wall_ab\nwall_ab.start(**%r)\n" % (HERE, dict(
                lanes=opts['lanes'], modes=tuple(opts['modes']), seconds=opts['seconds'], lag=opts['lag'], var=opts['var'], loss=opts['loss'], start_pie=True, quit=True)))
        # The project path has a space and UE's -ExecCmds parser stops at an inner quote: the script path travels in the environment
        env = dict(os.environ, WALL_AB_BOOT=driver_script.replace(os.sep, '/'))
        args = [ENGINE, UPROJECT, MAP, "-ExecCmds=py import os;exec(open(os.environ['WALL_AB_BOOT']).read())", '-unattended', '-nullrhi', '-nosplash', '-nosound', '-NoP4', f'-abslog={log}']
        print('starting editor:', ' '.join(args))
        proc = subprocess.Popen(args, env=env)
        try:
            proc.wait(timeout=opts['timeout'])
        except subprocess.TimeoutExpired:
            print('timeout: killing the editor this script started')
            proc.kill()
    finally:
        if original is not None:
            shutil.copy2(backup, INI)
            os.remove(backup)
        elif os.path.exists(INI):
            pass
    if os.path.exists(SUMMARY):
        print(open(SUMMARY, encoding='utf-8').read())
    else:
        print('no summary written; see', os.path.join(METRICS_DIR, 'wall_ab-editor.log'))


if __name__ == '__main__':
    if unreal is None:
        import argparse
        p = argparse.ArgumentParser(description='Wall slide A/B run (headless editor).')
        p.add_argument('--lanes', default='.', help='regex on lane ids')
        p.add_argument('--modes', nargs='+', default=list(MODES))
        p.add_argument('--seconds', type=float, default=3.5)
        p.add_argument('--lag', type=int, default=60)
        p.add_argument('--var', type=int, default=15)
        p.add_argument('--loss', type=int, default=0)
        p.add_argument('--timeout', type=int, default=3600, help='seconds before the editor is killed')
        launch(vars(p.parse_args()))
