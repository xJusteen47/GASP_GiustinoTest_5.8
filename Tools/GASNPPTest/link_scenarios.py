# M6 F10: the link scenarios (SUITE_LINK, not in SUITE_DEFAULT). The S1 link spike as a permanent scenario (a server-side test link, the
# grabber walking, dashing and circling for 20 s, forced reconciles inside the link) and the formation rollback scenario (a real grab hit through
# the LinkRequest step, forced reconciles on each client over the formation frames), each in three pairings: host grabs client, client grabs host,
# client grabs client. Every scenario starts in a FRESH PIE: run them with defence_scenarios.defence_batch(module=link_scenarios).
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
SUITE_LINK = [f'link_spike_{p}' for p in PAIRINGS] + [f'link_rollback_{p}' for p in PAIRINGS]
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
        return 'break', Lib.break_test_link(actor(0, rig.bkey))

    def restore():
        rig.stop()
        g._hit_logging(False)
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
              (T + 2.1, brk), (T + 3.2, _check(run, 'the link is gone on every world', rig.none_show)),
              (T + 3.3, restore), (T + 3.6, run.finish())]
    run.run(steps)


for _p in PAIRINGS:
    globals()[f'scenario_link_spike_{_p}'] = (lambda p: (lambda: _spike(p)))(_p)
    globals()[f'scenario_link_rollback_{_p}'] = (lambda p: (lambda: _rollback(p)))(_p)


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
