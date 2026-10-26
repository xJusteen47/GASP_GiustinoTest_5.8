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

_acc = {'rows': [], 'inbox': [], 'cycles': 0, 'missing': 0, 'rb': []}
_spike_parts = {'on': False}
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
    if _spike_parts['on']:
        _set_spike_parts()
    return 'six hurtboxes'


def _restore_hurtboxes():
    if _spike_parts['on']:
        parts_reset()
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
            st['rb0'] = g.rollbacks(client)
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
            _acc['rb'].append((i, g.rollbacks(client) - st['rb0']))
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
    # P8: does a wrong index change the attacker's predicted ricochet? Among the matched hits, the flagged bit (row field 5) of the two copies
    ric_n = ric_dis = ric_idx_dis = 0
    ric_rows = []
    for i, srv, cli in _acc['rows']:
        cmap = {(r[0], r[1], r[2], r[3]): r for r in cli}
        for r in srv:
            c = cmap.get((r[0], r[1], r[2], r[3]))
            if c is None:
                continue
            ric_n += r[5] == 1
            if c[5] != r[5]:
                ric_dis += 1
                ric_rows.append((i, r, c))
            if c[4] != r[4]:
                ric_idx_dis += 1
    rb_cycles = [x for x in _acc['rb'] if x[1] > 0]
    text += (f"; ricochet-flag disagreements among matched hits {ric_dis} (authority ricochets {ric_n}, index disagreements {ric_idx_dis}); attacker-client rollbacks: "
             f"{sum(x[1] for x in _acc['rb'])} in {len(rb_cycles)} of {len(_acc['rb'])} cycles; flag-disagreement rows {ric_rows[:6]}")
    if reset:
        _acc.update(rows=[], inbox=[], cycles=0, rb=[])
    return text


def _set_spike_parts():
    """P8 re-measure: the part set of the pawns in memory has one part per spike hurtbox, hard (hardness 5: the test move's penetration 0 bounces) and soft
    ones alternating, so a wrong overlap choice between neighbours flips the ricochet the attacker predicts"""
    part_set = unreal.load_asset(PARTS_DIR + 'PS_Test_Pawn')
    if 'parts' not in _ps_saved:
        _ps_saved['parts'] = list(part_set.get_editor_property('parts'))
    parts = []
    for name, hardness in (('torso', 0), ('head', 5), ('feet', 0), ('chest', 5), ('weak', 0), ('shoulder', 5)):
        part = unreal.GSTN_PartDefinition()
        part.set_editor_property('name', name.capitalize())
        part.set_editor_property('hurtboxes', [name])
        part.set_editor_property('hardness', hardness)
        part.set_editor_property('part_cap', 500.0)
        parts.append(part)
    part_set.set_editor_property('parts', parts)
    if 'rules' not in _ps_saved:
        unreal.GSTN_PartsContentLibrary.convert_rules_to_parts(g.hit_rules())
        _ps_saved['rules'] = True
    attach_everywhere(part_set)
    return 'spike part set attached'


def scenario_p0a_ricochet(client=1, cycles=130, cycle_s=2.8):
    """P8 re-measure of P0a: spike_overlap_part with the spike part set, so the attacker's ricochet prediction depends on the chosen part. Accumulates in
    the spike report (spike_report()), chunk after chunk, each chunk in a fresh PIE through parts_batch(['p0a_ricochet'] * n)"""
    _spike_parts['on'] = True
    return spike_overlap_part(client, cycles, cycle_s)


# ======================================================================================================================================================
# M4b P7 part 2 / P7b: the parts scenarios (SUITE_PARTS, not in SUITE_DEFAULT). Every scenario starts in a FRESH PIE (the loadouts and the part state stay,
# and a second scenario in one PIE fails the hit_basic geometry checks even without parts): parts_batch() restarts PIE between them.
#
#   import parts_scenarios as ps
#   ps.scenario_part_break()                 # or part_cap, part_hitgroups, part_rollback, part_host (listen only), part_restore
#   ps.last()                                # (verdict, checks) of the last one
#   ps.parts_batch(ps.SUITE_PARTS)           # restarts PIE before each, poll ps.parts_batch_report()
#
# The test content is the one of setup_gas_assets.py --parts-content, adjusted IN MEMORY by parts_setup() (never saved) and put back by parts_reset():
#   - the pawns' hurtbox set HBS_Test_Pawn gets the six hurtboxes of HBS_Test_PawnParts, all but the target part's one moved out of reach, and the target's
#     one given the shape of the torso box that the Root track of the test move is known to reach (so every hit lands on the target part);
#   - the target part gets hardness 0 (the "equal" band: 65 % of the hit's 8) and a small cap, so a few hits break it;
#   - the rules get PartResolve in place of HealthWeight and ApplyDamage (ConvertRulesToParts), put back with fill_standard.
# The part set is attached by hand on every world (GSTN_PartsKitLibrary.attach_part_set_to_actor): the pawn Blueprint is shared and keeps no attach component.
# ======================================================================================================================================================
import copy
import json
import time

PARTS_DIR = '/Game/GASNPPTest/Combat/'
PART_INDEX = {'Torso': 0, 'Head': 1, 'Legs': 2, 'ArmL': 3, 'ArmR': 4, 'Core': 5}
AT_CAP = 255
SUITE_PARTS = ['part_break', 'part_cap', 'part_hitgroups', 'part_rollback', 'part_host', 'part_restore', 'part_ricochet', 'part_stagger', 'part_weakpoint', 'part_break_launch']
_ps_saved = {}


def parts_reset():
    """Puts the in-memory test content back: the pawns' hurtbox set, the part set, the rules"""
    msgs = []
    if 'hbs' in _ps_saved:
        unreal.load_asset(PARTS_DIR + 'HBS_Test_Pawn').set_editor_property('hurtboxes', _ps_saved.pop('hbs'))
        msgs.append('hurtboxes')
    if 'parts' in _ps_saved:
        unreal.load_asset(PARTS_DIR + 'PS_Test_Pawn').set_editor_property('parts', _ps_saved.pop('parts'))
        msgs.append('parts')
    if 'rules' in _ps_saved:
        unreal.GSTN_CombatRules.fill_standard(g.hit_rules())
        g.hit_rules_setup()
        _ps_saved.pop('rules')
        msgs.append('rules')
    return 'restored ' + ','.join(msgs)


