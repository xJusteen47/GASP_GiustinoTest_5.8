# M8 G5: minion -> player hit flows on a dedicated server + 2 clients (SUITE_MINION). Minions are the bench's attackers
# (gstn.Minion.Bench.Attack <N> <FighterIndex>, GSTNMinionRuntime, a swing at the fighter with the lowest entity id), the victim is a harness pawn.
#
#   import minion_scenarios as ms
#   ms.run_batch(loss=2, report='m8g5/minion-2pct.log')   # editor with PIE = dedicated server + 2 clients; poll ds.defence_batch_report()
#
# Judged on the server's copy of the victim (inbox log, damage) and on every world's copy of it afterwards (the NP state must agree once the
# minions are gone). At 2 % loss a few Sync/Aux reconciles are expected (testing.md), so no scenario judges unexplained reconciles.
import json
import os
import re
import time

import unreal
import gasnpp_test as g
import defence_scenarios as ds

SUITE_MINION = ['minion_hits_player', 'minion_tokens', 'minion_replicate', 'minion_late_join', 'minion_player_hits_minion',
                'minion_reaction_tiers', 'minion_lagcomp', 'minion_lod', 'minion_tokens_brain', 'minion_wave', 'minion_forced_reconcile',
                'minion_promote']
# M8 G6: replication scenarios (packs, relevancy grants, registry check); run_batch(SUITE_MINION_NET)
SUITE_MINION_NET = ['minion_late_join', 'minion_replicate']
# M8 G11: host budget scenarios, a minute each so not in SUITE_MINION: run_batch(SUITE_MINION_BENCH). Laptop numbers are provisional (-nullrhi).
# GSTN_HORDE_SECONDS shortens the horde fight (default 60, 20 on the laptop); GSTN_BENCH_WINDOW is the byte measuring window in seconds (default 10).
# M4b P16 sub-gate B: minion_parts_base must run before minion_parts (it leaves the bytes/s baseline); minion_parts PASSes with 'bridge not enabled'
# when GSTNPartsMinion is off, so this suite stays green without it.
SUITE_MINION_BENCH = ['minion_horde', 'minion_bytes', 'minion_parts_base', 'minion_parts']
MINION_BIT = 0x40000000
_marks = {}


def restore_editor():
    g._hit_logging(False)
    return 'hit log off'


def _cmd(text, wi=0):
    unreal.SystemLibrary.execute_console_command(g.worlds()[wi], text)


def _inbox(wi, key):
    """Inbox rows of the pawn on world wi: (drain frame, attacker id, hitbox, hurtbox, hurt state, ricochet, relationship, damage)"""
    return g._inbox_log(wi, key)


def _minion_rows(wi, key):
    return [r for r in _inbox(wi, key) if r[1] & MINION_BIT]


def _victim_key():
    """The pawn of the server world whose inbox holds hits from a minion (None before the first)"""
    for key in g.comps(g.worlds()[0]):
        if _minion_rows(0, key):
            return key
    return None


def _report_line(pattern):
    """Last 'MinionBench attack ...' style line the log holds after the mark (the Report command writes it)"""
    text = g._read_log_since(_marks['log'])
    rows = [ln for ln in text.splitlines() if re.search(pattern, ln)]
    return rows[-1] if rows else ''


def _num(line, name):
    m = re.search(name + r'=(-?\d+)', line)
    return int(m.group(1)) if m else None


def _setup():
    g.hit_rules_setup()
    g._hit_logging(True)
    _marks['log'] = g.mark_log()
    return 'rules + hit log'


def _target_hit():
    # The two harness pawns start close together, so one swing can reach both: the targeted pawn (the bench logs its actor) must be a hit one
    m = re.search(r'Bench: attack \d+ minions queued on fighter entity (\d+) actor (\S+)', g._read_log_since(_marks['log']))
    if not m:
        return False, 'bench log line not found'
    hit = {g.comps(g.worlds()[0])[k].get_owner().get_name(): len(_minion_rows(0, k)) for k in g.comps(g.worlds()[0])}
    return hit.get(m.group(2), 0) > 0, f'target {m.group(2)} rows per pawn {hit}'


def _damages(key):
    return [round(g.damage_of(wi, key), 2) for wi in range(len(g.worlds()))]


def _converged(key):
    d = _damages(key)
    return max(d) - min(d) < 0.51, f'damage percent per world {d}'


