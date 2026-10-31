# M6 F10: the link scenarios (SUITE_LINK, not in SUITE_DEFAULT). The S1 link spike as a permanent scenario (a server-side test link, the
# grabber walking, dashing and circling for 20 s, forced reconciles inside the link) and the formation rollback scenario (a real grab hit through
# the LinkRequest step, forced reconciles on each client over the formation frames), each in three pairings: host grabs client, client grabs host,
# client grabs client. Every scenario starts in a FRESH PIE: run them with defence_scenarios.defence_batch(module=link_scenarios).
# F11 adds the watchdog's scenarios: link_break_distance (a leash and the server moving the grabber away, then a knock-out of either side, in the three
# pairings), grab_immunity (a regrab refused inside the immunity after a break), and link_rollback breaks through the watchdog with forced reconciles
# over the break frames. The test type's MaxDistanceCm and grab immunity are changed IN MEMORY and put back.
#
#   import link_scenarios as ls
#   ls.prepare_editor()                      # once, in the editor world BEFORE the first PIE: the plugin's test link type exists and is registered
#   ls.scenario_link_spike_host_grabs_client()   # in a running PIE (listen, 3 players, one process)
#   ls.last()                                # (verdict, checks)
#   import defence_scenarios as ds; ds.defence_batch(ls.SUITE_LINK, module=ls, report='f10-sweep/link-listen-0pct.log')
#
# Content: the plugin's /GSTNLink/Test/DA_LinkType_Grab_Test (Tools/GAS/setup_defence_link_content.py run_link()); everything else is changed IN
# MEMORY and put back: the combat rules get the GrabImmunity and LinkRequest steps (UGSTN_LinkContentLibrary.add_link_steps_to_rules), and for
# link_rollback the test kit's attack (KIT_Test_Hits' weapon, Command.Attack.Neutral) becomes a grab (hit type Combat.HitType.Grab, link type Combat.Link.Type.Grab)
# until the scenario ends. No shared Blueprint is edited.
#
# S1 thresholds (plan F0 S1, variant P): zero unforced reconciles on the grabber's and the grabbed's clients during the walk; exactly one correction
# at link start and one at link end per client that simulates one of the two; presentation error of the grabbed pawn's shown transform against the
# server's p95 <= 10 cm on the grabber's client, <= 25 cm on the grabbed's, max <= 60 cm; a forced reconcile inside the link converges in <= 2
# frames. The presentation error is measured per editor tick against the server's shown transform of the same pawn, AFTER removing the best
# constant delay of that world (searched -500..500 ms): what a client shows an interpolation or prediction lead later is not an error of the link,
# the shape of the path is (the delay is reported). Bytes per second are not measurable from Python: the link fragment's payload bytes are reported.
import json
import math
import time

import unreal
import gasnpp_test as g

LINK_DIR = '/GSTNLink/Test/'
LINK_TYPE_ASSET = LINK_DIR + 'DA_LinkType_Grab_Test'
GRAB = 'Combat.Link.Type.Grab'
ATTACK_COMMAND = 'Command.Attack.Neutral'   # the test weapon's attack (WPN_Test_Hits), the move link_rollback turns into a grab
LINKED_MODE = 'GSTN_Linked'
PAIRINGS = {'host_grabs_client': (0, 1), 'client_grabs_host': (1, 0), 'client_grabs_client': (1, 2)}
SUITE_LINK = ([f'link_spike_{p}' for p in PAIRINGS] + [f'link_rollback_{p}' for p in PAIRINGS] + [f'link_break_distance_{p}' for p in PAIRINGS]
              + ['grab_immunity'])
REASON = {'Forced': 11, 'Distance': 3, 'OwnDeath': 4, 'PartnerDeath': 5, 'Hit': 6}   # EGSTN_LinkBreakReason indices
S1 = {'own_p95': 10.0, 'partner_p95': 25.0, 'max': 60.0, 'converge_frames': 2}
WALK_SECONDS = 20.0
Lib = unreal.GSTN_LinkContentLibrary
_saved = {}
_last = {'rig': None}


# ---- content ---------------------------------------------------------------------------------------------------------------------------------
def prepare_editor():
    """The plugin's test link type (created by the content helper when missing) and a registry that knows it. Run before PIE"""
    if not unreal.load_asset(LINK_TYPE_ASSET):
        import sys
        import os
        sys.path.insert(0, os.path.join(unreal.Paths.project_dir(), 'Tools', 'GAS'))
        import setup_defence_link_content as content
        content.run_link()
    Lib.rebuild_registry_and_reattach()
    return 'link type index', Lib.get_link_type_index(g._tag(GRAB))


def restore_editor():
    return 'nothing to restore'


def link_setup(grab_attack=False, akey=None):
    """The in-memory content of a scenario: the link steps in the combat pipeline; with grab_attack the test kit's attack becomes a grab"""
    prepare_editor()
    Lib.add_link_steps_to_rules(g.hit_rules())
    if grab_attack:
        kit = unreal.load_asset('/Game/GASNPPTest/Combat/' + g.HIT_KIT)
        if 'attack' not in _saved:
            ok_old = Lib.get_kit_move_hit_type(kit, g._tag(ATTACK_COMMAND))
            _saved['attack'] = ok_old
        Lib.set_kit_move_hit_type(kit, g._tag(ATTACK_COMMAND), g._tag('Combat.HitType.Grab'), g._tag(GRAB))
        if akey:
            return g._apply_loadout(akey, g.HIT_KIT)
    return 'link steps'


def link_reset():
    msgs = []
    try:
        Lib.remove_link_steps_from_rules(g.hit_rules())
        msgs.append('steps')
    except Exception:
        pass
    if 'attack' in _saved:
        old = _saved.pop('attack')
        kit = unreal.load_asset('/Game/GASNPPTest/Combat/' + g.HIT_KIT)
        hit_type, link_type = _out_tags(old)
        Lib.set_kit_move_hit_type(kit, g._tag(ATTACK_COMMAND), hit_type, link_type)
        msgs.append('attack')
    return 'restored ' + ','.join(msgs)


def _out_tags(result):
    """get_kit_move_hit_type's Python form: (hit type, link type) or (ok, hit type, link type)"""
    if isinstance(result, tuple):
        tags = [x for x in result if isinstance(x, unreal.GameplayTag)]
        if len(tags) >= 2:
            return tags[0], tags[1]
    return unreal.GameplayTag(), unreal.GameplayTag()


# ---- readers ---------------------------------------------------------------------------------------------------------------------------------
def actor(wi, key):
    return g.comps(g.worlds()[wi])[key].get_owner()


def body(wi, key):
    """(role, phase, serial, link frame, partner id) of the Body link world wi shows for pawn `key`; None without a fragment"""
    r = Lib.get_shown_body_link(actor(wi, key))
    if r is None:
        return None
    vals = [x for x in (r if isinstance(r, tuple) else (r,)) if not isinstance(x, bool)]
    if isinstance(r, tuple) and isinstance(r[0], bool) and not r[0]:
        return None
    return tuple(int(x) for x in vals[:5]) if len(vals) >= 5 else None


def linked_location(wi, key):
    """Where world wi shows the parked pawn `key` (UGSTN_LinkPresentationComponent.GetLinkedTransform), or None while it shows no parked role"""
    comp = actor(wi, key).get_component_by_class(unreal.GSTN_LinkPresentationComponent)
    if comp is None:
        return None
    r = comp.get_linked_transform()
    t = None
    if isinstance(r, unreal.Transform):
        t = r
    elif isinstance(r, tuple):
        if any(x is False for x in r if isinstance(x, bool)):
            return None
        t = next((x for x in r if isinstance(x, unreal.Transform)), None)
    if t is None or not comp.is_linked_parked():
        return None
    loc = t.translation
    return (loc.x, loc.y, loc.z)


def mover_mode(wi, key):
    m = actor(wi, key).get_component_by_class(unreal.MoverComponent)
    return str(m.get_movement_mode_name()) if m else None


def watchdog():
    """(breaks, orphans, last reason index, last frame) of the server's link watchdog"""
    r = Lib.get_watchdog_stats(actor(0, g.key_of_player(0)))
    vals = [int(x) for x in (r if isinstance(r, tuple) else (r,)) if not isinstance(x, bool)]
    return tuple(vals[:4]) if len(vals) >= 4 else (0, 0, 0, -1)


def shown_break(wi, key):
    """(reason, serial, grab immunity end) the Body channel of world wi shows for `key` after a break"""
    r = Lib.get_shown_body_break(actor(wi, key))
    vals = [int(x) for x in (r if isinstance(r, tuple) else (r,)) if not isinstance(x, bool)]
    return tuple(vals[:3]) if len(vals) >= 3 else (0, 0, 0)


