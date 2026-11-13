# M7 G5: the mount scenarios (GSTNPiggyback; not in SUITE_DEFAULT). Every scenario starts in a FRESH PIE (listen host + 2 clients, one process):
#   import defence_scenarios as ds, mount_scenarios as ms
#   ms.prepare_editor()                          # once, before the first PIE: the plugin's test kits exist (created when missing) and are scanned
#   ds.defence_batch(ms.SUITE_MOUNT, module=ms, report='m7-g5/mount-listen-0pct.log')
#
# Content: the plugin's own test kits /GSTNPiggyback/Test/KIT_Test_Mount (rider weight 100, capacity 150) and KIT_Test_MountHeavy (rider weight
# 200: no carrier of the test kits can carry it), copies of the project's link test kit KIT_Test_Link (its slots, moves and grab extension) with a
# mount kit extension; the mount's three bindings come from GSTNPiggyback's loadout extension. The mount type itself is made by the plugin's code.
# Teams are the harness knob SetTestTeam: rider and carrier team 1, the third pawn team 2 (an enemy).
#
# Scenarios (pairing = rider, carrier; the third pawn is the other player):
#   mount_basic          the rider taps Interact next to an ally: the mount forms on every world (rider role 1, carrier role 2, one serial), the
#                        rider is parked; no rollback on the rider's client (optimistic accept), the carrier's client corrections are counted (S3)
#   mount_refused        the server links the carrier to the third pawn (a debug-formed grab) on the very tick the rider taps: the rider's client
#                        does not see it yet, accepts optimistically, and the authority refuses: the rider's client rolls back (1 to 4; 3 measured, S3), the
#                        carrier never shows the mount; the rider's visible snap is measured (S3). Then the heavy kit: no mount meaning, no request
#   dismount_drop        mount, dismount (rider tap: reason Dismount, the rider places its own exit off the carrier, G6), mount again,
#                        drop (carrier: Crouch held + Interact: reason Drop, the exit server-given)
#   dismount_predicted   M7 G6: mount, the carrier walks then runs, the rider dismounts while it moves: the rider's client predicts the break
#                        and its own exit (no rollback on it from the tap to a second later), the watchdog copied that exit, and once the rider
#                        stands its own client and the server agree within 5 cm
#   mount_interact_priority  an enemy and an ally both in reach: the probe projects Interact.Target.Grab, the tap grabs the enemy (INT.5)
#   mounted_hit          M7 G7: the third pawn hits the mounted rider from behind. Below the rider's break threshold the hit lands (the server's
#                        inbox of the rider has it) and the mount holds on every world; then the threshold is lowered IN MEMORY on the code-made
#                        mount type (the test move's knockback is far below the default 3600 cm/s) and the same hit breaks it with a plugin reason
#   rider_attack         M7 G7: the mounted rider attacks the enemy ahead of its carrier: the enemy's server inbox has the hit, the carrier (its
#                        partner) never receives one, the mount holds. NOT in SUITE_MOUNT: while riding the Grounded attack binding does not hold
#                        and no kit has the Role.Rider attack binding of spec 9.2 yet, so the rider makes no hit (M7-06 run: empty hit log)
#   throw_rider          M7 G9: the carrier's neutral special throws the rider (PM_ThrowRider, plugin test content): the mount breaks at the
#                        release on every world, the rider flies off along the carrier's forward with the exit launch; spike 4 is measured
#   boost_jump           M7 G9: the mounted rider's jump boosts it off the carrier (PM_BoostJump): the carrier braces (Combat.Lock.Movement) on
#                        the server and its own world, the rider rises well above its parked height; spike 4 is measured
#   mount_forced_reconcile  M7 G12: forced reconciles on both clients around the mount (request tick, then spans that reach back over the
#                        Pending and first Active frames) and around a throw (pair-move start, then a span over its outcome/release frame).
#                        The forced resimulations must change nothing: one mount with one serial everywhere, the throw ends it everywhere,
#                        each client's own pawn equals the server's after settle. np.ForceReconcile is one global cvar under one process, so
#                        the client that takes each force is noted, not gated; the frames are hit through ForceReconcileExtraFrames spans
#   Spike 4 (spec 2.4): the partner's visible reaction on the partner's own screen, against the same event on the initiator's screen (ms; one
#   PIE editor tick of jitter). Above 150 ms = open a forward-prediction milestone for pair partners
import json
import math
import os
import time

import unreal
import gasnpp_test as g
import link_scenarios as ls

TEST_DIR = '/GSTNPiggyback/Test/'
KIT_MOUNT = 'KIT_Test_Mount'
KIT_HEAVY = 'KIT_Test_MountHeavy'
SOURCE_KIT = '/Game/GASNPPTest/Combat/KIT_Test_Link'
PLib = unreal.GSTN_PiggybackContentLibrary
MOUNT_TARGET = 'Interact.Target.Mount'
GRAB_TARGET = 'Interact.Target.Grab'
FIRST_PLUGIN_REASON = 16
PAIRINGS = {'client_rides_client': (1, 2), 'client_rides_host': (1, 0), 'host_rides_client': (0, 1)}
SUITE_MOUNT = ['mount_basic', 'mount_refused', 'mount_heavy_refused', 'dismount_drop', 'mount_interact_priority', 'dismount_predicted',
               'mounted_hit', 'throw_rider', 'boost_jump', 'mount_forced_reconcile',
               'link_pair_move_delay', 'link_pair_move_delay_throw']   # rider_attack waits for a Role.Rider attack binding: no kit has one yet (M7-06)
PAIR_SET = 'PMS_Test_Mount'
PAIR_MOVES = (('PM_ThrowRider', 'Combat.PairMove.ThrowRider'), ('PM_BoostJump', 'Combat.PairMove.BoostJump'))
LOCK_MOVEMENT = 'Combat.Lock.Movement'
SPIKE4_MS = 150.0
PREDICT_MS = 150.0   # FP G1: the initiator sees the partner react at most this long after its own action
_last = {'rig': None}


# ---- content -----------------------------------------------------------------------------------------------------------------------------
def _ensure_kit(name, rider_weight, capacity):
    path = TEST_DIR + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset(SOURCE_KIT, path)
    kit = unreal.load_asset(path)
    PLib.write_mount_kit_data(kit, float(rider_weight), float(capacity))
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=True)
    return name, PLib.get_mount_kit_data(kit)


def _ensure_data_asset(name, cls):
    path = TEST_DIR + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', cls)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, TEST_DIR.rstrip('/'), cls, factory)
    return unreal.load_asset(path)