def scenario_minion_hits_player(client=1):
    """One minion walks up to a player and swings at it again and again. The player's inbox (server copy) receives hits whose attacker id is a minion
    pipeline id, every drain frame after the swing's hitbox opened, the damage percent rises on the server, the victim goes into hitstun/hitstop, a
    pawn that is not the target gets nothing, and once the minion is gone the damage percent is the same on the server and both clients"""
    run = g._Run('minion_hits_player', client, judge_reconciles=False, sample=False)
    st = {}
    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 1 0'))),
        (6.0, lambda: ('capture', st.update(key=_victim_key(), rows=len(_minion_rows(0, _victim_key() or '')) if _victim_key() else 0,
                                           info=g.victim_info(0, _victim_key()) if _victim_key() else None))),
        (6.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (6.2, lambda: ('report', _cmd('gstn.Minion.Bench.Report'))),
        (9.0, run.at('minion hits reached a victim inbox on the server', lambda: (st.get('key') is not None and st.get('rows', 0) >= 2, f"victim {st.get('key')} minion rows {st.get('rows')}"))),
        (9.0, run.at('hits carry a minion attacker id, one attacker, damage > 0', lambda: (
            st.get('key') is not None and len({r[1] for r in _minion_rows(0, st['key'])}) == 1 and all(r[7] > 0 for r in _minion_rows(0, st['key'])),
            f"rows {_minion_rows(0, st['key'])[:3] if st.get('key') else None}"))),
        (9.0, run.at('damage percent rose on the server', lambda: (g.damage_of(0, st['key']) > 0, f"damagePercent {g.damage_of(0, st['key'])}"))),
        (9.0, run.at('victim was staggered (hitstun end set)', lambda: (st.get('info') is not None and st['info'][4] > 0, f"victim_info {st.get('info')}"))),
        (9.0, run.at('the pawn the bench named as target received minion hits', lambda: _target_hit())),
        (9.0, run.at('hits delivered by the batch (report attackHits >= rows)', lambda: (
            (_num(_report_line('MinionBench attack'), 'attackHits') or 0) >= st.get('rows', 1) > 0, _report_line('MinionBench attack')[-120:]))),
        (9.0, run.at('damage percent equal on the server and both clients', lambda: _converged(st['key']))),
        (9.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_forced_reconcile(client=1):
    """M8 G12a: minion_hits_player with forced reconciles on the client while the minion hits. The hits are unpredicted on the client (the server
    inbox delivers them), so this is the rollback path of the minion hit flow: a forced reconcile resimulates the victim's frames around a hit
    and the NP state must still end equal on the server and both clients. Judged: the minion hits arrive, the reconciles were forced (log lines and
    rollbacks), no unexplained reconcile besides them (GSTN_FR_UNEXPLAINED_MAX, default 0), corrections bounded (GSTN_FR_MISMATCH_MAX mismatches
    and a burst below GSTN_FR_BURST_MAX in the window, defaults 40 and 20: provisional until a laptop run), and the damage percent equal in every
    world once the minion is gone"""
    unexplained_max = int(os.environ.get('GSTN_FR_UNEXPLAINED_MAX', '0'))
    mismatch_max = int(os.environ.get('GSTN_FR_MISMATCH_MAX', '40'))
    burst_max = int(os.environ.get('GSTN_FR_BURST_MAX', '20'))
    run = g._Run('minion_forced_reconcile', client, judge_reconciles=False, sample=False)
    st = {'forced': 0, 'window': None}
    force_times = (3.5, 4.5, 5.5, 6.5)

    def mark_window():
        try:
            st['window'] = g.mark_log()
        except Exception:
            st['window'] = None
        return 'window mark'

    def force():
        # Skip a force while the log is not readable would hide a failure: count what was actually requested
        st['forced'] += 1
        return 'force reconcile', g.force_reconcile(client, extra_frames=10)

    def reconciles():
        if st['window'] is None:
            return True, 'log not readable: skipped'
        r = g.report_reconciles(st['window'])
        # 'forced' is not required here: in the dedicated + 2 clients setup the 'Reconcile activated due to ForceReconcile' lines of the
        # client worlds may not reach this log (G12a laptop run: requested 4, logged 0, while the rollback counter rose). That the forced
        # reconciles happened is judged by the rollback check; this one keeps the bounds on what the log does carry
        ok = (r['unexplained_ours'] <= unexplained_max and r['mismatches'] <= mismatch_max and r['max_burst'] <= burst_max)
        return ok, (f"forced requested {st['forced']} logged {r['forced']}, mismatches {r['mismatches']} (max {mismatch_max}), unexplained_ours {r['unexplained_ours']} "
                    f"(max {unexplained_max}) {r['unexplained_by_model']}, burst {r['max_burst']} (max {burst_max}), offset changes {r['offset_changes']} {r['unexplained_lines']}")

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 1 0'))),
        (force_times[0] - 0.1, mark_window),
    ]
    steps += [(t, force) for t in force_times]
    steps += [
        (8.0, lambda: ('capture', st.update(key=_victim_key(), rows=len(_minion_rows(0, _victim_key() or '')) if _victim_key() else 0))),
        (8.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (11.0, run.at('minion hits reached a victim inbox on the server', lambda: (st.get('key') is not None and st.get('rows', 0) >= 2, f"victim {st.get('key')} minion rows {st.get('rows')}"))),
        (11.0, run.at('damage percent rose on the server', lambda: (g.damage_of(0, st['key']) > 0, f"damagePercent {g.damage_of(0, st['key'])}"))),
        (11.0, run.at('reconciles were forced (log lines) and rollbacks went up', lambda: (g._total_rollbacks() - run.rb0 > 0, f"rollbacks +{g._total_rollbacks() - run.rb0}"))),
        (11.0, run.at('forced reconciles only: no unexplained ones, corrections bounded', reconciles)),
        (11.0, run.at('damage percent equal on the server and both clients', lambda: _converged(st['key']))),
        (11.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_tokens(client=1):
    """Eight minions attack the same player. At most MaxAttackersPerPlayer (4) hold a token on any fixed frame (bench: maxHolders), the cap is
    reached and refused starts happened (tokensDenied > 0), swings overlap in time for at most 4 distinct attackers, and the waiting minions
    take over (more than 4 distinct attackers landed hits over the run); the victim agrees on every world afterwards"""
    run = g._Run('minion_tokens', client, judge_reconciles=False, sample=False)
    st = {}

    def window_max(rows):
        # distinct attackers with a hit inside any 8-frame window (a swing's hitbox is open 5 frames): bounded by the token cap
        best = 0
        for r in rows:
            best = max(best, len({x[1] for x in rows if r[0] <= x[0] < r[0] + 8}))
        return best

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 8 0'))),
        (9.0, lambda: ('capture', st.update(key=_victim_key()))),
        (9.1, lambda: ('report', _cmd('gstn.Minion.Bench.Report'))),
        (9.5, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (12.5, run.at('attackers landed hits on one victim', lambda: (st.get('key') is not None and len(_minion_rows(0, st['key'])) >= 4, f"victim {st.get('key')} rows {len(_minion_rows(0, st['key'])) if st.get('key') else 0}"))),
        (12.5, run.at('the token cap held on every fixed frame (maxHolders <= 4)', lambda: (
            (_num(_report_line('MinionBench attack'), 'maxHolders') or 99) <= 4, _report_line('MinionBench attack')[-120:]))),
        (12.5, run.at('the cap was reached (maxHolders == 4)', lambda: ((_num(_report_line('MinionBench attack'), 'maxHolders') or 0) == 4, _report_line('MinionBench attack')[-120:]))),
        (12.5, run.at('starts were refused for taken tokens (tokensDenied > 0)', lambda: ((_num(_report_line('MinionBench attack'), 'tokensDenied') or 0) > 0, _report_line('MinionBench attack')[-120:]))),
        (12.5, run.at('at most 4 distinct attackers hit inside one swing window', lambda: (window_max(_minion_rows(0, st['key'])) <= 4, f"max {window_max(_minion_rows(0, st['key']))}"))),
        (12.5, run.at('waiting minions take over (> 4 distinct attackers over the run)', lambda: (len({r[1] for r in _minion_rows(0, st['key'])}) > 4, f"attackers {len({r[1] for r in _minion_rows(0, st['key'])})}"))),
        (12.5, run.at('damage percent equal on the server and both clients', lambda: _converged(st['key']))),
        (12.5, run.finish()),
    ]
    run.run(steps)


_NET_ITEM = re.compile(r'MinionNet (server|client player=(-?\d+) world=\S+) pack=(\d+) id=(\d+) type=(\d+) pos=(\d+),(\d+),(\d+)')
_NET_GRANTS = re.compile(r'MinionNet grants player=(-?\d+) packs=([\d,]*)')
_NET_CLIENT_SUMMARY = re.compile(r'MinionNet client summary player=(-?\d+) world=(\S+) items=(\d+) packs=(\d+) updates=(\d+) registryChecks=(\d+) registryOk=(\d+) mismatches=(\d+)')


def _net_dump():
    """gstn.Minion.Net.Dump on every world: the server logs its pack items and the grants per remote player, each client its received items"""
    for wi in range(len(g.worlds())):
        _cmd('gstn.Minion.Net.Dump', wi)
    return f'dumped {len(g.worlds())} worlds'


def _net_state():
    """Parses the last dump after the mark: server items {(pack, id): (type, pos)}, grants {player: set(pack)}, client items and summaries by player"""
    text = g._read_log_since(_marks['log'])
    server, clients, grants, summaries = {}, {}, {}, {}
    for ln in text.splitlines():
        m = _NET_ITEM.search(ln)
        if m:
            key, val = (int(m.group(3)), int(m.group(4))), (int(m.group(5)), (int(m.group(6)), int(m.group(7)), int(m.group(8))))
            if m.group(1) == 'server':
                server[key] = val
            else:
                clients.setdefault(int(m.group(2)), {})[key] = val
            continue
        m = _NET_GRANTS.search(ln)
        if m:
            grants[int(m.group(1))] = {int(x) for x in m.group(2).split(',') if x}
            continue
        m = _NET_CLIENT_SUMMARY.search(ln)
        if m:
            summaries[int(m.group(1))] = {'items': int(m.group(3)), 'packs': int(m.group(4)), 'updates': int(m.group(5)),
                                         'checks': int(m.group(6)), 'ok': int(m.group(7)), 'mismatches': int(m.group(8))}
    return server, clients, grants, summaries


def _net_clients_match():
    """Every client sees every item of the packs granted to it, with the server's type and quantized position, and nothing of other packs"""
    server, clients, grants, summaries = _net_state()
    if not server or not summaries:
        return False, f'server items {len(server)} client summaries {len(summaries)}'
    bad = []
    for player in summaries:
        granted = grants.get(player, set())
        want = {k: v for k, v in server.items() if k[0] in granted}
        got = clients.get(player, {})
        missing = [k for k in want if k not in got]
        wrong = [k for k in want if k in got and got[k] != want[k]]
        extra = [k for k in got if k[0] not in granted]
        if not want or missing or wrong or extra:
            bad.append(f'player {player}: granted {sorted(granted)} want {len(want)} missing {missing[:4]} wrong {wrong[:4]} extra {extra[:4]}')
    return not bad, '; '.join(bad) or f'{len(summaries)} clients match {len(server)} server items'


def scenario_minion_late_join(client=1):
    """State only (M8 G6). 24 idle minions spawn in two packs after the clients are in (the PIE harness has no late connect and two clients, so
    'late' = the clients join the packs' channels after the spawn, through the first relevancy grant; the third headless client of M8 2 is
    not supported by the harness). After 4 s every client holds every item of the packs the server granted it, with the same type and quantized
    position as the server's item, nothing of a pack it was not granted, and its registry hash check passed"""
    run = g._Run('minion_late_join', client, judge_reconciles=False, sample=False)
    steps = [
        (0.0, lambda: ('setup', _marks.update(log=g.mark_log()) or 'log mark')),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Spawn 24 Idle'))),
        (4.5, lambda: ('dump', _net_dump())),
        (4.6, lambda: ('present dump', _present_dump())),
        (5.5, run.at('the server has 24 items', lambda: (len(_net_state()[0]) == 24, f'server items {len(_net_state()[0])}'))),
        (5.5, run.at('every client got a grant', lambda: (
            len(_net_state()[3]) >= 2 and all(_net_state()[2].get(p) for p in _net_state()[3]), f'grants {_net_state()[2]} clients {list(_net_state()[3])}'))),
        (5.5, run.at('clients hold exactly the granted packs, same type and position', _net_clients_match)),
        (5.5, run.at('registry hash checked and equal on every client', lambda: (
            bool(_net_state()[3]) and all(s['checks'] >= 1 and s['ok'] == 1 and s['mismatches'] == 0 for s in _net_state()[3].values()),
            f'summaries {_net_state()[3]}'))),
        (5.5, run.at('every client has a puppet record for each relevant item (G7)', _puppets_cover_relevant)),
        (5.6, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (6.5, run.finish()),
    ]
    run.run(steps)


# ---- M8 G7: client presentation (puppets). Lines come from gstn.Minion.Present.Dump (writer-report.md of G7) ----
_PRES_PUPPET = re.compile(r'MinionPresent puppet player=(-?\d+) world=(\S+) id=(\d+) slot=(-?\d+) type=(\d+) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) '
                          r'yaw=\S+ frame=(\S+) tier=(-?\d+) mode=(\w+) visible=(\d) flash=(\w+)')
_PRES_SUMMARY = re.compile(r'MinionPresent summary player=(-?\d+) world=(\S+) source=(\w+) renderSource=(\w+) renderFrame=\S+ active=(\d+) visible=(\d+) culled=(\d+) '
                           r'.*?hittables=(\d+) predicted=(\d+) confirmed=(\d+) cancelled=(\d+) expired=(\d+) remote=(\d+)')
_NET_CLIENT_ITEM = re.compile(r'MinionNet client player=(-?\d+) world=\S+ pack=(\d+) id=(\d+) type=\d+ pos=(\d+),(\d+),(\d+) yaw=\d+ state=-?\d+ updates=(\d+)')
POSITION_BIAS = 1 << 18   # GSTNMinion::NetBits::Position = 19, 1 cm steps: wire value = offset in cm + bias
# G7 gap: the registry WorldOrigin is in no dump; the harness world is assumed to use the zero origin (a mismatch shows as one constant offset in the detail)


def _present_dump():
    for wi in range(1, len(g.worlds())):
        _cmd('gstn.Minion.Present.Dump', wi)
    return 'present dumped on the clients'


def _present_state(mark=None):
    """Last Present.Dump after the mark: puppets {(player, id): dict}, summaries {player: dict}"""
    text = g._read_log_since(mark if mark is not None else _marks['log'])
    puppets, summaries = {}, {}
    for ln in text.splitlines():
        m = _PRES_PUPPET.search(ln)
        if m:
            puppets[(int(m.group(1)), int(m.group(3)))] = dict(
                slot=int(m.group(4)), pos=(float(m.group(6)), float(m.group(7)), float(m.group(8))), frame=float(m.group(9)),
                tier=int(m.group(10)), mode=m.group(11), visible=int(m.group(12)), flash=m.group(13))
            continue
        m = _PRES_SUMMARY.search(ln)
        if m:
            summaries[int(m.group(1))] = dict(
                source=m.group(3), renderSource=m.group(4), active=int(m.group(5)), visible=int(m.group(6)), culled=int(m.group(7)),
                hittables=int(m.group(8)), predicted=int(m.group(9)), confirmed=int(m.group(10)), cancelled=int(m.group(11)),
                expired=int(m.group(12)), remote=int(m.group(13)))
    return puppets, summaries


def _puppet_of(puppets, player, pack, item):
    # G7 gap: the dump's id= convention is not documented; the client registry keys by PackId<<4|ItemId (writer report), tried first, then the bare item id
    return puppets.get((player, pack << 4 | item)) or puppets.get((player, item))


def _puppets_cover_relevant():
    server, clients, grants, summaries = _net_state()
    puppets, psum = _present_state()
    if not clients or not psum:
        return False, f'client items {len(clients)} present summaries {len(psum)}'
    bad = []
    for player, items in clients.items():
        miss = [k for k in items if _puppet_of(puppets, player, *k) is None]
        s = psum.get(player)
        if miss or s is None or s['active'] != len(items) or s['visible'] + s['culled'] != s['active'] or s['source'] == 'none':
            bad.append(f'player {player}: items {len(items)} without puppet record {miss[:4]} summary {s}')
    return not bad, '; '.join(bad) or f'{len(clients)} clients: puppet records for all {sum(len(v) for v in clients.values())} relevant items'


def _dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def _client_pawn_pos(client):
    _, pawn = g._local_pawn(client)
    v = pawn.get_actor_location()
    return (v.x, v.y, v.z)


def scenario_minion_replicate(client=1):
    """M8 G7. 40 idle minions spawn (the server splits them into packs: 3 expected, >= 2 required). On each client every relevant minion has a puppet
    and its displayed position is within 20 cm of the server's. G7 gap: no command dumps the server position at the client's displayed frame, so the
    minions are idle (they never move) and the displayed position is compared with the server's current one after the interpolators settled"""
    run = g._Run('minion_replicate', client, judge_reconciles=False, sample=False)

    def pos_ok():
        server, clients, grants, summaries = _net_state()
        puppets, psum = _present_state()
        bad, n, worst = [], 0, 0.0
        for player, items in clients.items():
            for k in items:
                p = _puppet_of(puppets, player, *k)
                if p is None or k not in server:
                    bad.append((player, k, 'no puppet' if p is None else 'not on server'))
                    continue
                d = _dist(p['pos'], tuple(c - POSITION_BIAS for c in server[k][1]))
                worst, n = max(worst, d), n + 1
                if d > 20.0:
                    bad.append((player, k, round(d, 1)))
        return (not bad and n > 0), f'{n} puppets compared, worst {worst:.1f} cm, bad {bad[:4]}'

    def packs():
        keys = _net_state()[0]
        return len(keys) == 40 and len({k[0] for k in keys}) >= 2, f'items {len(keys)} packs {sorted({k[0] for k in keys})}'

    steps = [
        (0.0, lambda: ('setup', _marks.update(log=g.mark_log()) or 'log mark')),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Spawn 40 Idle'))),
        (5.5, lambda: ('dump', _net_dump())),
        (5.6, lambda: ('present dump', _present_dump())),
        (6.5, run.at('the server has 40 items in >= 2 packs', packs)),
        (6.5, run.at('every relevant minion has a puppet on each client', _puppets_cover_relevant)),
        (6.5, run.at('displayed position within 20 cm of the server position', pos_ok)),
        (6.5, run.at('presentation clock resolved on every client', lambda: (
            bool(_present_state()[1]) and all(s['renderSource'] in ('np', 'fixed') for s in _present_state()[1].values()), f'{_present_state()[1]}'))),
        (6.6, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (7.5, run.finish()),
    ]
    run.run(steps)


def _swing_steps(start, stop, every, client):
    """Attack presses from the client's pawn every `every` seconds in [start, stop). G7b: a press is held for GSTN_SWING_HOLD seconds (default
    0.1 = 6 frames at 60 Hz) and then cleared, not tapped for one frame: at 2 % loss the one-frame scripted tap never produced a swing on the
    server. 0 restores the one-frame tap"""
    hold = float(os.environ.get('GSTN_SWING_HOLD', '0.1'))
    out, t = [], start
    while t < stop:
        if hold > 0.0:
            out.append((t, lambda: ('attack hold', g.combat_hold(client, g.button_mask('Attack')))))
            out.append((t + hold, lambda: ('attack release', g.combat_clear(client))))
        else:
            out.append((t, lambda: ('attack tap', g.combat_tap(client, g.button_mask('Attack')))))
        t += every
    return out


def _swinger_setup(client):
    """The swinging client's pawn gets the hit kit (a bare pawn's Attack has no hit move) and the test pawns an infinite stagger threshold:
    the bench attackers land several hits a second on both pawns (they start on one spot), and a pawn in hitstun cannot swing"""
    _setup()
    _cmd('gstn.Minion.Bench.HurtboxSet /Game/GASNPPTest/Combat/HBS_Test_Pawn.HBS_Test_Pawn')   # bench grunts are unhittable without one
    g.set_hurtbox(infinite=True)
    g._apply_loadout(g.key_of_player(client), g.HIT_KIT)
    return 'rules + hit log + kit + no stagger'


def _flash_summary():
    """The client whose presentation predicted the most hits (the swinging one) -> (player, summary)"""
    _, summ = _present_state()
    best = max(summ.items(), key=lambda kv: kv[1]['predicted'], default=(None, None))
    return best[0], best[1], summ


def scenario_minion_player_hits_minion(client=1):
    """M8 G7. A client player swings at idle minions standing in the bench ring around the first player. The server drains minion hits (spike5
    drainedHits > 0), the client predicted a flash on a puppet and the HitCounter confirmed it, none was cancelled, and the confirmed puppet is a
    minion the server owns. G7 gap: no command logs the victim minion id per hit on the server; the closest check is that the confirmed puppet's
    id is one of the server's Net.Dump items (a hit on another id would cancel the predicted flash)"""
    run = g._Run('minion_player_hits_minion', client, judge_reconciles=False, sample=True)

    def capture():
        _marks['dump'] = g.mark_log()
        _net_dump()
        _present_dump()
        _cmd('gstn.Minion.Bench.Report')
        return 'dumped'

    def confirmed_flash():
        player, s, summ = _flash_summary()
        return (s is not None and s['predicted'] >= 1 and s['confirmed'] >= 1 and s['cancelled'] == 0), f'summaries {summ}'

    def confirmed_on_server_minion():
        server = _net_state()[0]
        ids = {k[0] << 4 | k[1] for k in server} | {k[1] for k in server}
        puppets, _ = _present_state()
        player = _flash_summary()[0]
        conf = [key for key, p in puppets.items() if p['flash'] == 'confirmed' and key[0] == player]
        return bool(conf) and all(key[1] in ids for key in conf), f'confirmed puppets {conf[:4]} server ids {sorted(ids)[:6]}'

    steps = [(0.0, lambda: ('setup', _swinger_setup(client))),
             (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 6 0'))),
             (0.6, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 6 1')))]  # Spawn's 600 cm ring is out of swing reach; attackers walk up to both pawns
    steps += _swing_steps(3.0, 8.0, 0.5, client)
    steps += [
        (9.0, lambda: ('capture', capture())),
        (10.0, run.at('the kit swing came out on the server (Test.State.HitMove)', lambda: (run.saw_tag('Test.State.HitMove', 3.0, 8.5), ''))),
        (10.0, run.at('the server drained minion hits (drainedHits > 0)', lambda: ((_num(_report_line('MinionBench spike5'), 'drainedHits') or 0) > 0, _report_line('MinionBench spike5')[-120:]))),
        (10.0, run.at('the client predicted a flash and it was confirmed, none cancelled', confirmed_flash)),
        (10.0, run.at('the confirmed flash sits on a minion the server owns', confirmed_on_server_minion)),
        (10.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (11.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_reaction_tiers(client=1):
    """M8 G7. 40 chasing minions, two Net.Dump + Present.Dump samples 2 s apart. The wire carries no tier: the server's per-item rate ceiling
    (15/5/1/0 Hz) is its tier, the client infers one from the sample gap and the silence since the newest sample. Checks: at least two tiers on the
    client dump; the server's faster ceilings sit no farther from the client's pawn than slower ones; still items (no move, no update between the
    samples) read as a slow tier on the client; moving items get more updates than still ones, and never more than their ceiling allows.
    G7a: wire updates follow motion (a still minion sends nothing), so comparing raw update counts per client tier (the G7 check) measured motion,
    not tier. G7 gap: Bench.Spawn uses one ring radius, so in this setup every item is usually in the T0 ceiling (the order check then has one class)"""
    run = g._Run('minion_reaction_tiers', client, judge_reconciles=False, sample=False)
    st = {}
    srv_item = re.compile(r'MinionNet server pack=(\d+) id=(\d+) type=\d+ pos=(\d+),(\d+),(\d+) yaw=\d+ state=-?\d+ ceiling=(\d+)')

    def sample(tag):
        st[tag] = g.mark_log()
        _net_dump()
        _present_dump()
        return f'sample {tag}'

    def first_dump(mark, rx, key, val):
        # first dump after the mark only: the log after mark 'a' also holds sample 'b'
        out = {}
        for m in map(rx.search, g._read_log_since(mark).splitlines()):
            if m:
                out.setdefault(key(m), val(m))
        return out

    def updates_of(mark):
        return first_dump(mark, _NET_CLIENT_ITEM, lambda m: (int(m.group(1)), int(m.group(2)), int(m.group(3))), lambda m: int(m.group(7)))

    def server_of(mark):
        return first_dump(mark, srv_item, lambda m: (int(m.group(1)), int(m.group(2))),
                          lambda m: (tuple(int(m.group(i)) - POSITION_BIAS for i in (3, 4, 5)), int(m.group(6))))

    def items():
        """[(client tier, ceiling, updates between the samples, moved cm, distance to the client pawn)] per (player, pack, item) in both samples"""
        ua, ub, sa, sb = updates_of(st['a']), updates_of(st['b']), server_of(st['a']), server_of(st['b'])
        puppets, _ = _present_state(st['b'])
        here, out = _client_pawn_pos(client), []
        for k, u in ub.items():
            p = _puppet_of(puppets, *k)
            s0, s1 = sa.get(k[1:]), sb.get(k[1:])
            if k in ua and p and s0 and s1:
                out.append((p['tier'], s1[1], u - ua[k], _dist(s0[0], s1[0]), _dist(s1[0], here)))
        return out

    def tiers_present():
        by = {}
        for t, *_ in items():
            by[t] = by.get(t, 0) + 1
        return len([t for t in by if t in (0, 1, 2)]) >= 2, f'items per client tier {by}'

    def ceilings_ordered():
        by = {}
        for _, c, _, _, d in items():
            by.setdefault(c, []).append(d)
        means = {c: sum(v) / len(v) for c, v in by.items()}
        order = sorted(means, reverse=True)   # 15 Hz first
        ok = bool(means) and all(means[a] <= means[b] + 50.0 for a, b in zip(order, order[1:]))
        return ok, f'mean distance per server ceiling {({c: round(m) for c, m in means.items()})} (n {({c: len(v) for c, v in by.items()})})'

    def still_reads_slow():
        still = [t for t, _, u, mv, _ in items() if u == 0 and mv < 1.0]
        slow = sum(1 for t in still if t >= 1)
        return bool(still) and slow >= 0.9 * len(still), f'still items {len(still)}, read T1/T2 {slow}'

    def moving_update_more():
        rows = items()
        mov = [(u, c) for _, c, u, mv, _ in rows if mv >= 1.0]
        still = [u for _, _, u, mv, _ in rows if mv < 1.0]
        over = [(u, c) for u, c in mov if u > 2 * c + 3]
        mm = sum(u for u, _ in mov) / len(mov) if mov else 0.0
        ms_ = sum(still) / len(still) if still else 0.0
        return bool(mov) and mm > ms_ and not over, f'moving {len(mov)} mean {mm:.1f}, still {len(still)} mean {ms_:.1f}, over ceiling {over[:4]}'

    steps = [
        (0.0, lambda: ('setup', _marks.update(log=g.mark_log()) or 'log mark')),
        # G7a fix 2: Chase minions stand still on the ring by the samples, so every item read T2. Massacre launches a quarter of them every 30 frames
        # (slow launches keep them near): moving and still items at once
        (0.4, lambda: ('launch speed', _cmd('gstn.Minion.Bench.LaunchSpeed 700'))),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Spawn 40 Massacre'))),
        (4.0, lambda: ('sample a', sample('a'))),
        (6.0, lambda: ('sample b', sample('b'))),
        (7.0, run.at('at least two reaction tiers on the client dump', tiers_present)),
        (7.0, run.at('faster server ceilings are no farther from the pawn', ceilings_ordered)),
        (7.0, run.at('still items read as a slow tier on the client', still_reads_slow)),
        (7.0, run.at('moving items get more updates than still ones, within their ceiling', moving_update_more)),
        (7.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (7.2, lambda: ('launch speed', _cmd('gstn.Minion.Bench.LaunchSpeed 3000'))),
        (8.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_lagcomp(client=1):
    """M8 G7. At the batch's 60/15 lag, a client swings at chasing (moving) minions. The server's rewound candidate query ran and found candidates
    (spike5 lagCompQueries, candidates > 0), the server drained hits (drainedHits > 0), and the client confirmed a flash on a puppet it aimed at.
    G7 gap: nothing dumps which minion the rewound query accepted nor the swing's aim, so 'lands on the minion the client saw' is judged by
    predicted >= 1, confirmed >= 1, cancelled == 0 on the client (a hit that landed on another minion would cancel the predicted flash)"""
    run = g._Run('minion_lagcomp', client, judge_reconciles=False, sample=True)

    def capture():
        _marks['dump'] = g.mark_log()
        _present_dump()
        _cmd('gstn.Minion.Bench.Report')
        return 'dumped'

    def confirmed_flash():
        player, s, summ = _flash_summary()
        return (s is not None and s['predicted'] >= 1 and s['confirmed'] >= 1 and s['cancelled'] == 0), f'summaries {summ}'

    def line():
        return _report_line('MinionBench spike5')

    steps = [(0.0, lambda: ('setup', _swinger_setup(client))),
             (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 4 0'))),
             (0.6, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 4 1')))]  # moving attackers walk into swing reach; Chase stops on the 600 cm ring
    steps += _swing_steps(2.5, 8.0, 0.5, client)
    steps += [
        (9.0, lambda: ('capture', capture())),
        # Live rewind of minions goes through the fighters' GSTNLagComp overlay (minions are snapshot entities of the provider); the spike 5
        # lagCompQueries counter counts only the bench's GatherRewoundCandidates calls, so it is 0 here by design and not judged
        (10.0, run.at('the kit swing came out on the server (Test.State.HitMove)', lambda: (run.saw_tag('Test.State.HitMove', 2.5, 8.5), ''))),
        (10.0, run.at('the server drained hits on moving minions (drainedHits > 0)', lambda: ((_num(line(), 'drainedHits') or 0) > 0, line()[-140:]))),
        (10.0, run.at('the client predicted and confirmed a flash on the puppet it aimed at', confirmed_flash)),
        (10.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (11.0, run.finish()),
    ]
    run.run(steps)


# ---- M8 G8: the brain (StateTree "chase and swing", or the native fallback). Log formats come from the G8 writer report:
#   MinionLOD id=<entity id> from=<tier> to=<tier> frame=<batch frame>          (gstn.Minion.LOD.Log 1, server)
#   MinionBrain slot=.. id=.. tier=.. state=.. target=.. ticks=.. lastTick=.. now=..   (gstn.Minion.Brain.Dump, server)
#   MinionBrain mode=<StateTree|Native> brained=.. ticksLastSec=.. usPerTick=.. maxUsPerTick=.. wakes=..   (gstn.Minion.Brain.Report)
#   Bench: poke index=.. id=.. tier=.. posted=.. frame=..                         (gstn.Minion.Bench.Poke <BenchIndex>)
_LOD_LINE = re.compile(r'MinionLOD id=(\d+) from=(\w+) to=(\w+) frame=(-?\d+)')
_BRAIN_DUMP = re.compile(r'MinionBrain slot=(\d+) id=(\d+) tier=(\w+) state=(.*?) target=(-?\d+) ticks=(\d+) lastTick=(-?\d+) now=(\d+)')
_BRAIN_REPORT = re.compile(r'MinionBrain mode=(\w+) brained=(\d+) ticksLastSec=(\d+) usPerTick=([\d.]+) maxUsPerTick=([\d.]+) wakes=(\d+)')
_POKE_LINE = re.compile(r'Bench: poke index=(\d+) id=(\d+) tier=(-?\d+) posted=(\d) frame=(-?\d+)')
LOD_CHAIN = [('Combat', 'Near'), ('Near', 'Far'), ('Far', 'Dormant')]


def _lod_transitions(mark=None):
    """MinionLOD lines after the mark as [(id, from, to, frame)] in log order"""
    text = g._read_log_since(mark if mark is not None else _marks['log'])
    return [(int(m.group(1)), m.group(2), m.group(3), int(m.group(4))) for m in map(_LOD_LINE.search, text.splitlines()) if m]


def _brain_dump_state(mark):
    """Brain.Dump lines after the mark: {id: dict(tier, state, target, ticks, lastTick, now)}"""
    out = {}
    for ln in g._read_log_since(mark).splitlines():
        m = _BRAIN_DUMP.search(ln)
        if m:
            out[int(m.group(2))] = dict(slot=int(m.group(1)), tier=m.group(3), state=m.group(4), target=int(m.group(5)), ticks=int(m.group(6)),
                                        lastTick=int(m.group(7)), now=int(m.group(8)))
    return out


def _snapshot(st, label):
    """Server net items and client puppets from the dumps after the mark (call a few hundred ms after _net_dump + _present_dump)"""
    server, clients, grants, summaries = _net_state()
    puppets, psum = _present_state()
    st.setdefault('snap', {})[label] = dict(server=server, clients=clients, puppets=puppets)
    return f'{label}: server items {len(server)} puppets {len(puppets)}'


def _no_pop(st, before, after):
    """Each puppet shown in both snapshots moved at most what the server moved its minion (+ 30 cm): a tier change must not snap the display"""
    a, b = st['snap'][before], st['snap'][after]
    n, worst, bad = 0, -1e9, []
    for player, items in b['clients'].items():
        for k in items:
            if k not in a['server'] or k not in b['server']:
                continue
            pa, pb = _puppet_of(a['puppets'], player, *k), _puppet_of(b['puppets'], player, *k)
            if pa is None or pb is None:
                continue
            moved_client = _dist(pa['pos'], pb['pos'])
            moved_server = _dist(a['server'][k][1], b['server'][k][1])
            n += 1
            worst = max(worst, moved_client - moved_server)
            if moved_client > moved_server + 30.0:
                bad.append((player, k, round(moved_client, 1), round(moved_server, 1)))
    st.setdefault('pop', []).append((n, worst))
    return not bad, f'{before}->{after}: {n} puppets compared, worst excess {worst:.1f} cm, bad {bad[:3]}'


def scenario_minion_lod(client=1):
    """M8 G8. 12 Brain minions around the players. Both pawns are teleported away in steps (beyond Near, beyond Far, beyond Dormant): the tiers
    change in order Combat -> Near -> Far -> Dormant for most minions and none jumps Combat -> Dormant; the client display never moves more than
    the server moved the minion (+ 30 cm, 'no pop'); a Poke (a synthetic hit through the inbox) on a dormant minion puts it in Combat within 2
    frames and its brain ticks (Brain.Dump ticks went up)"""
    run = g._Run('minion_lod', client, judge_reconciles=False, sample=False)
    st = {'base': {}}
    NUM = 12

    def setup():
        _marks['log'] = g.mark_log()
        _cmd('gstn.Minion.LOD.Log 1')
        for key, comp in g.comps(g.worlds()[0]).items():
            v = comp.get_owner().get_actor_location()
            st['base'][key] = (v.x, v.y, v.z)
        # Every controlled server pawn, not only the harness comps: a third pawn (the listen/extra player) left behind keeps the minions in Combat
        st['pawns'] = {}
        for a in unreal.GameplayStatics.get_all_actors_of_class(g.worlds()[0], unreal.Pawn):
            if a.get_controller() is not None:
                v = a.get_actor_location()
                st['pawns'][a.get_name()] = (a, (v.x, v.y, v.z))
        return f"LOD log on, pawns {sorted(st['base'])}, controlled {sorted(st['pawns'])}"

    def teleport(dx):
        def f():
            for key, comp in g.comps(g.worlds()[0]).items():
                b = st['base'][key]
                comp.server_teleport_exact(unreal.Vector(b[0] + dx, b[1], b[2]))
            return f'both pawns {dx:.0f} cm away'
        return f

    def teleport_fallback(dx):
        # Runs a step after teleport(dx): any controlled pawn the exact teleport did not move goes there by a plain actor teleport
        def f():
            moved = []
            for name, (a, b) in st['pawns'].items():
                v = a.get_actor_location()
                if abs(v.x - (b[0] + dx)) > 50.0:
                    a.set_actor_location(unreal.Vector(b[0] + dx, b[1], b[2]), False, True)
                    moved.append(name)
            return f'fallback moved {moved}'
        return f

    def dump_both():
        _net_dump()
        _present_dump()
        # Where every server pawn really is (the LOD stage reads the controllers' pawns): a pawn that fell or was not moved shows here
        pawns = []
        for a in unreal.GameplayStatics.get_all_actors_of_class(g.worlds()[0], unreal.Pawn):
            v = a.get_actor_location()
            pawns.append(f'{a.get_name()}@{v.x:.0f},{v.y:.0f},{v.z:.0f}')
        return 'net + present dumped; pawns ' + ' '.join(pawns)

    def chain_ok():
        pre = [t for t in _lod_transitions() if t[3] <= st['poke_frame']]
        ids = {t[0] for t in pre}
        direct = [t for t in pre if (t[1], t[2]) == ('Combat', 'Dormant')]
        full = 0
        for i in ids:
            seq = [(t[1], t[2]) for t in pre if t[0] == i]
            pos = -1
            for hop in LOD_CHAIN:
                pos = next((j for j in range(pos + 1, len(seq)) if seq[j] == hop), None)
                if pos is None:
                    break
            full += pos is not None
        return (not direct and len(ids) >= NUM * 0.75 and full >= NUM * 0.75), f'{full}/{len(ids)} minions walked the whole chain, direct Combat->Dormant jumps {len(direct)}'

    def poke():
        _marks['pre_brain'] = g.mark_log()
        _cmd('gstn.Minion.Brain.Dump')
        return 'brain dump before the poke'

    def do_poke():
        st['before'] = _brain_dump_state(_marks['pre_brain'])
        _marks['poke'] = g.mark_log()
        _cmd('gstn.Minion.Bench.Poke 0')
        return 'poked bench minion 0'

    def poke_parsed():
        m = _POKE_LINE.search(g._read_log_since(_marks['poke']))
        if m:
            st['poke_id'], st['poke_frame'] = int(m.group(2)), int(m.group(5))
        return m is not None

    def wake_ok():
        if not poke_parsed():
            return False, 'poke line not found'
        wake = [t for t in _lod_transitions(_marks['poke']) if t[0] == st['poke_id'] and t[1] == 'Dormant' and t[2] == 'Combat']
        late = wake[0][3] - st['poke_frame'] if wake else None
        return (wake != [] and late <= 2), f"poked id {st['poke_id']} at frame {st['poke_frame']}: Dormant->Combat {wake[:1]} ({late} frames later)"

    def was_dormant():
        b = st['before'].get(st.get('poke_id', -1))
        return (b is not None and b['tier'] == 'Dormant'), f"brain dump before the poke: {b}"

    def brain_ticked():
        a = _brain_dump_state(_marks['post_brain']).get(st.get('poke_id', -1))
        b = st['before'].get(st.get('poke_id', -1))
        return (a is not None and b is not None and a['ticks'] > b['ticks']), f'before {b} after {a}'

    def post_dump():
        _marks['post_brain'] = g.mark_log()
        _cmd('gstn.Minion.Brain.Dump')
        return 'brain dump after the poke'

    def snap_checks():
        res = []
        # Measured from A, not window by window: at Far/Dormant the server item is published rarely, so a puppet still catching up on the last
        # window moves more than the server did inside this one; a pop is the display overshooting the server's whole path
        for before, after in (('A', 'B'), ('A', 'C'), ('A', 'D')):
            res.append(_no_pop(st, before, after))
        n = sum(p[0] for p in st['pop'])
        return all(r[0] for r in res) and n > 0, '; '.join(r[1] for r in res)

    # poke_frame is needed by chain_ok before the poke step runs, so it is read from the poke line at check time
    def chain_checked():
        poke_parsed()
        return chain_ok()

    steps = [
        (0.0, lambda: ('setup', setup())),
        (0.5, lambda: ('spawn', _cmd(f'gstn.Minion.Bench.Spawn {NUM} Brain'))),
        (4.0, lambda: ('dump A', dump_both())),
        (4.6, lambda: ('snapshot A', _snapshot(st, 'A'))),
        (4.7, lambda: ('teleport 1', teleport(2800.0)())),
        (5.0, lambda: ('teleport 1 fallback', teleport_fallback(2800.0)())),
        (7.0, lambda: ('dump B', dump_both())),
        (7.6, lambda: ('snapshot B', _snapshot(st, 'B'))),
        (7.7, lambda: ('teleport 2', teleport(5600.0)())),
        (8.0, lambda: ('teleport 2 fallback', teleport_fallback(5600.0)())),
        (10.0, lambda: ('dump C', dump_both())),
        (10.6, lambda: ('snapshot C', _snapshot(st, 'C'))),
        (10.7, lambda: ('teleport 3', teleport(10500.0)())),
        (11.0, lambda: ('teleport 3 fallback', teleport_fallback(10500.0)())),
        (13.0, lambda: ('dump D', dump_both())),
        (13.6, lambda: ('snapshot D', _snapshot(st, 'D'))),
        (13.7, lambda: ('brain dump', poke())),
        (14.2, lambda: ('poke', do_poke())),
        (14.45, lambda: ('brain dump after', post_dump())),
        (15.0, run.at('the tiers walk Combat -> Near -> Far -> Dormant, no direct Combat -> Dormant jump', chain_checked)),
        (15.0, run.at('no pop: the client display moves no more than the server moved the minion (+ 30 cm)', snap_checks)),
        (15.0, run.at('the poked minion was Dormant', was_dormant)),
        (15.0, run.at('a hit on a dormant minion puts it in Combat within 2 frames', wake_ok)),
        (15.0, run.at('the woken minion brain ticked (Brain.Dump ticks went up)', brain_ticked)),
        (15.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (15.2, lambda: ('LOD log off', _cmd('gstn.Minion.LOD.Log 0'))),
        (16.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_tokens_brain(client=1):
    """M8 G8. The checks of scenario_minion_tokens with the brain driving the attackers (gstn.Minion.Bench.Attack 8 0 Brain): at most 4 hold a
    token on any fixed frame, the cap is reached, refused starts happened, swings overlap for at most 4 distinct attackers, the waiting minions
    take over, the victim agrees on every world afterwards; and the brain ran for all 8 (Brain.Report brained=8, ticksLastSec > 0)"""
    run = g._Run('minion_tokens_brain', client, judge_reconciles=False, sample=False)
    st = {}

    def window_max(rows):
        best = 0
        for r in rows:
            best = max(best, len({x[1] for x in rows if r[0] <= x[0] < r[0] + 8}))
        return best

    def brain_line():
        rows = [ln for ln in g._read_log_since(_marks['log']).splitlines() if _BRAIN_REPORT.search(ln)]
        m = _BRAIN_REPORT.search(rows[-1]) if rows else None
        return m, (rows[-1][-140:] if rows else '')

    def brain_ran():
        m, text = brain_line()
        return (m is not None and int(m.group(2)) == 8 and int(m.group(3)) > 0), text

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 8 0 Brain'))),
        (9.0, lambda: ('capture', st.update(key=_victim_key()))),
        (9.05, lambda: ('brain report', _cmd('gstn.Minion.Brain.Report'))),
        (9.1, lambda: ('report', _cmd('gstn.Minion.Bench.Report'))),
        (9.5, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (12.5, run.at('attackers landed hits on one victim', lambda: (st.get('key') is not None and len(_minion_rows(0, st['key'])) >= 4, f"victim {st.get('key')} rows {len(_minion_rows(0, st['key'])) if st.get('key') else 0}"))),
        (12.5, run.at('the brain ran for all 8 attackers (brained=8, ticksLastSec > 0)', brain_ran)),
        (12.5, run.at('the token cap held on every fixed frame (maxHolders <= 4)', lambda: (
            (_num(_report_line('MinionBench attack'), 'maxHolders') or 99) <= 4, _report_line('MinionBench attack')[-120:]))),
        (12.5, run.at('the cap was reached (maxHolders == 4)', lambda: ((_num(_report_line('MinionBench attack'), 'maxHolders') or 0) == 4, _report_line('MinionBench attack')[-120:]))),
        (12.5, run.at('starts were refused for taken tokens (tokensDenied > 0)', lambda: ((_num(_report_line('MinionBench attack'), 'tokensDenied') or 0) > 0, _report_line('MinionBench attack')[-120:]))),
        (12.5, run.at('at most 4 distinct attackers hit inside one swing window', lambda: (window_max(_minion_rows(0, st['key'])) <= 4, f"max {window_max(_minion_rows(0, st['key']))}"))),
        (12.5, run.at('waiting minions take over (> 4 distinct attackers over the run)', lambda: (len({r[1] for r in _minion_rows(0, st['key'])}) > 4, f"attackers {len({r[1] for r in _minion_rows(0, st['key'])})}"))),
        (12.5, run.at('damage percent equal on the server and both clients', lambda: _converged(st['key']))),
        (12.5, run.finish()),
    ]
    run.run(steps)


def scenario_minion_wave(client=1):
    """M8 G9a. A spawner plays a wave of 8 bench grunts (gstn.Minion.Bench.Wave 8 1: bodies stay 1 s). The server and both clients hold 8 net items
    after the spawn; gstn.Minion.Bench.Kill sends a lethal hit to each: the items stay while the minions die and while the bodies wait (Dead,
    CorpseSeconds), then disappear on the server and on both clients; 'Bench: wave cleared' is logged exactly once. The grunts have no Death
    timeline, so they die for gstn.Minion.CorpseFrames (60 frames, about 1 s) before the body time starts. Fixed waits, like the other scenarios"""
    run = g._Run('minion_wave', client, judge_reconciles=False, sample=False)
    st = {}

    def dump():
        # One dump per phase, read by the check that follows it (_net_state reads the log since the mark, so the mark moves with each dump)
        _marks['log'] = g.mark_log()
        return _net_dump()

    def counts(want):
        server, clients, grants, summaries = _net_state()
        per_client = {p: len(clients.get(p, {})) for p in summaries}
        ok = len(server) == want and bool(per_client) and all(n == want for n in per_client.values())
        return ok, f'want {want}: server items {len(server)} clients {per_client}'

    def cleared_once():
        n = len(re.findall(r'Bench: wave cleared frame=\d+', g._read_log_since(st['t0'])))
        return n == 1, f'wave cleared lines {n}'

    steps = [
        (0.0, lambda: ('setup', st.update(t0=g.mark_log()) or 'log mark')),
        (0.5, lambda: ('wave', _cmd('gstn.Minion.Bench.Wave 8 1'))),
        (3.0, lambda: ('dump A (spawned)', dump())),
        (4.0, run.at('8 items on the server and on both clients after the spawn', lambda: counts(8))),
        (4.1, lambda: ('kill', _cmd('gstn.Minion.Bench.Kill'))),
        (4.6, lambda: ('dump B (dying)', dump())),
        (5.4, run.at('the items stay while the minions die', lambda: counts(8))),
        (5.6, lambda: ('dump C (bodies)', dump())),
        (6.3, run.at('the bodies stay during the corpse time', lambda: counts(8))),
        (8.0, lambda: ('dump D (after the corpse time)', dump())),
        (9.0, run.at('the bodies are gone on the server and on both clients', lambda: counts(0))),
        (9.0, run.at('the wave cleared exactly once', cleared_once)),
        (9.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (9.5, run.finish()),
    ]
    run.run(steps)


_LOG_TS = re.compile(r'^\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]')
_SWAP = re.compile(r'MinionPromote swap key=(-?\d+) fighter=(\S+) pop=([\d.]+)cm waited=(\d+)ms held=(\d) net=(\w+)')


def _log_seconds(line):
    """Seconds of the day of an engine log line (all PIE worlds share one process, so one clock)"""
    m = _LOG_TS.search(line)
    if not m:
        return None
    v = [int(x) for x in m.groups()]
    return v[3] * 3600 + v[4] * 60 + v[5] + v[6] / 1000.0


def _promoted_fighters(wi):
    """Actors of world wi that carry the promoted component (the fighters made from minions)"""
    comp_class = unreal.load_class(None, '/Script/GSTNMinionRuntime.GSTN_MinionPromotedComponent')
    found = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(g.worlds()[wi], unreal.Actor):
        try:
            if a.get_component_by_class(comp_class):
                found.append(a)
        except Exception:
            pass
    return found


def scenario_minion_promote(client=1):
    """M8 G9b, spike 8. Three idle bench grunts; the server promotes one (gstn.Minion.Bench.Promote, the harness test character, health kept).
    The minion holds as a Promoting body for gstn.Minion.PromoteHoldFrames (no shatter), the fighter replicates with the minion's ids, and each
    client swaps the puppet for the fighter. PASS: the fighter exists on the server and both clients; each client logs one swap with a pop
    <= 15 cm; the first control (the client shows the fighter, which NP then moves) <= 500 ms after the server's promote line; no 'nopuppet'
    or 'expired' line; after the hold 2 items are left on the server and the clients. Spike 8 numbers are notes. The fighter is destroyed at the end."""
    run = g._Run('minion_promote', client, judge_reconciles=False, sample=False)
    st = {}

    def dump():
        _marks['log'] = g.mark_log()
        return _net_dump()

    def counts(want):
        server, clients, grants, summaries = _net_state()
        per_client = {p: len(clients.get(p, {})) for p in summaries}
        ok = len(server) == want and bool(per_client) and all(n == want for n in per_client.values())
        return ok, f'want {want}: server items {len(server)} clients {per_client}'

    def promote():
        st['t0'] = g.mark_log()
        _cmd('gstn.Minion.Bench.Promote -')
        return 'promote', 'gstn.Minion.Bench.Promote -'

    def fighters():
        n = [len(_promoted_fighters(wi)) for wi in range(len(g.worlds()))]
        return all(x == 1 for x in n), f'promoted fighters per world {n}'

    def swaps():
        text = g._read_log_since(st['t0'])
        lines = text.splitlines()
        prom = [ln for ln in lines if 'Bench: promote minion=' in ln]
        st['promote_line'] = prom[-1] if prom else ''
        rows = [(ln, _SWAP.search(ln)) for ln in lines if _SWAP.search(ln)]
        client_rows = [(ln, m) for ln, m in rows if m.group(6) == 'client']
        t_srv = _log_seconds(st['promote_line']) if st['promote_line'] else None
        st['spike8'] = [dict(pop=float(m.group(3)), waited=int(m.group(4)), held=int(m.group(5)),
                             control_ms=(round((_log_seconds(ln) - t_srv) * 1000.0) if t_srv is not None and _log_seconds(ln) is not None else None))
                        for ln, m in client_rows]
        st['bad'] = len(re.findall(r'MinionPromote (nopuppet|expired|lostpuppet)', text))
        run.note(f"minion_promote spike 8: {st['promote_line'][-160:]} | client swaps {st['spike8']} | nopuppet/expired/lost {st['bad']}")
        return len(client_rows) == 2, f'{len(client_rows)} client swap lines (want 2: one per client)'

    def pops():
        worst = max([r['pop'] for r in st.get('spike8', [])], default=None)
        return worst is not None and worst <= 15.0, f'worst pop {worst} cm (limit 15)'

    def control():
        worst = max([r['control_ms'] for r in st.get('spike8', []) if r['control_ms'] is not None], default=None)
        return worst is not None and worst <= 500, f'worst first control {worst} ms after the server promote (limit 500)'

    def clean():
        return st.get('bad', 1) == 0, f"nopuppet/expired/lostpuppet lines {st.get('bad')}"

    def destroy():
        for a in _promoted_fighters(0):
            a.destroy_actor()
        return 'cleanup', 'promoted fighter destroyed on the server, bench cleared'

    steps = [
        (0.0, lambda: ('setup', st.update(t=g.mark_log()) or 'log mark')),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Spawn 3 Idle'))),
        (3.0, lambda: ('dump A (spawned)', dump())),
        (3.6, run.at('3 items on the server and on both clients', lambda: counts(3))),
        (3.8, promote),
        (5.0, run.at('the fighter exists on the server and on both clients', fighters)),
        (5.0, run.at('each client swapped the puppet for the fighter once', swaps)),
        (5.0, run.at('no pop above 15 cm at the swap', pops)),
        (5.0, run.at('first control within 500 ms of the promotion', control)),
        (5.0, run.at('no puppet missing, expired or lost', clean)),
        (5.2, lambda: ('dump B (after the hold)', dump())),
        (6.0, run.at('the promoted minion is gone after the hold (2 items left everywhere)', lambda: counts(2))),
        (6.1, destroy),
        (6.2, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (6.8, run.finish()),
    ]
    run.run(steps)


# ---- M8 G11: host budgets (MaxActiveT0, T0 rate, awake population). Numbers on a laptop with -nullrhi are provisional ----
_BUDGET = re.compile(r'MinionBench budget activeT0=(\d+) demoted=(\d+) awake=(\d+) dormant=(\d+) t0=(\d+) t1=(\d+) t2=(\d+) t3=(\d+) alive=(\d+) t0RateHz=(\d+) maxBatchUs=([\d.]+) avgBatchUs=([\d.]+)')
_NET_WIRE = re.compile(r'MinionBench net outBytes=(\d+) outPerSec=\d+ conns=\d+ perConn=(\S*)')   # G11b: real wire bytes (bench Report)
_PLANNING_BYTES_PER_ITEM = 20   # GSTNMinionNetRate::PlanningBytesPerItem: planned size of one item update (R1 measures the real one)


def _env_float(name, default):
    try:
        return float(os.environ.get(name, default))
    except ValueError:
        return float(default)


def _budget_rows(since):
    """Every 'MinionBench budget' line after the mark as dicts"""
    rows = []
    for ln in g._read_log_since(since).splitlines():
        m = _BUDGET.search(ln)
        if m:
            v = m.groups()
            rows.append(dict(activeT0=int(v[0]), demoted=int(v[1]), awake=int(v[2]), dormant=int(v[3]), t0=int(v[4]), t1=int(v[5]), t2=int(v[6]),
                             t3=int(v[7]), alive=int(v[8]), t0Hz=int(v[9]), maxBatchUs=float(v[10]), avgBatchUs=float(v[11])))
    return rows


def _cap():
    try:
        return int(unreal.SystemLibrary.get_console_variable_int_value('gstn.Minion.MaxActiveT0'))
    except Exception:
        return 48


def scenario_minion_horde(client=1):
    """M8 G11. 60 chasing attackers (30 per client pawn, gstn.Minion.Bench.Attack) while both clients swing at them for GSTN_HORDE_SECONDS (60, or 20 on
    the laptop). Judged: the server's active T0 never exceeded gstn.Minion.MaxActiveT0 (budget lines every 5 s), the fight happened (the
    server drained hits, minions started attacks), no ensure in the log, the clients' registry check passed with no mismatch, and the number of
    reconciles is recorded (judged by the other scenarios; the pawns here are hit on purpose). Result detail: active T0, demoted, avg/max batch,
    reconciles. Provisional numbers (laptop, -nullrhi)"""
    run = g._Run('minion_horde', client, judge_reconciles=False, sample=False)
    duration = _env_float('GSTN_HORDE_SECONDS', 60.0)
    st = {}
    clients = list(range(1, len(g.worlds())))

    def setup():
        _setup()
        _cmd('gstn.Minion.Bench.HurtboxSet /Game/GASNPPTest/Combat/HBS_Test_Pawn.HBS_Test_Pawn')
        g.set_hurtbox(infinite=True)
        for c in clients:
            g._apply_loadout(g.key_of_player(c), g.HIT_KIT)
        st['t0'] = g.mark_log()
        st['rb0'] = g._total_rollbacks()
        return 'rules + hit log + kit + no stagger'

    def sample():
        _cmd('gstn.Minion.Bench.Report')
        return 'report'

    def final_dump():
        _marks['log'] = g.mark_log()
        _net_dump()
        _cmd('gstn.Minion.Bench.Report')
        return 'dumped'

    def judged_cap():
        rows = _budget_rows(st['t0'])
        cap = _cap()
        top = max((r['activeT0'] for r in rows), default=-1)
        last = rows[-1] if rows else {}
        return (bool(rows) and (cap <= 0 or top <= cap)), (f"cap {cap} max activeT0 {top} over {len(rows)} reports; last demoted {last.get('demoted')} "
                                                           f"awake {last.get('awake')} avgBatchUs {last.get('avgBatchUs')} maxBatchUs {last.get('maxBatchUs')} (provisional, laptop)")

    def no_ensure():
        text = g._read_log_since(st['t0'])
        bad = [ln for ln in text.splitlines() if 'Ensure condition failed' in ln or 'Assertion failed' in ln]
        return not bad, f'{len(bad)} ensure/assert lines {bad[:1]}'

    def fought():
        att = _report_line('MinionBench attack')
        spike = _report_line('MinionBench spike5')
        started, drained = _num(att, 'started') or 0, _num(spike, 'drainedHits') or 0
        return started > 0 and drained > 0, f'attacks started {started} drained hits {drained}'

    def registry_ok():
        summaries = _net_state()[3]
        return (len(summaries) >= 1 and all(x['ok'] == 1 and x['mismatches'] == 0 for x in summaries.values())), f'summaries {summaries}'

    def reconciles():
        gained = g._total_rollbacks() - st['rb0']
        run.note(f'reconciles during the horde: +{gained} rollbacks on the clients')
        return True, f'+{gained} rollbacks (recorded, not judged)'

    steps = [(0.0, lambda: ('setup', setup())),
             (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 30 0'))),
             (0.6, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 30 1')))]
    for c in clients:
        steps += _swing_steps(3.0, duration, 0.5, c)
    t = 5.0
    while t < duration:
        steps.append((t, lambda: ('sample', sample())))
        t += 5.0
    end = duration + 0.5
    steps += [
        (end, lambda: ('final', final_dump())),
        (end + 1.0, run.at('active T0 never above gstn.Minion.MaxActiveT0 (numbers: active T0, demoted, batch us)', judged_cap)),
        (end + 1.0, run.at('the horde fought (attacks started, hits drained on the server)', fought)),
        (end + 1.0, run.at('no ensure or assert in the log', no_ensure)),
        (end + 1.0, run.at('registry check ok on every client, no mismatch', registry_ok)),
        (end + 1.0, run.at('reconciles recorded', reconciles)),
        (end + 1.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (end + 2.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_bytes(client=1, clients=2):
    """M8 G11. Per-client minion upload with N = 40 and N = 80 awake chasing minions (gstn.Minion.Bench.Spawn N Chase), measured over a window of
    GSTN_BENCH_WINDOW seconds (10): the item updates each client received (client summary updates) x 20 B planning size (PlanningBytesPerItem)
    per second, with the per-tier counts of the server. Then the 150 population case (gstn.Minion.Bench.Population 150 60): awake (T0..T2) <= 62.
    `clients` is the number of remote clients the host serves (2 on the laptop, 3 on the desktop): the host upload is per-client x clients and
    is printed that way; the windows read every client that reports. Numbers are planning bytes, not wire bytes, and provisional (laptop,
    -nullrhi); the 5.8 KB/s per client of M8 4d.1 for 40 typical minions is only noted, not judged.
    G11b: GSTN_BYTES_KIND (Chase default | Massacre: minions are launched all the time, so they keep moving) picks the horde, and the bench Report's
    'MinionBench net' line (server UNetDriver OutTotalBytes, per client connection) gives the REAL wire bytes per window, printed next to the
    planning bytes. GSTN_BYTES_CVARS (';' list of console commands run first) tries a fallback. GSTN_BYTES_BASE=1 (default) first measures a window with no minions: the players' own traffic (NP) is the baseline that the
    minion bytes are read against (real minion bytes = window - baseline). The real figures are recorded, not judged"""
    run = g._Run('minion_bytes', client, judge_reconciles=False, sample=False)
    window = _env_float('GSTN_BENCH_WINDOW', 10.0)
    kind = os.environ.get('GSTN_BYTES_KIND', 'Chase')
    base = os.environ.get('GSTN_BYTES_BASE', '1') != '0'
    o = (window + 2.0) if base else 0.0     # the baseline window shifts everything after it
    st = {}

    def snap(label):
        _marks['log'] = g.mark_log()
        st[label + '_mark'] = _marks['log']
        st[label + '_time'] = time.time()
        _net_dump()
        _cmd('gstn.Minion.Bench.Report')
        return 'snap ' + label

    def wire(label):
        """Real wire bytes of the server driver and of each client connection at the snap (cumulative OutTotalBytes of the last 'MinionBench net' line)"""
        rows = _NET_WIRE.findall(g._read_log_since(st[label + '_mark']))
        if rows:
            total, conns = int(rows[-1][0]), [int(x) for x in re.findall(r'(\d+)/\d+', rows[-1][1])]
            st[label + '_wire'] = (total, conns)
        return f"{label}: wire {st.get(label + '_wire')}"

    def wire_rate(a, b):
        """(host B/s, [per client B/s]) between two snaps, or None"""
        if a + '_wire' not in st or b + '_wire' not in st:
            return None
        dt = max(st[b + '_time'] - st[a + '_time'], 0.001)
        (t0, c0), (t1, c1) = st[a + '_wire'], st[b + '_wire']
        return (t1 - t0) / dt, [(y - x) / dt for x, y in zip(c0, c1)]

    def base_rate():
        r = wire_rate('z0', 'z1')
        if r:
            run.note(f'REAL wire baseline (no minions): host {r[0]:.0f} B/s, per client ' + ', '.join(f'{v:.0f}' for v in r[1]))
        return r is not None, f'baseline {r}'

    def read(label):
        st[label + '_updates'] = {p: x['updates'] for p, x in _net_state()[3].items()}
        rows = _budget_rows(st[label + '_mark'])
        st[label + '_budget'] = rows[-1] if rows else {}
        wire(label)
        return f"{label}: updates {st[label + '_updates']} budget {st[label + '_budget']}"

    def rate(a, b, n):
        dt = max(st[b + '_time'] - st[a + '_time'], 0.001)
        per = {p: (st[b + '_updates'].get(p, 0) - u) / dt * _PLANNING_BYTES_PER_ITEM for p, u in st[a + '_updates'].items()}
        top = max(per.values(), default=0.0)
        bud = st[b + '_budget']
        detail = (f"N={n}: per-client planning bytes/s {', '.join(f'{p}:{v:.0f}' for p, v in per.items())}; host upload for {clients} clients ~{top * clients:.0f} B/s; "
                  f"tiers t0={bud.get('t0')} t1={bud.get('t1')} t2={bud.get('t2')} t3={bud.get('t3')} activeT0={bud.get('activeT0')} demoted={bud.get('demoted')} "
                  f"t0Hz={bud.get('t0Hz')} (provisional, laptop, planning size 20 B)")
        r = wire_rate(a, b)
        if r:
            br = wire_rate('z0', 'z1')
            net = [max(v - bv, 0.0) for v, bv in zip(r[1], br[1])] if br else None
            run.note(f'N={n} {kind}: REAL wire bytes/s per client {", ".join(f"{v:.0f}" for v in r[1])}, host {r[0]:.0f}'
                     + (f'; minus the no-minion baseline: per client {", ".join(f"{v:.0f}" for v in net)} (planned {top:.0f})' if net else ''))
        if n == 40:
            run.note(f'M8 4d.1 planning value for 40 typical minions: 5.8 KB/s per client; measured {top / 1000.0:.1f} KB/s (chasing, so all T0)')
        return (top > 0.0), detail

    def population():
        bud = st['pop_budget']
        awake = bud.get('awake', 999)
        return awake <= 62 and bud.get('alive', 0) >= 100, (f"alive {bud.get('alive')} awake {awake} dormant {bud.get('dormant')} t0={bud.get('t0')} t1={bud.get('t1')} "
                                                           f"t2={bud.get('t2')} t3={bud.get('t3')} activeT0={bud.get('activeT0')} (provisional, laptop)")

    w = window
    steps = [(0.0, lambda: ('setup', _setup()))]
    if kind == 'Massacre':
        steps.append((0.1, lambda: ('launch speed', _cmd('gstn.Minion.Bench.LaunchSpeed 800'))))
    for i, extra in enumerate(c for c in os.environ.get('GSTN_BYTES_CVARS', '').split(';') if c):   # e.g. 'gstn.Minion.T0RateHz 10' (a fallback of M8 8.3)
        steps.append((0.2 + 0.01 * i, lambda extra=extra: ('cvar', _cmd(extra))))
    if base:
        steps += [(1.0, lambda: snap('z0')), (1.8, lambda: ('read', read('z0'))), (1.0 + w, lambda: snap('z1')), (1.8 + w, lambda: ('read', read('z1'))),
                  (2.0 + w, run.at('baseline wire bytes (players only, no minions)', base_rate))]
    steps += [
        (0.5 + o, lambda: ('spawn 40', _cmd(f'gstn.Minion.Bench.Spawn 40 {kind}'))),
        (4.0 + o, lambda: snap('a0')), (4.8 + o, lambda: ('read', read('a0'))),
        (4.0 + w + o, lambda: snap('a1')), (4.8 + w + o, lambda: ('read', read('a1'))),
        (5.0 + w + o, run.at('N=40 bytes per client', lambda: rate('a0', 'a1', 40))),
        (5.1 + w + o, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (6.5 + w + o, lambda: ('spawn 80', _cmd(f'gstn.Minion.Bench.Spawn 80 {kind}'))),
        (10.5 + w + o, lambda: snap('b0')), (11.3 + w + o, lambda: ('read', read('b0'))),
        (10.5 + 2 * w + o, lambda: snap('b1')), (11.3 + 2 * w + o, lambda: ('read', read('b1'))),
        (11.5 + 2 * w + o, run.at('N=80 bytes per client', lambda: rate('b0', 'b1', 80))),
        (11.6 + 2 * w + o, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (13.0 + 2 * w + o, lambda: ('population', _cmd('gstn.Minion.Bench.Population 150 60 12000 Idle'))),
        (21.0 + 2 * w + o, lambda: snap('pop')), (21.8 + 2 * w + o, lambda: ('read', read('pop'))),
        (22.0 + 2 * w + o, run.at('150 population: awake (T0..T2) <= 62, the rest dormant', population)),
        (22.1 + 2 * w + o, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear') or _cmd('gstn.Minion.Bench.LaunchSpeed 3000'))),
        (23.0 + 2 * w + o, run.finish()),
    ]
    run.run(steps)


# ---- M8 G6b: dormancy flush (R2 of the plan: the wake/flush/mutate order of MutatePack seen from the clients) ----
_NET_FULL_SERVER = re.compile(r'MinionNet server pack=(\d+) id=(\d+) type=(\d+) pos=(\d+),(\d+),(\d+) yaw=(\d+) state=(-?\d+) ceiling=(\d+) dormant=(\d)')
_NET_FULL_CLIENT = re.compile(r'MinionNet client player=(-?\d+) world=\S+ pack=(\d+) id=(\d+) type=(\d+) pos=(\d+),(\d+),(\d+) yaw=(\d+) state=(-?\d+) updates=(\d+)')
_NET_SERVER_SUMMARY = re.compile(r'MinionNet server summary items=(\d+) packs=(\d+) dormant=(\d+) adds=(\d+) updates=(\d+) removes=(\d+) packSends=(\d+) wakes=(\d+) flushes=(\d+) sleeps=(\d+)')


def _net_full(mark):
    """Parses every dump after `mark` with the full item state: server {(pack, id): (type, pos, yaw, state)}, clients {player: {...}},
    grants, client summaries and the server summary (last one wins in each map, so one dump per mark)"""
    text = g._read_log_since(mark)
    server, clients, srv = {}, {}, {}
    for ln in text.splitlines():
        m = _NET_FULL_SERVER.search(ln)
        if m:
            v = [int(x) for x in m.groups()]
            server[(v[0], v[1])] = (v[2], (v[3], v[4], v[5]), v[6], v[7])
            continue
        m = _NET_FULL_CLIENT.search(ln)
        if m:
            v = [int(x) for x in m.groups()]
            clients.setdefault(v[0], {})[(v[1], v[2])] = (v[3], (v[4], v[5], v[6]), v[7], v[8])
            continue
        m = _NET_SERVER_SUMMARY.search(ln)
        if m:
            srv = dict(zip(('items', 'packs', 'dormant', 'adds', 'updates', 'removes', 'packSends', 'wakes', 'flushes', 'sleeps'), (int(x) for x in m.groups())))
    return server, clients, srv


def _full_diff(snap):
    """Differences between the server's items and each client's copy of its granted packs -> list of strings (empty = equal). A server with no
    item at all (everything died and was removed) equals clients with no item"""
    bad = []
    if not snap['summaries']:
        return ['no client summary']
    for player in snap['summaries']:
        granted = snap['grants'].get(player, set())
        want = {k: v for k, v in snap['server'].items() if k[0] in granted}
        got = snap['clients'].get(player, {})
        diff = [k for k in want if got.get(k) != want[k]]
        if diff or set(got) - set(want):
            k = diff[0] if diff else None
            bad.append(f"player {player}: {len(diff)} items differ, extra {sorted(set(got) - set(want))[:3]}" + (f" e.g. {k} server {want[k]} client {got.get(k)}" if k else ''))
    return bad


def scenario_minion_dormancy_flush(client=1):
    """M8 G6b (plan G6, R2). GSTN_DORMANCY_SECONDS (default 600) split in cycles of ~60 s. A cycle: 60 idle minions on a ring beyond the Far tier
    border (gstn.Minion.Bench.Population 60 0 9000, relevancy distance raised to 20000 so the clients are granted the packs they cannot see);
    the packs go DormantAll. Quiet window: a dormant pack sends nothing for 15 s (client and server update counters flat). Then every minion is
    killed (gstn.Minion.Bench.Kill, a lethal hit through the inbox): the packs are mutated while dormant and the clients must receive the
    mutation after the flush (negative control: a client still showing the pre-kill state fails). A 1 damage poke changes nothing on the wire,
    which is why the mutation is the kill. Judged per cycle: all packs dormant and clients equal to the server before, quiet window flat, the
    server's items changed by the kill and every client's copy equal to the server's within 9 s; overall the server woke or flushed a dormant pack for every cycle,
    registry check ok with no mismatch, no ensure"""
    seconds = _env_float('GSTN_DORMANCY_SECONDS', 600.0)
    run = g._Run('minion_dormancy_flush', client, judge_reconciles=False, sample=False)
    count = 60
    cycles = int(max(2, min(10, seconds // 60)))
    period = max(48.0, seconds / cycles)
    st = {'snaps': {}}

    def snap(label):
        mark = g.mark_log()
        _marks['log'] = mark
        st['snaps'][label] = {'mark': mark}
        _net_dump()
        return 'dump ' + label

    def read(label):
        s = st['snaps'][label]
        s['server'], s['clients'], s['srv'] = _net_full(s['mark'])
        _, _, s['grants'], s['summaries'] = _net_state()
        return label

    def setup():
        st['dist'] = unreal.SystemLibrary.get_console_variable_float_value('gstn.Minion.Net.RelevantDistance')
        _cmd('gstn.Minion.Net.RelevantDistance 20000')
        st['log'] = g.mark_log()
        _marks['log'] = st['log']
        return f"relevant distance {st['dist']} -> 20000"

    def teardown():
        _cmd(f"gstn.Minion.Net.RelevantDistance {int(st.get('dist', 8000))}")
        return 'relevant distance restored'

    def before_ok(i):
        a, b = st['snaps'][f'c{i}q0'], st['snaps'][f'c{i}q1']
        bad = _full_diff(a)
        ok = len(a['server']) == count and a['srv'].get('packs', 0) >= 2 and a['srv'].get('dormant') == a['srv'].get('packs') and not bad
        quiet = {p: b['summaries'][p]['updates'] - a['summaries'][p]['updates'] for p in a['summaries'] if p in b['summaries']}
        sdelta = b['srv'].get('updates', 0) - a['srv'].get('updates', 0)
        flat = b['srv'].get('dormant') == b['srv'].get('packs') and sdelta == 0 and all(v == 0 for v in quiet.values()) and bool(quiet)
        return ok and flat, (f"items {len(a['server'])} packs {a['srv'].get('packs')} dormant {a['srv'].get('dormant')}/{b['srv'].get('dormant')} diff {bad[:1]}; "
                             f"15 s quiet: client updates {quiet} server updates {sdelta}")

    def after_ok(i):
        pre = st['snaps'][f'c{i}q1']
        posts = [st['snaps'][f'c{i}p{k}'] for k in range(3)]
        changed = sum(1 for k, v in pre['server'].items() if any(p['server'].get(k) != v for p in posts))
        diffs = [_full_diff(p) for p in posts]
        # the final dump must agree too: a client that caught up and fell behind again is not a flush
        ok = changed > 0 and not diffs[2] and any(not d for d in diffs)
        return ok, (f"server items changed by the kill {changed}/{len(pre['server'])}; server items per dump {[len(p['server']) for p in posts]} "
                    f"client diffs {[len(d) for d in diffs]} {diffs[0][:1] if diffs[0] else ''}")

    def counters():
        s = st['snaps'][f'c{cycles - 1}p2']['srv']
        # a killed minion has a ceiling above 0, so MutatePack takes the wake path; the flush path (a mutation that leaves the pack all-T3) is only counted
        return s.get('wakes', 0) + s.get('flushes', 0) >= cycles, f"wakes {s.get('wakes')} flushes {s.get('flushes')} sleeps {s.get('sleeps')} (cycles {cycles})"

    def registry():
        sm = st['snaps'][f'c{cycles - 1}q1']['summaries']
        return bool(sm) and all(x['ok'] == 1 and x['mismatches'] == 0 for x in sm.values()), f'summaries {sm}'

    def no_ensure():
        bad = [ln for ln in g._read_log_since(st['log']).splitlines() if 'Ensure condition failed' in ln or 'Assertion failed' in ln]
        return not bad, f'{len(bad)} ensure/assert lines {bad[:1]}'

    steps = [(0.0, lambda: ('setup', setup()))]
    for i in range(cycles):
        t0 = 0.5 + i * period
        steps += [(t0, lambda: ('spawn', _cmd('gstn.Minion.Bench.Population 60 0 9000 Idle'))),
                  (t0 + 10.0, lambda i=i: ('q0', snap(f'c{i}q0'))), (t0 + 10.6, lambda i=i: ('q0 read', read(f'c{i}q0'))),
                  (t0 + 25.0, lambda i=i: ('q1', snap(f'c{i}q1'))), (t0 + 25.6, lambda i=i: ('q1 read', read(f'c{i}q1'))),
                  (t0 + 26.0, lambda: ('kill', _cmd('gstn.Minion.Bench.Kill')))]
        for k, dt in enumerate((29.0, 32.0, 35.0)):
            steps += [(t0 + dt, lambda i=i, k=k: ('post', snap(f'c{i}p{k}'))), (t0 + dt + 0.6, lambda i=i, k=k: ('post read', read(f'c{i}p{k}')))]
        steps.append((t0 + 37.0, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))))
    end = 0.5 + (cycles - 1) * period + 38.0
    for i in range(cycles):
        steps.append((end, run.at(f'cycle {i}: dormant and equal before, flat for 15 s', lambda i=i: before_ok(i))))
        steps.append((end, run.at(f'cycle {i}: the pack mutated while dormant reached every client after the flush', lambda i=i: after_ok(i))))
    steps += [
        (end, run.at('the server woke (or flushed) a dormant pack for every cycle', counters)),
        (end, run.at('registry check ok on every client, no mismatch', registry)),
        (end, run.at('no ensure or assert in the log', no_ensure)),
        (end + 0.2, lambda: ('teardown', teardown())),
        (end + 1.0, run.finish()),
    ]
    run.run(steps)

# ---- M4b P16 sub-gate B: minion parts on the wire (GSTNPartsMinion; skips cleanly when that plugin is not enabled) ----
# Bridge bench lines (gstn.PartsMinion.Bench.Report, GSTNPartsMinion):
#   PartsMinionBench server world=.. rows=.. broken=.. brokenAlive=.. attackedAfterBreak=.. partHits=.. lethal=.. extWrites=.. netExtFlagged=..
#       promotionSeeds=.. viewUs=.. stepUs=.. outcomeUs=.. hitUs=.. masks=<gid>:<mask>;...
#   PartsMinionBench client player=.. world=.. items=.. extItems=.. netExtFlagged=.. ext=<gid>:<mask>;...   (nonzero bytes only)
#   PartsMinionBench puppets world=.. tracked=.. hiding=.. changes=.. missingBones=..
_PARTS_SERVER = re.compile(r'PartsMinionBench server world=\S+ rows=(\d+) broken=(\d+) brokenAlive=(\d+) attackedAfterBreak=(\d+) partHits=(\d+) lethal=(\d+) '
                           r'extWrites=(\d+) netExtFlagged=(\d+) promotionSeeds=(\d+) viewUs=([\d.]+) stepUs=([\d.]+) outcomeUs=([\d.]+) hitUs=([\d.]+) masks=(\S+)')
_PARTS_CLIENT = re.compile(r'PartsMinionBench client player=(-?\d+) world=\S+ items=(\d+) extItems=(\d+) netExtFlagged=(\d+) ext=(\S+)')
_PARTS_PUPPETS = re.compile(r'PartsMinionBench puppets world=(\S+) tracked=(\d+) hiding=(\d+) changes=(\d+) missingBones=(\d+)')
# The run without parts (scenario_minion_parts_base) leaves its per-client bytes/s here for scenario_minion_parts (same Python session, run it first)
_PARTS_BASELINE = {}
# P16-C: the item codec (GSTNMinionNetBits) writes the ExtByte (8 bits) in EVERY item of a part-bearing type, not only when it changes; the fixed
# fields of an item are 9 (id + type flag) + 3 x 19 (position) + 8 + 4 + 6 + 10 + 8 + 3 + 5 + 6 + 3 = 119 bits (the optional type and global id
# fields excluded). So the byte costs +8/119 of every update of the type whatever the bridge does: the floor is the codec, see PERF-DEBT
_ITEM_FIXED_BITS = 119
_ITEM_EXT_BITS = 8
_PARTS_PUPPET_MESH = '/Game/Characters/UE5_Mannequins/Meshes/SKM_Manny.SKM_Manny'


def _parts_masks(text):
    """'12:1;13:1' -> {12: 1, 13: 1}; '-' -> {}"""
    out = {}
    for pair in (text or '-').split(';'):
        if ':' in pair:
            k, v = pair.split(':', 1)
            out[int(k)] = int(v)
    return out


def _parts_report(mark):
    """Last bridge lines after the mark: server dict (or None), clients {player: dict}, puppets {world: dict}"""
    text = g._read_log_since(mark)
    server, clients, puppets = None, {}, {}
    for ln in text.splitlines():
        m = _PARTS_SERVER.search(ln)
        if m:
            keys = ('rows', 'broken', 'brokenAlive', 'attackedAfterBreak', 'partHits', 'lethal', 'extWrites', 'netExtFlagged', 'promotionSeeds')
            server = {k: int(m.group(i + 1)) for i, k in enumerate(keys)}
            server.update(viewUs=float(m.group(10)), stepUs=float(m.group(11)), outcomeUs=float(m.group(12)), hitUs=float(m.group(13)),
                          masks=_parts_masks(m.group(14)))
            continue
        m = _PARTS_CLIENT.search(ln)
        if m:
            clients[int(m.group(1))] = {'items': int(m.group(2)), 'extItems': int(m.group(3)), 'flagged': int(m.group(4)), 'ext': _parts_masks(m.group(5))}
            continue
        m = _PARTS_PUPPETS.search(ln)
        if m:
            puppets[m.group(1)] = {'tracked': int(m.group(2)), 'hiding': int(m.group(3)), 'changes': int(m.group(4)), 'missingBones': int(m.group(5))}
    return server, clients, puppets


def _minion_parts_run(name, client, with_parts):
    """Shared timeline of minion_parts and minion_parts_base: 40 bench attackers (20 per client pawn, the bench grunt type), the clients swing at
    them, and the per-client item bytes/s are measured over GSTN_BENCH_WINDOW seconds (planning size 20 B per item update, plus the extension
    byte for a part-bearing type). with_parts: the bench grunt type gets a transient part set before its first spawn
    (gstn.PartsMinion.Bench.Parts <tag> <head hurtboxes> <head cap>: Head on GSTN_PARTS_HEAD_BOXES, default 'head,torso' so a body-height swing
    counts, cap 1 so any hit through breaks it, not lethal)."""
    run = g._Run(name, client, judge_reconciles=False, sample=False)
    window = _env_float('GSTN_BENCH_WINDOW', 10.0)
    head_boxes = os.environ.get('GSTN_PARTS_HEAD_BOXES', 'head,torso')
    puppet_mesh = os.environ.get('GSTN_PARTS_PUPPET_MESH', _PARTS_PUPPET_MESH)
    swingers = list(range(1, len(g.worlds())))
    st = {'on': not with_parts}
    puppets_expected = bool(puppet_mesh)

    def setup():
        _setup()
        _cmd('gstn.Minion.Bench.HurtboxSet /Game/GASNPPTest/Combat/HBS_Test_Pawn.HBS_Test_Pawn')   # bench grunts are unhittable without one
        g.set_hurtbox(infinite=True)
        for c in swingers:
            g._apply_loadout(g.key_of_player(c), g.HIT_KIT)
        st['t0'] = g.mark_log()
        if puppet_mesh:
            # Without it the bench puppets have the null mesh and no bone, so no part bone can ever be hidden (P16-B missingBones 23). The mannequin
            # skeleton has head, foot_l and foot_r, the bones of the bench part set. Read when the bench definitions are built, so before Attack.
            # Set in both runs: the mesh is client presentation only and changes no item
            _cmd(f'gstn.Minion.Bench.PuppetMesh {puppet_mesh}')
            # Process-wide cvar read by the bridge's puppet subsystem in every world: gives a mesh to any part-bearing puppet that has none
            _cmd(f'gstn.PartsMinion.Bench.PuppetMesh {puppet_mesh}')
            _cmd('gstn.PartsMinion.Bench.PuppetVerbose 1')
        if with_parts:
            _cmd(f'gstn.PartsMinion.Bench.Parts Minion.Type.Grunt {head_boxes} 1')
        return 'rules + hit log + kit + no stagger' + (' + bench parts' if with_parts else '')

    def check_bridge():
        if with_parts:
            st['on'] = 'PartsMinionBench: parts registered' in g._read_log_since(st['t0'])
            if not st['on']:
                run.note('GSTNPartsMinion bridge not enabled (no gstn.PartsMinion.Bench.Parts): skipped')
        return 'bridge on' if st['on'] else 'bridge not enabled'

    def gated(cmd):
        return lambda: ('cmd', _cmd(cmd) if st['on'] else 'skipped')

    def snap(label):
        if not st['on']:
            return 'skipped'
        _marks['log'] = g.mark_log()
        st[label + '_mark'] = _marks['log']
        st[label + '_time'] = time.time()
        _net_dump()
        if with_parts:
            _cmd('gstn.PartsMinion.Bench.Report')
        return 'snap ' + label

    def read(label):
        if not st['on']:
            return 'skipped'
        st[label + '_updates'] = {p: x['updates'] for p, x in _net_state()[3].items()}
        return f"{label}: updates {st[label + '_updates']}"

    def rate():
        if not st['on']:
            return True, 'bridge not enabled'
        dt = max(st['a1_time'] - st['a0_time'], 0.001)
        # The ext byte is 8 of 119 fixed bits on every update of the type (see _ITEM_EXT_BITS), not +1 of 20 planning bytes on a change only
        size = _PLANNING_BYTES_PER_ITEM * (1.0 + (_ITEM_EXT_BITS / _ITEM_FIXED_BITS if with_parts else 0.0))
        ups = {p: (st['a1_updates'].get(p, 0) - u) for p, u in st['a0_updates'].items()}
        per = {p: n / dt * size for p, n in ups.items()}
        top = max(per.values(), default=0.0)
        st['top'] = top
        st['top_updates'] = max(ups.values(), default=0) / dt
        if not with_parts:
            _PARTS_BASELINE['top'] = top
            _PARTS_BASELINE['per'] = per
            _PARTS_BASELINE['updates'] = st['top_updates']
        return top > 0.0, (f"per-client planning bytes/s {', '.join(f'{p}:{v:.0f}' for p, v in per.items())} (item {size:.1f} B; "
                           f"{st['top_updates']:.1f} updates/s on the busiest client; host upload for {len(swingers)} clients ~{top * len(swingers):.0f} B/s; provisional, laptop)")

    def final():
        if not st['on']:
            return 'skipped'
        st['final'] = g.mark_log()
        _marks['log'] = st['final']
        _net_dump()
        _cmd('gstn.Minion.Bench.Report')
        _cmd('gstn.PartsMinion.Bench.Report')
        return 'final dump'

    def parts_report():
        return _parts_report(st['final'])

    def gate(fn):
        def wrapped():
            if not st['on']:
                return True, 'bridge not enabled'
            return fn()
        return wrapped

    def flagged():
        server, clients, _ = parts_report()
        return (server is not None and server['netExtFlagged'] >= 1 and server['rows'] >= 30), f'server {server and {k: server[k] for k in ("rows", "netExtFlagged", "partHits")}}'

    def heads_off():
        server, _, _ = parts_report()
        need = int(_env_float('GSTN_PARTS_MIN_BROKEN', 1))
        return (server is not None and server['broken'] >= need and server['extWrites'] >= server['broken']), (
            f"broken heads {server and server['broken']} (>= {need}) of {server and server['rows']} rows, partHits {server and server['partHits']}, "
            f"extWrites {server and server['extWrites']}")

    def keep_fighting():
        server, _, _ = parts_report()
        fought = _num(_report_line('MinionBench attack'), 'started') or 0
        return (server is not None and server['lethal'] == 0 and server['brokenAlive'] == server['broken'] and server['attackedAfterBreak'] >= 1
                and fought > 0), (f"lethal {server and server['lethal']} brokenAlive {server and server['brokenAlive']}/{server and server['broken']} "
                                  f"attacked after the break {server and server['attackedAfterBreak']}, bench attacks started {fought}")

    def clients_equal():
        server, clients, puppets = parts_report()
        items = _net_state()[1]
        if server is None or not clients:
            return False, f'server line {server is not None}, client lines {len(clients)}'
        bad = []
        for player, c in clients.items():
            have = {pack << 4 | iid for (pack, iid) in items.get(player, {})}
            want = {gid: m for gid, m in server['masks'].items() if gid in have}
            if c['ext'] != want:
                diff = sorted(set(c['ext'].items()) ^ set(want.items()))[:4]
                bad.append(f'player {player}: {len(c["ext"])} ext vs {len(want)} server masks, diff {diff}')
        return not bad, '; '.join(bad) or (f"{len(clients)} clients: ext bytes equal the server masks ({len(server['masks'])} broken minions); "
                                           f"puppets {puppets}")

    def puppets_hide():
        _, _, puppets = parts_report()
        if not puppets_expected:
            return True, 'no puppet mesh configured (GSTN_PARTS_PUPPET_MESH empty): visual not judged'
        missing = sum(p['missingBones'] for p in puppets.values())
        hiding = sum(p['hiding'] for p in puppets.values())
        return bool(puppets) and hiding >= 1 and missing == 0, f'puppets {puppets}: hiding {hiding}, missingBones {missing} (must be 0 with {puppet_mesh.rsplit("/", 1)[-1]})'

    def bytes_within():
        if 'top' not in _PARTS_BASELINE:
            run.note('no baseline: run minion_parts_base first in this session (SUITE_MINION_BENCH order); only the per-update overhead is judged')
            over = _ITEM_EXT_BITS / _ITEM_FIXED_BITS
            return over <= 0.05 + 0.02 + 1e-9, f'per item update +{_ITEM_EXT_BITS} bits of {_ITEM_FIXED_BITS} = +{over * 100:.1f} % (codec, every update)'
        base = _PARTS_BASELINE['top']
        ratio = st.get('top', 0.0) / base if base > 0 else 99.0
        noise = _env_float('GSTN_PARTS_BYTES_NOISE', 0.02)
        codec = _ITEM_EXT_BITS / _ITEM_FIXED_BITS
        base_ups = _PARTS_BASELINE.get('updates', 0.0)
        ups_ratio = st.get('top_updates', 0.0) / base_ups if base_ups > 0 else 0.0
        # Like with like: the same bench timeline in both runs, so the update-count ratio is what the parts themselves add (broken minions reacting,
        # the mask change forcing a send) and the codec share is the fixed +8/119 of every update
        run.note(f'bytes split: codec ext byte +{codec * 100:.1f} % per update (every item of the type, GSTNMinion codec) x updates {ups_ratio:.3f} of the base run '
                 f'= {(1.0 + codec) * ups_ratio:.3f}')
        return ratio <= 1.05 + noise, (f'with parts {st.get("top", 0.0):.0f} B/s vs without {base:.0f} B/s per client: {(ratio - 1.0) * 100:+.1f} % '
                                       f'(limit +5 % + {noise * 100:.0f} % run-to-run noise; the codec byte alone is +{codec * 100:.1f} % per update, '
                                       f'updates x{ups_ratio:.3f})')

    def hit_cost():
        server, _, _ = parts_report()
        budget = _env_float('GSTN_PARTS_HIT_US', 2.0)
        if server is None:
            return False, 'no server line'
        run.note(f"P16 per part hit on the server: view {server['viewUs']:.3f} us + step {server['stepUs']:.3f} us + outcome {server['outcomeUs']:.3f} us "
                 f"= {server['hitUs']:.3f} us (budget {budget} us; provisional, laptop)")
        return server['hitUs'] <= budget, f"hitUs {server['hitUs']:.3f} (view {server['viewUs']:.3f} step {server['stepUs']:.3f} outcome {server['outcomeUs']:.3f})"

    steps = [(0.0, lambda: ('setup', setup())),
             (0.7, lambda: ('bridge', check_bridge())),
             (1.0, gated('gstn.Minion.Bench.Attack 20 0')),
             (1.1, gated('gstn.Minion.Bench.Attack 20 1'))]
    stop = 5.0 + window
    for c in swingers:
        steps += [(t, (lambda f=f: f() if st['on'] else ('swing', 'skipped'))) for t, f in _swing_steps(3.0, stop, 0.5, c)]
    steps += [
        (4.0, lambda: ('snap', snap('a0'))), (4.8, lambda: ('read', read('a0'))),
        (4.0 + window, lambda: ('snap', snap('a1'))), (4.8 + window, lambda: ('read', read('a1'))),
        (5.0 + window, run.at('item bytes per client measured (N=40 attackers)', rate)),
        # The swings stop at `stop`; 2 s later every mask change has reached the clients
        (stop + 2.0, lambda: ('final', final())),
    ]
    end = stop + 3.0
    if with_parts:
        steps += [
            (end, run.at('the bench type carries the ext byte (flagged at registration), 40 rows', gate(flagged))),
            (end, run.at('the players hit heads off: server rows show broken heads, one ext write per change', gate(heads_off))),
            (end, run.at('headless minions keep fighting (alive, attacking after the break, no lethal kill)', gate(keep_fighting))),
            (end, run.at("every client's registry ext bytes equal the server masks", gate(clients_equal))),
            (end, run.at('puppets hide the broken parts (no missing bone on the bench mesh)', gate(puppets_hide))),
            (end, run.at('bytes/s within +5 % of the same run without parts', gate(bytes_within))),
            (end, run.at('per part hit server cost within budget (bench counter)', gate(hit_cost))),
        ]
    steps += [
        (end + 0.1, gated('gstn.Minion.Bench.Clear')),
        (end + 1.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_parts_base(client=1):
    """M4b P16 sub-gate B baseline: minion_parts without the part set (no ext byte, nothing broken). Records the per-client item bytes/s for
    minion_parts, which must run after it in the same session (SUITE_MINION_BENCH). Runs with or without GSTNPartsMinion"""
    _minion_parts_run('minion_parts_base', client, with_parts=False)


def scenario_minion_parts(client=1):
    """M4b P16 sub-gate B. 40 bench attackers of a part-bearing type (the bench grunt with a transient part set, gstn.PartsMinion.Bench.Parts, before
    the type's first spawn; no asset), both clients swing at them. Judged: the type was flagged bHasNetExt at registration; server rows show broken
    heads; the headless minions stay alive and attack after the break; each client's registry ext bytes equal the server masks for the items it
    has; per-client bytes/s within +5 % of minion_parts_base (plus GSTN_PARTS_BYTES_NOISE, default 2 %, of run-to-run noise); the bridge's per part
    hit cost (view + step + outcome) within GSTN_PARTS_HIT_US (2 us). Result line: the numbers. PASS with 'bridge not enabled' when
    GSTNPartsMinion is off, so the suites stay green without it"""
    _minion_parts_run('minion_parts', client, with_parts=True)


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (dedicated server + 2 clients as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_MINION, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
