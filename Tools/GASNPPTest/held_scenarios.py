# M10 slice 1: a held object end to end (SUITE_HELD). The host spawns a test object near pawn 0, pawn 0 picks it up (ServerOnly pickup
# ability), every world draws it at its holder, pawn 0 throws it at pawn 1 (ServerOnly throw ability = a GSTNProjectile shot with the
# object's hit data), the shot hits pawn 1 through the projectile sweep and PostHit, the object lands, and pawn 1 picks it up again.
#
#   import held_scenarios as hs
#   hs.run_batch(report='m10/held.log')   # editor with PIE (listen + 2 by default); poll ds.defence_batch_report()
#
# Judged on the logs of the host (pickup, throw, fire, hit, landing), the victim's server inbox and damage, and the object's state and place
# in every world (HeldObjectReport). The grants are deliberate server-only changes, so reconciles are not judged.
import re

import unreal
import gasnpp_test as g
import defence_scenarios as ds

SUITE_HELD = ['held_pickup_throw_hit', 'held_drop_and_break']
_marks = {}

_HAND = re.compile(r'HeldHandReport world=(\S+) entity=(\d+) carrying=(\d) target=(\d)')
_REPORT = re.compile(r'HeldObjectReport world=(\S+) name=(\S+) state=(\d) holderEntity=(-?\d+) holderDistCm=(-?[\d.]+) loc=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)')


def _cmd(text, wi=0):
    unreal.SystemLibrary.execute_console_command(g.worlds()[wi], text)


def _setup():
    g.hit_rules_setup()
    g._hit_logging(True)
    _marks['log'] = g.mark_log()
    return 'rules + hit log'


def _log():
    return g._read_log_since(_marks['log'])


def _reports():
    """One HeldObjectReport row per world (the object's state, holder entity, distance to the holder and location in that world)"""
    out = []
    for wi in range(len(g.worlds())):
        before = len(_REPORT.findall(_log()))
        _cmd('gstn.Held.Report', wi)
        rows = _REPORT.findall(_log())[before:]
        out.append(rows[-1] if rows else None)
    return out


def _hands(targets=None):
    """Per world: {entity id: carrying 0/1}, the projected Combat.Held.Carrying of every combat pawn (from its fragment). With a list in
    targets, also per world {entity id: Interact.Target.Pickup 0/1} (slice 3) from the same rows: one report per world, as the log reads stall
    the game thread and shift the scenario's timeline"""
    out = []
    for wi in range(len(g.worlds())):
        before = len(_HAND.findall(_log()))
        _cmd('gstn.Held.Report', wi)
        rows = _HAND.findall(_log())[before:]
        out.append({int(r[1]): int(r[2]) for r in rows})
        if targets is not None:
            targets.append({int(r[1]): int(r[3]) for r in rows})
    return out


def _victim_key(attacker):
    for key in g.comps(g.worlds()[0]):
        if any(r[1] == attacker for r in g._inbox_log(0, key)):
            return key
    return None


