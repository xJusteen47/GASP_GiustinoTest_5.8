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
                'minion_reaction_tiers', 'minion_lagcomp', 'minion_lod', 'minion_tokens_brain', 'minion_wave']
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


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (dedicated server + 2 clients as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_MINION, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