def _ensure_pair_moves():
    """G9 test content: the two reference definitions and their set in the plugin's Content/Test, the set named by both test kits' mount data
    (the loadout then binds the initiators and makes the partners triggered abilities). Saved only when changed"""
    defs = []
    for name, tag in PAIR_MOVES:
        d = _ensure_data_asset(name, unreal.GSTN_PairMoveDefinition)
        PLib.fill_reference_pair_move(d, g._tag(tag))
        unreal.EditorAssetLibrary.save_asset(TEST_DIR + name, only_if_is_dirty=True)
        defs.append(d)
    pair_set = _ensure_data_asset(PAIR_SET, unreal.GSTN_PairMoveSet)
    PLib.write_pair_move_set(pair_set, defs)
    unreal.EditorAssetLibrary.save_asset(TEST_DIR + PAIR_SET, only_if_is_dirty=True)
    out = []
    for kit_name in (KIT_MOUNT, KIT_HEAVY):
        kit = unreal.load_asset(TEST_DIR + kit_name)
        out.append((kit_name, PLib.write_mount_kit_pair_moves(kit, pair_set)))
        unreal.EditorAssetLibrary.save_asset(TEST_DIR + kit_name, only_if_is_dirty=True)
    return 'pair moves', [n for n, _ in PAIR_MOVES], out


def prepare_editor():
    """The two test kits (created from KIT_Test_Link when missing, saved in the plugin's Content/Test), the G9 pair moves on them, the kit scan,
    the link content and the saved team rule on GSTNLink's test grab (setup_defence_link_content.py, G5). Before PIE"""
    out = [_ensure_kit(KIT_MOUNT, 100.0, 150.0), _ensure_kit(KIT_HEAVY, 200.0, 150.0), _ensure_pair_moves()]
    out.append(('scan', PLib.scan_plugin_kits()))
    out.append(('link', ls.prepare_editor()))
    out.append(('mount type index', PLib.get_mount_type_index()))
    return out


def restore_editor():
    return 'nothing to restore'


# ---- rig ---------------------------------------------------------------------------------------------------------------------------------
def predicted_move(comp):
    """FP G1: (active, move id, timeline frame) of the predicted partner move a presentation component shows (UGSTN_LinkPresentationComponent)"""
    r = comp.get_predicted_partner_move()
    if isinstance(r, tuple):
        return bool(r[0]), (int(r[1]) if len(r) > 1 else 0), (int(r[2]) if len(r) > 2 else 0)
    return bool(r), 0, 0


def predicted_events(wi, key):
    """FP G1: (started, ended, dropped) counts of the predicted partner move events on pawn `key`'s presentation component of world wi"""
    comp = ls.actor(wi, key).get_component_by_class(unreal.GSTN_LinkPresentationComponent)
    if comp is None:
        return (0, 0, 0)
    ev = unreal.GSTN_LinkPredictedPartnerEvent
    return tuple(int(comp.get_predicted_partner_event_count(e)) for e in (ev.STARTED, ev.ENDED, ev.DROPPED))


