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

SUITE_MINION = ['minion_hits_player', 'minion_tokens']
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


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (dedicated server + 2 clients as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_MINION, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