def scenario_held_pickup_throw_hit(client=1):
    """Pick up, hold, throw, hit, land, pick up again. Pawn 0 picks up the object (host log), every world shows it Held within 150 cm of its
    holder; pawn 0 throws it at pawn 1: one projectile fired and it hits pawn 1's entity, pawn 1's server inbox gets one row from pawn 0 with
    damage > 0, the damage is equal in every world; the object lands (host log) and rests at the same place in every world (5 cm); pawn 1
    then picks it up (host log, Held by pawn 1's entity on the server). On DefaultLevel_Harness with listen + 2 the third pawn stands in the
    line of fire and takes the hit: the hit is judged on whichever entity the log names"""
    run = g._Run('held_pickup_throw_hit', client, judge_reconciles=False, sample=False)
    st = {}

    def capture(name):
        st[name] = _reports()
        return f'{name} {st[name]}'

    def thrower():
        m = re.findall(r'HeldPickup entity (\d+) holds object (\d+)', _log())
        return int(m[0][0]) if m else None

    def hits():
        return re.findall(r'Projectile (\d+) hit (\d+) at frame (\d+)', _log())

    def held_everywhere():
        rows = st.get('held', [])
        ok = rows and all(r is not None and r[2] == '1' and 0.0 <= float(r[4]) < 150.0 for r in rows) and int(rows[0][3]) == thrower()
        return ok, f'thrower {thrower()} rows {rows}'

    def hit_target():
        text = _log()
        fired = re.findall(r'Projectile (\d+) fired by (-?\d+)', text)
        h = hits()
        st['target'] = int(h[0][1]) if h else None
        ok = len(re.findall(r'HeldObject \d+ thrown by', text)) == 1 and len(fired) == 1 and int(fired[0][1]) == thrower() and len(h) == 1
        return ok, f'fired {fired} hits {h}'

    def inbox():
        key = _victim_key(thrower())
        st['key'] = key
        rows = [r for r in g._inbox_log(0, key) if r[1] == thrower()] if key else []
        return key is not None and len(rows) == 1 and rows[0][7] > 0, f'key {key} rows {rows}'

    def damage_equal():
        key = st.get('key')
        d = [round(g.damage_of(wi, key), 2) for wi in range(len(g.worlds()))] if key else []
        return bool(d) and d[0] > 0 and max(d) - min(d) < 0.51, f'damage per world {d}'

    def landed_equal():
        rows = st.get('landed', [])
        if not rows or any(r is None or r[2] != '0' for r in rows):
            return False, f'rows {rows}'
        locs = [tuple(float(x) for x in r[5:8]) for r in rows]
        err = max(max(abs(a - b) for a, b in zip(locs[0], loc)) for loc in locs)
        return 'landed at frame' in _log() and err < 5.0, f'max delta {err:.2f} cm, rows {rows}'

    def repicked():
        m = re.findall(r'HeldPickup entity (\d+) holds object (\d+)', _log())
        rows = st.get('repick', [])
        # Pawn 1 picks it up where it landed (the shot may have hit another pawn standing in the line of fire: the harness map's spawns)
        other = int(m[1][0]) if len(m) == 2 else None
        return (other is not None and other != thrower() and rows and rows[0] is not None and rows[0][2] == '1' and int(rows[0][3]) == other,
                f'pickups {m} hit entity {st.get("target")} server row {rows[0] if rows else None}')

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('spawn', _cmd('gstn.Held.Spawn 0 120'))),
        (1.0, lambda: ('pickup', _cmd('gstn.Held.Pickup 0'))),
        (2.0, lambda: ('held', capture('held'))),
        (2.1, run.at('pawn 0 holds the object; every world draws it Held near its holder', held_everywhere)),
        (2.5, lambda: ('throw', _cmd('gstn.Held.Throw 0 1'))),
        (5.0, run.at('one throw, one projectile from the thrower, one hit on the target entity', hit_target)),
        (5.0, run.at('target inbox (server): one row from the thrower, damage > 0', inbox)),
        (5.1, run.at('damage percent equal on every world', damage_equal)),
        (5.2, lambda: ('landed', capture('landed'))),
        (5.3, run.at('the object landed and rests at the same place in every world', landed_equal)),
        (5.5, lambda: ('repick', _cmd('gstn.Held.Pickup 1'))),
        (6.5, lambda: ('repick', capture('repick'))),
        (6.6, run.at('pawn 1 picks the landed object up again', repicked)),
        (6.7, run.finish()),
    ]
    run.run(steps)


