# F12 task A: the S1 link spike with the clients in SEPARATE processes (PIE RunUnderOneProcess=false), so an editor hitch stalls only the host
# and cannot starve both clients alike (HANDOVER 58 OPEN 1). The editor's Python sees only the host's world there: it drives a client through
# UGSTN_GASNPPTestComponent.RemoteExec on the server's copy of that client's pawn (a reliable client RPC: hold / release / tap / exec) and reads
# the client from the records the client writes every engine frame to Saved/Logs/mirror/<pawn key>.txt (platform seconds, rollbacks, frame ms,
# and per pawn its Mover mode and UGSTN_LinkContentLibrary.GetMirrorText). A file, not the game connection: a per frame RPC competed with the
# client's input upstream. Every process on the machine shares FPlatformTime, so the records and the host's own samples sit on one clock.
#
# Only the link spike (three pairings) is ported: link_rollback / break_distance / grab_immunity read client state this mirror does not carry
# (break records, sim histories). Left out of the spike: the "forced reconcile converges frame by frame" check (it needs the client's sim
# history). Each scenario also reports the hitch frames (engine frames of 100 ms or more) of every process over its run.
#
#   Before the editor starts: python Tools/GASNPPTest/link_mp.py patch     (RunUnderOneProcess=False in the user's PIE settings; 'restore' undoes it)
#   In the editor, harness level, nothing playing:
#   import link_mp as mp; mp.batch(loss=2, report='f12-sweep/link-2pct-mp-1.log')   # poll mp.batch_report()
import json
import math
import os
import re
import shutil
import sys
import time

try:
    import unreal
    import gasnpp_test as g
    import link_scenarios as ls
except ImportError:          # plain Python: only patch / restore
    unreal = None

MIRROR_FN = '/Script/GSTNLinkNP.GSTN_LinkContentLibrary:GetMirrorText'
HITCH_MS = 100.0
PAIRINGS = {'host_grabs_client': (0, 1), 'client_grabs_host': (1, 0), 'client_grabs_client': (1, 2)}
SUITE = [f'link_spike_{p}' for p in PAIRINGS]
_batch = {'handle': None, 'results': [], 'status': 'idle', 'report': None}
_last = {'run': None}


# ---- the server side view of the clients ---------------------------------------------------------------------------------------------------
def server_comps():
    return g.comps(g.worlds()[0])


def keys():
    """[host key, client 1 key, client 2 key]: the host's locally controlled pawn, then the remote pawns by player id (join order)"""
    s = g.snap(g.worlds()[0])
    host = [k for k, v in s.items() if v['local']]
    remote = sorted((k for k, v in s.items() if not v['local']), key=lambda k: int(k[1:]) if k[1:].isdigit() else 1 << 30)
    return host + remote


def remote(ci, cmd):
    """Runs cmd on client process ci (1 or 2) through its pawn's server copy"""
    ok = server_comps()[keys()[ci]].remote_exec(cmd)
    if not ok:
        raise RuntimeError(f'remote_exec refused for client {ci}: {cmd}')
    return ok


def mirror_path(ci):
    return unreal.GSTN_GASNPPTestComponent.get_mirror_file_path(keys()[ci])


def read_mirror(ci):
    """Every complete record client ci wrote since its file was last deleted"""
    try:
        text = open(mirror_path(ci), encoding='utf-8').read()
    except OSError:
        return []
    out = []
    for line in text.split('\n')[:-1]:
        try:
            out.append(parse_record(line))
        except Exception:
            pass
    return out


def clear_mirror():
    for ci in (1, 2):
        try:
            os.remove(mirror_path(ci))
        except OSError:
            pass


def desired_buffer(value=None):
    """The server's FixedTickDesiredBufferedInputCount (NP settings object, in memory; read by a world manager when its world starts): reads it, or
    sets it when value is given. Diagnostic only: the committed setting stays in Config/DefaultNetworkPrediction.ini"""
    s = unreal.get_default_object(unreal.load_class(None, '/Script/NetworkPrediction.NetworkPredictionSettingsObject'))
    st = s.get_editor_property('settings')
    text = st.export_text()
    if value is not None:
        st.import_text(re.sub(r'FixedTickDesiredBufferedInputCount=\d+', f'FixedTickDesiredBufferedInputCount={int(value)}', text))
        s.set_editor_property('settings', st)
        text = s.get_editor_property('settings').export_text()
    return int(re.search(r'FixedTickDesiredBufferedInputCount=(\d+)', text).group(1))