class _MountRig:
    """Rider (world ir's player), carrier (world ic's player) and the third pawn (the other player). Samples, every editor tick, the rider's shown
    position on its own world (its parked presentation while it shows one, else its actor) and both pawns' Body link on every world"""

    def __init__(self, name, pairing):
        self.ir, self.ic = PAIRINGS[pairing]
        self.io = [i for i in g.all_players() if i not in (self.ir, self.ic)][0]
        self.rkey, self.ckey, self.okey = g.key_of_player(self.ir), g.key_of_player(self.ic), g.key_of_player(self.io)
        self.run = g._Run(name, self.ir, judge_reconciles=False)
        at = self.run.at
        self.run.at = lambda label, fn: at(label, lambda: (lambda r: (r[0], str(r[1])))(fn()))
        self.marks, self.shown, self.seen = {}, [], {wi: set() for wi in range(len(g.worlds()))}
        self.watch_from, self.first = None, {}   # spike 4: (event, world) -> first time seen after watch_from
        self.partner_key = None   # FP G1: the pawn whose presentation component shows the predicted partner reaction (set by the scenario)
        self.kits = None
        self.handle = unreal.register_slate_post_tick_callback(self._tick)
        _last['rig'] = self

    def _tick(self, _dt):
        try:
            parked = ls.linked_location(self.ir, self.rkey)
            loc = parked if parked else tuple(g.snap(g.worlds()[self.ir])[self.rkey]['location'])
            self.shown.append((self.run.now(), loc, bool(parked)))
            for wi in self.seen:
                r, c = ls.body(wi, self.rkey), ls.body(wi, self.ckey)
                self.seen[wi].add(((r or (0,))[0], (c or (0,))[0]))
                if self.watch_from is not None:
                    now = self.run.now()
                    if ('unparked', wi) not in self.first and ls.mover_mode(wi, self.rkey) != ls.LINKED_MODE:
                        self.first[('unparked', wi)] = now
                    if self.partner_key is not None and wi > 0 and ('predicted', wi) not in self.first:
                        comp = ls.actor(wi, self.partner_key).get_component_by_class(unreal.GSTN_LinkPresentationComponent)
                        if comp is not None and predicted_move(comp)[0]:
                            self.first[('predicted', wi)] = now
                    locked = ls.has_tag(wi, self.ckey, LOCK_MOVEMENT)
                    if ('braced', wi) not in self.first and locked:
                        self.first[('braced', wi)] = now
                    if ('braced', wi) in self.first and ('unbraced', wi) not in self.first and not locked:
                        self.first[('unbraced', wi)] = now
        except Exception:
            pass

    def stop(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None

    def setup(self, rider_kit=KIT_MOUNT):
        def f():
            ls.link_setup()
            self.kits = [g._apply_loadout(self.rkey, rider_kit), g._apply_loadout(self.ckey, KIT_MOUNT), g._apply_loadout(self.okey, KIT_MOUNT)]
            g._team_everywhere([self.rkey, self.ckey], 1)
            g._team_everywhere([self.okey], 2)
            return 'kits', self.kits, 'teams 1, 1, 2'
        return f

    def kits_ok(self):
        return self.run.at('the kits applied', lambda: (self.kits is not None and all(k == 'OK' for k in self.kits), f'{self.kits}'))

    def place(self, key, spot):
        return lambda: ('placed', key, spot, g._place_near(self.ir, key, spot))

    def clear_other(self):
        return self.place(self.okey, (-350.0, 400.0))

    def tap(self, wi, button='Interact'):
        return lambda: ('tap', wi, button, g.combat_tap(wi, g.button_mask(button)))

    def mark(self, name):
        def f():
            self.marks[name] = {'rb': {i: g.rollbacks(i) for i in g.all_players() if i > 0}, 't': self.run.now(), 'watchdog': ls.watchdog(),
                                'pos': {k: tuple(g.snap(g.worlds()[0])[k]['location']) for k in (self.rkey, self.ckey)}}
            return 'mark', name, self.marks[name]
        return f

    def rb_delta(self, a, b, wi):
        if wi <= 0:
            return 0   # the host's own pawn never rolls back
        return self.marks[b]['rb'][wi] - self.marks[a]['rb'][wi]

    def target(self, name, wi=None):
        wi = self.ir if wi is None else wi
        return self.run.at(f'the rider projects {name} on its own world and the server', lambda: (
            ls.has_tag(wi, self.rkey, name) and ls.has_tag(0, self.rkey, name),
            {w: (ls.has_tag(w, self.rkey, MOUNT_TARGET), ls.has_tag(w, self.rkey, GRAB_TARGET)) for w in range(len(g.worlds()))}))

    def mounted(self, label='mounted on every world: rider role 1, carrier role 2, Active, one serial and link frame'):
        def f():
            rows = {wi: (ls.body(wi, self.rkey), ls.body(wi, self.ckey)) for wi in range(len(g.worlds()))}
            ok = all(r and c and r[0] == 1 and c[0] == 2 and r[1] == 2 and c[1] == 2 for r, c in rows.values())
            keys = {(r[2], r[3], c[2], c[3]) for r, c in rows.values() if r and c}
            return ok and len(keys) == 1, f'{rows}'
        return self.run.at(label, f)

    def parked(self):
        return self.run.at('the rider is in the parked mode where it is simulated (server and its own client)', lambda: (
            all(ls.mover_mode(wi, self.rkey) == ls.LINKED_MODE for wi in {0, self.ir}),
            {wi: ls.mover_mode(wi, self.rkey) for wi in range(len(g.worlds()))}))

    def gone(self, label):
        def f():
            rows = {wi: (ls.body(wi, self.rkey), ls.body(wi, self.ckey)) for wi in range(len(g.worlds()))}
            return all((r is None or r[1] == 0) and (c is None or c[1] == 0) for r, c in rows.values()), f'{rows}'
        return self.run.at(label, f)

    def snap_cm(self, t_from, t_to):
        """Largest jump of the rider's shown position on its own world between two editor ticks in [t_from, t_to] (cm)"""
        pts = [(t, p) for t, p, _ in self.shown if t_from <= t <= t_to]
        return max((math.dist(a[1], b[1]) for a, b in zip(pts, pts[1:])), default=0.0)

    def end(self, t):
        def restore():
            self.stop()
            return ls.link_reset()
        return [(t, self.kits_ok()), (t + 0.05, restore), (t + 0.2, self.run.finish())]

    def mount_steps(self, t0, name='mount'):
        """Carrier 90 cm ahead of the rider, the mount meaning checked, a tap; formation checked 1 s later. Returns (steps, time after)"""
        return [(t0, self.place(self.ckey, (90.0, 0.0))), (t0 + 0.6, self.target(MOUNT_TARGET)), (t0 + 0.65, self.mark('pre_' + name)),
                (t0 + 0.7, self.tap(self.ir)), (t0 + 1.7, self.mark(name)), (t0 + 1.7, self.mounted()), (t0 + 1.7, self.parked())], t0 + 1.8


# ---- scenarios -----------------------------------------------------------------------------------------------------------------------------
def scenario_mount_basic(pairing='client_rides_client'):
    rig = _MountRig('mount_basic', pairing)
    run = rig.run
    st = {}

    def payload(name):
        def f():
            st[name] = {k: ls.Lib.get_shown_link_payload_bytes(ls.actor(0, k)) for k in (rig.rkey, rig.ckey, rig.okey)}
            return 'link payload bytes', name, st[name]
        return f

    steps = [(0.2, rig.setup()), (1.1, rig.clear_other()), (1.15, payload('idle'))]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t, payload('mounted')),
              (t, lambda: run.note(f"G12 byte table: link fragment payload per sync state (server), idle {st.get('idle')}, mounted "
                                   f"{st.get('mounted')} (rider, carrier, third pawn; B; bytes/s not measurable from Python)")),
              (t, run.at('the unlinked third pawn sends the same link bytes as before the mount', lambda: (
                  st['mounted'][rig.okey] == st['idle'][rig.okey], f"{st['idle'][rig.okey]} -> {st['mounted'][rig.okey]}"))),
              (t, run.at('no rollback on the rider\'s client for an accepted mount (optimistic accept)', lambda: (
                  rig.rb_delta('pre_mount', 'mount', rig.ir) == 0, f"rider client rollbacks {rig.rb_delta('pre_mount', 'mount', rig.ir)}"))),
              (t, lambda: run.note(f"S3 carrier's client corrections on acceptance: {rig.rb_delta('pre_mount', 'mount', rig.ic)} (world {rig.ic}); "
                                   f"rider's {rig.rb_delta('pre_mount', 'mount', rig.ir)}; snap on the rider's screen at the attach "
                                   f"{rig.snap_cm(rig.marks['pre_mount']['t'], rig.marks['mount']['t']):.1f} cm")),
              (t, run.at('the carrier keeps acting: not parked', lambda: (ls.mover_mode(0, rig.ckey) != ls.LINKED_MODE, ls.mover_mode(0, rig.ckey))))]
    steps += rig.end(t + 0.1)
    run.run(steps)


