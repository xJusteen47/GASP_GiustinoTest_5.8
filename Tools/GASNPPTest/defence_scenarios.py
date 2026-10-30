# M6 F7: the defence scenarios (SUITE_DEFENCE, not in SUITE_DEFAULT). Shield block, bubble reach, out of shield, break, mode off, grab (stub until
# F12), perfect shield parry and move parry. Every scenario starts in a FRESH PIE (the loadouts stay for the PIE session): defence_batch() restarts
# PIE between them, as parts_batch does.
#
#   import defence_scenarios as ds
#   ds.prepare_editor()                      # once, in the editor world BEFORE the first PIE: adds Input.Button.Shield to IC_GASNPP_Test in memory
#   ds.scenario_shield_block()               # in a running PIE (listen, 3 players, one process)
#   ds.last()                                # (verdict, checks) of the last one
#   ds.defence_batch(ds.SUITE_DEFENCE, loss=2, report='f7-sweep/defence-listen-2pct.log')   # restarts PIE before each, poll ds.defence_batch_report()
#
# The test content is the plugin's (Tools/GAS/setup_defence_link_content.py: /GSTNDefence/Test/DA_DefenceRules_Test and _TestPvE); everything else is
# adjusted IN MEMORY by defence_setup() (never saved) and put back by defence_reset():
#   - IC_GASNPP_Test gets the button Input.Button.Shield appended (prepare_editor, before PIE: the button map is built when a pawn initialises; the
#     other buttons keep their bits);
#   - KIT_Test_Hits gets the FGSTN_DefenceKitExtension (the rules, UGSTN_GA_Shield as its shield) and, for the counter, Command.Special.Neutral bound to
#     UGSTN_GA_CounterSample; the loadout contribution then grants the shield binding and the attribute set, and the attach runs on every machine;
#   - the combat rules get the Parry and Shield steps (UGSTN_DefenceContentLibrary.add_defence_steps_to_rules);
#   - a scenario may shorten numbers of the rules asset (the shield's maximum for the break; the perfect shield window and the bubble's release frames
#     for the parry, so a scripted release lands the hit inside them through 60 ms of lag).
# No shared Blueprint is edited. Every scenario asserts zero rollbacks on the host's own pawn (listen: it has no prediction and no wire to itself).
import json
import os
import time

import unreal
import gasnpp_test as g

DEF_DIR = '/GSTNDefence/Test/'
RULES_PVP = DEF_DIR + 'DA_DefenceRules_Test'
RULES_PVE = DEF_DIR + 'DA_DefenceRules_TestPvE'
KIT_PATH = '/Game/GASNPPTest/Combat/' + g.HIT_KIT
SHIELD_BUTTON = 'Input.Button.Shield'
BUBBLE = 0x80
SUITE_DEFENCE = ['shield_block', 'shield_bubble', 'shield_oos', 'shield_break', 'shield_mode_off', 'grab_shield', 'parry', 'parry_move',
                 'landing_bounce', 'landing_getup', 'tech_ground', 'tech_wall', 'sdi']
NEVER = 2147483647   # KnockdownEndFrame of a downed state with no automatic get-up (MaxDownedFrames 0)
_saved = {}
_batch = {'handle': None, 'results': [], 'status': 'idle', 'report': None}
Lib = unreal.GSTN_DefenceContentLibrary


# ---- content, in memory ------------------------------------------------------------------------------------------------------------------------
def prepare_editor():
    """Appends Input.Button.Shield to the test input config (in memory). Run in the editor world before PIE starts"""
    cfg = unreal.load_asset(g.INPUT_CONFIG)
    buttons = list(cfg.get_editor_property('buttons'))
    names = [g._tag_text(b.get_editor_property('tag')) for b in buttons]
    if SHIELD_BUTTON in names:
        return 'shield button present', names.index(SHIELD_BUTTON)
    _saved.setdefault('buttons', list(buttons))
    button = type(buttons[0])()
    button.set_editor_property('tag', g._tag(SHIELD_BUTTON))
    buttons.append(button)
    cfg.set_editor_property('buttons', buttons)
    return 'shield button added', len(buttons) - 1


def restore_editor():
    """Removes Input.Button.Shield from the input config again (in memory; with PIE stopped), so other suites run on the committed buttons"""
    _saved.pop('buttons', None)
    cfg = unreal.load_asset(g.INPUT_CONFIG)
    buttons = [b for b in cfg.get_editor_property('buttons') if g._tag_text(b.get_editor_property('tag')) != SHIELD_BUTTON]
    cfg.set_editor_property('buttons', buttons)
    return 'buttons', len(buttons)


def _rules(path):
    return unreal.load_asset(path)


def _set_struct(asset, prop, **values):
    s = asset.get_editor_property(prop)
    old = {k: s.get_editor_property(k) for k in values}
    for k, v in values.items():
        s.set_editor_property(k, v)
    asset.set_editor_property(prop, s)
    return old


def defence_setup(rules_path=RULES_PVP, shield=True, counter=False, shield_values=None, parry_values=None, landing=False, tech_set=False,
                  tech_values=None, sdi_values=None, ground_values=None):
    """The in-memory content of a scenario (see the header). F8: landing / tech_set give the kit the plugin's native landing and tech abilities
    (UGSTN_DefenceContentLibrary.set_kit_defence_moves); tech_values / sdi_values / ground_values shorten numbers of the rules' tech, SDI and ground
    bounce structs. Returns what it set"""
    kit = unreal.load_asset(KIT_PATH)
    rules = _rules(rules_path)
    if 'kit_bindings' not in _saved:
        _saved['kit_bindings'] = list(kit.get_editor_property('kit_bindings'))
    if shield_values:
        _saved.setdefault('shield', (rules_path, _set_struct(rules, 'shield', **shield_values)))
    if parry_values:
        _saved.setdefault('parry', (rules_path, _set_struct(rules, 'parry', **parry_values)))
    for prop, values in (('tech', tech_values), ('sdi', sdi_values), ('ground_bounce', ground_values)):
        if values:
            _saved.setdefault(prop, (rules_path, _set_struct(rules, prop, **values)))
    Lib.set_kit_defence_extension(kit, rules, unreal.GSTN_GA_Shield.static_class() if shield else None)
    if landing or tech_set:
        Lib.set_kit_defence_moves(kit, landing, tech_set)
    if counter:
        bindings = list(kit.get_editor_property('kit_bindings'))
        for b in bindings:
            if g._tag_text(b.get_editor_property('command')) == 'Command.Special.Neutral':
                b.set_editor_property('ability', unreal.GSTN_GA_CounterSample)
        kit.set_editor_property('kit_bindings', bindings)
    Lib.add_defence_steps_to_rules(g.hit_rules())
    return 'defence', rules_path.split('/')[-1], 'shield' if shield else 'no shield', 'counter' if counter else ''


