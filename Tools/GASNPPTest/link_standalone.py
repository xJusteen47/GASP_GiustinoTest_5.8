# link_mp scenarios with a STANDALONE listen host (a game process, no editor) and two client game processes (M6 F14, HANDOVER 61). The editor
# cannot host at tick rate here: one-process PIE is game-thread bound (~49 ms a frame for three worlds) and an editor host alone ran ~14 fps, so
# its long frames starved the server's input buffer (HANDOVER 59 OPEN 1). This file runs INSIDE the host process: link_standalone.ps1 copies it
# to a path without spaces and launches the host with -ExecCmds="py <copy>", then the clients. The harness reads the host's own game world (the
# PIE world list does not exist there) and drives / reads the clients through link_mp (RemoteExec + mirror files), as with the editor host.
#
# Environment (set by the launcher): GSTN_SA_SCENARIOS a comma list of link_mp scenario names, 'warm:<name>' for a warm-up run that is reported
# and not judged (a fresh uncooked game process loads assets on first use: the first dash cost a client 270-310 ms); GSTN_SA_LOSS (percent);
# GSTN_SA_REPORT (under Saved/Logs); GSTN_SA_IDLE seconds of idle measurement first; GSTN_SA_PRINT=1 for np.PrintReconciles on both clients;
# GSTN_SA_NOLINK=1 for the bytes/s control (the same walk without forming the link). The report adds the host's frame statistics and, per
# scenario, the host's upload over the held-link window (the net driver's byte totals, four samples a second). Ends by asking both clients and itself to quit.
import os
import sys
import time

import unreal

P = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
for d in (P + 'Tools/GASNPPTest', P + 'Tools/GAS'):
    if d not in sys.path:
        sys.path.append(d)

import gasnpp_test as g
import link_scenarios as ls
import link_mp as mp

SCEN = os.environ.get('GSTN_SA_SCENARIOS', 'link_spike_client_grabs_client')
QUEUE = SCEN.split(',')
RESULTS = []
LOSS = float(os.environ.get('GSTN_SA_LOSS', '2'))
REPORT = os.environ.get('GSTN_SA_REPORT', 'link-standalone.log')
LOGS = os.path.join(unreal.Paths.project_saved_dir(), 'Logs')
IDLE = float(os.environ.get('GSTN_SA_IDLE', '20'))
ST = {'phase': 'wait', 't': time.time(), 'poke': 0.0, 'dts': [], 'idle': [], 'prev': None, 'h': None, 'boot': time.time(), 'msg': []}


_cache = {'world': None}


def _game_world():
    # Cached: the spike samples the host world several times per frame, and walking every World object each time cost the host 30-100 ms frames
    w = _cache['world']
    try:
        if w is not None and unreal.GameplayStatics.get_game_mode(w) is not None:
            return w
    except Exception:
        pass
    _cache['world'] = None
    for w in unreal.ObjectIterator(unreal.World):
        try:
            if unreal.GameplayStatics.get_game_mode(w) is not None:
                _cache['world'] = w
                return w
        except Exception:
            pass
    return None


def _worlds():
    w = _game_world()
    return [w] if w else []


g.worlds = _worlds
g.key_of_player = lambda i: mp.keys()[i]      # the PIE version reads world i, which is another process here


class _NoLinkLib:
    """GSTN_SA_NOLINK=1: the spike's walk, dashes and forced reconciles without a link (formation and break are no-ops), for the bytes/s control"""
    def __getattr__(self, name):
        return getattr(unreal.GSTN_LinkContentLibrary, name)

    @staticmethod
    def form_test_link(a, b, tag):
        return True

    @staticmethod
    def break_test_link(a):
        return True


NOLINK = os.environ.get('GSTN_SA_NOLINK') == '1'
if NOLINK:
    ls.Lib = _NoLinkLib()


def _net_totals():
    """(out, in) bytes the host's game net driver sent / received since it started (UGSTN_GASNPPTestComponent.GetNetDriverTotals)"""
    try:
        t = unreal.GSTN_GASNPPTestComponent.get_net_driver_totals(_game_world())
        o, i = t.split()
        return int(o), int(i)
    except Exception:
        return None


def _rates_note(rig):
    """The host's upload to both clients over the held-link window (walk start + 1 s to walk end), from the net driver's byte totals"""
    rows = ST.get('rates', [])
    if not rig or len(rows) < 2:
        return f'host bytes/s: no samples (net driver totals {_net_totals() is not None})'
    m = rig.marks

    def rate(a, b):
        if m.get(a) is None or m.get(b) is None:
            return None, 0
        w = [r for r in rows if m[a] + 1.0 <= r[0] <= m[b]]
        if len(w) < 2:
            return None, 0
        return (w[-1][1] - w[0][1]) / (w[-1][0] - w[0][0]), len(w)
    walk, n = rate('walk_start', 'walk_end')
    pre, npre = rate('pre_form', 'formed') if not NOLINK else (None, 0)
    return (f'host upload ({"NO LINK control" if NOLINK else "link held"}): {round(walk) if walk is not None else None} B/s over the walk window ({n} samples, both clients)'
            + (f'; idle before formation {round(pre) if pre is not None else None} B/s' if not NOLINK else ''))


