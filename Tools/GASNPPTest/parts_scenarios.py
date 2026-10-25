"""Harness scenarios of the GSTNParts plugin (M4b). Imports gasnpp_test; run from the editor Python console, like the M4 scenarios.

SPIKE section (M4b P0a, throwaway; NOT in SUITE_DEFAULT): `spike_overlap_part` measures whether the attacker's predicted HurtboxIndex agrees with the
authority's under lag compensation, with six overlapping hurtboxes of different Priority and a victim that keeps moving.

    import parts_scenarios as ps
    ps.spike_overlap_part(client=1, cycles=120)     # attacker = remote client 1; client=0: the listen host attacks
    ps.spike_report()                               # the numbers so far (accumulated over calls)
"""
import math
import random

import unreal
import gasnpp_test as g

# (name, tag, priority, radius, half height, local x, local z): six capsules, torso/chest/weak overlap on purpose, head overlaps chest
SPIKE_HURTBOXES = (
    ('torso', 'Combat.Hurtbox.Torso', 1, 35.0, 55.0, 0.0, 15.0),
    ('head', 'Combat.Hurtbox.Head', 2, 22.0, 22.0, 0.0, 68.0),
    ('feet', 'Combat.Hurtbox.Feet', 0, 30.0, 30.0, 0.0, -62.0),
    ('chest', 'Combat.Hurtbox.Torso', 3, 28.0, 40.0, 12.0, 30.0),      # overlaps torso and head, higher Priority than both
    ('weak', 'Combat.Hurtbox.Torso', 5, 12.0, 12.0, 20.0, 20.0),       # small weak point inside torso and chest, highest Priority
    ('shoulder', 'Combat.Hurtbox.Arm', 2, 20.0, 20.0, 0.0, 35.0),      # same Priority as head: tie goes to the lower index
)

_acc = {'rows': [], 'inbox': [], 'cycles': 0, 'missing': 0}
_saved = {}


def _tag_container(names):
    c = unreal.GameplayTagContainer()
    return c


def _set_spike_hurtboxes():
    hs = unreal.load_asset('/Game/GASNPPTest/Combat/HBS_Test_Pawn')
    _saved['boxes'] = list(hs.get_editor_property('hurtboxes'))
    template = _saved['boxes'][0]
    boxes = []
    for name, tag, priority, radius, half_height, x, z in SPIKE_HURTBOXES:
        shape = unreal.GSTN_HitShape()
        shape.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
        shape.set_editor_property('extent', unreal.Vector(radius, radius, half_height))
        shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(x, 0.0, z)))
        box = unreal.GSTN_Hurtbox()
        box.set_editor_property('name', name)
        box.set_editor_property('shape', shape)
        box.set_editor_property('tags', template.get_editor_property('tags'))
        box.set_editor_property('priority', priority)
        boxes.append(box)
    hs.set_editor_property('hurtboxes', boxes)
    return 'six hurtboxes'


def _restore_hurtboxes():
    if 'boxes' in _saved:
        hs = unreal.load_asset('/Game/GASNPPTest/Combat/HBS_Test_Pawn')
        hs.set_editor_property('hurtboxes', _saved.pop('boxes'))
    return 'original hurtbox set restored'


def spike_overlap_part(client=1, cycles=120, cycle_s=3.2):
    """Each cycle: victim placed at a random spot in front of the attacker, given a random stick for the attack, the attack tapped, the hit rows of the
    authority's copy of the attacker and the attacker's own copy read (the logs keep 512 frames only, so every cycle)."""
    rig = g._HitRig('spike_overlap_part', client)
    run, akey, vkey, vi = rig.run, rig.akey, rig.vkey, rig.vi
    rng = random.Random(1234 + client)
    steps = [rig.setup_step(), (rig.cursor, _set_spike_hurtboxes)]
    t = rig.cursor + 0.3
    mode = g.detected_mode()

    def make_cycle(i):
        st = {}

        def place():
            g.set_damage(vkey, 0)
            g._place_near(client, vkey, (rng.uniform(110.0, 170.0), rng.uniform(-35.0, 35.0)))
            return 'placed', i

        def mark():
            st['F0'] = g._latest_frame(0, akey)
            a = g.snap(g.worlds()[0])
            p = a[vkey]['location']
            ang = rng.uniform(0.0, 2.0 * math.pi)
            tgt = (p[0] + 300 * math.cos(ang), p[1] + 300 * math.sin(ang))
            g.hold(vi, 'move', g._steer_value(vi, tgt))
            g.combat_tap(client, g.button_mask('Attack'))
            return 'attack', i

        def collect():
            g.release(vi, 'move')
            F0 = st['F0']
            srv = [r for r in g._hit_log(0, akey) if r[0] >= F0]
            cli = [r for r in g._hit_log(client, akey) if r[0] >= F0] if client != 0 else []
            ibs = [r for r in g._inbox_log(0, vkey) if r[0] >= F0]
            ibc = [r for r in g._inbox_log(vi, vkey) if r[0] >= F0]
            _acc['rows'].append((i, srv, cli))
            _acc['inbox'].append((i, ibs, ibc))
            _acc['cycles'] += 1
            return 'collected', i, len(srv), len(cli)

        return [(0.0, place), (1.0, mark), (2.2, lambda: (g.release(vi, "move"), "stick released")[1]), (cycle_s - 0.05, collect)]

    for i in range(cycles):
        for dt, fn in make_cycle(i):
            steps.append((t + dt, fn))
        t += cycle_s
    rig.cursor = t
    steps += rig.end_steps(extra=[_restore_hurtboxes])
    run.run(steps)
    return f'{cycles} cycles, ~{t:.0f} s, mode {mode}'


def spike_report(reset=False):
    """Per hit: the authority's hurtbox against the attacker's own client copy (same frame, victim, group, hitbox), and the inbox of the authority
    against the victim client's. Returns the text."""
    n = agree = dis = only_srv = only_cli = 0
    by_idx = {}
    dis_rows = []
    for i, srv, cli in _acc['rows']:
        cmap = {(r[0], r[1], r[2], r[3]): r for r in cli}
        for r in srv:
            n += 1
            by_idx[r[4]] = by_idx.get(r[4], 0) + 1
            c = cmap.pop((r[0], r[1], r[2], r[3]), None)
            if c is None:
                only_srv += 1
            elif c[4] == r[4]:
                agree += 1
            else:
                dis += 1
                dis_rows.append((i, r, c))
        only_cli += len(cmap)
    ib_n = ib_dis = ib_missing = 0
    for i, ibs, ibc in _acc['inbox']:
        cm = {(r[0], r[1], r[2]): r for r in ibc}
        for r in ibs:
            ib_n += 1
            c = cm.get((r[0], r[1], r[2]))
            if c is None:
                ib_missing += 1
            elif c[3] != r[3]:
                ib_dis += 1
    text = (f"cycles {_acc['cycles']}; authority hits {n}; matched on the client copy {agree + dis} (agree {agree}, DISAGREE {dis}); only on the authority {only_srv}, "
            f"only on the client {only_cli}; chosen index histogram {dict(sorted(by_idx.items()))}; authority inbox rows {ib_n}, "
            f"missing on the victim's client {ib_missing}, differing hurtbox {ib_dis}; disagreements {dis_rows[:6]}")
    if reset:
        _acc.update(rows=[], inbox=[], cycles=0)
    return text