def scenario_mount_refused(pairing='client_rides_client'):
    rig = _MountRig('mount_refused', pairing)
    run = rig.run
    st = {}

    def busy_and_tap():
        # Same editor tick: the server links the carrier (grabbed by the third pawn), the rider's client presses before it can know
        ok = ls.Lib.form_test_link(ls.actor(0, rig.okey), ls.actor(0, rig.ckey), g._tag(ls.GRAB))
        st['formed'] = ok
        return 'carrier linked on the server', ok, rig.tap(rig.ir)()

    # The third pawn far out of every reach (the debug link needs no distance), and a second for every copy to show the new places
    steps = [(0.2, rig.setup()), (1.1, rig.place(rig.okey, (-350.0, 450.0))), (1.2, rig.place(rig.ckey, (90.0, 0.0))),
             (2.2, rig.target(MOUNT_TARGET)), (2.25, rig.mark('pre')), (2.3, busy_and_tap),
             (2.4, rig.mark('pending')), (3.8, rig.mark('after')),
             (3.8, run.at('the debug link of the carrier formed on the server', lambda: (bool(st.get('formed')), st))),
             (3.8, run.at('the rider is not mounted on any world', lambda: (
                 all((ls.body(wi, rig.rkey) or (0, 0))[1] == 0 for wi in range(len(g.worlds()))), {wi: ls.body(wi, rig.rkey) for wi in range(len(g.worlds()))}))),
             (3.8, run.at('the unseen refusal cost the rider client 1 to 4 rollbacks (plan: exactly one; S3 records the number)', lambda: (
                 1 <= rig.rb_delta('pre', 'after', rig.ir) <= 4, f"rider client rollbacks {rig.rb_delta('pre', 'after', rig.ir)}"))),
             (3.8, run.at('the carrier never showed the carrier role (role 2 of the mount = only the grab\'s role 2 while held)', lambda: (
                 not any(pair[0] == 1 and pair[1] == 2 for wi in (0, rig.ic) for pair in rig.seen[wi]),
                 {wi: sorted(v) for wi, v in rig.seen.items()}))),
             (3.8, run.at('the rider showed itself mounted on its own screen before the refusal (optimistic accept)', lambda: (
                 any(p for t, _, p in rig.shown if rig.marks['pre']['t'] <= t <= rig.marks['after']['t']), 'parked samples seen'))),
             (3.85, lambda: run.note(f"S3 refused mount: rider client rollbacks {rig.rb_delta('pre', 'after', rig.ir)}, visible snap "
                                     f"{rig.snap_cm(rig.marks['pre']['t'], rig.marks['after']['t']):.1f} cm (largest per-tick jump of the rider's shown "
                                     f"position on its own screen), parked ticks {sum(1 for t, _, p in rig.shown if p and rig.marks['pre']['t'] <= t <= rig.marks['after']['t'])}"))]
    steps += rig.end(4.0)
    run.run(steps)


def scenario_mount_heavy_refused(pairing='client_rides_client'):
    """KIT_Test_MountHeavy on the rider: no carrier of the test kits can carry it, so its probe never offers the mount and a tap forms nothing"""
    rig = _MountRig('mount_heavy_refused', pairing)
    run = rig.run
    steps = [(0.2, rig.setup(KIT_HEAVY)), (1.1, rig.clear_other()), (1.2, rig.place(rig.ckey, (90.0, 0.0))),
             (1.8, run.at('the heavy rider is offered no mount', lambda: (not ls.has_tag(rig.ir, rig.rkey, MOUNT_TARGET) and not ls.has_tag(0, rig.rkey, MOUNT_TARGET), 'tags'))),
             (1.85, rig.mark('pre')), (1.9, rig.tap(rig.ir)), (3.0, rig.mark('after')),
             (3.0, rig.gone('nothing formed on any world')),
             (3.0, run.at('the rider never showed itself mounted on its own screen (nothing to roll back)', lambda: (
                 not any(p for t, _, p in rig.shown if t >= rig.marks['pre']['t']), f"rider client rollbacks {rig.rb_delta('pre', 'after', rig.ir)} (any cause)")))]
    steps += rig.end(3.1)
    run.run(steps)


def scenario_dismount_drop(pairing='client_rides_client'):
    rig = _MountRig('dismount_drop', pairing)
    run = rig.run
    st = {}

    def reason(name, other=None):
        def f():
            w = ls.watchdog()
            st[name] = w[2]
            ok = w[2] >= FIRST_PLUGIN_REASON and (other is None or w[2] != st.get(other))
            return ok, f'watchdog {w} (plugin reasons from {FIRST_PLUGIN_REASON}), earlier {st}'
        return run.at(f'the watchdog broke it with a plugin reason ({name})', f)

    def exit_check():
        r, c = g.snap(g.worlds()[0])[rig.rkey], g.snap(g.worlds()[0])[rig.ckey]
        d = math.dist(r['location'][:2], c['location'][:2])
        mine = g.snap(g.worlds()[rig.ir])[rig.rkey]['location']
        err = math.dist(mine, r['location'])
        run.note(f"dismount exit (rider-placed, G6): rider {d:.1f} cm from the carrier on the server; rider's own client vs server {err:.1f} cm")
        return 60.0 <= d <= 200.0 and ls.mover_mode(0, rig.rkey) != ls.LINKED_MODE, f'distance {d:.1f}, client-server {err:.1f}'

    steps = [(0.2, rig.setup()), (1.1, rig.clear_other())]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t + 0.3, rig.mark('pre_dismount')), (t + 0.35, rig.tap(rig.ir)), (t + 1.4, rig.mark('dismounted')),
              (t + 1.4, rig.gone('the dismount ended the link on every world')), (t + 1.4, reason('Dismount')),
              (t + 1.45, run.at('the rider stands off the carrier', exit_check)),
              (t + 1.45, lambda: run.note(f"dismount: rider client rollbacks {rig.rb_delta('pre_dismount', 'dismounted', rig.ir)}, "
                                          f"visible snap {rig.snap_cm(rig.marks['pre_dismount']['t'], rig.marks['dismounted']['t']):.1f} cm"))]
    mount2, t2 = rig.mount_steps(t + 1.8, 'mount2')
    steps += mount2
    hold = lambda: ('drop', g.combat_hold(rig.ic, g.button_mask('Crouch')), g.combat_tap(rig.ic, g.button_mask('Interact')))
    steps += [(t2 + 0.3, hold), (t2 + 0.7, lambda: ('clear', g.combat_clear(rig.ic))), (t2 + 1.4, rig.gone('the drop ended the link on every world')),
              (t2 + 1.4, reason('Drop', 'Dismount')),
              (t2 + 1.4, run.at('the rider left the parked mode everywhere', lambda: (
                  all(ls.mover_mode(wi, rig.rkey) != ls.LINKED_MODE for wi in range(len(g.worlds()))), {wi: ls.mover_mode(wi, rig.rkey) for wi in range(len(g.worlds()))})))]
    steps += rig.end(t2 + 1.5)
    run.run(steps)


