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

SUITE_PROJECTILE = ['projectile_ability_hits', 'projectile_ghost_adopt']
# Measurement, run on its own (listen + 2): wire bytes with ~20 projectiles alive, recorded not judged
SUITE_PROJECTILE_BYTES = ['projectile_bytes']
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


_GHOST_REPORT = re.compile(r'ProjectileGhostReport world=(\S+) spawned=(\d+) adopted=(\d+) snapped=(\d+) expired=(\d+) pending=(\d+) '
                           r'lastErrorCm=([\d.]+) maxAdoptedErrorCm=([\d.]+) lastArrivalFrames=(-?\d+) received=(\d+)')
_GHOST_ROW = re.compile(r'Projectile ghost (adopted|snapped) for \S+: ghost start (-?\d+), host start (-?\d+), frame (-?\d+), error ([\d.]+) cm')


def scenario_projectile_ghost_adopt(client=1):
    """Slice 3, predicted ghost: the client fires three shots through gstn.Projectile.GhostFire (its ghost at its next frame, the server
    fires the same pawn's shot starting at that frame, as for a predicted activation). On that client each ghost is adopted by the host's
    projectile when it arrives (same start frame, error under 1 cm, none snapped or expired), after a positive number of frames (the round
    trip the ghost hid). With gstn.Projectile.PredictGhost 0 a fourth shot draws no ghost"""
    run = g._Run('projectile_ghost_adopt', client, judge_reconciles=False, sample=False)
    st = {}

    def report():
        _cmd('gstn.Projectile.GhostReport', client)
        rows = _GHOST_REPORT.findall(_log())
        st['report'] = rows[-1] if rows else None
        st['rows'] = _GHOST_ROW.findall(_log())
        return f"report {st['report']}"

    def cvar_off_shot():
        _cmd('gstn.Projectile.PredictGhost 0', client)
        _cmd('gstn.Projectile.GhostFire 1500 0 90 30', client)
        _cmd('gstn.Projectile.PredictGhost 1', client)
        return 'shot with the ghost off'

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('straight', _cmd('gstn.Projectile.GhostFire 1500 0 90 30', client))),
        (2.0, lambda: ('arc', _cmd('gstn.Projectile.GhostFire 1200 -980 90 40', client))),
        (3.5, lambda: ('fast', _cmd('gstn.Projectile.GhostFire 3000 0 60 20', client))),
        (5.0, lambda: ('report', report())),
        (5.5, lambda: ('ghost off', cvar_off_shot())),
        (7.0, lambda: ('report', report())),
        (7.5, run.at('three ghost fires, each fired on the server', lambda: (
            len(f := re.findall(r'ProjectileGhostFire player=\S+ start=(-?\d+) .* ghost=(\d) fired=(\d)', _log())) == 4
            and all(x[2] == '1' for x in f) and [x[1] for x in f] == ['1', '1', '1', '0'], f'{f}'))),
        (7.5, run.at('each ghost adopted by the host projectile, same start frame, error < 1 cm', lambda: (
            len(st['rows']) == 3 and all(r[0] == 'adopted' and r[1] == r[2] and float(r[4]) < 1.0 for r in st['rows']), f"{st['rows']}"))),
        (7.5, run.at('client ghost counts: 3 spawned, 3 adopted, 0 snapped, 0 expired, 0 pending; arrival > 0 frames', lambda: (
            st['report'] is not None and st['report'][1:6] == ('3', '3', '0', '0', '0') and int(st['report'][8]) > 0, f"{st['report']}"))),
        (7.5, run.finish()),
    ]
    run.run(steps)


_NET_ROW = re.compile(r'ProjectileNet label=(\S+) time=([\d.]+) outBytes=(\d+) conns=(\d+) perConn=(\S*) fired=(\d+) active=(\d+)')


def scenario_projectile_bytes(client=1):
    """Slice 3, replication at scale (recorded, not judged): wire bytes of the host per client connection over three 10 s windows, idle,
    then ~20 projectiles alive as 20 shots per second living 62 frames, then ~20 alive as 10 shots per second living 124 frames. If the
    spawn is the only payload, the extra bytes follow the spawn rate, not the number alive: window C costs about half of window B"""
    run = g._Run('projectile_bytes', client, judge_reconciles=False, sample=False)
    st = {'active': []}

    def snap(label):
        _cmd(f'gstn.Projectile.NetReport {label}')
        return label

    def burst(count, life):
        _cmd(f'gstn.Projectile.Burst {count} {life}')
        m = re.findall(r'ProjectileBurst fired=(\d+) active=(\d+)', _log())
        if m:
            st['active'].append(int(m[-1][1]))
        return f'burst {count} {life}'

    def windows():
        rows = {r[0]: r for r in _NET_ROW.findall(_log())}
        out = {}
        for name, a, b in (('idle', 'a0', 'a1'), ('20x62', 'b0', 'b1'), ('10x124', 'c0', 'c1')):
            if a in rows and b in rows:
                dt = float(rows[b][1]) - float(rows[a][1])
                ca = [int(x.split('/')[0]) for x in rows[a][4].split(',') if x]
                cb = [int(x.split('/')[0]) for x in rows[b][4].split(',') if x]
                per = [(y - x) / dt / 1000.0 for x, y in zip(ca, cb)]
                out[name] = (round(dt, 2), [round(v, 2) for v in per], int(rows[b][5]) - int(rows[a][5]))
        st['w'] = out
        return out

    def judge():
        w = windows()
        if not all(k in w for k in ('idle', '20x62', '10x124')):
            return False, f'windows {w}'
        idle = w['idle'][1]
        for name in ('20x62', '10x124'):
            extra = [b - a for a, b in zip(idle, w[name][1])]
            fired = w[name][2]
            per_shot = [round(e * 1000.0 * w[name][0] / fired, 1) for e in extra] if fired else []
            msg = f'{name}: {w[name][1]} KB/s per client (idle {idle}), extra {[round(e, 2) for e in extra]} KB/s, fired {fired}, ~{per_shot} B per projectile per client'
            run.note(msg)
            unreal.log(f'ProjectileBytes {msg}')
        for wi in range(1, len(g.worlds())):
            _cmd('gstn.Projectile.GhostReport', wi)
        received = [int(r[9]) for r in _GHOST_REPORT.findall(_log())[-(len(g.worlds()) - 1):]]
        fired_total = int({r[0]: r for r in _NET_ROW.findall(_log())}['c1'][5])
        unreal.log(f"ProjectileBytes alive at each burst {st['active']}; fired {fired_total}, received per client {received}")
        return (w['20x62'][2] >= 190 and w['10x124'][2] >= 95 and received and all(n == fired_total for n in received),
                f'windows {w} fired {fired_total} received {received}')

    steps = [(0.0, lambda: ('setup', _setup())), (1.0, lambda: ('snap', snap('a0'))), (11.0, lambda: ('snap', snap('a1'))), (11.05, lambda: ('snap', snap('b0')))]
    for k in range(10):
        steps.append((11.1 + k, lambda: ('burst', burst(20, 62))))
    steps += [(21.05, lambda: ('snap', snap('b1'))), (21.1, lambda: ('snap', snap('c0')))]
    for k in range(10):
        steps.append((21.15 + k, lambda: ('burst', burst(10, 124))))
    steps += [(31.1, lambda: ('snap', snap('c1'))), (31.5, run.at('all bursts fired, every client received each projectile once (bytes in the log)', judge)), (31.6, run.finish())]
    run.run(steps)


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (dedicated server + 2 clients as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_PROJECTILE, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