def set_leash(cm):
    """The test link type's MaxDistanceCm, in memory (the scenarios put 0 back)"""
    d = unreal.load_asset(LINK_TYPE_ASSET)
    br = d.get_editor_property('break')
    br.set_editor_property('max_distance_cm', float(cm))
    d.set_editor_property('break', br)
    return 'leash', cm


def set_grab_immunity(frames):
    """ImmunityFramesAfterRelease of the test type's grab rule, in memory; returns the previous value"""
    d = unreal.load_asset(LINK_TYPE_ASSET)
    rule = d.get_editor_property('rules')[0]
    rules = rule.get_editor_property('rules')
    old = rules.get_editor_property('immunity_frames_after_release')
    rules.set_editor_property('immunity_frames_after_release', int(frames))
    rule.set_editor_property('rules', rules)
    return old


def move_away(key, back_cm=500.0):
    """Server teleport of a pawn back_cm behind itself (the authority moves it; clients get it as a correction)"""
    a = g.snap(g.worlds()[0])[key]
    yaw = math.radians(a['yaw'])
    loc = a['location']
    target = unreal.Vector(loc[0] - back_cm * math.cos(yaw), loc[1] - back_cm * math.sin(yaw), loc[2])
    g.comps(g.worlds()[0])[key].server_teleport_exact(target)
    return 'moved', (round(target.x), round(target.y))


def _check(run, label, fn):
    """run.at with the detail as text (a batch report slices it)"""
    def wrapped():
        ok, detail = fn()
        return ok, str(detail)
    return run.at(label, wrapped)


def last():
    return g.last_checks()


def _percentile(values, q):
    if not values:
        return 0.0
    s = sorted(values)
    k = min(len(s) - 1, max(0, int(math.ceil(q * len(s))) - 1))
    return s[k]


def _interp(series, t):
    """Linear interpolation of a [(t, (x, y, z))] series at t; None outside it"""
    if not series or t < series[0][0] or t > series[-1][0]:
        return None
    lo, hi = 0, len(series) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if series[mid][0] <= t:
            lo = mid
        else:
            hi = mid
    (t0, a), (t1, b) = series[lo], series[hi]
    u = 0.0 if t1 <= t0 else (t - t0) / (t1 - t0)
    return tuple(a[i] + (b[i] - a[i]) * u for i in range(3))