def now():
    return unreal.GSTN_GASNPPTestComponent.get_platform_seconds()


def hold(i, action, value):
    if i == 0:
        return g.hold(0, action, value)
    return remote(i, f'hold {g.IA[action]} {value[0]} {value[1]} {value[2]}')


def release(i, action):
    if i == 0:
        return g.release(0, action)
    return remote(i, f'release {g.IA[action]}')


def tap(i, mask):
    if i == 0:
        return g.combat_tap(0, mask)
    return remote(i, f'tap {mask}')


def net(lag, jitter, loss):
    """The same emulation in every process (in one process the cvars are shared by every net driver; here each process sets its own)"""
    w = g.worlds()[0]
    cmds = (f'NetEmulation.PktLag {lag}', f'NetEmulation.PktLagVariance {jitter}', f'NetEmulation.PktLoss {loss}')
    for c in cmds:
        unreal.SystemLibrary.execute_console_command(w, c)
    for ci in (1, 2):
        for c in cmds:
            remote(ci, 'exec ' + c)


def parse_record(line):
    """(t, rollbacks, frame ms, {key: (mode, body tuple or None, parked xyz or None)})"""
    parts = line.split('|')
    head = parts[0].split()
    pawns = {}
    for p in parts[1:]:
        f = p.split(' ', 2)
        if len(f) < 3:
            continue
        key, mode, text = f
        body, parked = None, None
        if text not in ('-', '?') and ';' in text:
            b, loc = text.split(';', 1)
            body = tuple(int(x) for x in b.split(','))
            if loc != '-':
                parked = tuple(float(x) for x in loc.split(','))
        pawns[key] = (mode, body, parked)
    return float(head[0]), int(head[1]), float(head[2]), pawns


