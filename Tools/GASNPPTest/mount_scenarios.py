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
import math
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
SUITE_MOUNT = ['mount_basic', 'mount_refused', 'mount_heavy_refused', 'dismount_drop', 'mount_interact_priority', 'dismount_predicted']
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


def prepare_editor():
    """The two test kits (created from KIT_Test_Link when missing, saved in the plugin's Content/Test), the kit scan, the link content and the
    saved team rule on GSTNLink's test grab (setup_defence_link_content.py, G5). Before PIE"""
    out = [_ensure_kit(KIT_MOUNT, 100.0, 150.0), _ensure_kit(KIT_HEAVY, 200.0, 150.0)]
    out.append(('scan', PLib.scan_plugin_kits()))
    out.append(('link', ls.prepare_editor()))
    out.append(('mount type index', PLib.get_mount_type_index()))
    return out


def restore_editor():
    return 'nothing to restore'


# ---- rig ---------------------------------------------------------------------------------------------------------------------------------
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
    steps = [(0.2, rig.setup()), (1.1, rig.clear_other())]
    mount, t = rig.mount_steps(1.2)
    steps += mount
    steps += [(t, run.at('no rollback on the rider\'s client for an accepted mount (optimistic accept)', lambda: (
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
