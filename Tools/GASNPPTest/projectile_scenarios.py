# Projectile slice 2: the world path of a projectile on a dedicated server + 2 clients (SUITE_PROJECTILE).
# The host fires through the ability (gstn.Projectile.Ability: ServerOnly UGSTN_GA_FireProjectileServer, event target = the other pawn),
# the subsystem sweeps it at the fixed tick and posts the hit into the victim's inbox (PostHit), the victim drains it and reacts.
#
#   import projectile_scenarios as ps
#   ps.run_batch(report='pj2/projectile.log')   # editor with PIE = dedicated server + 2 clients; poll ds.defence_batch_report()
#
# Judged on the server's copy of the victim (inbox rows, damage, reaction) and on every world's copy afterwards (damage converged).
# The grant is a deliberate server-only change, so reconciles are not judged.
import re

import unreal
import gasnpp_test as g
import defence_scenarios as ds

SUITE_PROJECTILE = ['projectile_ability_hits']
_marks = {}

# (speed cm/s, gravity Z cm/s^2, high arc, lifetime frames): straight, flat arc, lob (a near lob at 1200 cm/s flies ~2.5 s, past 120 frames)
SHOTS = [(1500, 0, 0, 120), (1200, -980, 0, 120), (1200, -980, 1, 240)]


def restore_editor():
    g._hit_logging(False)
    return 'hit log off'


def _cmd(text, wi=0):
    unreal.SystemLibrary.execute_console_command(g.worlds()[wi], text)


def _setup():
    g.hit_rules_setup()
    g._hit_logging(True)
    _marks['log'] = g.mark_log()
    return 'rules + hit log'


def _log():
    return g._read_log_since(_marks['log'])


def _ability_lines():
    return re.findall(r'ProjectileAbility shooter=(\d+) target=(\d+) .* activated=(\d)', _log())


def _shooter_target():
    rows = _ability_lines()
    return (int(rows[0][0]), int(rows[0][1])) if rows else (None, None)


def _victim_key(shooter):
    """The server pawn whose inbox holds rows from the shooter's entity id"""
    for key in g.comps(g.worlds()[0]):
        if any(r[1] == shooter for r in g._inbox_log(0, key)):
            return key
    return None


def _client_projectiles():
    cls = unreal.GSTN_Projectile.static_class()
    return [len(unreal.GameplayStatics.get_all_actors_of_class(w, cls)) for w in g.worlds()[1:]]


def scenario_projectile_ability_hits(client=1):
    """The host fires three shots at the other pawn through the fire ability: straight, flat arc and lob. Each activation fires one projectile
    (log), each projectile hits the target entity (log), the target's server inbox receives one row per shot with the shooter as attacker and
    damage > 0, the damage percent rises, the victim reacts (hitstop or hitstun set), the clients draw the lob while it flies, and once the
    shots are over the damage percent is the same on the server and both clients"""
    run = g._Run('projectile_ability_hits', client, judge_reconciles=False, sample=False)
    st = {'seen': []}

    def fire(i):
        speed, grav, high, life = SHOTS[i]
        _cmd(f'gstn.Projectile.Ability 0 1 {speed} {grav} {high} {life}')
        return f'shot {SHOTS[i]}'

    def capture():
        shooter, target = _shooter_target()
        key = _victim_key(shooter) if shooter is not None else None
        st.update(shooter=shooter, target=target, key=key)
        if key:
            st.update(rows=[r for r in g._inbox_log(0, key) if r[1] == shooter], info=g.victim_info(0, key), dmg=g.damage_of(0, key))
        return f'shooter {shooter} target {target} key {key}'

    def fired_and_hit():
        text = _log()
        fired = re.findall(r'Projectile (\d+) fired by (-?\d+)', text)
        hits = re.findall(r'Projectile (\d+) hit (\d+) at frame (\d+)', text)
        ok = len(fired) == 3 and len(hits) == 3 and all(int(h[1]) == st.get('target') for h in hits)
        return ok, f'fired {fired} hits {hits}'

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('straight', fire(0))),
        (3.0, lambda: ('flat arc', fire(1))),
        (5.5, lambda: ('lob', fire(2))),
        (6.0, lambda: ('sample', st['seen'].append(_client_projectiles()))),
        (6.5, lambda: ('sample', st['seen'].append(_client_projectiles()))),
        (9.5, lambda: ('capture', capture())),
        (10.0, run.at('three activations of the fire ability on the host', lambda: (
            len(_ability_lines()) == 3 and all(a[2] == '1' for a in _ability_lines()), f'{_ability_lines()}'))),
        (10.0, run.at('three projectiles fired, each hit the target entity', fired_and_hit)),
        (10.0, run.at('target inbox (server) got one row per shot from the shooter, damage > 0', lambda: (
            st.get('key') is not None and len(st.get('rows', [])) == 3 and all(r[7] > 0 for r in st['rows']), f"key {st.get('key')} rows {st.get('rows')}"))),
        (10.0, run.at('damage percent rose on the server', lambda: (st.get('dmg', 0) > 0, f"damagePercent {st.get('dmg')}"))),
        (10.0, run.at('victim reacted (hitstop or hitstun set)', lambda: (
            st.get('info') is not None and (st['info'][3] > 0 or st['info'][4] > 0), f"victim_info {st.get('info')}"))),
        (10.0, run.at('the clients drew the lob in flight', lambda: (any(n > 0 for s in st['seen'] for n in s), f"projectile actors per client {st['seen']}"))),
        (10.0, run.at('damage percent equal on the server and both clients', lambda: (
            st.get('key') is not None and max(d := [round(g.damage_of(wi, st['key']), 2) for wi in range(len(g.worlds()))]) - min(d) < 0.51,
            f"per world {[round(g.damage_of(wi, st['key']), 2) for wi in range(len(g.worlds()))] if st.get('key') else None}"))),
        (10.0, run.finish()),
    ]
    run.run(steps)


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (dedicated server + 2 clients as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_PROJECTILE, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