class _Rig:
    """Samples the host world every editor tick and drains both clients' mirrors into per client record lists"""

    def __init__(self, name, pairing):
        self.name = name
        self.ia, self.ib = PAIRINGS[pairing]
        k = keys()
        self.keys = k
        self.akey, self.bkey = k[self.ia], k[self.ib]
        self.clients = sorted({i for i in (self.ia, self.ib) if i > 0})
        self.observer = next((i for i in (1, 2) if i not in (self.ia, self.ib)), None)
        self.records = {1: [], 2: []}
        self.host = []     # (t, frame ms, {key: (mode, body, parked)})
        self.marks = {}
        self.forced = []
        self.run = g._Run(name, 0, judge_reconciles=False, sample=False)
        clear_mirror()
        # The server's starved fixed ticks (NP fork np.Spike.LogInputSend: 'starved at client frame', one line per tick and client)
        unreal.SystemLibrary.execute_console_command(g.worlds()[0], 'np.Spike.LogInputSend 1')
        self.log_mark = g.mark_log()
        self.t_start = now()
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def _tick(self, dt):
        try:
            t = now()
            view = {}
            for key in (self.akey, self.bkey):
                view[key] = (ls.mover_mode(0, key), ls.body(0, key), ls.linked_location(0, key))
            self.host.append((t, dt * 1000.0, view))
        except Exception:
            pass

    def drain(self):
        for ci in (1, 2):
            self.records[ci] = read_mirror(ci)

    def stop(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.drain()
        for ci in (1, 2):
            self.records[ci].sort(key=lambda r: r[0])

    def mark(self, name):
        def f():
            self.marks[name] = now()
            return 'mark', name
        return f

    # -- queries after the run
    def at(self, ci, t):
        """The last record of client ci at or before t"""
        best = None
        for r in self.records[ci]:
            if r[0] <= t:
                best = r
            else:
                break
        return best

    def rb(self, ci, mark):
        r = self.at(ci, self.marks[mark])
        return r[1] if r else None

    def rb_between(self, a, b, ci):
        x, y = self.rb(ci, a), self.rb(ci, b)
        return None if x is None or y is None else y - x

    def host_at(self, t):
        best = None
        for s in self.host:
            if s[0] <= t:
                best = s
            else:
                break
        return best

    def body_on(self, wi, key, t):
        if wi == 0:
            s = self.host_at(t)
            return s[2].get(key, (None, None, None))[1] if s else None
        r = self.at(wi, t)
        return r[3].get(key, (None, None, None))[1] if r else None

    def mode_on(self, wi, key, t):
        if wi == 0:
            s = self.host_at(t)
            return s[2].get(key, (None, None, None))[0] if s else None
        r = self.at(wi, t)
        return r[3].get(key, (None, None, None))[0] if r else None

    def series(self, wi, key, t0, t1):
        if wi == 0:
            return [(s[0], s[2][key][2]) for s in self.host if t0 <= s[0] <= t1 and s[2].get(key) and s[2][key][2]]
        return [(r[0], r[3][key][2]) for r in self.records[wi] if t0 <= r[0] <= t1 and r[3].get(key) and r[3][key][2]]

    def hitches(self, t0, t1):
        """Per process: (frames of HITCH_MS or more, frames, max frame ms) over [t0, t1]"""
        out = {}
        rows = {0: [(s[0], s[1]) for s in self.host], 1: [(r[0], r[2]) for r in self.records[1]], 2: [(r[0], r[2]) for r in self.records[2]]}
        for wi, xs in rows.items():
            w = [ms for t, ms in xs if t0 <= t <= t1]
            out[wi] = (sum(1 for ms in w if ms >= HITCH_MS), len(w), max(w) if w else 0.0)
        return out


def _spike(pairing):
    name = f'link_spike_{pairing}_mp'
    rig = _Rig(name, pairing)
    run = rig.run
    _last['run'] = rig
    grab = g._tag(ls.GRAB)
    T_FORM, T_WALK = 1.6, 2.6
    T_END = T_WALK + ls.WALK_SECONDS
    a = rig.ia

    def form():
        ok = ls.Lib.form_test_link(ls.actor(0, rig.akey), ls.actor(0, rig.bkey), grab)
        return 'formed' if ok else 'FORM REFUSED'

    def force(ci):
        def f():
            rig.forced.append((ci, now()))
            remote(ci, 'exec np.ForceReconcileExtraFrames 10')
            remote(ci, 'exec np.ForceReconcile 1')
            return 'forced', ci
        return f

    def brk():
        return 'break', ls.Lib.break_test_link(ls.actor(0, rig.bkey))

    def place():
        g._place_near(a, rig.bkey, (150.0, 0.0))
        return 'placed'

    steps = [(0.2, lambda: ls.link_setup()), (0.3, place), (T_FORM - 0.1, rig.mark('pre_form')), (T_FORM, form),
             (T_FORM + 0.9, rig.mark('formed')), (T_WALK, rig.mark('walk_start')), (T_WALK, lambda: hold(a, 'move', (0.0, 1.0, 0.0)))]
    t = T_WALK + 3.0
    while t < T_END - 0.5:
        k = int((t - T_WALK) * 10)
        ang = k * 0.25
        steps.append((t, (lambda v: (lambda: hold(a, 'move', v)))((math.sin(ang), math.cos(ang), 0.0))))
        if k % 40 == 0:
            steps.append((t + 0.05, lambda: tap(a, g.button_mask('Dash'))))
        t += 0.1
    steps.append((T_END, lambda: release(a, 'move')))
    for n, ci in enumerate(rig.clients):
        steps.append((T_WALK + 8.0 + 3.0 * n, rig.mark(f'pre_force_{ci}')))
        steps.append((T_WALK + 8.05 + 3.0 * n, force(ci)))
        steps.append((T_WALK + 9.5 + 3.0 * n, rig.mark(f'post_force_{ci}')))
    steps += [(T_END + 0.2, rig.mark('walk_end')), (T_END + 0.3, brk), (T_END + 1.5, rig.mark('broken'))]

    def judge():
        res = []
        tf, tb = rig.marks['formed'], rig.marks['broken']
        rows = {wi: (rig.body_on(wi, rig.akey, tf), rig.body_on(wi, rig.bkey, tf)) for wi in (0, 1, 2)}
        ok = all(x and y and x[0] == 1 and y[0] == 2 for x, y in rows.values())
        serials = {(x[2], x[3], y[2], y[3]) for x, y in rows.values() if x and y}
        res.append(('formed on every process: grabber role 1, grabbed role 2, one serial and link frame', ok and len(serials) == 1, f'{rows}'))
        sim = [0] + ([rig.ib] if rig.ib > 0 else [])
        modes = {wi: rig.mode_on(wi, rig.bkey, tf) for wi in (0, 1, 2)}
        res.append(('the grabbed copies that simulate it are in the parked mode', all(modes[wi] == ls.LINKED_MODE for wi in sim), f'{modes}'))
        rows = {wi: (rig.body_on(wi, rig.akey, tb), rig.body_on(wi, rig.bkey, tb)) for wi in (0, 1, 2)}
        res.append(('the link is gone on every process', all((x is None or x[1] == 0) and (y is None or y[1] == 0) for x, y in rows.values()), f'{rows}'))
        modes = {wi: rig.mode_on(wi, rig.bkey, tb) for wi in (0, 1, 2)}
        res.append(('the grabbed left the parked mode', all(m != ls.LINKED_MODE for m in modes.values()), f'{modes}'))
        for ci in rig.clients:
            start = rig.rb_between('pre_form', 'formed', ci)
            res.append((f'client {ci}: exactly one correction at link start', start == 1, f'{start}'))
            end = rig.rb_between('walk_end', 'broken', ci)
            res.append((f'client {ci}: exactly one correction at link end', end == 1, f'{end}'))
            walk = rig.rb_between('walk_start', 'walk_end', ci)
            in_forced = sum(rig.rb_between(f'pre_force_{c}', f'post_force_{c}', ci) or 0 for c in rig.clients)
            res.append((f'client {ci}: zero unforced reconciles during the walk', walk is not None and walk - in_forced == 0,
                        f'{walk} rollbacks, {in_forced} inside the forced windows'))
            for c in rig.clients:
                n = rig.rb_between(f'pre_force_{c}', f'post_force_{c}', ci)
                # One process per client: a forced reconcile is local to the process it was sent to (np.ForceReconcile is not shared here)
                res.append((f'client {ci}: forced window of client {c} costs at most one forced reconcile (2 counter steps)', n is not None and n <= 2, f'{n}'))
        if rig.observer is not None:
            n = rig.rb_between('walk_start', 'walk_end', rig.observer)
            n = None if n is None else n - sum(rig.rb_between(f'pre_force_{c}', f'post_force_{c}', rig.observer) or 0 for c in rig.clients)
            res.append((f'observer client {rig.observer}: no correction while the link is held', n is not None and n <= 0, f'{n} (outside the forced windows)'))
        ws = rig.marks['walk_start']
        server = rig.series(0, rig.bkey, ws, ws + 7.0)
        for wi in (1, 2):
            mine = rig.series(wi, rig.bkey, ws + 0.5, ws + 7.0)
            e = ls.presentation_error(mine, server)
            if e is None:
                res.append((f'process {wi}: presentation sampled', False, f'{len(mine)} samples, server {len(server)}'))
                continue
            p50, p95, mx, d = e
            limit = ls.S1['own_p95'] if wi == rig.ia else ls.S1['partner_p95']
            role = 'grabber' if wi == rig.ia else 'grabbed' if wi == rig.ib else 'observer'
            res.append((f'process {wi} ({role}): presentation p95 <= {limit} cm, max <= {ls.S1["max"]} cm', p95 <= limit and mx <= ls.S1['max'],
                        f'p50 {p50:.1f} p95 {p95:.1f} max {mx:.1f} cm, delay {d * 1000:.0f} ms, {len(mine)} samples'))
        unreal.SystemLibrary.execute_console_command(g.worlds()[0], 'np.Spike.LogInputSend 0')
        starved = [l for l in g._read_log_since(rig.log_mark).splitlines() if 'starved at client frame' in l]
        frames = [float(m.group(1)) for m in (re.search(r'engine frame ([0-9.]+) ms', l) for l in starved) if m]
        run.note(f'server starved fixed ticks (all clients) over the run: {len(starved)}' + (f', in host frames of {min(frames):.0f}-{max(frames):.0f} ms' if frames else '')
                 + f'; server input buffer FixedTickDesiredBufferedInputCount={desired_buffer()}')
        h = rig.hitches(rig.marks['pre_form'], tb)
        run.note('hitch frames (>= %d ms) per process over the run: ' % HITCH_MS + ', '.join(
            f"{'host' if wi == 0 else 'client %d' % wi} {n} of {tot} (max {mx:.0f} ms)" for wi, (n, tot, mx) in sorted(h.items())))
        run.note('not checked here: the forced reconcile convergence frame by frame (needs the client sim history)')
        return res

    def verdicts():
        rig.stop()
        for label, ok, detail in judge():
            run.check(label, ok, detail)
        run.note(f'rollbacks per mark: {[(n, {ci: rig.rb(ci, n) for ci in (1, 2)}) for n, _ in sorted(rig.marks.items(), key=lambda x: x[1])]}')
        return 'judged'

    def restore():
        rig.stop()
        release(a, 'move')
        return ls.link_reset()

    steps += [(T_END + 2.2, verdicts), (T_END + 2.3, restore), (T_END + 2.5, run.finish())]
    run.run(steps)


for _p in PAIRINGS:
    globals()[f'scenario_link_spike_{_p}'] = (lambda p: (lambda: _spike(p)))(_p)


def clean_level():
    """The harness's in-memory setup dirties the harness map (its game mode): a separate-process PIE then waits on a Save Content dialog. The flag is
    cleared WITHOUT saving (the level is shared content); returns the dirty maps left"""
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.GSTN_GASNPPTestComponent.clear_package_dirty_flag(w)
    for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        unreal.GSTN_GASNPPTestComponent.clear_package_dirty_flag(p)
    return [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()] +         [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]


def setup():
    """Once per editor session, before the first PIE: the harness's editor setup, the link test type, a clean level"""
    return g.setup_editor(True), ls.prepare_editor(), clean_level()


# ---- batch: a fresh PIE per scenario --------------------------------------------------------------------------------------------------------
def batch(names=None, lag=60, jitter=15, loss=0, settle=4.0, ready_timeout=150.0, timeout=120.0, report=None):
    """Each scenario in a fresh PIE (separate client processes: the play settings must have RunUnderOneProcess=False, see `patch`). Ready = the host
    world has three pawns and both clients' mirrors arrive; then each client process runs gstn.Link.Test.Prepare steps (what link_setup does in the
    editor) and the net emulation is set in every process. Non-blocking: poll batch_report()."""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if _batch['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_batch['handle'])
    queue = list(names or SUITE)
    st = {'phase': 'end', 't': time.time(), 'ending': False, 'cur': None, 'prev': None, 'poke': 0.0}
    _batch.update(results=[], status=f'{len(queue)} queued', report=report, net=(lag, jitter, loss))

    def mirrors_ok():
        ws = g.worlds()
        if len(ws) != 1:
            return False
        try:
            k = keys()
        except Exception:
            return False
        if len(k) < 3:
            return False
        comps = server_comps()
        if time.time() - st['poke'] > 2.0:
            st['poke'] = time.time()
            for ci in (1, 2):
                comps[k[ci]].remote_exec(f'mirror 1 {MIRROR_FN}')
        return all(read_mirror(ci) for ci in (1, 2))

    def done():
        unreal.unregister_slate_post_tick_callback(_batch['handle'])
        _batch['handle'] = None
        _batch['status'] = 'done'
        if _batch['report']:
            path = os.path.join(unreal.Paths.project_saved_dir(), 'Logs', _batch['report'])
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as f:
                f.write(time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()) + f' net{_batch["net"]} separate client processes\n' + batch_report() + '\n')

    def tick(_dt):
        try:
            elapsed = time.time() - st['t']
            if st['phase'] == 'end':
                if les.is_in_play_in_editor():
                    if not st['ending']:
                        les.editor_request_end_play()
                        st['ending'] = True
                elif elapsed < 20.0 and st['ending'] is not None:
                    return
                else:
                    if not queue:
                        return done()
                    ls.prepare_editor()
                    left = clean_level()
                    if left:
                        _batch['results'].append((st['cur'], f'ERROR dirty packages {left}', []))
                    st.update(phase='wait_ready', t=time.time(), ending=False, cur=queue.pop(0))
                    _batch['status'] = f"{st['cur']}: starting PIE"
                    les.editor_request_begin_play()
            elif st['phase'] == 'wait_ready':
                if les.is_in_play_in_editor() and elapsed > 5.0 and mirrors_ok():
                    for ci in (1, 2):
                        remote(ci, 'exec gstn.Link.Test.Prepare steps')
                    st.update(phase='settle', t=time.time())
                elif elapsed > ready_timeout:
                    _batch['results'].append((st['cur'], 'ERROR clients not ready', []))
                    st.update(phase='end', t=time.time(), ending=False)
            elif st['phase'] == 'settle' and elapsed >= settle:
                net(lag, jitter, loss)
                g.reset_all()
                st['prev'] = g._last_run.get('results')
                _batch['status'] = f"{st['cur']}: running"
                globals()['scenario_' + st['cur']]()
                st.update(phase='run', t=time.time())
            elif st['phase'] == 'run':
                if g._last_run.get('results') is not st['prev'] and g._last_run.get('results') is not None:
                    res = g._last_run
                    bad = [f'{l} -> {d[:300]}' for ok, l, d in res['results'] if ok is False]
                    notes = [l for ok, l, d in res['results'] if ok is None and (l.startswith('hitch') or l.startswith('server starved'))]
                    _batch['results'].append((st['cur'], res['verdict'], bad, notes))
                    st.update(phase='end', t=time.time(), ending=False)
                elif elapsed > timeout:
                    _batch['results'].append((st['cur'], 'TIMEOUT', []))
                    st.update(phase='end', t=time.time(), ending=False)
        except Exception as e:
            _batch['results'].append((st.get('cur'), f'EXCEPTION {e!r}', []))
            st.update(phase='end', t=time.time(), ending=False)

    _batch['handle'] = unreal.register_slate_post_tick_callback(tick)
    return _batch['status']


def batch_report():
    lines = []
    for r in _batch['results']:
        n, v, b = r[0], r[1], r[2]
        notes = r[3] if len(r) > 3 else []
        lines.append(f'{n}: {v}' + (f' FAILED {b}' if b else '') + ''.join(f'\n    {x}' for x in notes))
    return f"{_batch['status']}\n" + '\n'.join(lines)


# ---- plain Python: the PIE setting -----------------------------------------------------------------------------------------------------------
INI = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'Saved', 'Config', 'WindowsEditor',
                   'EditorPerProjectUserSettings.ini')