def scenario_dismount_predicted(pairing='client_rides_client'):
    """M7 G6: the rider dismounts while the carrier walks then runs. The rider's client predicts the break and the exit from its view of the
    carrier; the server places the same exit from the carrier rewound to that view and the watchdog copies it: no rollback on the rider's client"""
    rig = _MountRig('dismount_predicted', pairing)
    run = rig.run
    st = {}

    def walk(v):
        return lambda: ('carrier move', v, g.hold(rig.ic, 'move', v))

    def sprint(on):
        return lambda: ('carrier sprint', on, g.hold(rig.ic, 'sprint', (1.0, 0.0, 0.0)) if on else g.release(rig.ic, 'sprint'))

    def stop():
        return lambda: ('carrier stops', g.release(rig.ic, 'move'), g.release(rig.ic, 'sprint'))

    def carrier_moved():
        a, b = rig.marks['walking']['pos'][rig.ckey], rig.marks['pre_dismount']['pos'][rig.ckey]
        st['carrier_cm'] = math.dist(a[:2], b[:2])
        return st['carrier_cm'] > 50.0, f"carrier moved {st['carrier_cm']:.1f} cm between the marks on the server"

    def agree():
        server = g.snap(g.worlds()[0])[rig.rkey]['location']
        mine = g.snap(g.worlds()[rig.ir])[rig.rkey]['location']
        st['err'] = math.dist(mine, server)
        return st['err'] <= 5.0 and ls.mover_mode(rig.ir, rig.rkey) != ls.LINKED_MODE, f"rider's own client vs server {st['err']:.2f} cm"

    steps = [(0.2, rig.setup()), (1.1, rig.clear_other())]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t + 0.1, walk((0.0, 1.0, 0.0))), (t + 0.6, rig.mark('walking')), (t + 0.9, sprint(True)),
              (t + 1.5, rig.mark('pre_dismount')), (t + 1.55, rig.tap(rig.ir)), (t + 2.55, rig.mark('dismounted')),
              (t + 2.55, run.at('the carrier was moving when the rider left', carrier_moved)),
              (t + 2.55, rig.gone('the dismount ended the link on every world')),
              (t + 2.55, run.at('the watchdog broke it with the rider\'s reason', lambda: (
                  ls.watchdog()[2] >= FIRST_PLUGIN_REASON, f'watchdog {ls.watchdog()}'))),
              (t + 2.55, run.at('no rollback on the rider\'s client for its own dismount (predicted break and exit)', lambda: (
                  rig.rb_delta('pre_dismount', 'dismounted', rig.ir) == 0, f"rider client rollbacks {rig.rb_delta('pre_dismount', 'dismounted', rig.ir)}"))),
              (t + 2.6, stop()),
              (t + 3.4, run.at('the rider stands where the authority has it, within 5 cm, on its own client', agree)),
              (t + 3.45, lambda: run.note(f"G6 dismount while the carrier moved {st.get('carrier_cm', 0):.0f} cm in 0.9 s: rider client rollbacks "
                                          f"{rig.rb_delta('pre_dismount', 'dismounted', rig.ir)}, carrier client {rig.rb_delta('pre_dismount', 'dismounted', rig.ic)}, "
                                          f"visible snap {rig.snap_cm(rig.marks['pre_dismount']['t'], rig.marks['dismounted']['t']):.1f} cm, "
                                          f"client-server {st.get('err', -1):.2f} cm"))]
    steps += rig.end(t + 3.5)
    run.run(steps)


def scenario_mount_interact_priority(pairing='client_rides_client'):
    """An enemy and an ally in reach, symmetric about the rider's facing: the probe projects the grab (priority 100 over the mount's 50), the tap
    grabs the enemy; the ally is not mounted"""
    rig = _MountRig('mount_interact_priority', pairing)
    run = rig.run
    steps = [(0.2, rig.setup()), (1.1, rig.place(rig.okey, (90.0, 35.0))), (1.2, rig.place(rig.ckey, (90.0, -35.0))),
             (1.8, rig.target(GRAB_TARGET)),
             (1.8, run.at('and not the mount', lambda: (not ls.has_tag(rig.ir, rig.rkey, MOUNT_TARGET), 'tag'))),
             (1.85, rig.tap(rig.ir)),
             (2.9, run.at('the enemy is grabbed (parked on the server), the rider is not parked', lambda: (
                 ls.mover_mode(0, rig.okey) == ls.LINKED_MODE and ls.mover_mode(0, rig.rkey) != ls.LINKED_MODE,
                 {k: ls.mover_mode(0, k) for k in (rig.rkey, rig.ckey, rig.okey)}))),
             (2.9, run.at('the ally carries no link', lambda: ((ls.body(0, rig.ckey) or (0, 0))[1] == 0, ls.body(0, rig.ckey))))]
    steps += rig.end(3.0)
    run.run(steps)


# ---- G7: hits while mounted ----------------------------------------------------------------------------------------------------------------
def set_mount_break(rider=None, carrier=None):
    """MinKnockbackToBreak of the code-made mount type's hit breaks, in memory (the step reads them on every hit). Returns the old values by role"""
    old = {}
    for d in unreal.ObjectIterator(unreal.GSTN_LinkTypeDefinition):
        if not d.get_name().startswith('DA_LinkType_Mount'):
            continue
        br = d.get_editor_property('break')
        rows = []
        for hb in br.get_editor_property('hit_breaks'):
            role = str(hb.get_editor_property('role_tag').get_editor_property('tag_name'))
            want = rider if role.endswith('Rider') else carrier if role.endswith('Carrier') else None
            old[role] = hb.get_editor_property('min_knockback_to_break')
            if want is not None:
                hb.set_editor_property('min_knockback_to_break', float(want))
            rows.append(hb)
        br.set_editor_property('hit_breaks', rows)
        d.set_editor_property('break', br)
    return old


def _place_at(key, xy):
    """Server teleport of pawn `key` to a world XY (the floor under it is found by the teleport)"""
    spawn = g.comps(g.worlds()[0])[key].get_spawn_location()
    g.comps(g.worlds()[0])[key].server_reset_to_spawn(unreal.Vector(xy[0] - spawn.x, xy[1] - spawn.y, 0.0))
    return xy


def _along(key, from_key, cm):
    """World XY `cm` along pawn key's own facing from from_key's server location (negative: behind it)"""
    s = g.snap(g.worlds()[0])
    yaw = math.radians(s[key]['yaw'])
    loc = s[from_key]['location']
    return loc[0] + cm * math.cos(yaw), loc[1] + cm * math.sin(yaw)


def _inbox_since(key, frame):
    return [r for r in g._inbox_log(0, key) if r[0] >= frame]