def scenario_held_drop_and_break(client=1):
    """Slice 2, the hold as a link. Every pawn picks up its own object; every world sees each holder carrying (Combat.Held.Carrying projected
    from the holder's synced fragment, so a client sees it by correction). Pawn 0 puts its object down (drop command): HeldBreak reason
    Dropped, its hand empty in every world, the others still carrying. Pawn 0 picks it up again and throws it at pawn 1: the entity the shot
    hits (pawn 1 or a pawn in the line of fire) loses its object (HeldBreak reason HolderState, its hitstun), its hand empty in every world;
    a holder that was not hit keeps its object"""
    run = g._Run('held_drop_and_break', client, judge_reconciles=False, sample=False)
    st = {}

    def npawns():
        return len(g.comps(g.worlds()[0]))

    def capture(name):
        st[name + '_t'] = []
        st[name] = _hands(st[name + '_t'])
        return f'{name} {st[name]} targets {st[name + "_t"]}'

    def targets_free():
        # Slice 3: an object at rest in reach and an empty hand project Interact.Target.Pickup, in every world
        rows = st.get('free_t', [])
        ok = bool(rows) and all(sum(w.values()) >= npawns() for w in rows)
        return ok, f'targets {rows}'

    def targets_carry():
        # A full hand projects no pickup target, in every world
        rows = st.get('carry_t', [])
        ok = bool(rows) and all(w and all(v == 0 for v in w.values()) for w in rows)
        return ok, f'targets {rows}'

    def holders():
        # entity -> object, the first pickup of each entity
        out = {}
        for e, o in re.findall(r'HeldPickup entity (\d+) holds object (\d+)', _log()):
            out.setdefault(int(e), int(o))
        return out

    def all_carry():
        h = holders()
        rows = st.get('carry', [])
        ok = len(h) == npawns() and rows and all(all(w.get(e) == 1 for e in h) for w in rows)
        return ok, f'holders {h} hands {rows}'

    def dropped():
        h = holders()
        first = min(h) if h else None
        rows = st.get('dropped', [])
        breaks = re.findall(r'HeldBreak object=(\d+) entity=(-?\d+) reason=(\w+)', _log())
        st['first'] = first
        ok = (first is not None and [b for b in breaks if b[2] == 'Dropped'] == [(str(h[first]), str(first), 'Dropped')]
              and rows and all(w.get(first) == 0 and all(w.get(e) == 1 for e in h if e != first) for w in rows))
        return ok, f'breaks {breaks} hands {rows}'

    def broke_on_hit():
        hits = re.findall(r'Projectile (\d+) hit (\d+) at frame (\d+)', _log())
        breaks = re.findall(r'HeldBreak object=(\d+) entity=(-?\d+) reason=(\w+) frame=(\d+) (.*)', _log())
        hit = int(hits[0][1]) if hits else None
        state = [b for b in breaks if b[2] == 'HolderState']
        rows = st.get('after', [])
        others = [e for e in holders() if e not in (st.get('first'), hit)]
        ok = (len(hits) == 1 and len(state) == 1 and int(state[0][1]) == hit and 'Hitstun' in state[0][4]
              and rows and all(w.get(hit) == 0 and all(w.get(e) == 1 for e in others) for w in rows))
        return ok, f'hit {hit} breaks {breaks} others {others} hands {rows}'

    steps = [
        (0.0, lambda: ('setup', _setup())),
        (0.5, lambda: ('spawn', [_cmd(f'gstn.Held.Spawn {i} 120') for i in range(npawns())])),
        (0.7, lambda: ('free', capture('free'))),
        (0.75, run.at('an object in reach, hand empty: every world projects Interact.Target.Pickup for every pawn', targets_free)),
        (1.0, lambda: ('pickup', [_cmd(f'gstn.Held.Pickup {i}') for i in range(npawns())])),
        (2.5, lambda: ('carry', capture('carry'))),
        (2.6, run.at('every pawn holds its object; every world projects Carrying for each holder', all_carry)),
        (2.65, run.at('holding: no pawn projects a pickup target in any world', targets_carry)),
        (2.7, lambda: ('drop', _cmd('gstn.Held.Drop 0'))),
        (4.0, lambda: ('dropped', capture('dropped'))),
        (4.1, run.at('pawn 0 put it down (Dropped); its hand empty everywhere, the others still carry', dropped)),
        (4.2, lambda: ('repick', _cmd('gstn.Held.Pickup 0'))),
        (5.2, lambda: ('throw', _cmd('gstn.Held.Throw 0 1'))),
        (8.0, lambda: ('after', capture('after'))),
        (8.1, run.at('the hit holder dropped its object (HolderState, hitstun); its hand empty everywhere; others keep theirs', broke_on_hit)),
        (8.2, run.finish()),
    ]
    run.run(steps)


def run_batch(names=None, lag=60, jitter=15, loss=0, report=None):
    """Each scenario in a fresh PIE (as the editor's play settings say); poll ds.defence_batch_report()"""
    return ds.defence_batch(names or SUITE_HELD, lag=lag, jitter=jitter, loss=loss, report=report, module=__import__(__name__))