def parts_setup(target='ArmL', cap=12.0, hardness=0):
    """The in-memory test content for a scenario whose hits all land on the part `target` (see above). Returns the part set asset"""
    part_set = unreal.load_asset(PARTS_DIR + 'PS_Test_Pawn')
    hbs = unreal.load_asset(PARTS_DIR + 'HBS_Test_Pawn')
    parts_hbs = unreal.load_asset(PARTS_DIR + 'HBS_Test_PawnParts')
    if 'hbs' not in _ps_saved:
        _ps_saved['hbs'] = list(hbs.get_editor_property('hurtboxes'))
        _ps_saved['parts'] = list(part_set.get_editor_property('parts'))
    template = _ps_saved['hbs'][0]
    parts = list(part_set.get_editor_property('parts'))
    index = PART_INDEX[target]
    target_box = str(list(parts[index].get_editor_property('hurtboxes'))[0])
    parts[index].set_editor_property('part_cap', cap)
    parts[index].set_editor_property('hardness', hardness)
    part_set.set_editor_property('parts', parts)

    boxes = []
    for src in parts_hbs.get_editor_property('hurtboxes'):
        name = str(src.get_editor_property('name'))
        shape = unreal.GSTN_HitShape()
        shape.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
        if name == target_box:
            shape.set_editor_property('extent', unreal.Vector(35.0, 35.0, 55.0))
            shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(0.0, 0.0, 15.0)))
        else:
            shape.set_editor_property('extent', unreal.Vector(5.0, 5.0, 5.0))
            shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(0.0, 0.0, -900.0)))
        box = unreal.GSTN_Hurtbox()
        box.set_editor_property('name', name)
        box.set_editor_property('shape', shape)
        box.set_editor_property('tags', template.get_editor_property('tags'))
        box.set_editor_property('priority', src.get_editor_property('priority'))
        boxes.append(box)
    hbs.set_editor_property('hurtboxes', boxes)
    if 'rules' not in _ps_saved:
        unreal.GSTN_PartsContentLibrary.convert_rules_to_parts(g.hit_rules())
        _ps_saved['rules'] = True
    return part_set


def _actor(wi, key):
    return g.comps(g.worlds()[wi])[key].get_owner()


def parts_state(wi, key):
    return unreal.GSTN_PartsKitLibrary.get_parts_state_string(_actor(wi, key))


def parts_states(key):
    return [parts_state(w, key) for w in range(len(g.worlds()))]


def parse_state(text):
    """'mask=0x0008 serial=1 dmg=[0,0,0,255]' -> (mask, serial, [damage]); None for 'none' / 'no combat'"""
    if not text.startswith('mask='):
        return None
    mask = int(text.split('mask=')[1].split()[0], 16)
    serial = int(text.split('serial=')[1].split()[0])
    dmg = [int(x) for x in text.split('dmg=[')[1].rstrip(']').split(',') if x]
    return mask, serial, dmg


def attach_everywhere(part_set):
    for w in g.worlds():
        for comp in g.comps(w).values():
            unreal.GSTN_PartsKitLibrary.attach_part_set_to_actor(comp.get_owner(), part_set)


def part_events(wi, victim_key):
    """[(type, frame, part, serial)] the world's event subsystem published about the pawn since start_part_event_log"""
    actor = _actor(wi, victim_key)
    out = []
    for row in unreal.GSTN_PartsKitLibrary.get_part_event_log(actor).split(';'):
        if row:
            kind, frame, part, serial, target = row.split(':', 4)
            if target == actor.get_name():
                out.append((kind, int(frame), int(part), int(serial)))
    return out


def _client_worlds():
    return range(1, len(g.worlds()))


class _PartsRig(g._HitRig):
    """_HitRig with an explicit victim (the host can be either side) and the parts content set up in the first step"""

    def __init__(self, name, attacker=1, victim=2, target='ArmL', cap=12.0, hardness=0, both_kits=False, judge_reconciles=False):
        self.client, self.vi = attacker, victim
        self.akey, self.vkey = g.key_of_player(attacker), g.key_of_player(victim)
        self.run = g._Run(name, attacker, judge_reconciles=judge_reconciles, extra=[('v_srv', 0, self.vkey), ('v_cli', victim, self.vkey)])
        self.shots, self.vinfo, self.cursor, self.kit = {}, [], 0.3, None
        self.target, self.cap, self.hardness, self.both_kits = target, cap, hardness, both_kits
        self.part_set = None
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def swapped(self):
        """The same run with attacker and victim exchanged (own shots; keep the cursors together with sync_cursor)"""
        other = copy.copy(self)
        other.client, other.vi, other.akey, other.vkey = self.vi, self.client, self.vkey, self.akey
        other.shots, other.vinfo = {}, []
        return other

    def setup_step(self):
        def f():
            g.hit_rules_setup()
            g._hit_logging(True)
            self.kit = g._apply_loadout(self.akey, g.HIT_KIT)
            self.kit2 = g._apply_loadout(self.vkey, g.HIT_KIT) if self.both_kits else 'OK'
            self.part_set = parts_setup(self.target, self.cap, self.hardness)
            attach_everywhere(self.part_set)
            for w in g.worlds():
                for comp in g.comps(w).values():
                    unreal.GSTN_PartsKitLibrary.start_part_event_log(comp.get_owner())
            return 'kits', self.kit, self.kit2, 'parts', self.target
        return (self.cursor, f)

    def end_steps(self, extra=None):
        return super().end_steps(extra=[parts_reset] + list(extra or ()))

    def shot(self, name, **kw):
        """_HitRig.shot, plus: the hits of the shot are cut down to the ones that reached THIS victim (the test move's other tracks can touch the third pawn,
        which has parts too): a hit of the attacker's log is the victim's when its inbox has a row the frame after"""
        steps = super().shot(name, **kw)
        collect_at = steps[-1][0]

        def only_victim():
            s = self.shots[name]
            frames = {r[0] for r in g._inbox_log(0, self.vkey)}
            s['hits'] = [r for r in s['hits'] if r[0] + 1 in frames]
            return 'victim hits', name, len(s['hits'])
        steps.append((collect_at + 0.02, only_victim))
        return steps

    def kits_ok(self):
        return self.run.at('the kits were accepted', lambda: (self.kit == 'OK' and self.kit2 == 'OK', f'{self.kit}, {self.kit2}'))


def _states_equal(rig, label, key=None, mask=None, serial=None):
    def f():
        states = parts_states(key or rig.vkey)
        parsed = [parse_state(s) for s in states]
        ok = len(set(states)) == 1 and parsed[0] is not None
        if ok and mask is not None:
            ok = parsed[0][0] == mask
        if ok and serial is not None:
            ok = parsed[0][1] == serial
        return ok, f'{states}'
    return rig.run.at(label, f)


def has_tag(wi, key, name):
    """The pawn's ability system on world wi owns the tag (the sampler's tag lists carry the Test.* tags only)"""
    asc = _actor(wi, key).get_component_by_class(unreal.AbilitySystemComponent)
    return bool(asc.has_matching_gameplay_tag(g._tag(name)))


def last():
    return g.last_checks()


def _hit_on(rig, shot, part):
    """The shot landed one hit, on the hurtbox of `part` (the hurtbox index is the part index in the content)"""
    s = rig.shots[shot]
    return len(s['hits']) == 1 and s['hits'][0][4] == PART_INDEX[part], f"hits {s['hits']}"