def _stats(dts):
    d = sorted(dts)
    if not d:
        return 'host frames: none'
    mean = sum(d) / len(d)
    p95 = d[min(len(d) - 1, int(0.95 * len(d)))]
    slow = sum(1 for x in d if x >= 0.1)
    over = sum(1 for x in d if x > 0.0175)
    return (f'host frames (standalone listen host process): {len(d)}, mean {1.0 / mean:.1f} fps ({mean * 1000:.1f} ms), p95 {p95 * 1000:.1f} ms, '
            f'max {d[-1] * 1000:.0f} ms, > 17.5 ms {over}, >= 100 ms {slow}')


def _finish(text):
    path = os.path.join(LOGS, REPORT)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()) + f' net(60, 15, {LOSS:g}) standalone listen host + 2 client processes\n')
        f.write('\n'.join(RESULTS + ([text] if text else [])) + '\n' + _stats(ST['dts']) + '\n')
        f.write('idle before the scenario (nothing driven, 3 pawns standing): ' + _stats(ST['idle']) + '\n')
        f.write('driver: ' + ' | '.join(ST['msg']) + '\n')
    ST['phase'] = 'quit'
    ST['t'] = time.time()


def _tick(dt):
    try:
        now = time.time()
        el = now - ST['t']
        if ST['phase'] in ('settle', 'run'):
            ST['dts'].append(dt)
        if ST['phase'] == 'run' and now - ST.get('rate_t', 0.0) >= 0.25:
            ST['rate_t'] = now
            tot = _net_totals()
            if tot is not None:
                ST.setdefault('rates', []).append((mp.now(), tot[0], tot[1]))
        if ST['phase'] == 'wait':
            w = _game_world()
            if w is None:
                return
            try:
                k = mp.keys()
            except Exception:
                k = []
            if len(k) >= 3:
                if now - ST['poke'] > 2.0:
                    ST['poke'] = now
                    for ci in (1, 2):
                        mp.server_comps()[k[ci]].remote_exec(f'mirror 1 {mp.MIRROR_FN}')
                if all(mp.read_mirror(ci) for ci in (1, 2)):
                    ST['msg'].append(f'ready after {now - ST["boot"]:.0f} s, keys {k}')
                    if os.environ.get('GSTN_SA_PRINT') == '1':   # diagnostic: every client reconcile with its diff in the client logs
                        for ci in (1, 2):
                            mp.remote(ci, 'exec np.PrintReconciles 1')
                        ST['msg'].append('np.PrintReconciles 1 on both clients')
                    ST.update(phase='idle', t=now)
            if now - ST['boot'] > 600:
                _finish(f'{SCEN}: ERROR clients not ready')
        elif ST['phase'] == 'idle':
            ST['idle'].append(dt)
            if el >= 5.0 and not ST.get('dumped'):
                ST['dumped'] = True
                unreal.SystemLibrary.execute_console_command(_game_world(), 'stat dumpave -num=120 -ms=1 -root=GameThread')
            if el >= IDLE:
                ls.prepare_editor()
                for ci in (1, 2):
                    mp.remote(ci, 'exec gstn.Link.Test.Prepare steps')
                ST.update(phase='settle', t=now)
        elif ST['phase'] == 'settle' and el >= 4.0:
            if not QUEUE:
                return _finish('')
            ST['cur'] = QUEUE.pop(0)
            ST['rates'] = []
            name = ST['cur'].split(':')[-1]
            mp.net(60, 15, LOSS)
            g.reset_all()
            ST['prev'] = g._last_run.get('results')
            getattr(mp, 'scenario_' + name)()
            ST['msg'].append(f'started {ST["cur"]} at {now - ST["boot"]:.0f} s')
            ST.update(phase='run', t=now)
        elif ST['phase'] == 'run':
            res = g._last_run.get('results')
            cur = ST['cur']
            label = (cur.split(':')[-1] + ' (WARM-UP, not judged)') if cur.startswith('warm:') else cur
            if res is not ST['prev'] and res is not None:
                r = g._last_run
                bad = [f'{l} -> {d[:300]}' for ok, l, d in r['results'] if ok is False]
                notes = [f'{l} {d[:400]}'.strip() for ok, l, d in r['results'] if ok is None]
                RESULTS.append(f'{label}: {r["verdict"]}' + (f' FAILED {bad}' if bad else '') + ''.join(f'\n    {x}' for x in notes))
                RESULTS.append('    ' + _rates_note(mp._last.get('run')))
                ST.update(phase='settle', t=now)
            elif el > 150:
                RESULTS.append(f'{label}: TIMEOUT')
                ST.update(phase='settle', t=now)
        elif ST['phase'] == 'quit':
            if el > 1.0 and not ST.get('asked'):
                ST['asked'] = True
                for ci in (1, 2):
                    try:
                        mp.remote(ci, 'exec quit')
                    except Exception:
                        pass
            if el > 4.0:
                unreal.unregister_slate_post_tick_callback(ST['h'])
                unreal.SystemLibrary.execute_console_command(_game_world(), 'quit')
    except Exception as e:
        ST['msg'].append(f'EXCEPTION {e!r}')
        if ST['phase'] in ('settle', 'run', 'wait'):
            _finish(f'{ST.get("cur")}: EXCEPTION {e!r}')


ST['h'] = unreal.register_slate_post_tick_callback(_tick)
unreal.log(f'GSTN standalone host runner armed: {SCEN} loss {LOSS} -> {REPORT}')