def scenario_mounted_hit(pairing='client_rides_client'):
    rig = _MountRig('mounted_hit', pairing)
    run = rig.run
    st = {}

    def behind():
        # The third pawn 80 cm short of the rider along its OWN facing: it faces the rider whatever the spawn yaws
        st['spot'] = _place_at(rig.okey, _along(rig.okey, rig.rkey, -80.0))
        return 'attacker behind the rider', st['spot']

    def attack(tag):
        def f():
            st['F' + tag] = g._latest_frame(0, rig.rkey)
            return 'attack', tag, st['F' + tag], g.combat_tap(rig.io, g.button_mask('Attack'))
        return f

    def landed(tag):
        def f():
            r, c = _inbox_since(rig.rkey, st['F' + tag]), _inbox_since(rig.ckey, st['F' + tag])
            st['in' + tag] = (r, c)
            return bool(r), f"rider inbox {r}, carrier inbox {c} (server, from frame {st['F' + tag]})"
        return run.at(f'the hit landed on the rider ({tag})', f)

    def lower():
        st['old'] = set_mount_break(rider=1.0)
        st['wd'] = ls.watchdog()
        return 'rider break threshold 1 cm/s', st['old']

    def broke():
        wd = ls.watchdog()
        st['reason'] = wd[2]
        return wd[0] == st['wd'][0] + 1 and wd[2] >= FIRST_PLUGIN_REASON, f"watchdog {st['wd']} -> {wd} (one break, plugin reason from {FIRST_PLUGIN_REASON})"

    def restore():
        g._hit_logging(False)
        old = {k.rsplit('.', 1)[-1]: v for k, v in st.get('old', {}).items()}
        return 'restored', set_mount_break(rider=old.get('Rider', 3600.0), carrier=old.get('Carrier', 1800.0))

    steps = [(0.2, rig.setup()), (0.25, lambda: ('hit log', g._hit_logging(True))), (1.1, rig.place(rig.okey, (-150.0, 250.0)))]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t, behind), (t + 0.8, rig.mark('pre_hit')), (t + 0.85, attack('A')), (t + 1.85, rig.mark('hit')),
              (t + 1.85, landed('A')),
              (t + 1.85, rig.mounted('below the break threshold the mount holds on every world')),
              (t + 1.85, rig.parked()),
              (t + 1.9, lambda: run.note(f"G7 rider hit below threshold: rider client rollbacks {rig.rb_delta('pre_hit', 'hit', rig.ir)}, carrier client "
                                         f"{rig.rb_delta('pre_hit', 'hit', rig.ic)}, rider shown snap {rig.snap_cm(rig.marks['pre_hit']['t'], rig.marks['hit']['t']):.1f} cm, "
                                         f"inboxes {st.get('inA')}")),
              (t + 2.4, behind), (t + 2.9, lower), (t + 3.0, rig.mark('pre_break')), (t + 3.05, attack('B')), (t + 4.05, rig.mark('broken')),
              (t + 4.05, landed('B')),
              (t + 4.05, rig.gone('above the threshold the hit broke the mount on every world')),
              (t + 4.05, run.at("the watchdog broke it once with the plugin's reason (RiderHit)", broke)),
              (t + 4.1, lambda: run.note(f"G7 rider hit above threshold: reason index {st.get('reason')}, rider client rollbacks "
                                         f"{rig.rb_delta('pre_break', 'broken', rig.ir)}, inboxes {st.get('inB')}")),
              (t + 4.15, restore)]
    steps += rig.end(t + 4.2)
    run.run(steps)


def scenario_rider_attack(pairing='client_rides_client'):
    rig = _MountRig('rider_attack', pairing)
    run = rig.run
    st = {}

    def ahead():
        st['spot'] = _place_at(rig.okey, _along(rig.rkey, rig.rkey, 110.0))
        return 'enemy ahead of the carrier', st['spot']

    def attack():
        st['F'] = g._latest_frame(0, rig.rkey)
        return 'rider attacks', st['F'], g.combat_tap(rig.ir, g.button_mask('Attack'))

    def landed():
        e, c = _inbox_since(rig.okey, st['F']), _inbox_since(rig.ckey, st['F'])
        hits = [r for r in g._hit_log(0, rig.rkey) if r[0] >= st['F']]
        st['rows'] = (hits, e, c)
        return bool(e), f'rider hit log {hits}; enemy inbox {e}; carrier inbox {c}'

    def partner_clean():
        return not st['rows'][2], f"carrier inbox {st['rows'][2]}"

    steps = [(0.2, rig.setup()), (0.25, lambda: ('hit log', g._hit_logging(True))), (1.1, rig.place(rig.okey, (-150.0, 250.0)))]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t, ahead), (t + 0.8, rig.mark('pre_attack')), (t + 0.85, attack), (t + 1.85, rig.mark('attacked')),
              (t + 1.85, run.at("the rider's attack landed on the enemy (server inbox)", landed)),
              (t + 1.85, run.at('the carrier, its partner, received no hit from it', partner_clean)),
              (t + 1.85, rig.mounted("the rider's attack keeps the mount on every world")),
              (t + 1.9, lambda: run.note(f"G7 rider attack: rider client rollbacks {rig.rb_delta('pre_attack', 'attacked', rig.ir)}, "
                                         f"carrier client {rig.rb_delta('pre_attack', 'attacked', rig.ic)}, rows {st.get('rows')}")),
              (t + 1.95, lambda: ('hit log', g._hit_logging(False)))]
    steps += rig.end(t + 2.0)
    run.run(steps)


# ---- G9: pair moves ----------------------------------------------------------------------------------------------------------------------
def _spike4(rig, event, initiator_wi, partner_wi, label, gate=True):
    """Spike 4: the partner's screen against the initiator's for the same event (ms); also from the initiator's tap"""
    def f():
        a, b = rig.first.get((event, initiator_wi)), rig.first.get((event, partner_wi))
        tap = rig.watch_from
        if a is None or b is None:
            rig.run.note(f'spike 4 {label}: event {event} not seen on both screens ({rig.first})')
            return not gate, f'{rig.first}'
        d = (b - a) * 1000.0
        rig.run.note(f'spike 4 {label}: {event} on the partner screen (world {partner_wi}) {d:.0f} ms after the initiator screen (world {initiator_wi}); '
                     f'from the tap: initiator {(a - tap) * 1000.0:.0f} ms, partner {(b - tap) * 1000.0:.0f} ms; threshold {SPIKE4_MS:.0f} ms')
        return True, f'{d:.0f} ms'
    return rig.run.at(f'spike 4 measured ({label})', f)