def _kinds(wi, key, kind):
    return [e for e in part_events(wi, key) if e[0] == kind]


def _authority_only(key, kind, want):
    """`kind` events about the pawn: exactly `want` [(part, serial)] on the authority (world 0, the host in listen) and none on any client"""
    got = [(e[2], e[3]) for e in _kinds(0, key, kind)]
    clients = {w: _kinds(w, key, kind) for w in _client_worlds()}
    return got == want and all(not v for v in clients.values()), f'{kind}: authority {got} (want {want}), clients {clients}'


# ---------------------------------------------------------------------------------------------------------------------------------------------------
def scenario_part_break(client=1, hits=3, cap=12.0):
    """c1 hits ArmL of c2 until it breaks (cap 12, 5.2 per hit: the third breaks it). Judged on the host/server, the victim's own client and the third
    machine: the state string (mask, serial, quantised damage) is equal on every world after every hit; the mask is 0x0008 and the serial 1 after the
    third; Combat.Event.Part.Broken is published exactly once, by the authority only (the host world in listen), with the serial; Damaged once per hit; the
    Disable policy hides the broken arm (one more hit finds nobody); Combat.Part.Broken is projected on the authority and on the owner; the victim's
    client rolls back at most once per hit."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_break', client, victim, 'ArmL', cap)
    run = rig.run
    seen = {}
    steps = [rig.setup_step()]
    names = [f'h{i + 1}' for i in range(hits)]
    for i, name in enumerate(names + ['after']):
        steps += rig.shot(name)
        t = rig.cursor
        steps.append((t, lambda n=name: ('state', seen.__setitem__(n, parts_states(rig.vkey)))))
        if name != 'after':
            steps.append((t, _states_equal(rig, f'after {name}: the state string is equal on every world', mask=0x8 if i + 1 >= hits else 0)))
            steps.append((t, run.at(f'{name}: one hit, on the arm', lambda n=name: _hit_on(rig, n, 'ArmL'))))
    t = rig.cursor
    arm = lambda n: parse_state(seen[n][0])[2][3]
    steps += [
        (t, _states_equal(rig, 'broken: mask 0x0008, serial 1, equal on every world', mask=0x8, serial=1)),
        (t, run.at('the damage byte of the arm grows with each hit and is 255 once broken', lambda: (
            [arm(n) for n in names[:-1]] == sorted(arm(n) for n in names[:-1]) and 0 < arm(names[0]) < AT_CAP and all(arm(n) < AT_CAP for n in names[:-1]) and arm(names[-1]) == AT_CAP,
            f'{[(n, arm(n)) for n in names]}'))),
        (t, run.at('Part.Broken once for the arm (part 3, serial 1), published by the authority only', lambda: _authority_only(rig.vkey, 'Broken', [(3, 1)]))),
        (t, run.at('Part.Damaged once per damaging hit', lambda: (len(_kinds(0, rig.vkey, 'Damaged')) == hits, f'authority {part_events(0, rig.vkey)}'))),
        (t, run.at('the Disable policy hides the broken arm: the extra hit does no damage and makes no event', lambda: (
            rig.shots['after']['dmg'] == (0, 0) and len(_kinds(0, rig.vkey, 'Damaged')) == hits and parse_state(parts_state(0, rig.vkey))[0] == 0x8,
            f"hits {rig.shots['after']['hits']}, damage {rig.shots['after']['dmg']}, state {parts_state(0, rig.vkey)}, events {part_events(0, rig.vkey)}"))),
        (t, run.at('Combat.Part.Broken is projected on the authority and on the owner after the break', lambda: (
            has_tag(0, rig.vkey, 'Combat.Part.Broken') and has_tag(rig.vi, rig.vkey, 'Combat.Part.Broken'), f'{[has_tag(w, rig.vkey, "Combat.Part.Broken") for w in range(len(g.worlds()))]}'))),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(names))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_part_cap(client=1):
    """The torso takes a hit that pushes the main damage percent past the cap (97 % + 5.2): one KO with KOReason.Health, published by the authority only,
    the KO state on the authority and on the owner, the part state (torso damaged, nothing broken) equal on every world, Part.Damaged once, no Part.Broken."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_cap', client, victim, 'Torso', cap=500.0)
    run = rig.run
    KO = 5

    def prep():
        g.hit_rules().set_editor_property('damage_mode', unreal.GSTN_DamageMode.PERCENT_CAPPED)
        g.set_damage(rig.vkey, 97)

    def ko():
        s = rig.shots['cap']
        clients = [(w, [e for e in g.event_log(rig.vkey, w) if e[1] == 'KO']) for w in _client_worlds()]
        acts = (g.victim_info(0, rig.vkey)[1], g.victim_info(rig.vi, rig.vkey)[1])
        ok = len(s['kos']) == 1 and 'Combat.KOReason.Health' in s['kos'][0][4] and all(not c for _, c in clients) and acts == (KO, KO)
        return ok, f"KO events {s['kos']}, clients {clients}, action states (authority, owner) {acts}, damage {s['dmg']}"

    steps = [rig.setup_step()]
    steps += rig.shot('cap', prep=prep)
    t = rig.cursor
    steps += [
        (t, run.at('one hit, on the torso', lambda: _hit_on(rig, 'cap', 'Torso'))),
        (t, run.at('main percent past the cap: one KO (Health), authority only, KO state on both copies', ko)),
        (t, _states_equal(rig, 'the part state is equal on every world and nothing broke', mask=0)),
        (t, run.at('the torso took damage', lambda: (parse_state(parts_state(0, rig.vkey))[2][0] > 0, parts_state(0, rig.vkey)))),
        (t, run.at('Part.Damaged once, Part.Broken never', lambda: (len(_kinds(0, rig.vkey, 'Damaged')) == 1 and not _kinds(0, rig.vkey, 'Broken'), f'{part_events(0, rig.vkey)}'))),
        (t, run.at('the damage percent is past the cap on both copies', lambda: (g.damage_of(0, rig.vkey) >= 100 and g.damage_of(rig.vi, rig.vkey) >= 100,
                                                                              f'{g.damage_of(0, rig.vkey)} / {g.damage_of(rig.vi, rig.vkey)}'))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps(extra=[lambda: comps_respawn(rig)])
    run.run(steps)


def comps_respawn(rig):
    off = g.lane_offset(g.lane_keys().index(rig.vkey))
    g.comps(g.worlds()[0])[rig.vkey].server_reset_test_state(off)
    g.set_damage(rig.vkey, 0)
    return 'respawned'


def scenario_part_hitgroups(client=1, cap=12.0):
    """A lost arm stops its own hitbox group. Control: c2 (intact) hits c1 with the BakedSocket track (group 1). Then c1 breaks c2's ArmL (3 hits). Then c2
    attacks again from the same spot: the group 1 track finds c1 and does not hit: no hit in the log, no hitstop window of c2's, no damage or state on c1, no
    inbox row; the Root track (group 0) still hits. The broken arm also hides its hurtbox, the other pawn's arm took the control hit and the root hit only."""
    other = 2 if client == 1 else 1
    rig = _PartsRig('part_hitgroups', client, other, 'ArmL', cap, both_kits=True)
    rev = rig.swapped()
    run = rig.run
    st = {}

    def sync(a, b):
        b.cursor = a.cursor

    def mark_window(name):
        def f():
            st[name] = g._hit_window(0, rev.akey)
            return 'window', st[name]
        return f

    steps = [rig.setup_step()]
    sync(rig, rev)
    steps += rev.shot('ctrl', spot=(100.0, 170.0))
    sync(rev, rig)
    for i in range(3):
        steps += rig.shot(f'b{i + 1}')
    sync(rig, rev)
    t = rig.cursor
    steps += [
        (t, _states_equal(rig, 'c2\'s arm is broken (0x0008, serial 1), equal on every world', mask=0x8, serial=1)),
        (t, run.at('c2\'s arm broke by the third hit', lambda: _hit_on(rig, 'b3', 'ArmL'))),
    ]
    steps += rev.shot('blocked', spot=(100.0, 170.0), prep=lambda: (st.__setitem__('c1_state', parts_states(rev.vkey)), mark_window('before')()))
    steps += rev.shot('root', spot=(205.0, 0.0), prep=mark_window('after_blocked'))
    t = rev.cursor
    steps += [
        (t, run.at('control: the group 1 track hit c1 while c2\'s arm was intact', lambda: (
            len(rev.shots['ctrl']['hits']) == 1 and rev.shots['ctrl']['hits'][0][2] == 1, f"hits {rev.shots['ctrl']['hits']}"))),
        (t, run.at('broken arm: the group 1 track hits nothing (no hit row, no inbox row, no damage, no new hitstop window of the attacker)', lambda: (
            not rev.shots['blocked']['hits'] and not rev.shots['blocked']['recv'] and rev.shots['blocked']['dmg'] == (0, 0),
            f"hits {rev.shots['blocked']['hits']}, received {rev.shots['blocked']['recv']}, damage {rev.shots['blocked']['dmg']}"))),
        (t, run.at('no hitstop window opened by the blocked attack', lambda: (st.get('before') == st.get('after_blocked'), f"attacker window before {st.get('before')}, after the blocked attack {st.get('after_blocked')}"))),
        (t, run.at('the Root track (group 0) still hits with the arm gone', lambda: (
            len(rev.shots['root']['hits']) == 1 and rev.shots['root']['hits'][0][2] == 0, f"hits {rev.shots['root']['hits']}"))),
        (t, run.at('the state of c1 did not change during the blocked attack (only the control hit and the root hit touched it)', lambda: (
            len(set(st.get('c1_state', []))) == 1 and parse_state(st['c1_state'][0])[2][3] > 0 and parse_state(parts_state(0, rev.vkey))[2][3] > parse_state(st['c1_state'][0])[2][3],
            f"before the blocked attack {st.get('c1_state')}, at the end {parts_states(rev.vkey)}"))),
        (t, rig.kits_ok()),
    ]
    rig.cursor = rev.cursor
    steps += rig.end_steps()
    run.run(steps)


def scenario_part_rollback(client=1, cap=12.0):
    """As part_break, with forced reconciles around the break hit (the third): the victim's client three times while the hit is in flight, the attacker's
    client once. The reconciles must leave no duplicate event and no duplicate state: Part.Broken once (serial 1), the state string equal on every world
    (0x0008, serial 1), the arm's damage byte 255, Combat.Part.Broken projected on the owner at the end. The forced reconciles must have happened."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_rollback', client, victim, 'ArmL', cap)
    run = rig.run
    st = {}
    steps = [rig.setup_step()]
    steps += rig.shot('h1') + rig.shot('h2')
    t3 = rig.cursor
    steps += rig.shot('h3')
    # the attack of the shot is tapped at t3 + 1.75; the hit lands about a quarter second later on the authority and reaches the victim's client after the link
    steps.append((t3 + 1.8, lambda: ('rollbacks before', st.__setitem__('tot0', g._total_rollbacks()))))
    steps.append((t3 + 3.4, lambda: ('rollbacks after', st.__setitem__('tot1', g._total_rollbacks()))))
    for dt, who in ((1.85, rig.vi), (2.0, rig.client), (2.1, rig.vi), (2.3, rig.vi)):
        steps.append((t3 + dt, lambda w=who: ('forced reconcile', st.setdefault('rb', []).append((w, g.rollbacks(w))), g.force_reconcile(w, 8))))
    steps += rig.shot('after')
    t = rig.cursor
    steps += [
        (t, _states_equal(rig, 'after the forced reconciles: 0x0008, serial 1, equal on every world', mask=0x8, serial=1)),
        (t, run.at('Part.Broken once for the arm, authority only: no duplicate from the re-simulated frames', lambda: _authority_only(rig.vkey, 'Broken', [(3, 1)]))),
        (t, run.at('Part.Damaged once per damaging hit', lambda: (len(_kinds(0, rig.vkey, 'Damaged')) == 3, f'{part_events(0, rig.vkey)}'))),
        (t, run.at('the arm\'s damage byte is 255 on every world', lambda: (all(parse_state(s)[2][3] == AT_CAP for s in parts_states(rig.vkey)), f'{parts_states(rig.vkey)}'))),
        (t, run.at('the forced reconciles happened (more rollbacks than the one the hit costs the victim client)', lambda: (
            st['tot1'] - st['tot0'] >= 2, f"rollbacks of all clients in the window of the break hit: {st['tot1'] - st['tot0']}, forced calls {st.get('rb')}"))),
        (t, run.at('Combat.Part.Broken is projected on the owner at the end', lambda: (has_tag(rig.vi, rig.vkey, 'Combat.Part.Broken'), 'tag'))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_part_host(client=0, cap=12.0):
    """Listen server only. The host attacks c1's arm until it breaks, then c1 attacks the host's arm until it breaks, with c2 watching both. Judged: the state
    string of each victim equal on the host and both clients (0x0008, serial 1); Part.Broken once per victim, published by the host only (the clients publish
    nothing); the host rolls back zero times during the whole run (it is its own authority)."""
    if g.detected_mode() != 'listen':
        g._last_run.update(name='part_host', results=[(None, 'skipped: the host plays only in a listen server', '')], verdict='PASS 0 checks (skipped)')
        return
    rig = _PartsRig('part_host', 0, 1, 'ArmL', cap, both_kits=True)
    rev = rig.swapped()
    run = rig.run
    st = {}
    steps = [rig.setup_step(), (rig.cursor, lambda: ('host rollbacks', st.__setitem__('rb0', g.rollbacks(0))))]
    for i in range(3):
        steps += rig.shot(f'a{i + 1}')
    t = rig.cursor
    steps += [
        (t, _states_equal(rig, 'host attacker: c1\'s arm is broken (0x0008, serial 1), equal on the host and both clients', mask=0x8, serial=1)),
        (t, run.at('host attacker: the third hit landed on the arm', lambda: _hit_on(rig, 'a3', 'ArmL'))),
        (t, run.at('host attacker: Part.Broken once for c1, published by the host only', lambda: _authority_only(rig.vkey, 'Broken', [(3, 1)]))),
    ]
    rev.cursor = rig.cursor
    for i in range(3):
        steps += rev.shot(f'v{i + 1}')
    t = rev.cursor
    steps += [
        (t, _states_equal(rig, 'host victim: the host\'s arm is broken (0x0008, serial 1), equal on the host and both clients', key=rev.vkey, mask=0x8, serial=1)),
        (t, run.at('host victim: the third hit landed on the arm', lambda: _hit_on(rev, 'v3', 'ArmL'))),
        (t, run.at('host victim: Part.Broken once for the host pawn, no client publishes it', lambda: _authority_only(rev.vkey, 'Broken', [(3, 1)]))),
        (t, run.at('the host rolled back zero times', lambda: (g.rollbacks(0) == st['rb0'], f"host rollbacks {st['rb0']} -> {g.rollbacks(0)}"))),
        (t, run.at('c2 sees both states', lambda: (len(set(parts_state(2, rig.vkey) for _ in (0,))) == 1 and parse_state(parts_state(2, rig.vkey))[0] == 0x8
                                                   and parse_state(parts_state(2, rev.vkey))[0] == 0x8, f'{parts_state(2, rig.vkey)} / {parts_state(2, rev.vkey)}'))),
        (t, rig.kits_ok()),
    ]
    rig.cursor = rev.cursor
    steps += rig.end_steps()
    run.run(steps)


def request_restore(vkey, mask, fraction):
    """The healer: the authority (world 0) applies a restore request to the pawn's ability system (UGSTN_PartsLibrary.RequestRestore)"""
    target = _actor(0, vkey)
    return unreal.GSTN_PartsLibrary.request_restore(target, mask, fraction, None, None)


def scenario_part_restore(client=1, cap=12.0, fraction=0.25):
    """P7b. c1 breaks c2's ArmL (3 hits). The authority restores the arm (a quarter of the cap kept). The arm is whole on every world (mask 0, serial 2,
    damage byte ceil(0.25 x 255) = 64), the Disable policy un-hides it, Combat.Part.Broken goes. c1 breaks it again (two more hits take it from 64/255 of
    12 to the cap; a sixth finds nobody): mask 0x0008, serial 3. Part.Broken exactly twice (serials 1 and 3) and Part.Restored exactly once (serial 2), all
    published by the authority only, the host included; the states equal on every world at every check."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_restore', client, victim, 'ArmL', cap)
    run = rig.run
    st = {}
    steps = [rig.setup_step()]
    for i in range(3):
        steps += rig.shot(f'b{i + 1}')
    t = rig.cursor
    steps += [
        (t, _states_equal(rig, 'broken: 0x0008, serial 1', mask=0x8, serial=1)),
        (t - 0.1, lambda: ('host rollbacks', st.__setitem__('host_rb0', g.rollbacks(0) if g.detected_mode() == 'listen' else 0))),
        (t, lambda: ('restore requested', st.__setitem__('applied', request_restore(rig.vkey, 1 << 3, fraction)), st.__setitem__('t_restore', run.now()))),
        # forced reconciles across the restore frame: the effect is on the target's ability system from the request on, the tick that consumes it follows
        (t + 0.05, lambda: ('forced reconcile', g.force_reconcile(rig.vi, 8))),
        (t + 0.2, lambda: ('forced reconcile', g.force_reconcile(rig.vi, 8))),
        (t + 0.4, lambda: ('forced reconcile', g.force_reconcile(rig.client, 8))),
        (t + 1.2, _states_equal(rig, 'restored: mask 0, serial 2, equal on every world', mask=0, serial=2)),
        (t + 1.2, run.at('the restore request was applied and consumed (damage byte of the arm = ceil(fraction x 255))', lambda: (
            st['applied'] and all(parse_state(s)[2][3] == -(-int(fraction * 255 * 1000) // 1000) for s in parts_states(rig.vkey)), f"applied {st['applied']}, {parts_states(rig.vkey)}"))),
        (t + 1.2, run.at('Combat.Part.Broken is gone from the authority and the owner while the arm is whole', lambda: (
            not has_tag(0, rig.vkey, 'Combat.Part.Broken') and not has_tag(rig.vi, rig.vkey, 'Combat.Part.Broken'), 'tags'))),
        (t + 1.2, run.at('Part.Restored once (part 3, serial 2), published by the authority only', lambda: _authority_only(rig.vkey, 'Restored', [(3, 2)]))),
        (t + 1.3, lambda: ('rest of the pause', None)),
    ]
    rig.cursor = t + 1.4
    for name in ('r1', 'r2', 'r3'):
        steps += rig.shot(name)
        steps.append((rig.cursor, _states_equal(rig, f'after {name}: equal on every world', key=rig.vkey)))
    t = rig.cursor
    steps += [
        (t, _states_equal(rig, 'broken again: 0x0008, serial 3, equal on every world', mask=0x8, serial=3)),
        (t, run.at('the healer (the authority) rolled back zero times and the restore stayed exactly once through the forced reconciles', lambda: (
            (g.rollbacks(0) == st['host_rb0'] if g.detected_mode() == 'listen' else True) and len(_kinds(0, rig.vkey, 'Restored')) == 1,
            f"host rollbacks {st['host_rb0']} -> {g.rollbacks(0) if g.detected_mode() == 'listen' else 'n/a'}, Restored {_kinds(0, rig.vkey, 'Restored')}"))),
        (t, run.at('Part.Broken twice (serials 1 and 3), authority only', lambda: _authority_only(rig.vkey, 'Broken', [(3, 1), (3, 3)]))),
        (t, run.at('Part.Restored still exactly once', lambda: _authority_only(rig.vkey, 'Restored', [(3, 2)]))),
        (t, run.at('the restored arm could be hit again (hits r1 and r2 landed, r3 found a hidden arm or landed the break)', lambda: (
            len(rig.shots['r1']['hits']) == 1 and len(rig.shots['r2']['hits']) == 1, f"hits {[rig.shots[n]['hits'] for n in ('r1', 'r2', 'r3')]}"))),
        (t, run.at('Combat.Part.Broken is back after the second break', lambda: (has_tag(0, rig.vkey, 'Combat.Part.Broken') and has_tag(rig.vi, rig.vkey, 'Combat.Part.Broken'), 'tags'))),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(['b1', 'b2', 'b3', 'r1', 'r2', 'r3']))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


# ---------------------------------------------------------------------------------------------------------------------------------------------------
# P8: hit feel through parts. The test move hits with 8 damage (stagger force 8 with StaggerPerDamage 1), a bracket of 500 and an authored hitstop (8 frames
# victim, 6 attacker). ArmR is the armoured plate (stagger threshold 100, MaxReaction PushBack, hardness 3 in the content: a hit of penetration 0 bounces),
# Head the weak point (threshold 5, main transfer 2), Legs break into a 900 cm/s launch. A scenario that wants a part to take the hit lowers its hardness in memory.
STAGGERED_ACTIONS = {2, 3, 4}


def _stagger_tuning(per_damage=1.0, push=540.0, launch=1080.0):
    return g.set_tuning(stagger_per_damage=per_damage, push_back_threshold=push, launch_threshold=launch)


def _tuning_reset():
    return g.set_tuning(stagger_per_damage=1.0, **g.TUNING_DEFAULT)


def _actions(rig, name):
    """(authority, owner) sets of the victim's action states during the shot"""
    v = rig.shots[name]['vinfo']
    return {r[1][1] for r in v}, {r[2][1] for r in v}


def _staggered_on(rig, name):
    a, o = _actions(rig, name)
    return bool(a & STAGGERED_ACTIONS), bool(o & STAGGERED_ACTIONS)


def _windows_after_hit(rig, name):
    """(authority, owner) distinct (hitstop start, hitstop end, hitstun end) windows from the frame of the hit on"""
    s = rig.shots[name]
    fr = s['hits'][0][0] if s['hits'] else 10 ** 9
    return ([w for w in g.windows_seen(s['vinfo'], 1) if w[0] >= fr], [w for w in g.windows_seen(s['vinfo'], 2) if w[0] >= fr])


def _rise(samples):
    return (max(x['loc'][2] for x in samples) - samples[0]['loc'][2]) if samples else 0.0


def scenario_part_ricochet(client=1):
    """P8. c1 hits c2's armoured plate (ArmR, hardness 3) with the light test move (penetration 0): the band below ricochets. Judged on the authority, the
    attacker's client and the victim: one hit flagged as a ricochet, predicted identically on the attacker's client (same row); the attacker recoils (recoil
    start the frame after the hit on both copies, the recoil outlives the 32 frame move), no hitstop window opened on either side, no part damage and no
    damage percent, the part state untouched on every world, no hitstun, the victim's inbox holds the flagged entry; no Part.Damaged or Part.Broken. The
    attacker's client does not roll back during the shot (the recoil is predicted, not corrected)."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_ricochet', client, victim, 'ArmR', cap=500.0, hardness=3)
    run = rig.run
    st = {}

    def mark():
        st['w_att'] = (g._hit_window(0, rig.akey), g._hit_window(rig.client, rig.akey))
        st['w_vic'] = g._hit_window(0, rig.vkey)
        st['rb0'] = g.rollbacks(rig.client)
        st['f0'] = g._latest_frame(0, rig.akey)
        st['part0'] = parts_states(rig.vkey)
        return 'mark', st['f0']

    def release():
        st['rb1'] = g.rollbacks(rig.client)
        st['w_att1'] = (g._hit_window(0, rig.akey), g._hit_window(rig.client, rig.akey))
        st['w_vic1'] = g._hit_window(0, rig.vkey)
        return 'released'

    def recoil():
        rows = [r for r in g._hit_log(0, rig.akey) if r[0] >= st['f0']]
        st['rows'] = rows
        st['owner_rows'] = [r for r in g._hit_log(rig.client, rig.akey) if r[0] >= st['f0']]
        st['recoil'] = (g._recoil_info(0, rig.akey), g._recoil_info(rig.client, rig.akey))
        return 'recoil', st['recoil']

    steps = [rig.setup_step()]
    t0 = rig.cursor
    steps += rig.shot('bounce', hold=mark, release=release)
    t_attack = t0 + 1.75
    steps.append((t_attack + 0.6, recoil))
    t = rig.cursor
    s = lambda: rig.shots['bounce']
    steps += [
        (t, run.at('one hit, on the armoured plate, flagged as a ricochet, no attacker hitstop in the log', lambda: (
            len(s()['hits']) == 1 and s()['hits'][0][4] == PART_INDEX['ArmR'] and s()['hits'][0][5] == 1 and s()['hits'][0][6] == 0, f"hits {s()['hits']}"))),
        (t, run.at('predicted on the attacker\'s client: the same row as the authority\'s', lambda: (
            len(st['rows']) == 1 and st['owner_rows'] == st['rows'], f"authority {st['rows']}, owner {st['owner_rows']}"))),
        (t, run.at('the attacker recoils: recoil start is the frame after the hit, equal on the authority and the owner', lambda: (
            len(st['rows']) == 1 and st['recoil'][0][4] == st['rows'][0][0] + 1 and st['recoil'][1][4] == st['recoil'][0][4], f"rows {st['rows']}, recoil {st['recoil']}"))),
        (t, g._in_timeline_seen(run, 'the recoil outlives the 32 frame move (still in the timeline)', t_attack + 0.64, t_attack + 0.72)),
        (t, run.at('no hitstop window on either side', lambda: (
            st['w_att1'] == st['w_att'] and st['w_vic1'] == st['w_vic'], f"attacker (authority, owner) {st['w_att']} -> {st['w_att1']}, victim {st['w_vic']} -> {st['w_vic1']}"))),
        (t, run.at('no damage percent and no hitstun on the victim', lambda: (
            s()['dmg'] == (0, 0) and not any(_staggered_on(rig, 'bounce')) and not s()['recv'], f"damage {s()['dmg']}, actions {_actions(rig, 'bounce')}, received {s()['recv']}"))),
        (t, run.at('the part state is untouched on every world (no fragment written, or all zero)', lambda: (all(x == 'none' or (parse_state(x)[0] == 0 and not any(parse_state(x)[2])) for x in parts_states(rig.vkey)), f'{parts_states(rig.vkey)}'))),
        (t, run.at('the victim\'s inbox holds the flagged entry (cue and event only)', lambda: (
            len([e for e in g._inbox_log(0, rig.vkey) if e[5] == 1 and e[3] == PART_INDEX['ArmR']]) == 1, f'{g._inbox_log(0, rig.vkey)}'))),
        (t, run.at('no Part.Damaged, no Part.Broken', lambda: (not _kinds(0, rig.vkey, 'Damaged') and not _kinds(0, rig.vkey, 'Broken'), f'{part_events(0, rig.vkey)}'))),
        (t, run.at('the attacker\'s client did not roll back during the shot (the recoil is predicted)', lambda: (
            st['rb1'] - st['rb0'] == 0, f"rollbacks of the attacker's client {st['rb0']} -> {st['rb1']}"))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_part_stagger(client=1):
    """P8, the stagger test per part (force against the part's threshold; hitstun length comes from the knockback only). Light blow (force 8) on the armoured
    plate (threshold 100): damage and a flinch, no hitstun. Heavy blow (StaggerPerDamage 20, force 160) on the plate: hitstun, and MaxReaction PushBack caps
    the tier (the bracket 500 would launch with the thresholds used here): a push back on the ground, no launch. Light blow on the weak point (threshold 5):
    hitstun. Judged on the authority and the owner: the same action states and windows, at most one rollback per hit on the victim's client."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_stagger', client, victim, 'ArmR', cap=500.0, hardness=0)
    run = rig.run
    S = rig.shots

    def prep(target, per_damage):
        return lambda: (parts_setup(target, 500.0, 0), _stagger_tuning(per_damage, push=300.0, launch=400.0))

    steps = [rig.setup_step()]
    steps += rig.shot('light_armour', prep=prep('ArmR', 1.0))
    steps += rig.shot('heavy_armour', prep=prep('ArmR', 20.0))
    steps += rig.shot('light_weak', prep=prep('Head', 1.0))
    t = rig.cursor

    def one(name, part):
        return len(S[name]['hits']) == 1 and S[name]['hits'][0][4] == PART_INDEX[part] and not S[name]['hits'][0][5], f"{name}: hits {S[name]['hits']}"

    def flinch():
        ok, d = one('light_armour', 'ArmR')
        stag = _staggered_on(rig, 'light_armour')
        return ok and not any(stag) and S['light_armour']['dmg'][0] > 0 and S['light_armour']['dmg'][0] == S['light_armour']['dmg'][1] and rig.displacement('light_armour') < 5.0, \
            f"{d}; staggered (authority, owner) {stag}, damage {S['light_armour']['dmg']}, moved {rig.displacement('light_armour'):.1f} cm"

    def pushed():
        ok, d = one('heavy_armour', 'ArmR')
        stag = _staggered_on(rig, 'heavy_armour')
        tags = rig.hit_tags('heavy_armour')
        modes = (rig.modes('heavy_armour'), rig.modes('heavy_armour', 'cli'))
        ok = ok and all(stag) and 'Combat.Event.Hit.Staggered' in tags and 'Combat.Event.Hit.Reaction.PushBack' in tags and 'Combat.Event.Hit.Reaction.Launch' not in tags
        ok = ok and rig.displacement('heavy_armour') > 20.0 and _rise(S['heavy_armour']['srv']) < 5.0 and _rise(S['heavy_armour']['cli']) < 5.0
        return ok, f"{d}; staggered {stag}, tags {sorted(t for t in tags if 'Reaction' in t or 'Stag' in t)}, moved {rig.displacement('heavy_armour'):.1f} cm, rise {_rise(S['heavy_armour']['srv']):.1f} / {_rise(S['heavy_armour']['cli']):.1f}, modes {modes}"

    def weak():
        ok, d = one('light_weak', 'Head')
        stag = _staggered_on(rig, 'light_weak')
        return ok and all(stag) and 'Combat.Event.Hit.Staggered' in rig.hit_tags('light_weak'), f"{d}; staggered {stag}, tags {sorted(rig.hit_tags('light_weak'))}"

    def equal(name):
        def f():
            a, o = _windows_after_hit(rig, name)
            return bool(a) and [w[:2] for w in o][:1] == [w[:2] for w in a][:1] and _actions(rig, name)[0] == _actions(rig, name)[1], f'authority {a}, owner {o}, actions {_actions(rig, name)}'
        return f

    steps += [
        (t, run.at('light blow on the armour: damage, a flinch, no hitstun, no displacement', flinch)),
        (t, run.at('heavy blow on the armour: hitstun and a grounded push back (MaxReaction caps the launch)', pushed)),
        (t, run.at('light blow on the weak point: hitstun', weak)),
        (t, run.at('the windows and action states are equal on the authority and the owner, in all three', lambda: (
            all(equal(n)()[0] for n in ('light_armour', 'heavy_armour', 'light_weak')), '; '.join(equal(n)()[1] for n in ('light_armour', 'heavy_armour', 'light_weak'))))),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(['light_armour', 'heavy_armour', 'light_weak']))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps(extra=[_tuning_reset])
    run.run(steps)


def scenario_part_weakpoint(client=1):
    """P8. The same light blow on the torso and on the head (weak point, main transfer 2): both take the equal band (hardness 0 against penetration 0), so the
    head takes twice the torso's main damage. Judged on the authority and the owner: damage percent per blow, Part.WeakPointHit once (head), published by the
    authority only, none for the torso."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_weakpoint', client, victim, 'Torso', cap=500.0, hardness=0)
    run = rig.run
    S = rig.shots
    steps = [rig.setup_step()]
    steps += rig.shot('torso', prep=lambda: parts_setup('Torso', 500.0, 0))
    steps += rig.shot('head', prep=lambda: parts_setup('Head', 500.0, 0))
    t = rig.cursor

    def damage():
        a, b = S['torso']['dmg'], S['head']['dmg']
        ok = a[0] > 0 and abs(b[0] - 2.0 * a[0]) < 0.2 and abs(b[1] - 2.0 * a[1]) < 0.2 and abs(a[0] - a[1]) < 0.01 and abs(b[0] - b[1]) < 0.01
        return ok, f'damage percent per blow: torso {a}, head {b} (authority, owner)'

    steps += [
        (t, run.at('one hit each, on the torso and on the head', lambda: (
            len(S['torso']['hits']) == 1 and S['torso']['hits'][0][4] == PART_INDEX['Torso'] and len(S['head']['hits']) == 1 and S['head']['hits'][0][4] == PART_INDEX['Head'],
            f"torso {S['torso']['hits']}, head {S['head']['hits']}"))),
        (t, run.at('the head takes twice the torso\'s main damage, equal on the authority and the owner', damage)),
        (t, run.at('Part.WeakPointHit once (the head, serial 0), authority only', lambda: _authority_only(rig.vkey, 'WeakPointHit', [(PART_INDEX['Head'], 0)]))),
        (t, run.at('Part.Damaged for each blow', lambda: (len(_kinds(0, rig.vkey, 'Damaged')) == 2, f'{part_events(0, rig.vkey)}'))),
        (t, _states_equal(rig, 'the part state is equal on every world and nothing broke', mask=0)),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(['torso', 'head']))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_part_break_launch(client=1, cap=12.0):
    """P8. The light blows do not stagger (the hurtbox threshold is up). c1 breaks c2's Legs (cap 12: the third blow), whose break throws the victim 900 cm/s,
    60 % of it up. Judged on the authority and the owner: blows one and two flinch only; the breaking blow staggers and launches (the pawn rises, one hitstun
    window, equal on both); a blow on the stump launches nothing; Part.Broken once; every world agrees on where the victim ends up."""
    victim = 2 if client == 1 else 1
    rig = _PartsRig('part_break_launch', client, victim, 'Legs', cap, hardness=0)
    run = rig.run
    S = rig.shots
    steps = [rig.setup_step()]
    steps += rig.shot('h1', prep=lambda: g.set_hurtbox(stagger=1000.0, infinite=False))
    steps += rig.shot('h2') + rig.shot('h3') + rig.shot('h4')
    t = rig.cursor

    def calm(name):
        def f():
            stag = _staggered_on(rig, name)
            return not any(stag) and rig.displacement(name) < 5.0 and len(S[name]['hits']) == 1, f"{name}: staggered {stag}, moved {rig.displacement(name):.1f}, hits {S[name]['hits']}"
        return f

    def launched():
        stag = _staggered_on(rig, 'h3')
        wins = _windows_after_hit(rig, 'h3')
        rises = (_rise(S['h3']['srv']), _rise(S['h3']['cli']))
        speed = (max((x['vel'][2] for x in S['h3']['srv']), default=0.0), max((x['vel'][2] for x in S['h3']['cli']), default=0.0))
        ok = all(stag) and len(S['h3']['hits']) == 1 and len(wins[0]) == 1 and len(wins[1]) == 1 and wins[0][0][:2] == wins[1][0][:2]
        ok = ok and min(rises) > 30.0 and min(speed) > 300.0
        return ok, f"staggered {stag}, windows {wins}, rise (authority, owner) {rises}, peak vertical speed {speed}, hits {S['h3']['hits']}"

    def same_place():
        spots = [tuple(g.snap(g.worlds()[w])[rig.vkey]['location']) for w in range(len(g.worlds()))]
        spread = max(math.dist(a, b) for a in spots for b in spots)
        return spread < 15.0, f'victim position on every world {[tuple(round(x) for x in p) for p in spots]}, spread {spread:.1f} cm'

    steps += [
        (t, run.at('blow one: a flinch only', calm('h1'))),
        (t, run.at('blow two: a flinch only', calm('h2'))),
        (t, run.at('blow three breaks the legs and launches the victim, equal on the authority and the owner', launched)),
        (t, run.at('blow four hits the stump and launches nothing', lambda: (not any(_staggered_on(rig, 'h4')) and len(S['h4']['hits']) == 1, f"staggered {_staggered_on(rig, 'h4')}, hits {S['h4']['hits']}"))),
        (t, run.at('Part.Broken once for the legs (part 2, serial 1), authority only', lambda: _authority_only(rig.vkey, 'Broken', [(PART_INDEX['Legs'], 1)]))),
        (t, _states_equal(rig, 'the legs are broken (0x0004, serial 1), equal on every world', mask=0x4, serial=1)),
        (t, run.at('the victim ends up in the same place on every world', same_place)),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(['h1', 'h2', 'h3', 'h4']))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


# ---------------------------------------------------------------------------------------------------------------------------------------------------
# The batch driver: a fresh PIE per scenario (end play, 15 s, begin play with the current play settings, wait for the pawns, network emulation, scenario).
_batch = {'handle': None, 'results': [], 'status': 'idle'}


def parts_batch(names=None, lag=60, jitter=15, loss=2, settle=3.0, ready_timeout=90.0, timeout=120.0):
    """Runs the scenarios one by one, each in a fresh PIE. Non-blocking: poll parts_batch_report(). The play settings are the ones the editor has now (start
    PIE once with them, e.g. through Rider ue_play); the emulation is lag/jitter/loss of net()"""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if _batch['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_batch['handle'])
    queue = list(names or SUITE_PARTS)
    st = {'phase': 'end', 't': time.time(), 'ending': False, 'cur': None, 'prev': None}
    _batch.update(results=[], status=f'{len(queue)} queued')
    expected = len(g.worlds()) or 3
    pawns = expected if g.detected_mode() == 'listen' else expected - 1    # a dedicated server has no pawn of its own

    def ready():
        ws = g.worlds()
        if len(ws) < expected:
            return False
        for w in ws[0 if pawns == expected else 1:]:
            c = g.comps(w)
            if len(c) < pawns or not any(json.loads(x.get_state_snapshot()).get('inputReady') for x in c.values()):
                return False
        return True

    def done():
        unreal.unregister_slate_post_tick_callback(_batch['handle'])
        _batch['handle'] = None
        _batch['status'] = 'done'

    def tick(_dt):
        try:
            elapsed = time.time() - st['t']
            if st['phase'] == 'end':
                if les.is_in_play_in_editor():
                    if not st['ending']:
                        les.editor_request_end_play()
                        st['ending'] = True
                elif elapsed < 15.0 and st['ending'] is not None:
                    return
                else:
                    if not queue:
                        return done()
                    st.update(phase='wait_ready', t=time.time(), ending=False, cur=queue.pop(0))
                    _batch['status'] = f"{st['cur']}: starting PIE"
                    les.editor_request_begin_play()
            elif st['phase'] == 'wait_ready':
                if les.is_in_play_in_editor() and ready():
                    st.update(phase='settle', t=time.time())
                elif elapsed > ready_timeout:
                    _batch['results'].append((st['cur'], 'ERROR pawns not ready', []))
                    st.update(phase='end', t=time.time(), ending=False)
            elif st['phase'] == 'settle' and elapsed >= settle:
                net(lag, jitter, loss) if False else g.net(lag, jitter, loss)
                g.reset_all()
                st['prev'] = g._last_run.get('results')
                _batch['status'] = f"{st['cur']}: running"
                globals()['scenario_' + st['cur']]()
                st.update(phase='run', t=time.time())
            elif st['phase'] == 'run':
                if g._last_run.get('results') is not st['prev'] and g._last_run.get('results') is not None:
                    res = g._last_run
                    bad = [f'{l} -> {d[:240]}' for ok, l, d in res['results'] if ok is False]
                    _batch['results'].append((st['cur'], res['verdict'], bad))
                    st.update(phase='end', t=time.time(), ending=False)
                elif elapsed > timeout:
                    _batch['results'].append((st['cur'], 'TIMEOUT', []))
                    st.update(phase='end', t=time.time(), ending=False)
        except Exception as e:
            _batch['results'].append((st.get('cur'), f'EXCEPTION {e!r}', []))
            st.update(phase='end', t=time.time(), ending=False)

    _batch['handle'] = unreal.register_slate_post_tick_callback(tick)
    return _batch['status']


def parts_batch_report():
    lines = [f'{n}: {v}' + (f' FAILED {b}' if b else '') for n, v, b in _batch['results']]
    return f"{_batch['status']}\n" + '\n'.join(lines)