SECTION = '[/Script/UnrealEd.LevelEditorPlaySettings]'


def patch(one_process=False):
    """RunUnderOneProcess, PlayNetMode listen, 2 clients in the user's PIE settings (a backup next to it first; the editor must be closed)"""
    text = open(INI, encoding='utf-8').read()
    if not os.path.exists(INI + '.mp.bak'):
        shutil.copy2(INI, INI + '.mp.bak')
    want = {'PlayNetMode': 'PIE_ListenServer', 'RunUnderOneProcess': 'True' if one_process else 'False', 'PlayNumberOfClients': '3'}
    if SECTION not in text:
        text += '\n' + SECTION + '\n'
    for k, v in want.items():
        if re.search(rf'^{k}=', text, re.M):
            text = re.sub(rf'^{k}=.*$', f'{k}={v}', text, flags=re.M)
        else:
            text = text.replace(SECTION, f'{SECTION}\n{k}={v}', 1)
    open(INI, 'w', encoding='utf-8').write(text)
    return want


def restore():
    if os.path.exists(INI + '.mp.bak'):
        shutil.copy2(INI + '.mp.bak', INI)
        os.remove(INI + '.mp.bak')
        return 'restored'
    return 'no backup'


if __name__ == '__main__' and unreal is None:
    print(patch() if sys.argv[1:] == ['patch'] else restore() if sys.argv[1:] == ['restore'] else 'usage: link_mp.py patch|restore')
