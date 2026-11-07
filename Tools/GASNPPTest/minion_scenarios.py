# M8 G5: minion -> player hit flows on a dedicated server + 2 clients (SUITE_MINION). Minions are the bench's attackers
# (gstn.Minion.Bench.Attack <N> <FighterIndex>, GSTNMinionRuntime, a swing at the fighter with the lowest entity id), the victim is a harness pawn.
#
#   import minion_scenarios as ms
#   ms.run_batch(loss=2, report='m8g5/minion-2pct.log')   # editor with PIE = dedicated server + 2 clients; poll ds.defence_batch_report()
#
# Judged on the server's copy of the victim (inbox log, damage) and on every world's copy of it afterwards (the NP state must agree once the
# minions are gone). At 2 % loss a few Sync/Aux reconciles are expected (testing.md), so no scenario judges unexplained reconciles.
import json
import re
import time

import unreal
import gasnpp_test as g
import defence_scenarios as ds

SUITE_MINION = ['minion_hits_player', 'minion_tokens', 'minion_replicate', 'minion_late_join', 'minion_player_hits_minion',
                'minion_reaction_tiers', 'minion_lagcomp']
# M8 G6: replication scenarios (packs, relevancy grants, registry check); run_batch(SUITE_MINION_NET)
SUITE_MINION_NET = ['minion_late_join', 'minion_replicate']
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
    """Attack taps from the client's pawn every `every` seconds in [start, stop)"""
    out, t = [], start
    while t < stop:
        out.append((t, lambda: ('attack tap', g.combat_tap(client, g.button_mask('Attack')))))
        t += every
    return out


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
    run = g._Run('minion_player_hits_minion', client, judge_reconciles=False, sample=False)

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

    steps = [(0.0, lambda: ('setup', _setup())),
             (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 6 0'))),
             (0.6, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 6 1')))]  # Spawn's 600 cm ring is out of swing reach; attackers walk up to both pawns
    steps += _swing_steps(3.0, 8.0, 0.5, client)
    steps += [
        (9.0, lambda: ('capture', capture())),
        (10.0, run.at('the server drained minion hits (drainedHits > 0)', lambda: ((_num(_report_line('MinionBench spike5'), 'drainedHits') or 0) > 0, _report_line('MinionBench spike5')[-120:]))),
        (10.0, run.at('the client predicted a flash and it was confirmed, none cancelled', confirmed_flash)),
        (10.0, run.at('the confirmed flash sits on a minion the server owns', confirmed_on_server_minion)),
        (10.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (11.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_reaction_tiers(client=1):
    """M8 G7. 40 chasing minions: the client dump carries tiers. At least two tiers occur, a lower tier is on average no farther from the client's pawn
    than a higher one (T0 <= T1 <= T2, 50 cm slack), and the lowest tier present gets more wire updates per item than the highest over the 2 s between
    two Net.Dump samples. G7 gap: Bench.Spawn puts every minion on one ring radius, so distance variety comes from the chase and the relevancy range
    only; the client infers the tier from the sample gap (writer report), so this checks the wire cadence, not a server-side tier id; and the Present id
    to (pack, item) join relies on the id convention of _puppet_of"""
    run = g._Run('minion_reaction_tiers', client, judge_reconciles=False, sample=False)
    st = {}

    def sample(tag):
        st[tag] = g.mark_log()
        _net_dump()
        _present_dump()
        return f'sample {tag}'

    def updates_of(mark):
        text = g._read_log_since(mark)
        # first dump after the mark only: the log after mark 'a' also holds sample 'b'
        out = {}
        for m in map(_NET_CLIENT_ITEM.search, text.splitlines()):
            if m:
                out.setdefault((int(m.group(1)), int(m.group(2)), int(m.group(3))), int(m.group(7)))
        return out

    def distances():
        puppets, _ = _present_state(st['b'])
        by = {0: [], 1: [], 2: []}
        here = _client_pawn_pos(client)
        for (player, pid), p in puppets.items():
            if p['tier'] in by and p['visible']:
                by[p['tier']].append(_dist(p['pos'], here))
        return by

    def tiers_present():
        by = distances()
        return sum(1 for v in by.values() if v) >= 2, f'puppets per tier {({t: len(v) for t, v in by.items()})}'

    def tiers_ordered():
        means = {t: sum(v) / len(v) for t, v in distances().items() if v}
        order = sorted(means)
        return len(order) >= 2 and all(means[a] <= means[b] + 50.0 for a, b in zip(order, order[1:])), f'mean distance per tier {({t: round(m) for t, m in means.items()})}'

    def lowest_tier_updates_more():
        a, b = updates_of(st['a']), updates_of(st['b'])
        puppets, _ = _present_state(st['b'])
        rate = {0: [], 1: [], 2: []}
        for k in b:
            p = _puppet_of(puppets, *k) if k in a else None
            if p and p['tier'] in rate:
                rate[p['tier']].append(b[k] - a[k])
        mean = {t: sum(v) / len(v) for t, v in rate.items() if v}
        if len(mean) < 2:
            return False, f'fewer than two tiers with a joined update count: {mean}'
        return mean[min(mean)] > mean[max(mean)], f'mean updates per item {({t: round(m, 1) for t, m in mean.items()})} (n {({t: len(v) for t, v in rate.items()})})'

    steps = [
        (0.0, lambda: ('setup', _marks.update(log=g.mark_log()) or 'log mark')),
        (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Spawn 40 Chase'))),
        (4.0, lambda: ('sample a', sample('a'))),
        (6.0, lambda: ('sample b', sample('b'))),
        (7.0, run.at('at least two reaction tiers on the client dump', tiers_present)),
        (7.0, run.at('lower tiers are no farther from the pawn (T0 <= T1 <= T2)', tiers_ordered)),
        (7.0, run.at('the lowest tier updates more often than the highest', lowest_tier_updates_more)),
        (7.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (8.0, run.finish()),
    ]
    run.run(steps)


def scenario_minion_lagcomp(client=1):
    """M8 G7. At the batch's 60/15 lag, a client swings at chasing (moving) minions. The server's rewound candidate query ran and found candidates
    (spike5 lagCompQueries, candidates > 0), the server drained hits (drainedHits > 0), and the client confirmed a flash on a puppet it aimed at.
    G7 gap: nothing dumps which minion the rewound query accepted nor the swing's aim, so 'lands on the minion the client saw' is judged by
    predicted >= 1, confirmed >= 1, cancelled == 0 on the client (a hit that landed on another minion would cancel the predicted flash)"""
    run = g._Run('minion_lagcomp', client, judge_reconciles=False, sample=False)

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

    steps = [(0.0, lambda: ('setup', _setup())),
             (0.5, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 4 0'))),
             (0.6, lambda: ('spawn', _cmd('gstn.Minion.Bench.Attack 4 1')))]  # moving attackers walk into swing reach; Chase stops on the 600 cm ring
    steps += _swing_steps(2.5, 8.0, 0.5, client)
    steps += [
        (9.0, lambda: ('capture', capture())),
        (10.0, run.at('the server ran rewound queries and found candidates', lambda: (
            (_num(line(), 'lagCompQueries') or 0) > 0 and (_num(line(), 'candidates') or 0) > 0, line()[-140:]))),
        (10.0, run.at('the server drained hits on moving minions (drainedHits > 0)', lambda: ((_num(line(), 'drainedHits') or 0) > 0, line()[-140:]))),
        (10.0, run.at('the client predicted and confirmed a flash on the puppet it aimed at', confirmed_flash)),
        (10.1, lambda: ('clear', _cmd('gstn.Minion.Bench.Clear'))),
        (11.0, run.finish()),
    ]
    run.run(steps)


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (dedicated server + 2 clients as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_MINION, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