def scenario_throw_rider(pairing='client_rides_client'):
    """The carrier throws its rider (PM_ThrowRider): carrier-initiated, the exit server-given with the definition's launch (G9 part A)"""
    rig = _MountRig('throw_rider', pairing)
    run = rig.run
    st = {}

    def throw():
        st['start'] = {k: tuple(g.snap(g.worlds()[0])[k]['location']) for k in (rig.rkey, rig.ckey)}
        rig.first.clear()
        rig.watch_from = run.now()
        return 'carrier throws', g.combat_tap(rig.ic, g.button_mask('Special'))

    def flew():
        r, c = g.snap(g.worlds()[0])[rig.rkey]['location'], st['start'][rig.ckey]
        d = math.dist(r[:2], c[:2])
        return d >= 250.0 and ls.mover_mode(0, rig.rkey) != ls.LINKED_MODE, f'rider {d:.1f} cm from where the carrier threw (server)'

    def agree():
        mine, srv = g.snap(g.worlds()[rig.ir])[rig.rkey]['location'], g.snap(g.worlds()[0])[rig.rkey]['location']
        err = math.dist(mine, srv)
        run.note(f'throw: rider own client vs server after the flight {err:.1f} cm')
        return err <= 10.0, f'{err:.1f} cm'

    steps = [(0.2, rig.setup()), (1.1, rig.clear_other())]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t + 0.3, rig.mark('pre_throw')), (t + 0.35, throw), (t + 1.0, rig.gone('the throw ended the link on every world')),
              (t + 1.0, _spike4(rig, 'unparked', rig.ic, rig.ir, 'throw (rider released)')),
              (t + 1.4, run.at('the rider flew off the carrier (server)', flew)),
              (t + 2.4, rig.mark('thrown')), (t + 2.4, run.at("the rider's client and the server agree once it landed", agree)),
              (t + 2.45, lambda: run.note(f"throw: rollbacks rider client {rig.rb_delta('pre_throw', 'thrown', rig.ir)}, "
                                          f"carrier client {rig.rb_delta('pre_throw', 'thrown', rig.ic)}"))]
    steps += rig.end(t + 2.5)
    run.run(steps)


def scenario_boost_jump(pairing='client_rides_client'):
    """The rider boost-jumps off its carrier (PM_BoostJump): rider-initiated, predicted on the rider; the carrier braces on the server"""
    rig = _MountRig('boost_jump', pairing)
    run = rig.run
    st = {}

    def boost():
        st['z0'] = g.snap(g.worlds()[rig.ir])[rig.rkey]['location'][2]
        rig.first.clear()
        rig.watch_from = run.now()
        return 'rider boosts', g.combat_tap(rig.ir, g.button_mask('Jump'))

    def rose():
        zs = [p[2] for t, p, _ in rig.shown if t >= rig.watch_from]
        top = max(zs, default=st['z0']) - st['z0']
        run.note(f'boost: the rider rose {top:.1f} cm above its parked height on its own screen')
        return top >= 100.0, f'{top:.1f} cm'

    def braced():
        # Gate: the authority braces. The carrier's own client is only noted: it gets the forced partner move by correction, not by prediction, so a
        # 12-frame brace may never show there (spike 4; forward prediction for pair partners is a pending owner decision)
        run.note(f"boost: the carrier's own client saw the brace: {('braced', rig.ic) in rig.first}")
        return ('braced', 0) in rig.first, f'{rig.first}'

    def unbraced():
        # The Tags track's consumer removes the lock when the window closes (no leak after the brace, on any world that saw it)
        held = [wi for wi in range(len(g.worlds())) if ls.has_tag(wi, rig.ckey, LOCK_MOVEMENT)]
        return not held, f'still locked on {held}'

    steps = [(0.2, rig.setup()), (1.1, rig.clear_other())]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t + 0.3, rig.mark('pre_boost')), (t + 0.35, boost), (t + 1.2, rig.gone('the boost ended the link on every world')),
              (t + 1.2, run.at('the rider rose above the carrier', rose)),
              (t + 1.2, run.at('the carrier braced on the server (Combat.Lock.Movement on its ASC)', braced)),
              (t + 1.2, _spike4(rig, 'braced', 0, rig.ic, 'boost (carrier braced, server vs carrier screen)', gate=False)),
              (t + 1.2, _spike4(rig, 'unparked', rig.ir, rig.ic, 'boost (rider leaves, rider vs carrier screen)')),
              (t + 2.2, rig.mark('boosted')),
              (t + 2.2, run.at('the brace lock is gone on every world', unbraced)),
              (t + 2.25, lambda: run.note(f"boost: rollbacks rider client {rig.rb_delta('pre_boost', 'boosted', rig.ir)} (predicted: expect 0), "
                                          f"carrier client {rig.rb_delta('pre_boost', 'boosted', rig.ic)}"))]
    steps += rig.end(t + 2.3)
    run.run(steps)


