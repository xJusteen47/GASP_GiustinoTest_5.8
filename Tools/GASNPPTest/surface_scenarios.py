# Traversal surface scenarios (traversal-surfaces-plan.md T3, T9): PIE network runs against enemy platform shapes (GSTNPlatform).
#
#   scenario_surface_wallslide_enemy: the scripted test fighter (AGSTN_PlatformTestFighter: a WallSlide flank and a Stand top, on rails at
#   300 cm/s) is spawned on the server by the driver (gstn.Platform.SpawnTestFighter, no map edit). For the client's pawn and then the
#   host's: launch at the flank, slide on it, jump off; then land on the top, ride it, jump off. Pass: the grab happens, 0 reconciles while
#   on the flank and while standing (after the landing settles), <= 1 around the grab.
#
# Two ways to run it:
#
#  1. Headless, from a shell (editor closed). This script starts the editor on the A/B map (any map with a floor and GM_GASNPPTest works),
#     drives it and restores the ini file it patched:
#       python Tools/GASNPPTest/surface_scenarios.py
#       python Tools/GASNPPTest/surface_scenarios.py --lag 60 --var 15 --loss 2 --subjects client
#     As wall-slide-v2.md 6.10: listen host + 1 client in one process, PlayNumberOfClients patched in
#     Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini for the run (a backup is restored at the end, also on errors).
#
#  2. In an open editor (Python console), PIE running as listen server with 2 players and the test game mode
#     (gasnpp_test.setup_editor(True, 'listen', 1, pawn='SandboxCharacter_Mover')):
#       import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import surface_scenarios; surface_scenarios.start()
#     Read the table when the log says "surface_scenarios done".
#
# Needs the GSTNPlatform plugin enabled (the .uproject of this branch does) and the pawn's Falling transitions surface driven (the wall slide v2
# setup of SandboxCharacter_Mover): the flank resolves to Surface.Behaviour.WallSlide through the collider's provider.
import os, sys, re, json, time, subprocess, shutil, datetime

try:
    import unreal
except ImportError:
    unreal = None

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(HERE, '..', '..'))
UPROJECT = os.path.join(PROJECT_DIR, 'GASP_Giustino2.uproject')
MAP = '/Game/Levels/L_WallSlideAB'
METRICS_DIR = os.path.join(PROJECT_DIR, 'Saved', 'Metrics')
SUMMARY = os.path.join(METRICS_DIR, 'surface-scenarios-summary.md')
INI = os.path.join(PROJECT_DIR, 'Saved', 'Config', 'WindowsEditor', 'EditorPerProjectUserSettings.ini')
ENGINE = r'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'

# Where the fighter is spawned on the A/B map's floor (x -3000..1000), far from the lanes' walls (x >= -50) and from the parked pawns
FIGHTER_AT = (-2000.0, 1500.0, 0.0)
FLANK_FACE_DX = -175.0      # the flank's -X face, in the fighter's frame (AGSTN_PlatformTestFighter)
TOP_Z = 580.0               # the top's upper face
WALL_MODES = ('WallSlidingV2', 'WallClimbingV2', 'WallSliding')
PARK = {0: (-1500.0, -1000.0, 100.0), 1: (-1500.0, -600.0, 100.0)}


def _recon(t, mark):
    """(mismatches, unexplained) after `mark`: unexplained leaves out the mismatches logged on the frame of an input offset change (NP adjusting the
    client's input buffer to the jitter, ReconcileFromInputOffset), which are not prediction errors"""
    r = t.report_reconciles(mark)
    return [r['mismatches'], r['unexplained']]