def defence_reset():
    """Puts the in-memory content back (the kit, the rules values, the combat pipeline)"""
    msgs = []
    kit = unreal.load_asset(KIT_PATH)
    Lib.clear_kit_defence_extension(kit)
    if 'kit_bindings' in _saved:
        kit.set_editor_property('kit_bindings', _saved.pop('kit_bindings'))
        msgs.append('kit')
    for prop in ('shield', 'parry', 'tech', 'sdi', 'ground_bounce'):
        if prop in _saved:
            path, old = _saved.pop(prop)
            _set_struct(_rules(path), prop, **old)
            msgs.append(prop)
    try:
        Lib.remove_defence_steps_from_rules(g.hit_rules())
        msgs.append('steps')
    except Exception:
        pass
    return 'restored ' + ','.join(msgs)


# ---- readers -----------------------------------------------------------------------------------------------------------------------------------
def _actor(wi, key):
    return g.comps(g.worlds()[wi])[key].get_owner()


def state(wi, key):
    return Lib.get_defence_state_string(_actor(wi, key))


def states(key):
    return [state(w, key) for w in range(len(g.worlds()))]


def parse(text):
    """'value=86.25@120 flags=1 press=100 ...' -> dict of ints (value in quarter points); None for 'none' / 'no combat'"""
    if not text.startswith('value='):
        return None
    out = {}
    for part in text.split():
        k, v = part.split('=', 1)
        if k == 'value':
            num, stamp = v.split('@')
            whole, frac = num.split('.')
            out['valueq'], out['stamp'] = int(whole) * 4 + int(frac) // 25, int(stamp)
        elif k == 'parry':
            win, prof = v.split(')p')
            a, b = win.strip('[').split(',')
            out['parry'], out['profile'] = (int(a), int(b)), int(prof)
        else:
            out[k] = int(v)
    return out


def events(key, wi=0):
    """[(type, frame, magnitude)] the world's defence event log holds about the pawn"""
    name = _actor(wi, key).get_name()
    out = []
    for row in Lib.get_defence_event_log(_actor(wi, key)).split(';'):
        if row:
            kind, frame, mag, target = row.split(':', 3)
            if target == name:
                out.append((kind, int(frame), int(mag)))
    return out


def _kinds(key, kind, wi=0):
    return [e for e in events(key, wi) if e[0] == kind]


def _authority_only(key, kind, want_count):
    got = _kinds(key, kind, 0)
    clients = {w: _kinds(key, kind, w) for w in range(1, len(g.worlds()))}
    return len(got) == want_count and all(not v for v in clients.values()), f'{kind}: authority {got} (want {want_count}), clients {clients}'


def has_tag(wi, key, name):
    asc = _actor(wi, key).get_component_by_class(unreal.AbilitySystemComponent)
    return bool(asc.has_matching_gameplay_tag(g._tag(name)))


def last():
    return g.last_checks()


def _shield_mask():
    return g.button_mask(SHIELD_BUTTON)


# ---- the rig -------------------------------------------------------------------------------------------------------------------------------------
class _DefenceRig(g._HitRig):
    """_HitRig whose setup step gives both test pawns the defence kit; the host's own rollbacks are counted from the setup to the end"""

    def __init__(self, name, attacker=1, victim=2, **setup):
        self.client, self.vi = attacker, victim
        self.akey, self.vkey = g.key_of_player(attacker), g.key_of_player(victim)
        self.run = g._Run(name, attacker, judge_reconciles=False, extra=[('v_srv', 0, self.vkey), ('v_cli', victim, self.vkey)])
        self.shots, self.vinfo, self.cursor, self.kit, self.kit2 = {}, [], 0.3, None, None
        self.setup, self.host_rb0, self.marks = setup, None, {}
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def setup_step(self):
        def f():
            g.hit_rules_setup()
            g._hit_logging(True)
            what = defence_setup(**self.setup)
            self.kit = g._apply_loadout(self.akey, g.HIT_KIT)
            self.kit2 = g._apply_loadout(self.vkey, g.HIT_KIT)
            for w in range(len(g.worlds())):
                Lib.start_defence_event_log(_actor(w, self.vkey))
            self.host_rb0 = g.rollbacks(0) if g.detected_mode() == 'listen' else None
            return what, 'kits', self.kit, self.kit2
        t = self.cursor
        self.cursor += 1.0
        return (t, f)

    def end_steps(self, extra=None):
        def stop_logs():
            for w in range(len(g.worlds())):
                Lib.stop_defence_event_log(_actor(w, self.vkey))
            return 'logs stopped'
        t = self.cursor + 0.1
        steps = [(t, self.run.at('zero rollbacks of the host\'s own pawn (listen: no prediction, no wire to itself)', self._host_clean))]
        return steps + super().end_steps(extra=[stop_logs, defence_reset] + list(extra or ()))

    def _host_clean(self):
        if self.host_rb0 is None:
            return True, 'dedicated server: no host pawn'
        gained = g.rollbacks(0) - self.host_rb0
        return gained == 0, f'host rollbacks +{gained}'

    def kits_ok(self):
        return self.run.at('the kits were accepted', lambda: (self.kit == 'OK' and self.kit2 == 'OK', f'{self.kit}, {self.kit2}'))

    def shot(self, name, **kw):
        """_HitRig.shot, with the hits cut down to the ones on THIS victim (the spin of the test move also reaches the third pawn)"""
        steps = super().shot(name, **kw)
        collect_at = steps[-1][0]

        def only_victim():
            # (drain frame, hurtbox) of the victim's inbox: a hit of the attacker's log is the victim's when it arrived the frame after on that hurtbox
            seen = {(r[0], r[3]) for r in g._inbox_log(0, self.vkey)}
            s = self.shots[name]
            s['hits'] = [r for r in s['hits'] if (r[0] + 1, r[4]) in seen]
            return 'victim hits', name, len(s['hits'])
        steps.append((collect_at + 0.02, only_victim))
        return steps

    def place(self, spot=(205.0, 0.0)):
        def f():
            g._place_near(self.client, self.vkey, spot)
            return 'placed'
        return f

    def mark(self, label):
        def f():
            self.marks[label] = {'states': states(self.vkey), 'astates': states(self.akey), 'rb': g.rollbacks(self.vi), 'arb': g.rollbacks(self.client),
                                 'dmg': (g.damage_of(0, self.vkey), g.damage_of(self.vi, self.vkey))}
            return 'mark', label, self.marks[label]['states'][0]
        return f