def presentation_error(world_series, server_series):
    """(p50, p95, max, delay s) of |world(t) - server(t - delay)| with the delay that minimises the mean, searched -0.5..0.5 s in 8 ms steps (a
    negative delay: the world runs AHEAD of the server, as the grabber's own predicting client does)"""
    best = None
    for step in range(-62, 63):
        d = step * 0.008
        errs = []
        for t, p in world_series:
            q = _interp(server_series, t - d)
            if q is not None:
                errs.append(math.dist(p, q))
        if len(errs) < max(10, len(world_series) // 2):
            continue
        mean = sum(errs) / len(errs)
        if best is None or mean < best[0]:
            best = (mean, d, errs)
    if best is None:
        return None
    _, d, errs = best
    return (_percentile(errs, 0.5), _percentile(errs, 0.95), max(errs), d)


class _LinkRig:
    """One pairing: the grabber A (world ia's player), the grabbed B (world ib's player). Samples B's shown linked location and Mover mode on every
    world each editor tick; the verdict goes through gasnpp_test._Run (its finish() logs PASS/FAIL and fills last_checks())"""

    def __init__(self, name, pairing):
        self.ia, self.ib = PAIRINGS[pairing]
        self.akey, self.bkey = g.key_of_player(self.ia), g.key_of_player(self.ib)
        self.clients = sorted({i for i in (self.ia, self.ib) if i > 0})
        self.observer = next((i for i in (1, 2) if i not in (self.ia, self.ib)), None)
        # The grabbed's own client is the main sampled copy when it is a client ('cli'), so its sim history is drained once
        main = self.ib if self.ib > 0 else self.clients[0]
        self.own_side = 'cli' if self.ib > 0 else None
        extra = [('b_srv', 0, self.bkey)]
        self.run = g._Run(name, main, judge_reconciles=False, extra=extra)
        self.series = {wi: [] for wi in range(len(g.worlds()))}
        self.modes = {wi: [] for wi in range(len(g.worlds()))}
        self.marks = {}
        _last['rig'] = self
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def _tick(self, _dt):
        try:
            t = self.run.now()
            for wi in self.series:
                p = linked_location(wi, self.bkey)
                if p is not None:
                    self.series[wi].append((t, p))
                self.modes[wi].append((t, mover_mode(wi, self.bkey)))
        except Exception:
            pass

    def stop(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None

    def mark(self, name):
        def f():
            self.marks[name] = {'t': self.run.now(), 'rb': {i: g.rollbacks(i) for i in (1, 2) if i < len(g.worlds())}}
            return 'mark', name
        return f

    def rb_between(self, a, b, wi):
        return self.marks[b]['rb'][wi] - self.marks[a]['rb'][wi]

    def place_partner(self, spot=(150.0, 0.0)):
        def f():
            g._place_near(self.ia, self.bkey, spot)
            return 'placed', spot
        return f

    def all_show(self, want_a_role, want_b_role):
        """Every world shows A with want_a_role and B with want_b_role, on one serial and one link frame"""
        rows = {wi: (body(wi, self.akey), body(wi, self.bkey)) for wi in range(len(g.worlds()))}
        ok = all(a and b and a[0] == want_a_role and b[0] == want_b_role for a, b in rows.values())
        serials = {(a[2], a[3], b[2], b[3]) for a, b in rows.values() if a and b}
        return ok and len(serials) == 1, f'{rows}'

    def none_show(self):
        rows = {wi: (body(wi, self.akey), body(wi, self.bkey)) for wi in range(len(g.worlds()))}
        ok = all((a is None or a[1] == 0) and (b is None or b[1] == 0) for a, b in rows.values())
        return ok, f'{rows}'


def _walk_steps(rig, t0, seconds):
    """The grabber walks, turns, circles and dashes: (time, callable) steps from t0 for `seconds`"""
    a = rig.ia
    steps = [(t0, lambda: g.hold(a, 'move', (0.0, 1.0, 0.0)))]
    t = t0 + 3.0
    while t < t0 + seconds - 0.5:
        k = int((t - t0) * 10)
        ang = k * 0.25
        steps.append((t, (lambda v: (lambda: g.hold(a, 'move', v)))((math.sin(ang), math.cos(ang), 0.0))))
        if k % 40 == 0:
            steps.append((t + 0.05, lambda: g.combat_tap(a, g.button_mask('Dash'))))
        t += 0.1
    steps.append((t0 + seconds, lambda: g.release(a, 'move')))
    return steps


def _spike(pairing):
    name = f'link_spike_{pairing}'
    rig = _LinkRig(name, pairing)
    run = rig.run
    grab = g._tag(GRAB)
    T_FORM, T_WALK = 1.6, 2.6
    T_END = T_WALK + WALK_SECONDS
    forced = []

    def form():
        ok = Lib.form_test_link(actor(0, rig.akey), actor(0, rig.bkey), grab)
        return 'formed' if ok else 'FORM REFUSED'

    def force(ci):
        def f():
            forced.append((ci, run.now()))
            g.force_reconcile(ci, extra_frames=10)
            return 'forced', ci
        return f

    def brk():
        return 'break', Lib.break_test_link(actor(0, rig.bkey))

    steps = [(0.2, lambda: link_setup()), (0.3, rig.place_partner()), (T_FORM - 0.1, rig.mark('pre_form')), (T_FORM, form),
             (T_FORM + 0.9, rig.mark('formed')),
             (T_FORM + 0.9, _check(run, 'formed on every world: grabber role 1, grabbed role 2, one serial and link frame', lambda: rig.all_show(1, 2))),
             (T_FORM + 0.9, _check(run, 'the grabbed copies that simulate it are in the parked mode',
                                   lambda: (all(mover_mode(wi, rig.bkey) == LINKED_MODE for wi in [0] + ([rig.ib] if rig.ib > 0 else [])),
                                            {wi: mover_mode(wi, rig.bkey) for wi in range(len(g.worlds()))}))),
             (T_FORM + 0.95, lambda: run.note(f'link fragment payload while linked: grabber {Lib.get_shown_link_payload_bytes(actor(0, rig.akey))} B, '
                                              f'grabbed {Lib.get_shown_link_payload_bytes(actor(0, rig.bkey))} B per sync state (bytes/s not measurable from Python)')),
             (T_WALK, rig.mark('walk_start'))]
    steps += _walk_steps(rig, T_WALK, WALK_SECONDS)
    # Forced reconciles inside the link, on each client that simulates one of the two (after the clean measurement window of the walk's first 7 s)
    for n, ci in enumerate(rig.clients):
        steps.append((T_WALK + 8.0 + 3.0 * n, rig.mark(f'pre_force_{ci}')))
        steps.append((T_WALK + 8.05 + 3.0 * n, force(ci)))
        steps.append((T_WALK + 9.5 + 3.0 * n, rig.mark(f'post_force_{ci}')))
    steps += [(T_END + 0.2, rig.mark('walk_end')), (T_END + 0.3, brk), (T_END + 1.5, rig.mark('broken')),
              (T_END + 1.5, _check(run, 'the link is gone on every world', rig.none_show)),
              (T_END + 1.5, _check(run, 'the grabbed left the parked mode', lambda: (all(mover_mode(wi, rig.bkey) != LINKED_MODE for wi in range(len(g.worlds()))),
                                                                              {wi: mover_mode(wi, rig.bkey) for wi in range(len(g.worlds()))})))]

    def judge():
        res = []
        for ci in rig.clients:
            start = rig.rb_between('pre_form', 'formed', ci)
            res.append((f'client {ci}: exactly one correction at link start', start == 1, f'{start}'))
            end = rig.rb_between('walk_end', 'broken', ci)
            res.append((f'client {ci}: exactly one correction at link end', end == 1, f'{end}'))
            # Outside the forced windows nothing may reconcile; one forced reconcile (with its extra frames) adds 2 to the harness counter (measured
            # in F10: one 'Reconcile activated due to ForceReconcile' line, +2), and np.ForceReconcile is one cvar any client may take
            walk = rig.rb_between('walk_start', 'walk_end', ci)
            in_forced = sum(rig.rb_between(f'pre_force_{c}', f'post_force_{c}', ci) for c in rig.clients)
            res.append((f'client {ci}: zero unforced reconciles during the walk', walk - in_forced == 0, f'{walk} rollbacks, {in_forced} inside the forced windows'))
            for c in rig.clients:
                n = rig.rb_between(f'pre_force_{c}', f'post_force_{c}', ci)
                res.append((f'client {ci}: forced window of client {c} costs at most one forced reconcile (2 counter steps)', n <= 2, f'{n}'))
        if rig.observer is not None:
            # Over the held link only: the PIE start-up corrections of every client (frame ~360) can land in the formation window of a batch run
            n = rig.rb_between('walk_start', 'walk_end', rig.observer) - sum(rig.rb_between(f'pre_force_{c}', f'post_force_{c}', rig.observer) for c in rig.clients)
            res.append((f'observer client {rig.observer}: no correction while the link is held', n <= 0, f'{n} (outside the forced windows)'))
        # Forced reconciles converge: the grabbed's simulated copy on its own client matches the authority's frame by frame within 2 frames after each
        if rig.ib > 0:
            srv = run.final_by_frame('b_srv', T_WALK, T_END)
            own = run.final_by_frame(rig.own_side, T_WALK, T_END)
            for ci, tf in forced:
                after = sorted(f for f, s in own.items() if f in srv and tf <= s['t'] <= tf + 1.5)
                bad = [f for f in after if math.dist(own[f]['loc'], srv[f]['loc']) > 1.0]
                res.append((f'forced reconcile on client {ci} converges in <= {S1["converge_frames"]} frames (grabbed capsule vs authority)',
                            len(bad) <= S1['converge_frames'], f'{len(bad)} of {len(after)} frames off by > 1 cm'))
        # Presentation error of the grabbed pawn's shown transform, against the server's (best constant delay removed)
        server = [(t, p) for t, p in rig.series[0] if T_WALK <= t <= T_WALK + 7.0]
        for wi in range(1, len(g.worlds())):
            mine = [(t, p) for t, p in rig.series[wi] if T_WALK + 0.5 <= t <= T_WALK + 7.0]
            e = presentation_error(mine, server)
            if e is None:
                res.append((f'world {wi}: presentation sampled', False, f'{len(mine)} samples'))
                continue
            p50, p95, mx, d = e
            limit = S1['own_p95'] if wi == rig.ia else S1['partner_p95']
            role = 'grabber' if wi == rig.ia else 'grabbed' if wi == rig.ib else 'observer'
            res.append((f'world {wi} ({role}): presentation p95 <= {limit} cm, max <= {S1["max"]} cm', p95 <= limit and mx <= S1['max'],
                        f'p50 {p50:.1f} p95 {p95:.1f} max {mx:.1f} cm, delay {d * 1000:.0f} ms, {len(mine)} samples'))
        if rig.series[0]:
            run.note(f'host/server shows its own authoritative transform: error 0 by definition ({len(rig.series[0])} samples)')
        return res

    def verdicts():
        for label, ok, detail in judge():
            run.check(label, ok, detail)
        run.note(f'rollbacks per mark: {[(n, m["rb"]) for n, m in sorted(rig.marks.items(), key=lambda x: x[1]["t"])]}')
        run.note(f'link fragment payload after the break: {Lib.get_shown_link_payload_bytes(actor(0, rig.bkey))} B')
        return 'judged'

    def restore():
        rig.stop()
        g.release(rig.ia, 'move')
        return link_reset()

    steps += [(T_END + 1.6, verdicts), (T_END + 1.7, restore), (T_END + 1.9, run.finish())]
    run.run(steps)


def _rollback(pairing):
    """Formation through the pipeline (a grab hit), forced reconciles on each client over the formation frames: F+1 / F+2 alike everywhere"""
    name = f'link_rollback_{pairing}'
    rig = _LinkRig(name, pairing)
    run = rig.run
    st = {}

    def setup():
        st['kit'] = link_setup(grab_attack=True, akey=rig.akey)
        g._hit_logging(True)
        return 'kit', st['kit']

    def mark():
        st['F0'] = g._latest_frame(0, rig.akey)
        return 'mark', st['F0']

    def attack():
        g.combat_tap(rig.ia, g.button_mask('Attack'))
        return 'attack'

    def force(ci):
        def f():
            g.force_reconcile(ci, extra_frames=12)
            return 'forced', ci
        return f

    def formed_frames():
        hits = [r for r in g._hit_log(0, rig.akey) if r[0] >= st.get('F0', 0)]
        if not hits:
            return False, 'no grab hit on the authority'
        F = hits[0][0]
        rows = {wi: (body(wi, rig.akey), body(wi, rig.bkey)) for wi in range(len(g.worlds()))}
        ok = all(a and b and a[0] == 1 and b[0] == 2 and a[3] == F + 1 and b[3] == F + 1 and a[2] == b[2] for a, b in rows.values())
        return ok, f'hit frame {F}: link frame F+1 = {F + 1} expected on both sides everywhere; {rows}'

    def kit_ok():
        return st.get('kit') == 'OK', f"loadout {st.get('kit')}"

    def brk():
        # F11: the break goes through the watchdog (a leash and the grabber moved away), with forced reconciles over the break frames
        st['wd'] = watchdog()
        set_leash(150.0)
        return move_away(rig.akey)

    def broke_once():
        wd = watchdog()
        rows = {wi: (shown_break(wi, rig.akey), shown_break(wi, rig.bkey)) for wi in range(len(g.worlds()))}
        ok = wd[0] == st['wd'][0] + 1 and wd[2] == REASON['Distance']
        return ok, f'watchdog {st["wd"]} -> {wd} (one break, reason Distance); shown (reason, serial, immunity) {rows}'

    def restore():
        rig.stop()
        g._hit_logging(False)
        set_leash(0.0)
        r = link_reset()
        g._apply_loadout(rig.akey, g.HIT_KIT)
        return r

    T = 2.0
    steps = [(0.2, setup), (0.4, rig.place_partner((205.0, 0.0))), (T - 0.1, mark), (T, attack)]
    for n, ci in enumerate(rig.clients):
        steps.append((T + 0.03 + 0.04 * n, force(ci)))
    steps += [(T + 2.0, _check(run, 'the attack kit (a grab) was accepted', kit_ok)),
              (T + 2.0, _check(run, 'formed at F+1 on the target and the same link on the initiator, on every world (after the forced reconciles)', formed_frames)),
              (T + 2.0, _check(run, 'the grabbed is parked where it simulates', lambda: (all(mover_mode(wi, rig.bkey) == LINKED_MODE for wi in [0] + ([rig.ib] if rig.ib > 0 else [])),
                                                                                  {wi: mover_mode(wi, rig.bkey) for wi in range(len(g.worlds()))}))),
              (T + 2.1, brk)]
    for n, ci in enumerate(rig.clients):
        steps.append((T + 2.16 + 0.04 * n, force(ci)))
    steps += [(T + 3.2, _check(run, 'the link is gone on every world (after forced reconciles over the break frames)', rig.none_show)),
              (T + 3.2, _check(run, 'one watchdog break, reason Distance', broke_once)),
              (T + 3.2, _check(run, 'the grabbed left the parked mode on every world', lambda: (all(mover_mode(wi, rig.bkey) != LINKED_MODE for wi in range(len(g.worlds()))),
                                                                                         {wi: mover_mode(wi, rig.bkey) for wi in range(len(g.worlds()))}))),
              (T + 3.3, restore), (T + 3.6, run.finish())]
    run.run(steps)


def _break_distance(pairing):
    """F11: breaks only through the watchdog. (1) A leash (MaxDistanceCm 150 in memory) and the server moving the grabber 500 cm away: one break with
    reason Distance, the link gone on every world, the grabbed out of the parked mode and placed by the exit placer near the grabber, its grab immunity
    set, at most one correction (2 counter steps) on each involved client. (2) A new link, the grabbed knocked out in a KO zone: a death break.
    (3) The same with the grabber knocked out."""
    name = f'link_break_distance_{pairing}'
    rig = _LinkRig(name, pairing)
    run = rig.run
    grab = g._tag(GRAB)
    st = {}

    def form():
        return 'formed' if Lib.form_test_link(actor(0, rig.akey), actor(0, rig.bkey), grab) else 'FORM REFUSED'

    def snapshot(tag):
        def f():
            st[tag] = watchdog()
            rig.mark(tag)()
            return tag, st[tag]
        return f

    def broke(tag, reasons):
        def f():
            wd = watchdog()
            ok = wd[0] == st[tag][0] + 1 and wd[2] in reasons
            return ok, f'watchdog {st[tag]} -> {wd} (one break, reason in {reasons})'
        return f

    def exit_near():
        a = g.snap(g.worlds()[0])[rig.akey]['location']
        b = g.snap(g.worlds()[0])[rig.bkey]['location']
        d = math.dist(a[:2], b[:2])
        return 60.0 <= d <= 250.0, f'grabbed {d:.0f} cm from the grabber on the authority (ring placer radius 120)'

    def immune():
        r = shown_break(0, rig.bkey)
        return r[2] > 0, f'grabbed (reason, serial, immunity end) {r}'

    def one_correction(tag_a, tag_b):
        def f():
            n = {ci: rig.rb_between(tag_a, tag_b, ci) for ci in rig.clients}
            return all(v <= 2 for v in n.values()), f'counter steps per involved client {n} (one correction = 2)'
        return f

    def spawn_zone_on(key):
        """A KO zone 900 cm away with `key` (the grabber) teleported into it"""
        def f():
            loc = g.snap(g.worlds()[0])[key]['location']
            centre = unreal.Vector(loc[0] + 900.0, loc[1], loc[2])
            for w in g.worlds():
                unreal.GSTN_KOZone.spawn_zone(w, centre, unreal.Vector(150.0, 150.0, 400.0))
            g.comps(g.worlds()[0])[key].server_teleport_exact(centre)
            return 'KO zone for', key
        return f

    def spawn_zone_ahead():
        """A parked pawn is not moved by a teleport (its combat position follows the partner's socket): a small KO zone 50..170 cm ahead of the
        grabber holds the grabbed (parked 90 cm ahead, its capsule left 150 cm ahead) and not the grabber"""
        a = g.snap(g.worlds()[0])[rig.akey]
        yaw = math.radians(a['yaw'])
        loc = a['location']
        centre = unreal.Vector(loc[0] + 110.0 * math.cos(yaw), loc[1] + 110.0 * math.sin(yaw), loc[2])
        for w in g.worlds():
            unreal.GSTN_KOZone.spawn_zone(w, centre, unreal.Vector(60.0, 60.0, 400.0))
        return 'KO zone ahead of the grabber', (round(centre.x), round(centre.y))

    def clear_zones():
        for w in g.worlds():
            unreal.GSTN_KOZone.destroy_all_zones(w)
        return 'zones removed'

    def respawn():
        for key in (rig.akey, rig.bkey):
            g.comps(g.worlds()[0])[key].server_reset_test_state(unreal.Vector(0.0, 0.0, 0.0))
        return 'respawned'

    gone = lambda: rig.none_show()
    unparked = lambda: (all(mover_mode(wi, rig.bkey) != LINKED_MODE for wi in range(len(g.worlds()))), {wi: mover_mode(wi, rig.bkey) for wi in range(len(g.worlds()))})
    death = (REASON['OwnDeath'], REASON['PartnerDeath'])
    def setup():
        st['imm'] = set_grab_immunity(600)   # long enough to still be seen at the check
        return link_setup()

    steps = [(0.2, setup), (0.3, rig.place_partner()), (1.6, form),
             (2.5, _check(run, 'formed on every world', lambda: rig.all_show(1, 2))),
             (2.6, lambda: set_leash(150.0)), (2.7, snapshot('pre_distance')), (2.8, lambda: move_away(rig.akey)),
             (4.2, rig.mark('post_distance')),
             (4.2, _check(run, 'distance: one watchdog break, reason Distance', broke('pre_distance', (REASON['Distance'],)))),
             (4.2, _check(run, 'distance: the link is gone on every world', gone)),
             (4.2, _check(run, 'distance: the grabbed left the parked mode on every world', unparked)),
             (4.2, _check(run, 'distance: the exit placer put the grabbed near the grabber', exit_near)),
             (4.2, _check(run, 'distance: the grabbed is grab immune after the break', immune)),
             (4.2, _check(run, 'distance: at most one correction on each involved client', one_correction('pre_distance', 'post_distance'))),
             (4.3, lambda: set_leash(0.0)),
             (4.5, rig.place_partner()), (5.2, form), (6.1, _check(run, 'formed again', lambda: rig.all_show(1, 2))),
             (6.2, snapshot('pre_ko_b')), (6.3, spawn_zone_ahead),
             (8.0, _check(run, 'grabbed knocked out: one death break', broke('pre_ko_b', death))),
             (8.0, _check(run, 'grabbed knocked out: the link is gone on every world', gone)),
             (8.1, clear_zones), (8.2, respawn),
             (10.0, rig.place_partner()), (10.7, form), (11.6, _check(run, 'formed a third time', lambda: rig.all_show(1, 2))),
             (11.7, snapshot('pre_ko_a')), (11.8, spawn_zone_on(rig.akey)),
             (13.5, _check(run, 'grabber knocked out: one death break', broke('pre_ko_a', death))),
             (13.5, _check(run, 'grabber knocked out: the link is gone on every world', gone)),
             (13.6, clear_zones), (13.7, respawn)]

    def restore():
        rig.stop()
        set_leash(0.0)
        if 'imm' in st:
            set_grab_immunity(st['imm'])
        clear_zones()
        return link_reset()

    steps += [(15.5, restore), (15.8, run.finish())]
    run.run(steps)


def scenario_grab_immunity(pairing='client_grabs_host', immunity_frames=120):
    """F11: a regrab is refused for the grab rule's ImmunityFramesAfterRelease after a break (set to `immunity_frames` in memory): a grab hit forms the
    link, the watchdog breaks it (leash and the grabber moved away), a second grab inside the immunity whiffs (no link on any world), a third after it forms"""
    rig = _LinkRig('grab_immunity', pairing)
    run = rig.run
    st = {}

    def setup():
        st['kit'] = link_setup(grab_attack=True, akey=rig.akey)
        st['imm'] = set_grab_immunity(immunity_frames)
        return 'kit', st['kit'], 'immunity was', st['imm']

    def attack():
        g.combat_tap(rig.ia, g.button_mask('Attack'))
        return 'attack'

    def brk():
        set_leash(150.0)
        return move_away(rig.akey)

    def linked(want):
        def f():
            rows = {wi: (body(wi, rig.akey), body(wi, rig.bkey)) for wi in range(len(g.worlds()))}
            got = all(a and b and a[0] == 1 and b[0] == 2 for a, b in rows.values())
            idle = all((a is None or a[1] == 0) and (b is None or b[1] == 0) for a, b in rows.values())
            return (got if want else idle), f'{rows}; target (reason, serial, immunity end) {shown_break(0, rig.bkey)}'
        return f

    def restore():
        rig.stop()
        set_leash(0.0)
        if 'imm' in st:
            set_grab_immunity(st['imm'])
        r = link_reset()
        g._apply_loadout(rig.akey, g.HIT_KIT)
        return r

    steps = [(0.2, setup), (0.4, rig.place_partner((205.0, 0.0))), (2.0, attack),
             (3.5, _check(run, 'the first grab formed', linked(True))),
             (3.6, brk), (4.4, lambda: set_leash(0.0)), (4.5, rig.place_partner((205.0, 0.0))), (5.0, attack),
             (6.0, _check(run, f'a regrab inside the {immunity_frames}-frame immunity is refused (no link on any world)', linked(False))),
             (6.1, rig.place_partner((205.0, 0.0))), (7.8, attack),
             (9.0, _check(run, 'after the immunity the grab forms again', linked(True))),
             (9.1, lambda: Lib.break_test_link(actor(0, rig.bkey))), (10.0, restore), (10.3, run.finish())]
    run.run(steps)


for _p in PAIRINGS:
    globals()[f'scenario_link_spike_{_p}'] = (lambda p: (lambda: _spike(p)))(_p)
    globals()[f'scenario_link_rollback_{_p}'] = (lambda p: (lambda: _rollback(p)))(_p)
    globals()[f'scenario_link_break_distance_{_p}'] = (lambda p: (lambda: _break_distance(p)))(_p)


def _walk_probe(pairing, link, sample):
    """Task A probe (F11): the spike's walk with or without the held link and with or without the rig's per-tick sampling of every world, no forced
    reconciles. Reports the walking client's unforced reconciles and the editor frame times (one process: every world shares the engine frame)"""
    name = f'link_probe_{pairing}_{"link" if link else "nolink"}_{"sample" if sample else "nosample"}'
    rig = _LinkRig(name, pairing)
    run = rig.run
    if not sample:
        rig.stop()
    dts = []
    probe = {'h': None}

    def dt_tick(dt):
        dts.append(dt)

    def form():
        if not link:
            return 'no link'
        return 'formed' if Lib.form_test_link(actor(0, rig.akey), actor(0, rig.bkey), g._tag(GRAB)) else 'FORM REFUSED'

    def start():
        probe['h'] = unreal.register_slate_post_tick_callback(dt_tick)
        return 'dt probe on'

    T_FORM, T_WALK = 1.6, 2.6
    T_END = T_WALK + WALK_SECONDS
    steps = [(0.2, lambda: link_setup()), (0.3, rig.place_partner()), (T_FORM, form), (T_WALK - 0.1, start), (T_WALK, rig.mark('walk_start'))]
    steps += _walk_steps(rig, T_WALK, WALK_SECONDS)

    def verdicts():
        unreal.unregister_slate_post_tick_callback(probe['h'])
        for ci in rig.clients:
            n = rig.rb_between('walk_start', 'walk_end', ci)
            run.check(f'client {ci}: zero reconciles during the walk', n == 0, f'{n}')
        s = sorted(dts)
        run.note(f'PROBE {name}: frames {len(s)}, mean {1000 * sum(s) / max(1, len(s)):.1f} ms, p95 {1000 * _percentile(s, 0.95):.1f} ms, '
                 f'max {1000 * (s[-1] if s else 0):.1f} ms, > 64 ms {sum(1 for x in s if x > 0.064)}, > 100 ms {sum(1 for x in s if x > 0.1)}')
        unreal.log(f'LINK PROBE {name}: walk rollbacks {[rig.rb_between("walk_start", "walk_end", ci) for ci in rig.clients]}, frames {len(s)}, '
                   f'p95 {1000 * _percentile(s, 0.95):.1f} ms, > 64 ms {sum(1 for x in s if x > 0.064)}')
        return 'judged'

    def restore():
        rig.stop()
        g.release(rig.ia, 'move')
        if link:
            Lib.break_test_link(actor(0, rig.bkey))
        return link_reset()

    steps += [(T_END + 0.2, rig.mark('walk_end')), (T_END + 0.3, verdicts), (T_END + 0.4, restore), (T_END + 1.6, run.finish())]
    run.run(steps)


for _p in ('client_grabs_host', 'client_grabs_client'):
    for _l in (True, False):
        for _s in (True, False):
            globals()[f'scenario_link_probe_{_p}_{"link" if _l else "nolink"}_{"sample" if _s else "nosample"}'] = (lambda p, l, s: (lambda: _walk_probe(p, l, s)))(_p, _l, _s)


def control_walk(client=1, seconds=WALK_SECONDS):
    """The spike's walk with NO link (a control for the reconcile counts under loss): logs 'LINK CONTROL' with the rollbacks of both clients"""
    class _Walker:
        ia = client
    st = {}

    def mark(name):
        def f():
            st[name] = tuple(g.rollbacks(i) for i in (1, 2))
            return name, st[name]
        return f

    def report():
        delta = tuple(b - a for a, b in zip(st['start'], st['end']))
        unreal.log(f'LINK CONTROL walk of client {client}, {seconds} s, net {g._cfg.get("net")}: rollbacks per client {delta}')
        return 'control', delta
    g.run_timeline([(0.5, mark('start'))] + _walk_steps(_Walker, 0.5, seconds) + [(seconds + 0.7, mark('end')), (seconds + 0.8, report)])
    return 'control started'


# ---- F12a: the grab moves -------------------------------------------------------------------------------------------------------------------------
# The kit of the harness pawns (KIT_Test_Hits) gets, IN MEMORY, the standard link slots and the plugin's harness link moves (UGSTN_GA_LinkTest*: grab on a
# tap of Interact, pummel on Attack, throw on Crouch + Attack (Throw.Down: the test input config has no Strong button), release on Interact as the grabber) and a link extension with default
# rules (UGSTN_LinkContentLibrary.add_link_test_bindings), put back by grab_content_reset. Every scenario runs in a fresh PIE (defence_batch).
REASON.update({'ShakeOff': 1, 'Escape': 8, 'Release': 9, 'Throw': 10})
SUITE_GRAB = ['grab_throw', 'grab_mash', 'grab_interact', 'grab_gate', 'throw_at_enemy']
SUITE_LINK = SUITE_LINK + SUITE_GRAB
_grab = {'kit': None}


def _kit():
    return unreal.load_asset('/Game/GASNPPTest/Combat/' + g.HIT_KIT)


def grab_content_setup(keys, bounce=False):
    """The link steps in the pipeline, the harness link moves on the test kit, the kit applied to the pawns `keys`. Returns the loadout answers.
    Applying the kit a pawn already runs changes nothing (no new revision, the old table stays): run it in a fresh PIE, or before the first apply
    (grab_content_setup([]) then the rig's own apply). bounce applies another kit first, which the server refuses within 30 frames of a change"""
    link_setup()
    added = Lib.add_link_test_bindings(_kit())
    answers = []
    for k in keys:
        if bounce:
            g._apply_loadout(k, 'KIT_Test_Alpha')
        answers.append(g._apply_loadout(k, g.HIT_KIT))
    _grab['kit'] = answers
    return 'link moves', added, _grab['kit']


def grab_content_reset():
    msgs = [str(Lib.remove_link_test_bindings(_kit()))]
    msgs.append(link_reset())
    return 'grab content reset', msgs


def numbers(wi, key):
    """(mash end, mash processed, cooldown end, grab immunity end, break reason) world wi shows for `key`"""
    r = Lib.get_shown_link_numbers(actor(wi, key))
    vals = [int(x) for x in (r if isinstance(r, tuple) else (r,)) if not isinstance(x, bool)]
    return tuple(vals[:5]) if len(vals) >= 5 else (0, 0, 0, 0, 0)


def link_text(wi, key):
    return Lib.get_shown_link_text(actor(wi, key))


def has_tag(wi, key, name):
    asc = actor(wi, key).get_component_by_class(unreal.AbilitySystemComponent)
    return bool(asc.has_matching_gameplay_tag(g._tag(name)))


def _tag_exists(name):
    """The tag is registered (a tag the import cannot resolve reads back as None)"""
    return g._tag_text(g._tag(name)) == name


class _GrabRig:
    """One grab pairing: A (world ia's player) grabs B (world ib's player). The verdict goes through gasnpp_test._Run. Samples B's mash end on every
    world each editor tick (its last value before the escape is the predicted escape frame)"""

    def __init__(self, name, pairing):
        self.ia, self.ib = PAIRINGS[pairing]
        self.akey, self.bkey = g.key_of_player(self.ia), g.key_of_player(self.ib)
        self.others = [i for i in (0, 1, 2) if i not in (self.ia, self.ib)]
        main = self.ib if self.ib > 0 else self.ia
        self.run = g._Run(name, main, judge_reconciles=False)
        # Details as text: the batch report slices them
        at = self.run.at
        self.run.at = lambda label, fn: at(label, lambda: (lambda r: (r[0], str(r[1])))(fn()))
        self.marks, self.ends = {}, {wi: [] for wi in range(len(g.worlds()))}
        # What each world showed, every editor tick: B's break (reason, serial, immunity end) and A's cooldown end, the values a break leaves for a
        # few frames only (the idle hygiene drops them once the exit is past), so the checks read what was seen, not what is left
        self.seen = {wi: {'breaks': set(), 'immunity': set(), 'cooldown': set()} for wi in range(len(g.worlds()))}
        self.trace = []
        # The pending reactions B's authority copy showed (EGSTN_HitReaction: 0 stagger in place, 1 push back, 2 launch)
        self.reactions = set()
        self.handle = unreal.register_slate_post_tick_callback(self._tick)
        _last['rig'] = self

    def _tick(self, _dt):
        try:
            for wi in self.ends:
                n = numbers(wi, self.bkey)
                if n[0]:
                    self.ends[wi].append(n[0])
                b = shown_break(wi, self.bkey)
                if b[0]:
                    self.seen[wi]['breaks'].add((b[0], b[1]))
                if n[3]:
                    self.seen[wi]['immunity'].add(n[3])
                c = numbers(wi, self.akey)[2]
                if c:
                    self.seen[wi]['cooldown'].add(c)
            info = g.victim_info(0, self.bkey)
            if info[5]:
                self.reactions.add(info[5] - 1)
            row = (body(0, self.akey), body(0, self.bkey), numbers(0, self.bkey)[3], numbers(0, self.akey)[2], watchdog())
            if not self.trace or self.trace[-1][1:] != row:
                self.trace.append((round(self.run.now(), 2),) + row)
        except Exception:
            pass

    def stop(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None

    def setup(self):
        return lambda: grab_content_setup([self.akey, self.bkey] + [g.key_of_player(i) for i in self.others])

    def place(self, spot=(90.0, 0.0), key=None):
        def f():
            g._place_near(self.ia, key or self.bkey, spot)
            return 'placed', key or self.bkey, spot
        return f

    def tap(self, wi, button):
        return lambda: ('tap', wi, button, g.combat_tap(wi, g.button_mask(button)))

    def throw(self):
        """Command.Link.Throw.Down: Crouch held and an Attack press (the test input config has no Strong button). Release with self.unhold()"""
        return lambda: ('throw', g.combat_hold(self.ia, g.button_mask('Crouch')), g.combat_tap(self.ia, g.button_mask('Attack')))

    def unhold(self):
        return lambda: ('buttons cleared', g.combat_clear(self.ia))

    def mark(self, name):
        def f():
            self.marks[name] = {'rb': {i: g.rollbacks(i) for i in (1, 2)}, 'dmg': [g.damage_of(w, self.bkey) for w in range(len(g.worlds()))],
                                'frame': g._latest_frame(0, self.akey), 'watchdog': watchdog()}
            return 'mark', name, self.marks[name]
        return f

    def formed(self, label='formed on every world: A role 1, B role 2, one serial and link frame'):
        def f():
            rows = {wi: (body(wi, self.akey), body(wi, self.bkey)) for wi in range(len(g.worlds()))}
            ok = all(a and b and a[0] == 1 and b[0] == 2 and a[1] == 2 and b[1] == 2 for a, b in rows.values())
            serials = {(a[2], a[3], b[2], b[3]) for a, b in rows.values() if a and b}
            return ok and len(serials) == 1, f'{rows}'
        return self.run.at(label, f)

    def parked(self):
        return self.run.at('B is in the parked mode where it is simulated', lambda: (
            all(mover_mode(wi, self.bkey) == LINKED_MODE for wi in [0] + ([self.ib] if self.ib > 0 else [])),
            {wi: mover_mode(wi, self.bkey) for wi in range(len(g.worlds()))}))

    def gone(self, label='the link is gone on every world'):
        def f():
            rows = {wi: (body(wi, self.akey), body(wi, self.bkey)) for wi in range(len(g.worlds()))}
            return all((a is None or a[1] == 0) and (b is None or b[1] == 0) for a, b in rows.values()), f'{rows}'
        return self.run.at(label, f)

    def reason(self, name):
        """The break's reason. A watchdog break (Release, Escape...) is the watchdog's last; a throw breaks in the simulation (the thrower at its key,
        the thrown at its next frame) and the watchdog posts nothing for it. The reason itself stays on the fragment for a frame or two (the idle
        hygiene), too short for the editor tick to sample on every world"""
        def f():
            w = watchdog()
            if name == 'Throw':
                before = self.marks.get('pre_grab', {}).get('watchdog', (0,))[0]
                return w[0] == before, f'watchdog {w}, breaks before the grab {before}'
            return w[2] == REASON[name], f'{name}={REASON[name]}: watchdog {w}'
        return self.run.at(f'the link broke with reason {name}', f)

    def seen_equal(self, label, what, positive=True):
        """Every world showed the same value(s) of `what` ('immunity' of B, 'cooldown' of A), at least one when positive"""
        def f():
            rows = {wi: sorted(v[what]) for wi, v in self.seen.items()}
            last = {wi: (v[-1] if v else None) for wi, v in rows.items()}
            return (all(last.values()) or not positive) and len(set(last.values())) == 1, f'{rows}'
        return self.run.at(label, f)

    def equal_texts(self, label):
        """Both pawns' Body link summary (role, phase, serial, link frame, partner) and B's mash end, equal on every world. The full text also holds
        the mash's last counted frame, which each world shows with its own presentation delay"""
        def f():
            rows = {wi: (body(wi, self.akey), body(wi, self.bkey), numbers(wi, self.bkey)[0]) for wi in range(len(g.worlds()))}
            return len(set(rows.values())) == 1, f'{rows}'
        return self.run.at(label, f)

    def target_tag(self):
        return self.run.at('A projects Interact.Target.Grab before the tap (its own world and the server)', lambda: (
            has_tag(self.ia, self.akey, 'Interact.Target.Grab') and has_tag(0, self.akey, 'Interact.Target.Grab'),
            {wi: has_tag(wi, self.akey, 'Interact.Target.Grab') for wi in range(len(g.worlds()))}))

    def kits_ok(self):
        return self.run.at('the kits took the link moves', lambda: (_grab['kit'] is not None and all(k == 'OK' for k in _grab['kit']), f"{_grab['kit']}"))

    def end(self, t):
        def restore():
            self.stop()
            return grab_content_reset()
        trace = lambda: self.run.note(f'server trace (t, A body, B body, B immunity, A cooldown, watchdog): {self.trace[:40]}')
        return [(t, self.kits_ok()), (t, trace), (t + 0.05, restore), (t + 0.2, self.run.finish())]

    def clear_others(self):
        """The uninvolved pawns out of every reach (3 m behind A, 4 m apart): with a second candidate in reach the grab probe's focus is not the target"""
        def f():
            for n, i in enumerate(self.others):
                g._place_near(self.ia, g.key_of_player(i), (-300.0, 400.0 * (n + 1)))
            return 'others cleared'
        return f

    def grab_steps(self, t0=1.2):
        """Place B in reach, check the target tag, tap the grab, check the formation and the parked mode. Returns (steps, time after)"""
        return [(t0 - 0.05, self.clear_others()), (t0, self.place()), (t0 + 0.6, self.target_tag()), (t0 + 0.65, self.mark('pre_grab')), (t0 + 0.7, self.tap(self.ia, 'Interact')),
                (t0 + 1.5, self.formed()), (t0 + 1.5, self.parked())], t0 + 1.6


def scenario_grab_throw(pairing='client_grabs_client'):
    """A grabs B with a tap of Interact, pummels twice (Attack), throws (Crouch + Attack: the down throw). Every world shows the same link before, after the pummels
    and after the throw: the pummels land on the partner only (damage 3 each, the link stays), the throw ends both sides with reason Throw (the thrown
    side at the thrower's socket, then launched), the thrown side carries the AfterThrow grab immunity and the thrower its cooldown, equal everywhere"""
    rig = _GrabRig('grab_throw', pairing)
    run = rig.run
    steps = [(0.2, rig.setup())]
    grab, t = rig.grab_steps()
    steps += grab
    # The grab lasts 150 frames (2.4 s) without a mash: the pummels and the throw come well inside it
    steps += [(t + 0.15, rig.mark('pre_pummel')), (t + 0.2, rig.tap(rig.ia, 'Attack')), (t + 0.5, rig.tap(rig.ia, 'Attack')), (t + 0.85, rig.mark('pummeled')),
              (t + 0.85, rig.formed('after two pummels the link still holds, the same on every world')),
              (t + 0.85, run.at('the pummels did 3 + 3 damage to B on the authority', lambda: (
                  abs((rig.marks['pummeled']['dmg'][0] - rig.marks['pre_pummel']['dmg'][0]) - 6.0) < 0.6,
                  f"{rig.marks['pre_pummel']['dmg']} -> {rig.marks['pummeled']['dmg']}"))),
              (t + 0.9, rig.equal_texts('the link states are equal on every world after the pummels')),
              (t + 1.0, rig.throw()), (t + 1.3, rig.unhold()), (t + 3.1, rig.mark('thrown')),
              (t + 3.1, rig.gone('the throw ended the link on every world')), (t + 3.1, rig.reason('Throw')),
              (t + 3.1, rig.seen_equal('B carried the after-throw grab immunity, the same end frame on every world', 'immunity')),
              (t + 3.1, rig.seen_equal('A carried its grab cooldown, the same end frame on every world', 'cooldown')),
              (t + 3.1, run.at('B left the parked mode everywhere', lambda: (all(mover_mode(wi, rig.bkey) != LINKED_MODE for wi in range(len(g.worlds()))),
                                                                           {wi: mover_mode(wi, rig.bkey) for wi in range(len(g.worlds()))}))),
              (t + 3.1, run.at('the throw did its damage (8), and every world shows the same damage after it', lambda: (
                  rig.marks['thrown']['dmg'][0] - rig.marks['pummeled']['dmg'][0] > 7.0 and len({round(d, 1) for d in rig.marks['thrown']['dmg']}) == 1,
                  f"{rig.marks['pre_pummel']['dmg']} -> {rig.marks['pummeled']['dmg']} -> {rig.marks['thrown']['dmg']}"))),
              (t + 3.15, rig.equal_texts('the link states are equal on every world after the throw')),
              (t + 3.2, lambda: run.note(f"rollbacks per mark: {[(n, m['rb']) for n, m in rig.marks.items()]}"))]
    steps += rig.end(t + 3.3)
    run.run(steps)


def scenario_grab_mash(pairing='client_grabs_client'):
    """A grabs B; B mashes Attack every 0.12 s and its client is forced to reconcile in the middle of it. The mash end is equal on every world while it
    runs (the presses count once, a re-simulation counts nothing twice), and the link breaks with reason Escape on every world; the watchdog's break frame
    is at or after the mash end that every world last showed (the escape on the predicted frame), within one engine frame of ticks"""
    rig = _GrabRig('grab_mash', pairing)
    run = rig.run
    steps = [(0.2, rig.setup())]
    grab, t = rig.grab_steps()
    steps += grab
    steps.append((t, run.at('B has a mash end from the formation on, equal on every world', lambda: (
        len({numbers(wi, rig.bkey)[0] for wi in range(len(g.worlds()))}) == 1 and numbers(0, rig.bkey)[0] > 0, {wi: numbers(wi, rig.bkey) for wi in range(len(g.worlds()))}))))
    mash_from = t + 0.1
    k = 0
    while mash_from + 0.12 * k < t + 2.2:
        steps.append((mash_from + 0.12 * k, rig.tap(rig.ib, 'Attack')))
        k += 1
    if rig.ib > 0:
        steps.append((mash_from + 0.5, lambda: ('forced', g.force_reconcile(rig.ib, extra_frames=10))))
    steps += [(t + 3.4, rig.gone('the grabbed escaped on every world')), (t + 3.4, rig.reason('Escape')),
              (t + 3.4, run.at('the mash end shrank with the presses', lambda: (min(rig.ends[0]) < max(rig.ends[0]) if rig.ends[0] else False, f'{rig.ends[0][:1]} -> {rig.ends[0][-1:]}'))),
              (t + 3.4, run.at('every world last showed the same mash end (the predicted escape frame)', lambda: (
                  len({v[-1] for v in rig.ends.values() if v}) == 1 and all(rig.ends.values()), {wi: v[-1:] for wi, v in rig.ends.items()}))),
              (t + 3.4, run.at('the watchdog broke it at that frame or within an engine frame after (reason Escape)', lambda: (
                  watchdog()[2] == REASON['Escape'] and rig.ends[0] and 0 <= watchdog()[3] - rig.ends[0][-1] <= 12, f'watchdog {watchdog()}, end {rig.ends[0][-1:]}')))]
    steps += rig.end(t + 3.5)
    run.run(steps)


def scenario_grab_interact(pairing='client_grabs_client'):
    """The interact button: a tap on a grab target grabs; a tap as the grabber releases (reason Release, the release breather on B, equal everywhere); a
    hold with nothing in reach forms nothing. The pick up rows of the button belong to the held-objects plugin: with it off, Command.Link.Pickup does not
    exist and the kit has no binding for it"""
    rig = _GrabRig('grab_interact', pairing)
    run = rig.run
    steps = [(0.2, rig.setup())]
    grab, t = rig.grab_steps()
    steps += grab
    steps += [(t + 0.3, rig.tap(rig.ia, 'Interact')), (t + 1.3, rig.gone('the second tap released B on every world')), (t + 1.3, rig.reason('Release')),
              (t + 1.3, rig.seen_equal('B carried the release breather, the same end frame on every world', 'immunity')),
              (t + 1.4, rig.place((450.0, 0.0))),
              (t + 2.0, lambda: ('hold', g.combat_hold(rig.ia, g.button_mask('Interact')), g.combat_tap(rig.ia, g.button_mask('Interact')))),
              (t + 2.6, lambda: ('release', g.combat_clear(rig.ia))),
              (t + 3.2, rig.gone('a hold with nothing in reach forms nothing')),
              (t + 3.2, run.at('without the held-objects plugin there is no pick up command and no binding for it', lambda: (
                  not _tag_exists('Command.Link.Pickup') and not any('Pickup' in g._tag_text(b.get_editor_property('command')) for b in _kit().get_editor_property('kit_bindings')),
                  f"tag {_tag_exists('Command.Link.Pickup')}")))]
    steps += rig.end(t + 3.3)
    run.run(steps)


def scenario_grab_gate(pairing='client_grabs_client'):
    """The grab gate of a heavy enemy (M4b P9, the deferred scenario_part_grabbable). Weight: with the kit's MaxGrabWeight at 50 the standard fighter (100)
    is not a grab target (no Interact.Target.Grab, the tap forms nothing); at 120 it is grabbed. Parts: with GSTNParts loaded B carries a part set whose grab
    gate needs ArmL broken: Combat.Part.GrabLocked is projected, the tap forms nothing; three hits break the arm, the tag goes and the tap grabs. Without
    GSTNParts the tag does not exist and the weight decides alone"""
    import parts_scenarios as ps
    parts = _tag_exists('Combat.Part.GrabLocked')
    rig = _GrabRig('grab_gate', pairing)
    run = rig.run
    kit = _kit()

    def weights(value):
        return lambda: ('max grab weight', value, Lib.set_kit_link_weights(kit, value, -1.0))

    steps = [(0.2, rig.setup()), (0.4, weights(50.0)), (1.2, rig.place()), (1.8, rig.tap(rig.ia, 'Interact')),
             (2.6, run.at('above MaxGrabWeight (50): no grab target, nothing forms', lambda: (
                 not has_tag(0, rig.akey, 'Interact.Target.Grab') and all(body(wi, rig.bkey) is None or body(wi, rig.bkey)[1] == 0 for wi in range(len(g.worlds()))),
                 {wi: body(wi, rig.bkey) for wi in range(len(g.worlds()))}))),
             (2.7, weights(120.0))]
    grab, t = rig.grab_steps(2.8)
    steps += grab
    steps += [(t, lambda: run.note(f'watchdog after the weight grab: {watchdog()}, seen {rig.seen[0]}')),
              (t + 0.2, rig.tap(rig.ia, 'Interact')), (t + 1.0, rig.gone('released before the parts half'))]
    t += 3.0
    if parts:
        part_set = {}

        def gate_on():
            p = ps.parts_setup('ArmL', 12.0, 0)
            gate = p.get_editor_property('grab_gate')
            gate.set_editor_property('required_parts', [ps.PART_INDEX['ArmL']])
            p.set_editor_property('grab_gate', gate)
            ps.attach_everywhere(p)
            part_set['p'] = p
            g.hit_rules_setup()
            return 'grab gate: ArmL'

        def gate_off():
            p = part_set.get('p')
            if p:
                gate = p.get_editor_property('grab_gate')
                gate.set_editor_property('required_parts', [])
                p.set_editor_property('grab_gate', gate)
            return ps.parts_reset()

        steps += [(t, gate_on), (t + 1.0, rig.place()),
                  (t + 1.6, run.at('GrabLocked is projected while the arm is whole, on the authority', lambda: (has_tag(0, rig.bkey, 'Combat.Part.GrabLocked'), 'tag'))),
                  (t + 1.7, rig.tap(rig.ia, 'Interact')),
                  (t + 2.5, run.at('GrabLocked: the tap forms nothing', lambda: (all(body(wi, rig.bkey) is None or body(wi, rig.bkey)[1] == 0 for wi in range(len(g.worlds()))),
                                                                              {wi: body(wi, rig.bkey) for wi in range(len(g.worlds()))})))]
        t += 2.7
        for i in range(3):
            steps += [(t, rig.place((205.0, 0.0))), (t + 1.6, rig.tap(rig.ia, 'Attack'))]
            t += 3.6
        steps.append((t, run.at('three hits broke the arm: GrabLocked is gone', lambda: (not has_tag(0, rig.bkey, 'Combat.Part.GrabLocked'),
                                                                                        ps.parts_state(0, rig.bkey)))))

        def body_boxes():
            # The parts content put every hurtbox on the arm; the arm is broken and hidden now, so the standard boxes come back for the grab (the
            # broken mask, which the gate reads, stays). parts_reset puts the same list back
            unreal.load_asset(ps.PARTS_DIR + 'HBS_Test_Pawn').set_editor_property('hurtboxes', ps._ps_saved['hbs'])
            return 'standard hurtboxes back'
        steps.append((t + 0.05, body_boxes))
        grab, t = rig.grab_steps(t + 0.1)
        steps += grab
        steps += [(t + 0.2, rig.tap(rig.ia, 'Interact')), (t + 1.0, gate_off)]
        t += 1.2
    else:
        steps.append((t, run.at('without GSTNParts Combat.Part.GrabLocked does not exist: the weight decides alone', lambda: (
            not _tag_exists('Combat.Part.GrabLocked'), 'tag absent'))))
        t += 0.2
    steps += [(t, weights(120.0))]
    steps += rig.end(t + 0.1)
    run.run(steps)


def scenario_throw_at_enemy(pairing='client_grabs_client'):
    """A throws B toward C (the third player, 260 cm ahead of A). With the held-objects plugin off the throw is the plain M6 launch: B gets a launch
    reaction (the harness pawns have no knockback move effect, so the reaction is read, not a displacement); C takes no damage from the thrown body
    (there is no thrown-body hitbox)"""
    rig = _GrabRig('throw_at_enemy', pairing)
    run = rig.run
    ckey = g.key_of_player(rig.others[0])
    held = _tag_exists('Combat.State.Thrown')
    steps = [(0.2, rig.setup())]
    grab, t = rig.grab_steps()
    steps += grab
    pos = {}

    def where(label):
        def f():
            pos[label] = (actor(0, rig.akey).get_actor_location(), actor(0, rig.bkey).get_actor_location(), g.damage_of(0, ckey))
            return 'where', label
        return f

    steps += [(t, rig.place((260.0, 0.0), key=ckey)), (t + 0.6, where('before')), (t + 0.7, rig.throw()), (t + 1.0, rig.unhold()), (t + 2.4, where('after')),
              (t + 2.4, rig.gone('the throw ended the link')),
              # The harness pawns carry no knockback move effect, so a launch shows as the victim's pending reaction, not as a displacement
              (t + 2.45, run.at('the throw launched B (pending reaction Launch on the authority, the plain M6 throw)', lambda: (
                  2 in rig.reactions, f"reactions {sorted(rig.reactions)}, distance {(pos['before'][1] - pos['before'][0]).length():.0f} -> {(pos['after'][1] - pos['after'][0]).length():.0f} cm"))),
              (t + 2.45, run.at('the held-objects plugin is off: no thrown body, C takes no damage from B', lambda: (
                  not held and abs(pos['after'][2] - pos['before'][2]) < 0.01, f"held plugin {held}, C damage {pos['before'][2]} -> {pos['after'][2]}")))]
    steps += rig.end(t + 2.5)
    run.run(steps)


_probe = {'rows': [], 'mark': None, 'h': None, 't0': 0.0}


def probe_throw(pairing='client_grabs_client'):
    """Debug aid (not a scenario): grab, throw (Crouch + Attack) and sample the thrown pawn on the server every editor tick: (t, location, velocity,
    Mover mode, Body link, action state, damage). Read probe_rows() after about 5 s"""
    ia, ib = PAIRINGS[pairing]
    b = g.key_of_player(ib)
    grab_content_setup([g.key_of_player(i) for i in (0, 1, 2)])
    _probe.update(rows=[], mark=None)

    def tick(_dt):
        try:
            act = actor(0, b)
            mv = act.get_component_by_class(unreal.MoverComponent)
            v, l = mv.get_velocity(), act.get_actor_location()
            e = json.loads(g.comps(g.worlds()[0])[b].read_snapshot_entry(-1))
            _probe['rows'].append((round(time.time() - _probe['t0'], 2), round(l.x), round(l.y), round(l.z), round(v.x), round(v.y), round(v.z),
                                   str(mv.get_movement_mode_name()), body(0, b), g.victim_info(0, b), e.get('damagePercent')))
        except Exception as ex:
            _probe['rows'].append(str(ex)[:80])

    def start():
        _probe['t0'] = time.time()
        _probe['h'] = unreal.register_slate_post_tick_callback(tick)
        return 'sampling'

    def throw():
        _probe['mark'] = len(_probe['rows'])
        g.combat_hold(ia, g.button_mask('Crouch'))
        g.combat_tap(ia, g.button_mask('Attack'))
        return 'throw'

    def stop():
        g.combat_clear(ia)
        unreal.unregister_slate_post_tick_callback(_probe['h'])
        return 'stopped'

    g.run_timeline([(0.8, lambda: g._place_near(ia, b, (90.0, 0.0))), (1.4, start), (1.5, lambda: g.combat_tap(ia, g.button_mask('Interact'))),
                    (2.4, throw), (2.6, lambda: g.combat_clear(ia)), (4.6, stop)])
    return 'probe started'


def probe_rows(before=2, after=40):
    m = _probe['mark'] or 0
    return _probe['rows'][max(m - before, 0): m + after]


def probe_hit_after_release(pairing='client_grabs_client', release=True):
    """Debug aid (not a scenario): A hits B with the test move (B not linked), then grabs and (release=True) lets go, then hits B again. B sampled on the
    server as in probe_throw: does a hit after a link still launch? probe_rows() afterwards; _probe['marks'] holds the row index of each hit"""
    ia, ib = PAIRINGS[pairing]
    b = g.key_of_player(ib)
    grab_content_setup([g.key_of_player(i) for i in (0, 1, 2)])
    _probe.update(rows=[], mark=None, marks={})

    def tick(_dt):
        try:
            act = actor(0, b)
            mv = act.get_component_by_class(unreal.MoverComponent)
            v, l = mv.get_velocity(), act.get_actor_location()
            _probe['rows'].append((round(time.time() - _probe['t0'], 2), round(l.x), round(l.y), round(l.z), round(v.x), round(v.y), round(v.z),
                                   str(mv.get_movement_mode_name()), body(0, b), g.victim_info(0, b)))
        except Exception as ex:
            _probe['rows'].append(str(ex)[:80])

    def start():
        _probe['t0'] = time.time()
        _probe['h'] = unreal.register_slate_post_tick_callback(tick)
        return 'sampling'

    def hit(name):
        def f():
            _probe['marks'][name] = len(_probe['rows'])
            return g.combat_tap(ia, g.button_mask('Attack'))
        return f

    def stop():
        unreal.unregister_slate_post_tick_callback(_probe['h'])
        return 'stopped'

    steps = [(0.5, start), (0.6, lambda: g._place_near(ia, b, (205.0, 0.0))), (1.4, hit('first')),
             (4.0, lambda: g._place_near(ia, b, (90.0, 0.0))), (4.8, lambda: g.combat_tap(ia, g.button_mask('Interact')))]
    if release:
        steps.append((5.6, lambda: g.combat_tap(ia, g.button_mask('Interact'))))
    steps += [(7.0, lambda: g._place_near(ia, b, (205.0, 0.0))), (7.8, hit('second')), (10.0, stop)]
    g.run_timeline(steps)
    return 'probe started'


def probe_shield_target(client=1, victim=2):
    """Debug aid (not a scenario): the defence content, the link moves, the victim shielding in reach; then the grab probe's description on every world
    (UGSTN_LinkContentLibrary.describe_grab_probe) into _probe['describe']"""
    import defence_scenarios as ds
    a, b = g.key_of_player(client), g.key_of_player(victim)
    _probe['describe'] = {}

    def setup():
        g.hit_rules_setup()
        ds.defence_setup()
        return grab_content_setup([a, b])

    def describe(label):
        def f():
            _probe['describe'][label] = {wi: Lib.describe_grab_probe(actor(wi, a)) for wi in range(len(g.worlds()))}
            return 'described', label
        return f

    g.run_timeline([(0.2, setup), (1.2, lambda: g._place_near(client, b, (90.0, 0.0))), (1.9, describe('idle')),
                    (2.0, lambda: (g.combat_hold(victim, g.button_mask('Shield')), g.combat_tap(victim, g.button_mask('Shield')))),
                    (2.6, describe('shielding')), (2.8, lambda: g.combat_clear(victim)), (3.4, describe('after')), (3.5, ds.defence_reset)])
    return 'probe started'