def _log(msg):
    if unreal:
        unreal.log('surface_scenarios: ' + msg)
    else:
        print(msg)


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
        return pawn.get_editor_property('player_state').get_editor_property('player_id')

    def pawn_by_id(self, world, pid):
        """The copy of player `pid`'s pawn in `world` (the server copy in world 0, the proxy or the local pawn in a client world)."""
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
            ps = a.get_editor_property('player_state')
            if ps and ps.get_editor_property('player_id') == pid:
                return a
        return None

    def mode_of(self, pawn):
        mover = pawn.get_component_by_class(unreal.MoverComponent) if pawn else None
        return str(mover.get_movement_mode_name()) if mover else '?'

    def base_of(self, pawn):
        mover = pawn.get_component_by_class(unreal.MoverComponent) if pawn else None
        for name in ('get_movement_base',):
            try:
                base = getattr(mover, name)()
                return base.get_name() if base else 'None'
            except Exception:
                pass
        return '?'

    def fighter(self, world_index):
        found = unreal.GameplayStatics.get_all_actors_of_class(self.worlds()[world_index], unreal.GSTN_PlatformTestFighter)
        return found[0] if found else None

    def set_entry_mode(self, mode):
        """As wall_ab.set_entry_mode: points the Falling mode's wall entry transitions at `mode` for unmarked surfaces, in memory, on every pawn copy."""
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

    # -- the scenario, as a coroutine: every `yield seconds` waits that long on the editor tick
    def script(self):
        o = self.opts
        import gasnpp_test as t
        yield 2.0
        if o.get('start_pie', True):
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
        t.net(o['lag'], o['var'], o['loss'])
        self.cmd('np.PrintReconciles 1')
        self.cmd('gstn.Platform.Draw 1')
        _log(f'entry transitions pointed at WallSlidingV2: {self.set_entry_mode("WallSlidingV2")}')

        self.cmd('gstn.Platform.SpawnTestFighter %.0f %.0f %.0f 0' % FIGHTER_AT, self.worlds()[0])
        yield 2.0
        for i in range(len(self.worlds())):
            f = self.fighter(i)
            if not f:
                raise RuntimeError(f'no test fighter in world {i} (plugin GSTNPlatform not enabled, or not replicated)')
            names = sorted(c.get_name() for c in f.get_components_by_class(unreal.PrimitiveComponent))
            _log(f'world {i}: fighter {f.get_name()} at {f.get_actor_location()} colliders {names}')

        pids = {i: self.player_id(i) for i in range(len(self.worlds()))}
        for i, pid in pids.items():
            self.cmd(f'gstn.Wall.Teleport {PARK[i][0]} {PARK[i][1]} {PARK[i][2]} 0 {pid}')
        yield 1.0

        subjects = [1, 0] if o['subjects'] == 'both' else ([1] if o['subjects'] == 'client' else [0])
        for i in subjects:
            for rep in range(o['repeats']):
                r = {'subject': 'client' if i else 'host', 'rep': rep}
                if 'wall' in o['phases']:
                    yield from self.wall_phase(i, pids[i], r)
                if 'stand' in o['phases']:
                    yield from self.stand_phase(i, pids[i], r)
                self.cmd(f'gstn.Wall.Teleport {PARK[i][0]} {PARK[i][1]} {PARK[i][2]} 0 {pids[i]}')
                yield 1.0
                _log('result ' + json.dumps(r))
                self.results.append(r)

    def wall_phase(self, i, pid, r):
        """Launch at the flank from 3 m (the grab comes after ~1 s, outside the correction window of the harness's own teleport and launch,
        wall-slide-v2.md 6.12), slide, jump off."""
        import gasnpp_test as t
        w = self.worlds()
        origin_y = FIGHTER_AT[1]
        face_x = FIGHTER_AT[0] + FLANK_FACE_DX
        t._set_yaw(i, 0.0)
        self.cmd('gstn.Wall.DebugInput ""')
        self.cmd(f'gstn.Wall.Teleport {face_x - 300.0} {origin_y} 120 0 {pid}')
        # The wall memory of the previous run (same collider, HeightCap) would refuse the grab on one machine and not on the other: the same-wall
        # policy compares world heights, meaningless on a moving base (traversal-surfaces.md 4.3, the T2 entry transition). wall_ab does the same
        self.cmd('gstn.Wall.ResetMemory')
        yield 1.5
        self.cmd('gstn.Wall.DebugInput "stick 0 1"')
        mark_launch = t.mark_log()
        self.cmd(f'gstn.Wall.Teleport {face_x - 300.0} {origin_y} 400 0 {pid}')
        self.cmd(f'gstn.Wall.Launch 300 0 520 0 {pid}')
        t0 = time.time()
        grabbed, mark_grab = None, None
        while time.time() - t0 < 3.0:
            yield 0.0
            mode = self.mode_of(self.pawn_of(i)[1])
            if mode in WALL_MODES:
                grabbed = mode
                break
            if mode == 'Walking' and time.time() - t0 > 1.0:
                break
        r['wall_mode'] = grabbed or self.mode_of(self.pawn_of(i)[1])
        r['wall_grab_s'] = round(time.time() - t0, 2)
        if not grabbed:
            r['wall_recon_launch'] = _recon(t, mark_launch)
            self.cmd('gstn.Wall.DebugInput ""')
            return
        _, pawn = self.pawn_of(i)
        f = self.fighter(i)
        rel0 = pawn.get_actor_location().y - f.get_actor_location().y
        server_copy = self.pawn_by_id(w[0], pid)
        r['wall_server_mode_at_grab'] = self.mode_of(server_copy)
        yield 0.35
        mark_on = t.mark_log()
        r['wall_recon_launch_to_grab'] = _recon(t, mark_launch)
        yield 0.35
        _, pawn = self.pawn_of(i)
        f = self.fighter(i)
        r['wall_rel_y_drift'] = round((pawn.get_actor_location().y - f.get_actor_location().y) - rel0, 1)
        r['wall_mode_before_jump'] = self.mode_of(pawn)
        r['wall_recon_on'] = _recon(t, mark_on)
        mark_jump = t.mark_log()
        self.cmd('gstn.Wall.DebugInput ""')
        r['wall_jump'] = t._bare_input(i, 'jump', pulse=True) if self.opts['jumps'] else 'skipped'
        yield 1.2
        r['wall_mode_after_jump'] = self.mode_of(self.pawn_of(i)[1])
        r['wall_recon_jump'] = _recon(t, mark_jump)

    def stand_phase(self, i, pid, r):
        """Teleport above the top, land, ride it (0 reconciles once the landing settled), jump off."""
        import gasnpp_test as t
        f0 = self.fighter(0)
        fl = f0.get_actor_location()
        mark_land = t.mark_log()
        self.cmd(f'gstn.Wall.Teleport {fl.x} {FIGHTER_AT[1]} {TOP_Z + 200.0} 0 {pid}')
        t0 = time.time()
        landed = False
        while time.time() - t0 < 3.0:
            yield 0.0
            _, pawn = self.pawn_of(i)
            if self.mode_of(pawn) == 'Walking' and pawn.get_actor_location().z > TOP_Z:
                landed = True
                break
        r['stand_landed'] = landed
        if not landed:
            _, pawn = self.pawn_of(i)
            r['stand_mode'] = self.mode_of(pawn)
            r['stand_z'] = round(pawn.get_actor_location().z, 1)
            return
        yield 1.0
        r['stand_recon_landing'] = _recon(t, mark_land)
        mark_on = t.mark_log()
        _, pawn = self.pawn_of(i)
        f = self.fighter(i)
        rel0 = pawn.get_actor_location().y - f.get_actor_location().y
        y0 = pawn.get_actor_location().y
        r['stand_base'] = self.base_of(pawn)
        r['stand_base_server'] = self.base_of(self.pawn_by_id(self.worlds()[0], pid))
        yield 1.5
        _, pawn = self.pawn_of(i)
        f = self.fighter(i)
        r['stand_rel_y_drift'] = round((pawn.get_actor_location().y - f.get_actor_location().y) - rel0, 1)
        r['stand_travel_y'] = round(pawn.get_actor_location().y - y0, 1)
        r['stand_mode'] = self.mode_of(pawn)
        r['stand_z'] = round(pawn.get_actor_location().z, 1)
        r['stand_recon_on'] = _recon(t, mark_on)
        mark_jump = t.mark_log()
        r['stand_jump'] = t._bare_input(i, 'jump', pulse=True) if self.opts['jumps'] else 'skipped'
        yield 1.5
        r['stand_mode_after_jump'] = self.mode_of(self.pawn_of(i)[1])
        r['stand_recon_jump'] = _recon(t, mark_jump)

    # -- plumbing (as wall_ab)
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
        try:
            write_summary(self.results, self.opts, self.error)
        except Exception as e:
            _log(f'summary not written: {e}')
        _log('surface_scenarios done' + (f' ({self.error.splitlines()[0]})' if self.error else ''))
        try:
            with open(os.path.join(METRICS_DIR, 'surface_scenarios.done'), 'w') as f:
                f.write(self.error or 'ok')
        except OSError:
            pass
        if self.opts.get('quit', False):
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')

    def run(self):
        os.makedirs(METRICS_DIR, exist_ok=True)
        self.gen = self.script()
        self.handle = unreal.register_slate_post_tick_callback(self.tick)