def _equal_states(rig, label, key=None, want=None):
    """The defence state string of the pawn is equal on every world; want(parsed) adds a condition on it"""
    def f():
        ss = states(key or rig.vkey)
        p = parse(ss[0])
        ok = len(set(ss)) == 1 and p is not None and (want is None or want(p))
        return ok, f'{ss}'
    return rig.run.at(label, f)


def _hold(client, on):
    def f():
        if on:
            # The scripted hold changes the held mask only; the press the binding anchors on is a tap of the same frame
            g.combat_hold(client, _shield_mask())
            g.combat_tap(client, _shield_mask())
        else:
            g.combat_clear(client)
        return 'shield', 'held' if on else 'released', client
    return f


def _bubble_hits(rig, name):
    s = rig.shots[name]
    return [r for r in s['hits'] if r[4] == BUBBLE]


# ---------------------------------------------------------------------------------------------------------------------------------------------------
def scenario_shield_block(client=1):
    """c2 raises the shield, c1's test move hits the bubble (twice: the Root sweep and the spin). Every hit lands on the bubble (hurtbox 0x80); the victim
    takes no damage; Combat.Event.Blocked once per hit, by the authority only; after the release the defence state (value lower than the maximum, the
    shield stun stamp, the release) is identical on the host and both clients; Combat.State.Shielding is projected on the authority and the owner while
    held; the victim's client rolls back at most once per hit; zero rollbacks on the host."""
    rig = _DefenceRig('shield_block', client, 2 if client == 1 else 1)
    run = rig.run
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += rig.shot('block', wait=1.6)
    steps.insert(-3, (t + 1.2, _hold(rig.vi, True)))
    steps.append((t + 1.6, lambda: ('shielding', rig.marks.__setitem__('tags', (has_tag(0, rig.vkey, 'Combat.State.Shielding'), has_tag(rig.vi, rig.vkey, 'Combat.State.Shielding'))))))
    t = rig.cursor
    steps += [(t, _hold(rig.vi, False)), (t + 0.8, rig.mark('after'))]
    rig.cursor = t + 0.9
    t = rig.cursor
    steps += [
        (t, run.at('every hit landed on the bubble', lambda: (len(rig.shots['block']['hits']) >= 1 and len(_bubble_hits(rig, 'block')) == len(rig.shots['block']['hits']), f"hits {rig.shots['block']['hits']}"))),
        (t, run.at('no damage on the victim (authority and owner)', lambda: (rig.shots['block']['dmg'] == (0, 0), f"damage {rig.shots['block']['dmg']}"))),
        (t, run.at('Combat.Event.Blocked once per hit, authority only', lambda: _authority_only(rig.vkey, 'Blocked', len(rig.shots['block']['hits'])))),
        (t, _equal_states(rig, 'blocked hit identical on the host and both clients (value, stun, release)',
                          want=lambda p: 0 < p['valueq'] < 400 and p['stun'] > 0 and p['release'] > p['press'] > 0)),
        (t, run.at('Combat.State.Shielding while held, on the authority and the owner', lambda: (rig.marks.get('tags') == (True, True), f"{rig.marks.get('tags')}"))),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: (rig.shots['block']['rb'] <= max(1, len(rig.shots['block']['hits'])), f"rollbacks {rig.shots['block']['rb']} for {len(rig.shots['block']['hits'])} hits"))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_shield_bubble(client=1):
    """The victim stands where the test move's Root sweep misses its body (280 cm ahead: the sphere reaches 220, the torso starts at 245) but reaches the
    bubble (95 cm radius: 185). Without the shield: no hit at all. With the shield: hits, all on the bubble, blocked, no damage, Blocked events."""
    rig = _DefenceRig('shield_bubble', client, 2 if client == 1 else 1)
    run = rig.run
    steps = [rig.setup_step()]
    steps += rig.shot('bare', spot=(280.0, 0.0), wait=1.6)
    t = rig.cursor
    steps += rig.shot('shield', spot=(280.0, 0.0), wait=1.6)
    steps.insert(-3, (t + 1.2, _hold(rig.vi, True)))
    t = rig.cursor
    steps += [(t, _hold(rig.vi, False))]
    rig.cursor = t + 0.8
    t = rig.cursor
    steps += [
        (t, run.at('without the shield the move misses the body', lambda: (rig.shots['bare']['hits'] == [], f"hits {rig.shots['bare']['hits']}"))),
        (t, run.at('with the shield it hits the bubble only', lambda: (len(rig.shots['shield']['hits']) >= 1 and len(_bubble_hits(rig, 'shield')) == len(rig.shots['shield']['hits']), f"hits {rig.shots['shield']['hits']}"))),
        (t, run.at('blocked: no damage', lambda: (rig.shots['shield']['dmg'] == (0, 0), f"damage {rig.shots['shield']['dmg']}"))),
        (t, run.at('Combat.Event.Blocked per hit, authority only', lambda: _authority_only(rig.vkey, 'Blocked', len(rig.shots['shield']['hits'])))),
        (t, _equal_states(rig, 'the defence state is equal on every world', want=lambda p: p['valueq'] < 400)),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_shield_oos(client=1):
    """Out of shield: c2 holds the shield, then taps Jump. The shield timeline's cancel window lets the jump start at once: the release is stamped on
    every world (equal state), the pawn leaves the ground (Falling seen on the authority), and Combat.State.Shielding is gone afterwards."""
    victim = 2 if client == 1 else 1
    rig = _DefenceRig('shield_oos', client, victim)
    run = rig.run
    seen = {}
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += [(t, rig.place())]
    t += 1.0
    steps += [
        (t, _hold(victim, True)),
        (t + 0.5, lambda: ('held', seen.__setitem__('held', (parse(state(0, rig.vkey)) or {}).get('flags'), ))),
        (t + 0.55, lambda: ('jump', g.combat_tap(victim, g.button_mask('Jump')))),
        (t + 0.6, lambda: ('mark', seen.__setitem__('t0', run.now()))),
        (t + 1.4, lambda: ('modes', seen.__setitem__('modes', {s['mode'] for s in run._win('v_srv', seen['t0'], run.now(), sim=True)}))),
        (t + 1.45, _hold(victim, False)),
        (t + 2.2, lambda: ('after', seen.__setitem__('tag', has_tag(0, rig.vkey, 'Combat.State.Shielding')))),
    ]
    rig.cursor = t + 2.3
    t = rig.cursor
    steps += [
        (t, run.at('the shield was raised before the jump', lambda: (seen.get('held') is not None and seen['held'] & 1 == 1, f"flags {seen.get('held')}"))),
        (t, run.at('the jump left the ground out of shield (Falling on the authority)', lambda: ('Falling' in seen.get('modes', set()), f"modes {seen.get('modes')}"))),
        (t, _equal_states(rig, 'released by the cancel: equal state, release stamped', want=lambda p: p['release'] >= p['press'] > 0 and p['flags'] & 1 == 0)),
        (t, run.at('Combat.State.Shielding is gone', lambda: (seen.get('tag') is False, f"{seen.get('tag')}"))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_shield_break(client=1):
    """A shield of 7 points that barely drains (the rules' maximum and drain, in memory) takes the test move's hit: it breaks. Breaking on every world with the same break end; the
    victim is launched up its local up without tumble and takes no damage; Combat.Event.ShieldBroken once, authority only; no bubble afterwards (a later
    hit lands on the body)."""
    rig = _DefenceRig('shield_break', client, 2 if client == 1 else 1, shield_values={'max_shield': 7.0, 'drain_per_frame': 0.02})
    run = rig.run
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += rig.shot('break', wait=1.6)
    steps.insert(-3, (t + 1.2, _hold(rig.vi, True)))
    # 0.6 s after the attack: inside the 90 frame break stun
    steps.append((t + 2.35, rig.mark('after')))
    t = rig.cursor
    steps += [(t, _hold(rig.vi, False))]
    rig.cursor = t + 0.4
    t = rig.cursor
    steps += [
        (t, run.at('the first hit landed on the bubble', lambda: (bool(rig.shots['break']['hits']) and rig.shots['break']['hits'][0][4] == BUBBLE, f"hits {rig.shots['break']['hits']}"))),
        (t, run.at('broken on every world at the hit: Breaking, value 0, the same break end', lambda: (
            len(set(rig.marks['after']['states'])) == 1 and (lambda p: p is not None and p['flags'] & 2 == 2 and p['valueq'] == 0 and p['break'] == p['release'] + 90)(parse(rig.marks['after']['states'][0])),
            f"{rig.marks['after']['states']}"))),
        (t, _equal_states(rig, 'recovered at the break end on every world: RecoverFractionAfterBreak (0.6) of the maximum, stamped there',
                          want=lambda p: p['flags'] & 2 == 0 and p['break'] == -1 and p['valueq'] == 17 and p['stamp'] == p['release'] + 90)),
        (t, run.at('Combat.Event.ShieldBroken once, authority only', lambda: _authority_only(rig.vkey, 'ShieldBroken', 1))),
        (t, run.at('launched without tumble: Falling seen, no Combat.State.Tumble', lambda: (
            'Falling' in rig.modes('break') and not any('Combat.State.Tumble' in s['tags'] for s in rig.shots['break']['srv']), f"modes {rig.modes('break')}"))),
        (t, run.at('nothing is blocked after the break (a later hit rewound onto the old bubble resolves as a body hit)', lambda: _authority_only(rig.vkey, 'Blocked', 0))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_shield_mode_off(client=1):
    """PvE rules (bShieldEnabled false): nothing of the shield exists. Holding the shield button does nothing (no binding, no fragment, no Shielding
    tag); the test move hits the body with its damage; no Blocked event."""
    rig = _DefenceRig('shield_mode_off', client, 2 if client == 1 else 1, rules_path=RULES_PVE)
    run = rig.run
    seen = {}
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += rig.shot('off', wait=1.6)
    steps.insert(-3, (t + 1.2, _hold(rig.vi, True)))
    steps.append((t + 1.6, lambda: ('tags', seen.__setitem__('tag', has_tag(0, rig.vkey, 'Combat.State.Shielding')))))
    t = rig.cursor
    steps += [(t, _hold(rig.vi, False))]
    rig.cursor = t + 0.5
    t = rig.cursor
    steps += [
        (t, run.at('the move hit the body', lambda: (bool(rig.shots['off']['hits']) and all(r[4] != BUBBLE for r in rig.shots['off']['hits']), f"hits {rig.shots['off']['hits']}"))),
        (t, run.at('it did its damage', lambda: (rig.shots['off']['dmg'][0] > 0, f"damage {rig.shots['off']['dmg']}"))),
        (t, run.at('no shield state on any world', lambda: (all(parse(s) is None or (parse(s)['press'] == -1 and parse(s)['valueq'] == 0) for s in states(rig.vkey)), f'{states(rig.vkey)}'))),
        (t, run.at('no Shielding tag', lambda: (seen.get('tag') is False, f"{seen.get('tag')}"))),
        (t, run.at('no Blocked event', lambda: _authority_only(rig.vkey, 'Blocked', 0))),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def scenario_grab_shield(client=1):
    """STUB until F12 (grabs are not built yet): the grab-like hit types the bubble ignores and the steps skip are checked through the pure library on a
    shielding victim's machine. The full scenario (a grab passing through the bubble to the body, forming the link) replaces it in F12."""
    rig = _DefenceRig('grab_shield', client, 2 if client == 1 else 1)
    run = rig.run
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += [(t, rig.place()), (t + 1.0, _hold(rig.vi, True)), (t + 1.6, _hold(rig.vi, False))]
    rig.cursor = t + 2.0
    t = rig.cursor
    steps += [
        (t, run.at('STUB: Combat.HitType.Grab is grab-like (the bubble ignores it, Parry and Shield skip it)',
                   lambda: (bool(unreal.GSTN_DefenceMath.is_grab_like_hit_type(g._tag('Combat.HitType.Grab'))), 'pure check; full scenario in F12'))),
        (t, run.at('STUB: an ordinary hit type is not', lambda: (not unreal.GSTN_DefenceMath.is_grab_like_hit_type(g._tag('Combat.HitType.Normal')), 'pure check'))),
        (t, _equal_states(rig, 'the shield was raised and released, equal on every world', want=lambda p: p['release'] >= p['press'] > 0)),
        (t, rig.kits_ok()),
    ]
    steps += rig.end_steps()
    run.run(steps)


def _parry_checks(rig, name, source, attacker_key):
    run = rig.run
    t = rig.cursor
    return [
        (t, run.at('the hit was parried: no damage on the victim', lambda: (bool(rig.shots[name]['hits']) and rig.shots[name]['dmg'] == (0, 0), f"hits {rig.shots[name]['hits']}, damage {rig.shots[name]['dmg']}"))),
        (t, run.at(f'Combat.Event.Parried once, source {source}, authority only', lambda: (
            _authority_only(rig.vkey, 'Parried', 1)[0] and _kinds(rig.vkey, 'Parried')[0][2] == source, f"{events(rig.vkey)}"))),
        (t, _equal_states(rig, 'the victim\'s LastParryFrame is equal on every world', want=lambda p: p['last'] > 0)),
        (t, _equal_states(rig, 'the attacker is punished on every world (same punish end)', key=attacker_key, want=lambda p: p['punish'] > 0)),
        (t, run.at('the parry cost the attacker\'s client at most one rollback', lambda: (
            rig.marks['after']['arb'] - rig.marks['before']['arb'] <= 1, f"attacker client rollbacks +{rig.marks['after']['arb'] - rig.marks['before']['arb']}"))),
        (t, run.at('the attacker\'s spin (frames 20..23) never came: its move was cancelled', lambda: (len(rig.shots[name]['hits']) == 1, f"hits {rig.shots[name]['hits']}"))),
        (t, rig.kits_ok()),
    ]


def scenario_parry(client=1):
    """The perfect shield (release-anchored, Ultimate style): c2 holds the shield and releases it just before c1's hit lands on the bubble. The window
    and the bubble's release frames are widened in memory to 30 frames so the scripted timing survives 60 ms of lag. Parried (source Shield = 1), no
    damage, the attacker punished on every world, at most one rollback for the attacker's client, the victim's LastParryFrame equal everywhere."""
    rig = _DefenceRig('parry', client, 2 if client == 1 else 1, shield_values={'release_frames': 30}, parry_values={'window_frames': 30})
    run = rig.run
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += rig.shot('parry', wait=1.6)
    steps.insert(-3, (t + 1.1, _hold(rig.vi, True)))
    steps.insert(-3, (t + 1.65, rig.mark('before')))
    steps.insert(-3, (t + 1.7, _hold(rig.vi, False)))
    t = rig.cursor
    steps += [(t + 0.3, rig.mark('after'))]
    rig.cursor = t + 0.4
    steps += _parry_checks(rig, 'parry', 1, rig.akey)
    steps += rig.end_steps()
    run.run(steps)


def scenario_parry_move(client=1):
    """The move parry (source Move = 2) with the PvE rules: no shield exists, the counter move (UGSTN_GA_CounterSample on Special: a parry window over its
    frames 2..14 and an OnParry cancel into the attacks) still parries. c2 starts the counter just before c1's hit. Same outcome and rollback count as
    the perfect shield."""
    rig = _DefenceRig('parry_move', client, 2 if client == 1 else 1, rules_path=RULES_PVE, counter=True)
    run = rig.run
    steps = [rig.setup_step()]
    t = rig.cursor
    steps += rig.shot('counter', wait=1.6)
    steps.insert(-3, (t + 1.65, rig.mark('before')))
    steps.insert(-3, (t + 1.72, lambda: ('counter', g.combat_tap(rig.vi, g.button_mask('Special')))))
    t = rig.cursor
    steps += [(t + 0.3, rig.mark('after'))]
    rig.cursor = t + 0.4
    steps += _parry_checks(rig, 'counter', 2, rig.akey)
    steps += rig.end_steps()
    run.run(steps)


# ---- batch -------------------------------------------------------------------------------------------------------------------------------------
# ---- M6 F8: landing, tech, SDI ---------------------------------------------------------------------------------------------------------------------
# The test move of KIT_Test_Hits launches the victim; with the thresholds at 0 every launch tumbles. The landing scenarios give the kit the plugin's
# native landing abilities (set_kit_defence_moves), the tech ones its tech set too. Presses of the shield button come from the victim's own client,
# timed from its own copy of the hitstop (a press before the hitstop end never counts). Every check that reads the defence state compares the state
# string of the victim on the host and both clients.
import math


def _tumble_tuning():
    g.set_tuning(push_back_threshold=0.0, launch_threshold=0.0, tumble_threshold=0.0)


def _across(rig, side=1.0):
    """The world direction across the attacker -> victim line (left for side 1) and a stick target 300 cm that way from the victim"""
    v = g.snap(g.worlds()[0])
    a, p = v[rig.akey]['location'], v[rig.vkey]['location']
    n = max(math.hypot(p[0] - a[0], p[1] - a[1]), 1e-3)
    d = ((p[0] - a[0]) / n, (p[1] - a[1]) / n)
    left = (-d[1] * side, d[0] * side)
    return left, (p[0] + 300.0 * left[0], p[1] + 300.0 * left[1])


def _starts(key):
    """(forced, command) timeline starts the authority published for the pawn"""
    c = (g.snap(g.worlds()[0])[key].get('combat') or {}).get('timelineStarted') or {}
    return int(c.get('forced', 0)), int(c.get('command', 0))


def _event_count(rig, kind, since_frame, until_frame=None):
    """(authority events, clients' events) of a defence event about the victim in [since_frame, until_frame)"""
    def inside(e):
        return e[1] >= since_frame and (until_frame is None or e[1] < until_frame)
    auth = [e for e in _kinds(rig.vkey, kind, 0) if inside(e)]
    clients = {w: [e for e in _kinds(rig.vkey, kind, w) if inside(e)] for w in range(1, len(g.worlds()))}
    return auth, clients


def _tagged(samples, tag):
    return [s for s in samples if tag in s['tags']]


def _tap_after_hitstop(rig, st, key, delay_frames=2):
    """A slate callback: once the victim's own copy is past a NEW hitstop end by delay_frames and tumbling, its client taps the shield once"""
    def tick(_dt):
        try:
            if st.get(key) is not None or st.get(key + '_armed') is None:
                return
            info = g.victim_info(rig.vi, rig.vkey)
            frame, action, end = info[0], info[1], info[3]
            if end > st[key + '_armed'] and action == 3 and frame >= end + delay_frames:
                g.combat_tap(rig.vi, _shield_mask())
                st[key] = frame
        except Exception:
            pass
    return tick


def scenario_landing_bounce(client=1):
    """M6 F8, the landing of a kit WITHOUT a tech set (spec 6, owner revision), shot 'steered': the victim holds the stick across the launch from the
    attack on. At the landing the mini bounce goes the stick's way (the first rising tick after the landing moves along the stick), the victim is
    downed (Combat.State.Knockdown on the authority and the owner; Combat.Event.KnockedDown once, authority only), and with the stick still held once the
    bounce is over the get-up roll starts by itself (the authority's Forced starts +2: the bounce and the roll; downed no more). Zero rollbacks of the
    host. The lying and jump get-up half is scenario_landing_getup (its own PIE: the first shot leaves the attacker turned)."""
    _landing_scenario('landing_bounce', client, steered=True)


def scenario_landing_getup(client=1):
    """M6 F8, the downed state of a kit without a tech set, shot 'lying': no input, still downed 1.5 s after the landing (no automatic get-up: down
    = never on every world, Combat.State.Knockdown on the authority and the owner), then a jump press gets up (a command start, downed no more, the
    defence state equal on every world). Zero rollbacks of the host."""
    _landing_scenario('landing_getup', client, steered=False)


def _landing_scenario(name, client, steered):
    rig = _DefenceRig(name, client, 2 if client == 1 else 1, landing=True)
    run, S = rig.run, rig.shots
    st = {}
    steps = [rig.setup_step()]

    def hold_across():
        st['dir'], target = _across(rig)
        st['starts0'] = _starts(rig.vkey)
        g.hold(rig.vi, 'move', g._steer_value(rig.vi, target))

    def let_go():
        g.release(rig.vi, 'move')
        st['starts1'] = _starts(rig.vkey)
    if steered:
        steps += rig.shot('steered', prep=_tumble_tuning, hold=hold_across, release=let_go, wait=4.2)

    def lying_mark():
        st['lying'] = (states(rig.vkey), has_tag(0, rig.vkey, 'Combat.State.Knockdown'), has_tag(rig.vi, rig.vkey, 'Combat.State.Knockdown'), _starts(rig.vkey))
        return 'lying', st['lying'][0][0]

    def jump():
        # The combat button (Input.Button.Jump), not the movement input action: the get-up is a combat command on the kit's jump slot
        g.combat_tap(rig.vi, g.button_mask('Jump'))
        return 'jump', rig.vi

    def up_mark():
        st['up'] = (states(rig.vkey), has_tag(0, rig.vkey, 'Combat.State.Knockdown'), has_tag(rig.vi, rig.vkey, 'Combat.State.Knockdown'), _starts(rig.vkey))
        return 'up', st['up'][0][0]
    if not steered:
        steps += rig.shot('lying', prep=_tumble_tuning, wait=3.6)
        t = rig.cursor
        steps += [(t, lying_mark), (t + 0.1, jump), (t + 1.2, up_mark)]
        rig.cursor = t + 1.3

    def landed(name):
        def f():
            s = S[name]
            srv, cli = _tagged(s['srv'], 'Combat.State.Knockdown'), _tagged(s['cli'], 'Combat.State.Knockdown')
            auth, clients = _event_count(rig, 'KnockedDown', s['F0'])
            ok = bool(srv) and bool(cli) and len(auth) == 1 and not any(clients.values())
            seen = sorted({t for x in s['srv'] for t in x['tags'] if t.startswith('Combat.State.')})
            return ok, (f'downed ticks authority {len(srv)}, owner {len(cli)}; KnockedDown authority {auth}, clients {clients}; hits {len(s["hits"])}, '
                        f'authority states seen {seen}, modes {sorted({x["mode"] for x in s["srv"]})}')
        return f

    def bounce_steered():
        s = S['steered']
        down = sorted(_tagged(s['srv'], 'Combat.State.Knockdown'), key=lambda x: x['fr'])
        if not down or 'dir' not in st:
            return False, 'no landing'
        f0 = down[0]['fr']
        rising = [x for x in sorted(s['srv'], key=lambda x: x['fr']) if x['fr'] >= f0 and x['vel'][2] > 30.0]
        if not rising:
            return False, f'no rising tick after the landing frame {f0}'
        v = rising[0]['vel']
        h = math.hypot(v[0], v[1])
        along = (v[0] * st['dir'][0] + v[1] * st['dir'][1]) / max(h, 1e-3)
        return h > 20.0 and along > 0.7, f'landing frame {f0}, first rising tick {rising[0]["fr"]} velocity {tuple(round(x) for x in v)}, along the stick {along:.2f}'

    def rolled():
        a0, a1 = st.get('starts0', (0, 0)), st.get('starts1', (0, 0))
        last_srv = sorted(S['steered']['srv'], key=lambda x: x['fr'])[-1:]
        last_cli = sorted(S['steered']['cli'], key=lambda x: x['fr'])[-1:]
        still = [bool(x and 'Combat.State.Knockdown' in x[0]['tags']) for x in (last_srv, last_cli)]
        return a1[0] - a0[0] >= 2 and not any(still), f'Forced starts {a0} -> {a1} (bounce + roll), downed at the end (authority, owner) {still}'

    def lying():
        ss, a, o, _ = st.get('lying', ([], False, False, None))
        p = parse(ss[0]) if ss else None
        return bool(ss) and len(set(ss)) == 1 and p is not None and p.get('down') == NEVER and a and o, f'states {ss}, Knockdown tag (authority, owner) ({a}, {o})'

    def got_up():
        ss, a, o, starts = st.get('up', ([], True, True, None))
        p = parse(ss[0]) if ss else None
        cmd = (starts[1] - st['lying'][3][1]) if starts and 'lying' in st else 0
        return (bool(ss) and len(set(ss)) == 1 and p is not None and p.get('down') not in (None, NEVER) and not a and not o and cmd >= 1,
                f'states {ss}, Knockdown tag (authority, owner) ({a}, {o}), command starts +{cmd}')

    t = rig.cursor + 0.1
    if steered:
        steps += [
            (t, run.at('steered shot: landed downed on the authority and the owner, KnockedDown once from the authority', landed('steered'))),
            (t, run.at('the mini bounce follows the stick', bounce_steered)),
            (t, run.at('stick held after the bounce: the get-up roll starts by itself, downed no more', rolled)),
        ]
    else:
        steps += [
            (t, run.at('lying shot: landed downed', landed('lying'))),
            (t, run.at('no input: still downed 1.5 s after the landing, equal on every world (no automatic get-up)', lying)),
            (t, run.at('a jump press gets up (a command), equal on every world', got_up)),
        ]
    steps.append((t, rig.kits_ok()))
    rig.cursor = t
    steps += rig.end_steps()
    run.run(steps)


def _tech_scenario(name, client, wall):
    """The tech scenarios: a kit with a tech set, the window widened in memory to 60 frames so a press at the victim's hitstop end + 2 frames reaches
    a contact up to a second later. Shot 'teched': the press (and a forced reconcile of the victim's client 3 frames after it); shot 'missed': no press"""
    vi = 2 if client == 1 else 1
    rig = _DefenceRig(name, client, vi, landing=True, tech_set=True, tech_values={'window_frames': 60, 'lockout_frames': 120})
    run, S = rig.run, rig.shots
    st = {}
    h = {'tap': unreal.register_slate_post_tick_callback(_tap_after_hitstop(rig, st, 'press'))}

    def prep():
        if wall:
            g.set_tuning(push_back_threshold=50.0, launch_threshold=100.0, tumble_threshold=200.0)
            st['rows'] = unreal.GSTN_ImpactTestLibrary.configure_impact_table(g.IMPACT_ROWS)
            unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(0, 0, False, 1.0)
        else:
            _tumble_tuning()

    def surface():
        if not wall:
            return 'floor'
        ctx = g.comps(g.worlds()[0])[rig.akey]
        unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(ctx)
        a = g.snap(g.worlds()[0])[rig.akey]
        yaw = a['yaw']
        fwd = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
        half = (50.0, 500.0, 500.0)
        c = (a['location'][0] + fwd[0] * (g.IMPACT_FACE + half[0]), a['location'][1] + fwd[1] * (g.IMPACT_FACE + half[0]), a['location'][2] + 200.0)
        unreal.GSTN_ImpactTestLibrary.spawn_test_surface(ctx, unreal.Vector(*c), unreal.Vector(*half), yaw, 'Wall')
        for other in [k for k in g.comps(g.worlds()[0]) if k not in (rig.akey, rig.vkey)]:
            g._place_near(client, other, (-350.0, 350.0))
        g.set_damage(rig.vkey, g.IMPACT_DAMAGE)
        return 'wall'

    def arm(label, press):
        def f():
            surface()
            st['counts0_' + label] = (g._impact_counts(0, rig.vkey), g._impact_counts(vi, rig.vkey))
            st['starts0_' + label] = _starts(rig.vkey)
            st['press'] = None
            st['press_armed'] = g.victim_info(vi, rig.vkey)[3] if press else None
            return 'armed', label, press
        return f

    def reconcile_after_press(_dt):
        try:
            if st.get('press') is not None and st.get('reconciled') is None and g.victim_info(vi, rig.vkey)[0] >= st['press'] + 3:
                st['reconciled'] = g.rollbacks(vi)
                g.force_reconcile(vi, 10)
        except Exception:
            pass
    h['rc'] = unreal.register_slate_post_tick_callback(reconcile_after_press)

    def done(label):
        def f():
            st['press_armed'] = None
            st['counts1_' + label] = (g._impact_counts(0, rig.vkey), g._impact_counts(vi, rig.vkey))
            st['starts1_' + label] = _starts(rig.vkey)
            st['states_' + label] = states(rig.vkey)
            st['press_' + label] = st.get('press')
            return 'done', label, st['states_' + label][0]
        return f
    spot = (g.IMPACT_DISTANCE, 0.0) if wall else (205.0, 0.0)
    steps = [rig.setup_step()]
    steps += rig.shot('teched', prep=prep, spot=spot, hold=arm('teched', True), release=done('teched'), wait=3.0)
    steps += rig.shot('missed', prep=prep, spot=spot, hold=arm('missed', False), release=done('missed'), wait=3.0)

    def teched():
        s = S['teched']
        auth, clients = _event_count(rig, 'Teched', s['F0'], S['missed'].get('F0'))
        ss = st.get('states_teched', [])
        p = parse(ss[0]) if ss else None
        want = 4 if wall else 1   # WallTech / InPlace
        tags = (bool(_tagged(s['srv'], 'Combat.State.Teching')), bool(_tagged(s['cli'], 'Combat.State.Teching')))
        ok = (st.get('press_teched') is not None and len(auth) == 1 and not any(clients.values()) and auth[0][2] == want and p is not None
              and p.get('techtype') == want and len(set(ss)) == 1 and all(tags))
        return ok, f'press frame {st.get("press_teched")}, Teched authority {auth}, clients {clients}; Teching tag (authority, owner) {tags}; states {ss}'

    def cancelled():
        s = S['teched']
        c0, c1 = st.get('counts0_teched'), st.get('counts1_teched')
        down = _event_count(rig, 'KnockedDown', s['F0'], S['missed'].get('F0'))[0]
        kd = (bool(_tagged(s['srv'], 'Combat.State.Knockdown')), bool(_tagged(s['cli'], 'Combat.State.Knockdown')))
        return c0 == c1 and not down and not any(kd), f'impact counts (authority, owner) {c0} -> {c1}; KnockedDown {down}; downed ticks {kd}'

    def missed():
        s = S['missed']
        c0, c1 = st.get('counts0_missed'), st.get('counts1_missed')
        auth = _event_count(rig, 'Teched', s['F0'])[0]
        if wall:
            # The M4 impact is unchanged: the splat of the Wall row on both copies
            splat = c1 is not None and c0 is not None and c1[0][0] - c0[0][0] == 1 and c1[1][0] - c0[1][0] >= 1
            return splat and not auth, f'impact counts (splat, bounce, break) authority/owner {c0} -> {c1}; Teched {auth}'
        down = _event_count(rig, 'KnockedDown', s['F0'])[0]
        return len(down) == 1 and not auth, f'KnockedDown {down}; Teched {auth}; states {st.get("states_missed")}'

    def reconciled():
        return st.get('reconciled') is not None, f"forced reconcile of the victim's client after the press (rollbacks before it {st.get('reconciled')})"

    def stop():
        for k in ('tap', 'rc'):
            if h.get(k) is not None:
                unreal.unregister_slate_post_tick_callback(h[k])
                h[k] = None
        if wall:
            unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(g.comps(g.worlds()[0])[rig.akey])
            unreal.GSTN_ImpactTestLibrary.configure_impact_table('')
        return 'stopped'

    kind = 'wall' if wall else 'ground'
    t = rig.cursor + 0.1
    steps += [
        (t, run.at(kind + ' tech: Teched once from the authority, its kind on every world, Teching on the authority and the owner, through a forced reconcile', teched)),
        (t, run.at('the forced reconcile ran', reconciled)),
        (t, run.at('the tech cancelled the impact: no impact, no knockdown', cancelled)),
        (t, run.at('no press: ' + ('the M4 splat, unchanged' if wall else 'downed after the landing'), missed)),
        (t, rig.kits_ok()),
    ]
    rig.cursor = t
    steps += rig.end_steps(extra=[stop])
    run.run(steps)


def scenario_tech_ground(client=1):
    """M6 F8, the optional tech on the floor: a kit with a tech set; the victim presses the shield after its hitstop (its own client), the window
    widened to 60 frames in memory. Teched (InPlace) once from the authority, equal on every world and through a forced reconcile of the victim's
    client; no knockdown. Without the press: downed after the landing."""
    _tech_scenario('tech_ground', client, wall=False)


def scenario_tech_wall(client=1):
    """M6 F8, the wall tech: the impact_wall layout (a Wall test surface, which the wall mode slides on like any collider, ahead of the attacker; the
    victim at 80 % damage flies into it), a kit with a tech set. With the press: WallTech, and the impact table's splat never happens (impact counts
    unchanged on the authority and the owner). Without it: the M4 splat, unchanged."""
    _tech_scenario('tech_wall', client, wall=True)


def scenario_sdi(client=1):
    """M6 F8, held-direction SDI with the rules asset's values (DA_DefenceRules_Test): a launch, the victim (locked in place, so its stick only feeds the
    combat tick) holding the stick across it from the attack on. During the hitstop the victim drifts that way: the drift used is above 0 and at most
    the rules' maximum, equal on the authority and the owner, and the victim's hitstop ticks move along the stick by about that much. A control shot with
    a neutral stick drifts nothing."""
    rig = _DefenceRig('sdi', client, 2 if client == 1 else 1)
    run, S = rig.run, rig.shots
    st = {}
    steps = [rig.setup_step()]

    def hold_across():
        st['dir'], target = _across(rig)
        g.hold(rig.vi, 'move', g._steer_value(rig.vi, target))

    def read(label):
        def f():
            st[label] = (Lib.get_sdi_distance_used(_actor(0, rig.vkey)), Lib.get_sdi_distance_used(_actor(rig.vi, rig.vkey)))
            g.release(rig.vi, 'move')
            return 'sdi', label, st[label]
        return f

    def prep():
        _tumble_tuning()
        g.comps(g.worlds()[0])[rig.vkey].server_set_movement_lock(True)
    steps += rig.shot('held', prep=prep, hold=hold_across, release=read('held'), wait=1.2)
    steps += rig.shot('neutral', prep=prep, release=read('neutral'), wait=1.2)

    def drift():
        rules = _rules(RULES_PVP).get_editor_property('sdi')
        cap = rules.get_editor_property('max_distance_cm_per_hit')
        used = st.get('held')
        ticks = sorted(_tagged(S['held']['srv'], 'Combat.State.Hitstop'), key=lambda x: x['fr'])
        moved = 0.0
        if len(ticks) >= 2 and 'dir' in st:
            a, b = ticks[0]['loc'], ticks[-1]['loc']
            moved = (b[0] - a[0]) * st['dir'][0] + (b[1] - a[1]) * st['dir'][1]
        ok = used is not None and used[0] > 0.0 and used[0] <= cap + 1e-3 and abs(used[0] - used[1]) < 1e-3 and 0.0 < moved <= cap + 2.0
        return ok, f'drift used (authority, owner) {used}, cap {cap} cm; hitstop ticks moved {moved:.2f} cm along the stick over {len(ticks)} ticks'

    def neutral():
        used = st.get('neutral')
        return used is not None and used[0] == 0.0 and used[1] == 0.0, f'drift used with a neutral stick {used}'

    def unlock():
        g.comps(g.worlds()[0])[rig.vkey].server_set_movement_lock(False)
        return 'unlocked'

    t = rig.cursor + 0.1
    steps += [
        (t, run.at("held stick: the victim drifts along it during the hitstop, at most the rules' maximum, equal on the owner", drift)),
        (t, run.at('neutral stick: no drift', neutral)),
        (t, rig.kits_ok()),
    ]
    rig.cursor = t
    steps += rig.end_steps(extra=[unlock])
    run.run(steps)


def defence_batch(names=None, lag=60, jitter=15, loss=0, settle=3.0, ready_timeout=90.0, timeout=120.0, report=None, module=None):
    """Runs the scenarios one by one, each in a fresh PIE (20 s between stop and play). Non-blocking: poll defence_batch_report(). The play settings
    are the ones the editor has now (listen, 3 players, one process). report = a path under Saved/Logs written at the end (first line: UTC time).
    module = another scenario module (parts_scenarios, gasnpp_test) to run its scenarios with the same driver"""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if _batch['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_batch['handle'])
    queue = list(names or SUITE_DEFENCE)
    scope = module.__dict__ if module else globals()
    st = {'phase': 'end', 't': time.time(), 'ending': False, 'cur': None, 'prev': None}
    _batch.update(results=[], status=f'{len(queue)} queued', report=report, net=(lag, jitter, loss))

    def ready():
        ws = g.worlds()
        if len(ws) < 3:
            return False
        for w in ws:
            c = g.comps(w)
            if len(c) < 3 or not any(json.loads(x.get_state_snapshot()).get('inputReady') for x in c.values()):
                return False
        return True

    def done():
        unreal.unregister_slate_post_tick_callback(_batch['handle'])
        _batch['handle'] = None
        _batch['status'] = 'done'
        if _batch['report']:
            path = os.path.join(unreal.Paths.project_saved_dir(), 'Logs', _batch['report'])
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8') as f:
                f.write(time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()) + f' net{_batch["net"]}\n' + defence_batch_report() + '\n')

    def tick(_dt):
        try:
            elapsed = time.time() - st['t']
            if st['phase'] == 'end':
                if les.is_in_play_in_editor():
                    if not st['ending']:
                        les.editor_request_end_play()
                        st['ending'] = True
                elif elapsed < 20.0 and st['ending'] is not None:
                    return
                else:
                    if not queue:
                        return done()
                    if not module:
                        prepare_editor()
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
                g.net(lag, jitter, loss)
                g.reset_all()
                st['prev'] = g._last_run.get('results')
                _batch['status'] = f"{st['cur']}: running"
                scope['scenario_' + st['cur']]()
                st.update(phase='run', t=time.time())
            elif st['phase'] == 'run':
                if g._last_run.get('results') is not st['prev'] and g._last_run.get('results') is not None:
                    res = g._last_run
                    bad = [f'{l} -> {d[:300]}' for ok, l, d in res['results'] if ok is False]
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


def defence_batch_report():
    lines = [f'{n}: {v}' + (f' FAILED {b}' if b else '') for n, v, b in _batch['results']]
    return f"{_batch['status']}\n" + '\n'.join(lines)
