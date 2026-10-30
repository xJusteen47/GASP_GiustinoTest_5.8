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
SUITE_DEFENCE = ['shield_block', 'shield_bubble', 'shield_oos', 'shield_break', 'shield_mode_off', 'grab_shield', 'parry', 'parry_move']
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


def defence_setup(rules_path=RULES_PVP, shield=True, counter=False, shield_values=None, parry_values=None):
    """The in-memory content of a scenario (see the header). Returns what it set"""
    kit = unreal.load_asset(KIT_PATH)
    rules = _rules(rules_path)
    if 'kit_bindings' not in _saved:
        _saved['kit_bindings'] = list(kit.get_editor_property('kit_bindings'))
    if shield_values:
        _saved.setdefault('shield', (rules_path, _set_struct(rules, 'shield', **shield_values)))
    if parry_values:
        _saved.setdefault('parry', (rules_path, _set_struct(rules, 'parry', **parry_values)))
    Lib.set_kit_defence_extension(kit, rules, unreal.GSTN_GA_Shield.static_class() if shield else None)
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
    for prop in ('shield', 'parry'):
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