def verdict(r):
    """scenario_surface_wallslide_enemy pass rule for one run."""
    ok = r.get('wall_mode') in WALL_MODES and r.get('stand_landed')
    if r['subject'] == 'client':
        ok = ok and r.get('wall_recon_on', [1, 1])[1] == 0 and r.get('stand_recon_on', [1, 1])[1] == 0 and r.get('wall_recon_launch_to_grab', [9, 9])[1] <= 1
    return ok


def write_summary(results, opts, error):
    os.makedirs(METRICS_DIR, exist_ok=True)
    cols = ['subject', 'rep', 'wall_mode', 'wall_grab_s', 'wall_recon_launch_to_grab', 'wall_recon_on', 'wall_rel_y_drift', 'wall_mode_after_jump', 'wall_recon_jump',
            'stand_landed', 'stand_recon_landing', 'stand_base', 'stand_base_server', 'stand_recon_on', 'stand_rel_y_drift', 'stand_travel_y', 'stand_z', 'stand_mode_after_jump', 'stand_recon_jump']
    lines = [f"# scenario_surface_wallslide_enemy ({datetime.datetime.now().strftime('%Y-%m-%d %H:%M')})", '',
             f"{opts['lag']} ms lag, {opts['var']} ms jitter, {opts['loss']} % loss, listen host + 1 client, map {MAP}, fighter at {FIGHTER_AT} (300 cm/s, +-300 cm along Y)", '']
    if error:
        lines += ['**Error:** ' + error.splitlines()[0], '']
    lines.append('| pass | ' + ' | '.join(cols) + ' |')
    lines.append('|' + '---|' * (len(cols) + 1))
    for r in results:
        lines.append(f"| {'yes' if verdict(r) else 'NO'} | " + ' | '.join(str(r.get(c, '')) for c in cols) + ' |')
    lines += ['', 'Reconcile counts are [mismatches, unexplained] in the log (np.PrintReconciles) within each window, unexplained = not on the frame of an input '
              'offset change (NP re-sizing the client input buffer to the jitter); the pass rule uses unexplained. Windows: launch_to_grab = from the harness teleport and launch '
              'to 0.35 s after the grab (includes the harness\'s own teleport correction, wall-slide-v2.md 6.12); on = 0.35..0.70 s after the grab; landing = '
              'teleport above the top to 1 s after the landing (teleport artefact); stand on = the next 1.5 s riding. The host never reconciles.']
    open(SUMMARY, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    _log('summary: ' + SUMMARY)


_driver = None


def start(lag=60, var=15, loss=0, subjects='both', repeats=2, start_pie=False, quit=False, phases=('wall', 'stand'), jumps=True):
    """Starts the scenario on the editor tick and returns at once (log line 'surface_scenarios done' at the end)."""
    global _driver
    _driver = Driver({'lag': lag, 'var': var, 'loss': loss, 'subjects': subjects, 'repeats': repeats, 'start_pie': start_pie, 'quit': quit,
                      'phases': tuple(phases), 'jumps': jumps})
    _driver.run()
    return _driver


# ------------------------------------------------------------------------------------------------ launcher (plain Python)


def launch(opts):
    """Starts the editor with the driver and waits for it. Patches PlayNumberOfClients in the user's PIE settings for the run only."""
    backup = INI + '.surface.bak'
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
        done = os.path.join(METRICS_DIR, 'surface_scenarios.done')
        if os.path.exists(done):
            os.remove(done)
        log = os.path.join(METRICS_DIR, 'surface_scenarios-editor.log')
        driver_script = os.path.join(METRICS_DIR, '_surface_scenarios_run.py')
        with open(driver_script, 'w', encoding='utf-8') as f:
            f.write("import sys\nsys.path.insert(0, r'%s')\nimport surface_scenarios\nsurface_scenarios.start(**%r)\n" % (HERE, dict(
                lag=opts['lag'], var=opts['var'], loss=opts['loss'], subjects=opts['subjects'], repeats=opts['repeats'], start_pie=True, quit=True,
                phases=tuple(opts['phases']), jumps=not opts['no_jumps'])))
        # The project path has a space and UE's -ExecCmds parser stops at an inner quote: the script path travels in the environment
        env = dict(os.environ, SURFACE_SCENARIOS_BOOT=driver_script.replace(os.sep, '/'))
        args = [ENGINE, UPROJECT, MAP, "-ExecCmds=py import os;exec(open(os.environ['SURFACE_SCENARIOS_BOOT']).read())", '-unattended', '-nullrhi', '-nosplash', '-nosound', '-NoP4', f'-abslog={log}']
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
    if os.path.exists(SUMMARY):
        print(open(SUMMARY, encoding='utf-8').read())
    else:
        print('no summary written; see', os.path.join(METRICS_DIR, 'surface_scenarios-editor.log'))


if __name__ == '__main__':
    if unreal is None:
        import argparse
        p = argparse.ArgumentParser(description='Traversal surface scenarios (headless editor).')
        p.add_argument('--lag', type=int, default=60)
        p.add_argument('--var', type=int, default=15)
        p.add_argument('--loss', type=int, default=0)
        p.add_argument('--subjects', choices=('both', 'client', 'host'), default='both')
        p.add_argument('--repeats', type=int, default=2)
        p.add_argument('--phases', nargs='+', default=['wall', 'stand'], choices=('wall', 'stand'))
        p.add_argument('--no-jumps', action='store_true', help='leave the flank by sliding to the floor and the top by staying on it (no jump input)')
        p.add_argument('--timeout', type=int, default=900, help='seconds before the editor is killed')
        launch(vars(p.parse_args()))