def _pair_move_delay(name, pairing, move):
    """FP G1: the initiator's screen against its own action, with the predicted partner presentation on (gstn.Link.PredictPartnerPresentation 1).
    move 'boost': the rider (initiator) boost-jumps; its own event is the rider leaving on its own world, the partner is the carrier.
    move 'throw': the carrier (initiator) throws; its own events are its tap and the rider's release on its world, the partner is the rider.
    PASS: the predicted reaction is shown on the initiator's world no later than PREDICT_MS after its own event, and the move still completes.
    The partner-screen numbers of spike 4 are measured as before (not gated)."""
    rig = _MountRig(name, pairing)
    run = rig.run
    st = {}
    boost = move == 'boost'
    ini, par = (rig.ir, rig.ckey) if boost else (rig.ic, rig.rkey)
    other = rig.ic if boost else rig.ir   # the partner's own world
    rig.partner_key = par

    def act():
        st['z0'] = g.snap(g.worlds()[rig.ir])[rig.rkey]['location'][2]
        st['start'] = {k: tuple(g.snap(g.worlds()[0])[k]['location']) for k in (rig.rkey, rig.ckey)}
        rig.first.clear()
        rig.watch_from = run.now()
        # FP G2: the video capture aligns its frames on this line's log time
        unreal.log(f'[FP] tap {move} initiator world {ini} partner world {other} present {os.environ.get("GSTN_FP_PRESENT", "1")}')
        if boost:
            return 'rider boosts', g.combat_tap(rig.ir, g.button_mask('Jump'))
        return 'carrier throws', g.combat_tap(rig.ic, g.button_mask('Special'))

    def cvar_on():
        # GSTN_FP_PRESENT=0 runs the same move with the predicted presentation off (FP G2 video, the "before" side); the gates then fail by design
        value = os.environ.get('GSTN_FP_PRESENT', '1')
        unreal.SystemLibrary.execute_console_command(g.worlds()[ini], f'gstn.Link.PredictPartnerPresentation {value}')
        return 'cvar', f'gstn.Link.PredictPartnerPresentation {value}'

    def seen():
        if ini == 0:
            run.note('the initiator is the host (authority): the predicted presentation is client-only, nothing to gate')
            return True, 'host initiator'
        return rig.first.get(('predicted', ini)) is not None, f'{rig.first}'

    def delay():
        a, tap = rig.first.get(('predicted', ini)), rig.watch_from
        own = rig.first.get(('unparked', ini))
        if ini == 0:
            return True, 'host initiator'
        if a is None:
            return False, f'{rig.first}'
        run.note(f"{name}: the initiator's world showed the predicted partner reaction {(a - tap) * 1000.0:.0f} ms after its tap"
                 + (f", {(a - own) * 1000.0:.0f} ms after its own unpark event" if own is not None else ', its own unpark event not seen'))
        if own is None:
            return False, f'{rig.first}'
        d = (a - own) * 1000.0
        # Boost: the rider's own action is the unpark (predicted, ~130 ms after the tap). Throw: the unpark is the server-given release, the
        # prediction shows at the throw's PartnerMove key, which is not later than it
        return d <= PREDICT_MS, f'{d:.0f} ms (threshold {PREDICT_MS:.0f} ms)'

    def events():
        s, e, d = predicted_events(ini, par)
        run.note(f'{name}: predicted partner events on the initiator world (started, ended, dropped) = {(s, e, d)}')
        return (ini == 0 or (s >= 1 and d == 0)), f'{(s, e, d)}'

    def completed():
        if boost:
            zs = [p[2] for t, p, _ in rig.shown if t >= rig.watch_from]
            top = max(zs, default=st['z0']) - st['z0']
            run.note(f'{name}: the rider rose {top:.1f} cm above its parked height on its own screen')
            return top >= 100.0, f'{top:.1f} cm'
        r, c = g.snap(g.worlds()[0])[rig.rkey]['location'], st['start'][rig.ckey]
        dist = math.dist(r[:2], c[:2])
        return dist >= 250.0 and ls.mover_mode(0, rig.rkey) != ls.LINKED_MODE, f'rider {dist:.1f} cm from where the carrier threw (server)'

    def clean():
        held = [wi for wi in range(len(g.worlds())) if ls.has_tag(wi, rig.ckey, LOCK_MOVEMENT)]
        return not held, f'brace lock still on {held}'

    steps = [(0.2, rig.setup()), (0.3, cvar_on), (1.1, rig.clear_other())]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t + 0.3, rig.mark('pre_move')), (t + 0.35, act), (t + 1.2, rig.gone('the move ended the link on every world')),
              (t + 1.2, run.at('the move completed (the partner part is not stuck or cut)', completed)),
              (t + 1.2, run.at('the predicted partner reaction was shown on the initiator world', seen)),
              (t + 1.2, run.at(f'the initiator saw the partner react within {PREDICT_MS:.0f} ms of its own action', delay)),
              (t + 1.2, run.at('the predicted move started on the initiator world and was not dropped', events)),
              (t + 1.2, _spike4(rig, 'unparked', ini, other, f'{move} (unparked, initiator vs partner screen)', gate=False)),
              (t + 1.2, _spike4(rig, 'braced', 0, rig.ic, f'{move} (carrier braced, server vs carrier screen)', gate=False)),
              (t + 2.2, rig.mark('moved')),
              (t + 2.2, run.at('no brace lock is left on any world', clean)),
              (t + 2.25, lambda: run.note(f"{name}: rollbacks rider client {rig.rb_delta('pre_move', 'moved', rig.ir)}, "
                                          f"carrier client {rig.rb_delta('pre_move', 'moved', rig.ic)}"))]
    steps += rig.end(t + 2.3)
    run.run(steps)


def scenario_link_pair_move_delay(pairing='client_rides_client'):
    """FP G1, boost jump: the rider (initiator) sees the carrier brace within 150 ms of its own action (predicted partner presentation)"""
    return _pair_move_delay('link_pair_move_delay', pairing, 'boost')


def scenario_link_pair_move_delay_throw(pairing='client_rides_client'):
    """FP G1, throw: the carrier (initiator) sees the rider react within 150 ms of its own release event; the wind-up before it is design time"""
    return _pair_move_delay('link_pair_move_delay_throw', pairing, 'throw')


def scenario_mount_forced_reconcile(pairing='client_rides_client'):
    """Spec 9: forced reconciles at the request, Pending, first Active, pair-move start and outcome frames. A forced reconcile rolls the client
    back ForceReconcileExtraFrames before its last server frame and resimulates: forcing right after an event with a span longer than the
    event's age puts the event's frame inside the resimulated range. Clean = the link rows and each client's own pawn equal the server's"""
    rig = _MountRig('mount_forced_reconcile', pairing)
    run = rig.run
    st = {'forces': []}

    def force(label, wi, frames):
        def f():
            st['forces'].append((label, wi, frames, g.rollbacks(rig.ir), g.rollbacks(rig.ic)))
            return 'force', label, wi, frames, g.force_reconcile(wi, frames)
        return f

    def own_equal(label):
        def f():
            srv = g.snap(g.worlds()[0])
            bad = {}
            for wi, key in ((rig.ir, rig.rkey), (rig.ic, rig.ckey)):
                if wi <= 0:
                    continue
                d = g._diff(srv[key], g.snap(g.worlds()[wi])[key], pos_tol=10.0)
                if d:
                    bad[wi] = d
            return not bad, f'{bad}'
        return run.at(label, f)

    def throw():
        return 'carrier throws', g.combat_tap(rig.ic, g.button_mask('Special'))

    def forced_happened():
        rb = {wi: g.rollbacks(wi) for wi in g.all_players() if wi > 0}
        start = rig.marks['pre_mount']['rb']
        gained = {wi: rb[wi] - start[wi] for wi in rb}
        run.note(f'forced reconciles: {len(st["forces"])} requested, rollbacks gained per client {gained} (one global cvar: any client may take one)')
        return sum(gained.values()) >= 3, f'{gained}'

    steps = [(0.2, rig.setup()), (1.1, rig.clear_other()), (1.2, rig.place(rig.ckey, (90.0, 0.0))), (1.8, rig.target(MOUNT_TARGET)),
             (1.85, rig.mark('pre_mount')),
             (1.9, rig.tap(rig.ir)), (1.9, force('request frame', rig.ir, 2)),
             (2.15, force('Pending frame (span over the request + 1)', rig.ic, 20)),
             (2.45, force('first Active frame (span over the accept)', rig.ir, 30)),
             (3.0, rig.mark('mount')), (3.0, rig.mounted()), (3.0, rig.parked()),
             (3.3, own_equal("after the mount's forced reconciles, each client's own pawn equals the server's")),
             (3.6, throw), (3.6, force('pair-move start', rig.ic, 4)),
             (3.95, force('outcome/release frame (span over the throw)', rig.ir, 30)),
             (4.4, rig.gone('the throw ended the link on every world after the forced reconciles')),
             (5.8, rig.mark('settled')),
             (5.8, own_equal("after the throw's forced reconciles and the landing, each client's own pawn equals the server's")),
             (5.85, run.at('the forced reconciles happened', forced_happened))]
    steps += rig.end(5.9)
    run.run(steps)
