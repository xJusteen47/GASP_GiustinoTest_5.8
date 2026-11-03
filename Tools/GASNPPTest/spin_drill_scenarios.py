# Spin and drill, plans D4 and D5: the spin scenarios (SUITE_SPIN, not in SUITE_DEFAULT). Ground spin ring, air spin hop and low gravity, one air spin per
# airtime, the opposite-spin cancel. Same harness as defence_scenarios / link_scenarios: run inside the editor's Python console, PIE "Run Under One Process", listen server
# first (host + 2 clients), then 60/15 ms with and without 2 % loss.
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import spin_drill_scenarios as sd
#   sd.prepare_editor()                 # once, in the editor world BEFORE PIE: the spin bindings go on the test kit IN MEMORY (never saved)
#   sd.on_ready()                       # once per PIE, a second before the first scenario: the test kit on every pawn
#   sd.scenario_spin_ground_hits_ring() # in a running PIE
#   sd.scenario_spin_air_hop_low_gravity()
#   sd.scenario_air_spin_once_per_airtime()
#   sd.scenario_spin_opposite_cancel()
#   sd.last()                           # (verdict, checks) of the last one
#   sd.restore_editor()                 # with PIE stopped: the kit's bindings as saved
#
# Needs the GSTNSpinDrill plugin enabled (EnabledByDefault false: enable it locally, never commit the .uproject change). What stays in memory:
# the nine default spin bindings (ground and air cancels first) (UGSTN_SpinDrillLibrary.add_spin_bindings_to_kit, slots inline, so neither the command set nor the input config
# changes). No shared Blueprint is edited. The stick rotation is driven through IA_Move like a pad: 9 positions 35 ms apart (a 360 in ~0.32 s,
# inside the 30-frame window of the Spin condition).
import math
import re
import time

import unreal
import gasnpp_test as g

KIT_PATH = '/Game/GASNPPTest/Combat/' + g.HIT_KIT
SUITE_SPIN = ['spin_ground_hits_ring', 'spin_air_hop_low_gravity', 'air_spin_once_per_airtime', 'spin_opposite_cancel', 'drill_ground_lag', 'drill_nondrillable_bounce']
# Plan D9/D10 (SD-05): the latch on another player, from the dive's tip to the exit
SUITE_DRILL = ['drill_latch_static_multi_hit', 'drill_release_early_pop', 'drill_launch_after_bore', 'drill_target_destroyed', 'drill_latch_moving_enemy',
               'drill_hit_while_latched_detaches']
SUITE_SPIN = SUITE_SPIN + SUITE_DRILL
NEEDED_BUTTONS = ['Input.Button.Spin', 'Input.Button.Crouch']
TAG_SPINNING = 'Combat.State.Spinning'
TAG_AIR_SPENT = 'Combat.State.AirSpinSpent'
TAG_LOW_GRAVITY = 'Combat.State.SpinLowGravity'
PHASE_SPINNING = 1
STEP_S = 0.035
_saved = {}
_last = {'verdict': None, 'checks': []}
Lib = unreal.GSTN_SpinDrillLibrary
TestLib = unreal.GSTN_SpinDrillTestLibrary   # GSTNSpinDrillTests: harness helpers (drill immunity)


# ---- content, in memory ------------------------------------------------------------------------------------------------------------------------
def _button_names(cfg):
    return [g._tag_text(b.get_editor_property('tag')) for b in cfg.get_editor_property('buttons')]


def prepare_editor():
    """Adds the default spin bindings to the test kit and the Spin and Crouch buttons to IC_GASNPP_Test that lack them (all in memory). Run in the editor
    world before PIE: the loadout and the button map are built when a pawn starts. Idempotent; restore_editor undoes it (defence_batch calls it at the end)"""
    kit = unreal.load_asset(KIT_PATH)
    _saved.setdefault('kit_bindings', list(kit.get_editor_property('kit_bindings')))
    cfg = unreal.load_asset(g.INPUT_CONFIG)
    buttons = list(cfg.get_editor_property('buttons'))
    names = _button_names(cfg)
    missing = [b for b in NEEDED_BUTTONS if b not in names]
    if missing:
        _saved.setdefault('buttons', list(buttons))
        for name in missing:
            button = type(buttons[0])()
            button.set_editor_property('tag', g._tag(name))
            buttons.append(button)
        cfg.set_editor_property('buttons', buttons)
    # The kit extension lists the bore ability and the drill tuning: without it a dive latches but never bores (SD-11)
    if Lib.add_spin_drill_extension_to_kit(kit):
        _saved['extension_added'] = True
    return 'spin bindings added', Lib.add_spin_bindings_to_kit(kit, True), 'buttons added', missing


def restore_editor():
    """Puts the kit's bindings and the input config's buttons back as they were before prepare_editor (with PIE stopped)"""
    out = []
    if 'kit_bindings' in _saved:
        kit = unreal.load_asset(KIT_PATH)
        kit.set_editor_property('kit_bindings', _saved.pop('kit_bindings'))
        out.append('kit bindings restored')
    if _saved.pop('extension_added', False):
        Lib.remove_spin_drill_extension_from_kit(unreal.load_asset(KIT_PATH))
        out.append('kit extension removed')
    if 'buttons' in _saved:
        unreal.load_asset(g.INPUT_CONFIG).set_editor_property('buttons', _saved.pop('buttons'))
        out.append('buttons restored')
    return out or 'nothing to restore'


def last():
    return _last['verdict'], _last['checks']


# ---- reads -----------------------------------------------------------------------------------------------------------------------------------------
def actor(wi, key):
    c = g.comps(g.worlds()[wi]).get(key)
    return c.get_owner() if c else None


def drill(wi, key):
    """(phase, phase start frame, air spins used, low gravity until) of the pawn `key` as world `wi` shows it, None without a fragment"""
    # UE Python folds a bool return with out params: None when it returns false (no drill fragment on that pawn), the out params alone when
    # true (a 4-tuple; the bool is not in it). A 5-tuple is accepted too in case a build keeps the return value
    r = Lib.get_shown_drill_numbers(actor(wi, key))
    if isinstance(r, tuple) and len(r) == 5:
        return tuple(r[1:]) if r[0] else None
    return tuple(r) if isinstance(r, tuple) and len(r) == 4 else None


def drill_text(wi, key):
    return Lib.get_shown_drill_text(actor(wi, key))

NO_FRAGMENT = (0, -1, 0, 0, -1, 0)


def read(wi, key):
    """(phase, phase start frame, air spins used, low gravity until, latch frame, latch hits) of the pawn `key` as world `wi` shows it; NO_FRAGMENT without one"""
    d = drill(wi, key)
    if d is None:
        return NO_FRAGMENT
    m = re.search(r'latch (-?\d+) seat -?\d+ max -?\d+ hits (\d+)', drill_text(wi, key) or '')
    return tuple(d) + ((int(m.group(1)), int(m.group(2))) if m else (-1, 0))


class _Spy:
    """Per-world sampler of the pawn `key` (SD-06). The authority and the other clients show a spin, a dive or a latch 100-250 ms after the predicting
    client (60/15 ms lag + input buffer), so a check that reads every world at one instant misses it. Every editor tick it records read() on every
    world; a check asks 'first seen on world wi in [t0, t1]' (sighted) and the scenarios compare the frames the worlds report, not the instant."""

    def __init__(self, run, key):
        self.run, self.key, self.rows = run, key, []
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def _tick(self, _dt):
        try:
            self.rows.append((self.run.now(), {wi: read(wi, self.key) for wi in range(len(g.worlds()))}))
        except Exception:
            pass

    def stop(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None

    def first(self, wi, pred, t0, t1):
        return next((r[1][wi] for r in self.rows if t0 <= r[0] <= t1 and wi in r[1] and pred(r[1][wi])), None)

    def last(self, wi, pred, t0, t1):
        return next((r[1][wi] for r in reversed(self.rows) if t0 <= r[0] <= t1 and wi in r[1] and pred(r[1][wi])), None)

    def sighted(self, pred, t0, t1):
        """{world: first reading in [t0, t1] that satisfies pred, or None}"""
        return {wi: self.first(wi, pred, t0, t1) for wi in range(len(g.worlds()))}

    def never(self, pred, t0, t1, worlds=None):
        """True when no reading of any (listed) world in [t0, t1] satisfies pred"""
        return not any(pred(v) for r in self.rows if t0 <= r[0] <= t1 for wi, v in r[1].items() if worlds is None or wi in worlds)


def _spy(run, key):
    run.spy = _Spy(run, key)
    return run.spy


def _seen_all(run, pred, t0, t1, same_start=False, same_latch=False):
    """(ok, details): every world has a reading satisfying pred in [t0, t1]; same_start / same_latch also want one phase start / latch frame across them"""
    s = run.spy.sighted(pred, t0, t1)
    ok = all(v is not None for v in s.values())
    if ok and same_start:
        ok = len({v[1] for v in s.values()}) == 1
    if ok and same_latch:
        # The predicting client may see the latch a few frames early and be corrected when the authority's state arrives (a rollback, which the
        # rollback checks count): what must agree is the settled latch frame, the last latched reading of each world in the window (SD-11)
        settled = {wi: run.spy.last(wi, pred, t0, t1) for wi in s}
        ok = len({v[4] for v in settled.values()}) == 1
        return ok, {'first': s, 'settled': settled}
    return ok, s


def _teleport_under(key, vkey, wi=0):
    """Server teleport of `vkey` onto the floor right under `key` as world `wi` shows it (the authority moves it; clients get it as a
    correction). A set_actor_location is undone by Mover on the next tick (SD-07). The target keeps its own height (standing on the floor)
    and takes the diver's x, y. SD-08 probe: placed 20 cm under a diver the authority still showed on the ground, the target was asked into
    the floor and pushed 60 cm aside; placed under the authority's diver 50 ms before the stick turn ended, it sat 15 cm off the spot the
    dive left from (the turn drifts the diver) and the tip slid off its head. So the latch scenarios place it at the end of the turn under the
    diver's own client copy (wi = the diver's world), which is where the dive starts; the authority dives there 100-250 ms later"""
    a, v = actor(wi, key).get_actor_location(), actor(0, vkey).get_actor_location()
    return g.comps(g.worlds()[0])[vkey].server_teleport_exact(unreal.Vector(a.x, a.y, v.z + 2.0))


# ---- input -----------------------------------------------------------------------------------------------------------------------------------------
def spin_steps(client, t, clockwise=True):
    """Timeline steps that turn the stick of `client` once around (IA_Move: x right, y forward), starting up, then let go. Returns (steps, end time)"""
    sense = -1.0 if clockwise else 1.0   # clockwise seen from above: up -> right -> down -> left
    steps = []
    for k in range(9):
        a = math.pi / 2 + sense * k * math.pi / 4
        v = (math.cos(a), math.sin(a), 0.0)
        steps.append((t + k * STEP_S, (lambda v=v: ('stick', g.hold(client, 'move', v)))))
    end = t + 9 * STEP_S
    steps.append((end, lambda: ('let go', g.release(client, 'move'))))
    return steps, end


def jump_steps(client, t):
    # Held for 0.25 s, not a one-frame press_input: at -nullrhi editor rates a one-frame injection was lost or answered up to 0.5 s late
    # (SD-08 probe: no jump at all before 0.8 s in three latch scenarios), which moved every later step of the scenario off its jump. A 0.1 s
    # hold still failed in a run where the editor ticked every ~0.1 s (one frame again): 0.25 s spans at least two editor frames
    return [(t, lambda: ('jump', g.hold(client, 'jump', (1.0, 0.0, 0.0)))), (t + 0.25, lambda: ('jump let go', g.release(client, 'jump')))]


def _apply_kit():
    """The test kit (with the spin bindings added in memory by prepare_editor) on every pawn: the pawns start on their default loadout, which has none"""
    out = [g._apply_loadout(g.key_of_player(i), g.HIT_KIT) for i in g.all_players()]
    if any(r != 'OK' for r in out):
        # A refused loadout leaves the pawns on their default kit (no spin bindings): every check after it would fail for that one reason
        unreal.log_error(f'spin harness: the test kit was refused: {out}')
    return 'kit applied', out


_kit_state = {'applied': False}


def on_ready():
    """defence_batch calls it once per PIE when the pawns are ready, before its settle time: the kit change (a loadout revision) is out of the way
    before the scenario's inputs. A loadout change at the scenario's start left the first jump and spin unanswered (SD-05)"""
    _kit_state['applied'] = True
    return _apply_kit(), _settle_pawns()


def _settle_pawns():
    """Drops every pawn onto the floor where it stands. SD-09 probe: one of the two spawns leaves its pawn in Falling at z 90.00 for the whole PIE
    (it never lands while it stands still), so its jump is refused; whichever player gets that spawn changes between PIEs, which is why the dive
    scenarios failed only some of the time. A server_teleport_exact (forced Falling, zero velocity) 2 cm above the spot makes it land in Walking
    within the batch's settle time. Only a pawn that is not Walking is moved: the drop leaves it standing 2 cm above the floor (Walking does not look
    for a floor at rest), and settling the standing pawn too lifted the drill target 2 cm and the latch stopped forming (SD-09 run 2)"""
    out = []
    for i in g.all_players():
        key = g.key_of_player(i)
        a = actor(0, key)
        mover = a.get_component_by_class(unreal.CharacterMoverComponent)
        if mover and str(mover.get_movement_mode_name()) == 'Walking':
            continue
        v = a.get_actor_location()
        out.append((i, g.comps(g.worlds()[0])[key].server_teleport_exact(unreal.Vector(v.x, v.y, v.z + 2.0))))
    return 'pawns settled', out


def _run(run, steps):
    # By hand (no batch): call on_ready() once in the running PIE and wait a second before the first scenario
    run.run(list(steps) if _kit_state['applied'] else [(0.05, _apply_kit)] + list(steps))
    if getattr(run, 'spy', None):
        run.spy.rows.clear()   # run.run restarted the clock: readings from before it carry the old time base


# ---- scenarios ---------------------------------------------------------------------------------------------------------------------------------------
def _finish(run, name):
    def step():
        if getattr(run, 'spy', None):
            run.spy.stop()
        _last['verdict'] = all(ok for ok, _, _ in run.results if ok is not None)
        _last['checks'] = list(run.results)
        unreal.log(f"SPIN {name}: {'PASS' if _last['verdict'] else 'FAIL'}")
        return 'done'
    return step


def scenario_spin_ground_hits_ring(client=1, victim=2):
    """A ground spin: the spinner's copies (authority and its own client) show Combat.State.Spinning for about the spin's 30 frames, the drill
    fragment's phase start frame is equal on every world, the pawn standing next to the spinner takes the ring's damage on the authority, and the
    spinner's client has no rollback caused by the spin (a predicted spin the server agrees with)"""
    key, vkey = g.key_of_player(client), g.key_of_player(victim)
    # The victim's authoritative copy is sampled too ('vic'). A combat hit raises the victim's damage percent (the combat snapshot), not Health (SD-07)
    run = g._Run('spin_ground_hits_ring', client, extra=[('vic', 0, vkey)])
    _spy(run, key)
    marks = {}

    def mark(name):
        def f():
            vic = run.latest('vic')
            marks[name] = {'rb': g.rollbacks(client), 'hp': g.damage_of(0, vkey),
                           'drill': {wi: drill(wi, key) for wi in range(len(g.worlds()))}}
            return name, marks[name]['drill']
        return f

    def place_victim():
        # 80 cm in front of the spinner, inside the ring (110 cm), through the simulation: a set_actor_location is undone by Mover (SD-07)
        return 'victim placed', g._place_near(client, vkey, (80.0, 0.0))

    spin, t_end = spin_steps(client, 0.6, clockwise=True)
    steps = [(0.2, place_victim), (0.5, mark('before'))] + spin + [
        (t_end + 0.8, run.at('the spinner shows the Spinning phase on every world (first sighting in the spin window)', lambda: _seen_all(
            run, _is_spin, t_end - 0.1, t_end + 0.75))),
        (t_end + 0.8, run.at('the phase start frame is the same on every world', lambda: _seen_all(
            run, _is_spin, t_end - 0.1, t_end + 0.75, same_start=True))),
        (t_end + 1.0, run.at('Combat.State.Spinning was held on the authority and on the predicting client', lambda: (
            run.saw_tag(TAG_SPINNING, 0.5, t_end + 1.0, 'srv') and run.saw_tag(TAG_SPINNING, 0.5, t_end + 1.0, 'cli'), ''))),
        (t_end + 1.0, mark('after')),
        (t_end + 1.0, run.at('the spin ended: no phase on any world after its timeline', lambda: (
            all((marks['after']['drill'][wi] or (0,))[0] != PHASE_SPINNING for wi in marks['after']['drill']), marks['after']['drill']))),
        (t_end + 1.02, run.at("the pawn next to the spinner took the ring's damage on the authority", lambda: (
            marks['before']['hp'] is not None and marks['after']['hp'] is not None and marks['after']['hp'] > marks['before']['hp'] + 1.0,
            f"{marks['before']['hp']} -> {marks['after']['hp']}"))),
        (t_end + 1.05, run.at('no rollback on the spinner during the spin', lambda: (
            marks['after']['rb'] - marks['before']['rb'] == 0, f"{marks['before']['rb']} -> {marks['after']['rb']}"))),
        (t_end + 1.1, run.finish()), (t_end + 1.15, _finish(run, 'spin_ground_hits_ring'))]
    _run(run, steps)
    return 'running'


def scenario_spin_air_hop_low_gravity(client=1):
    """Jump, then spin at the top: the speed along gravity-up becomes at least the hop speed (450) right after the spin starts, the low gravity effect is
    on during the spin (Combat.State.SpinLowGravity on both copies), the fall is slower than the same fall without a spin (the descent rate over the
    spin's frames compared with a plain jump's), and the effect is gone after the spin"""
    run = g._Run('spin_air_hop_low_gravity', client)
    key = g.key_of_player(client)

    def up_speed(t0, t1, side='srv'):
        return [s['vel'][2] for s in run._win(side, t0, t1, sim=True)]

    # The spin is recognised at the end of the stick turn (t_end), about the jump's top (apex ~0.7 s after the press), and the authority hops
    # 100-250 ms later: the window runs to t_end + 0.3, where a plain jump already falls. The sim samples are taken before the tick integrates,
    # so the hop (450) reads as 402, a few ticks of gravity later (same value in SD-06 and SD-08): 400 is the bar, a plain jump never reads it there
    t_jump2 = 2.5
    spin, t_end = spin_steps(client, t_jump2 + 0.4, clockwise=False)
    steps = jump_steps(client, 0.3) + jump_steps(client, t_jump2) + spin + [
        (t_jump2 + 0.39, run.at('before the spin the jump is near its top', lambda: (True, f"vz {up_speed(t_jump2 + 0.3, t_jump2 + 0.39)[-1:]}"))),
        (t_end + 0.3, run.at('the hop lifts: vz reaches the hop speed after the spin starts', lambda: (
            max(up_speed(t_end - 0.05, t_end + 0.3) or [0.0]) >= 400.0, f"max vz {max(up_speed(t_end - 0.05, t_end + 0.3) or [0.0]):.0f}"))),
        (t_end + 0.3, run.at('the low gravity effect is on during the spin, on both copies', lambda: (
            run.saw_tag(TAG_LOW_GRAVITY, t_end, t_end + 0.3, 'srv') and run.saw_tag(TAG_LOW_GRAVITY, t_end, t_end + 0.3, 'cli'), ''))),
        (t_end + 1.5, run.at('the low gravity effect is gone after the spin', lambda: (
            not run.saw_tag(TAG_LOW_GRAVITY, t_end + 1.2, t_end + 1.5, 'srv'), ''))),
        (t_end + 2.2, run.at('the air spin was spent, then given back on landing', lambda: (
            run.saw_tag(TAG_AIR_SPENT, t_end, t_end + 0.6, 'srv') and not run.saw_tag(TAG_AIR_SPENT, t_end + 2.0, t_end + 2.2, 'srv'), ''))),
        (t_end + 2.25, lambda: run.note(f"descent with the spin vs the plain jump: {up_speed(t_end + 0.3, t_end + 0.6)[:3]} / {up_speed(0.9, 1.2)[:3]}")),
        (t_end + 2.3, run.finish()), (t_end + 2.35, _finish(run, 'spin_air_hop_low_gravity'))]
    _run(run, steps)
    return 'running'


def scenario_air_spin_once_per_airtime(client=1):
    """Two spins in one jump: the first is the air spin (hop, AirSpinSpent), the second starts nothing (the air spin classes are blocked, the fragment
    refuses it), a ground spin after landing works, and a new jump allows one air spin again. Getting hit is not a refresh (owner 2026-11-01): not
    exercised here, the unit test GSTNSpinDrill.Core.Spin.HopAndRefresh pins it"""
    run = g._Run('air_spin_once_per_airtime', client)
    key = g.key_of_player(client)
    _spy(run, key)
    seen = {}

    def first_spin(name, t0, t1, used):
        # The authority shows the spin up to 250 ms after the client: first sighting in a window, with the air spins used the spin should leave
        def f():
            seen[name] = run.spy.first(0, lambda r: r[0] == PHASE_SPINNING and r[2] == used, t0, t1)
            return name, seen[name]
        return f

    first, t1 = spin_steps(client, 0.45, clockwise=True)
    second, t2 = spin_steps(client, t1 + 0.1, clockwise=False)
    ground, t3 = spin_steps(client, 2.6, clockwise=True)
    again, t4 = spin_steps(client, 3.85, clockwise=True)
    def spin_starts(t0, t1):
        # distinct phase start frames of spins the authority showed in the window: the second rotation must not add one
        return {r[1] for _, row in run.spy.rows if t0 <= _ <= t1 for r in [row[0]] if r[0] == PHASE_SPINNING}

    steps = jump_steps(client, 0.3) + first + [(t1 + 0.7, first_spin('first', t1 - 0.2, t1 + 0.6, 1))] + second + ground + [
        (t3 + 0.7, first_spin('ground', t3 - 0.2, t3 + 0.6, 0))] + jump_steps(client, 3.7) + again + [
        (t4 + 0.7, first_spin('again', t4 - 0.2, t4 + 0.6, 1)),
        (t4 + 0.75, run.at('the first spin in the air is the air spin', lambda: (seen['first'] is not None, seen))),
        (t4 + 0.75, run.at('the second spin in the same airtime started nothing new', lambda: (
            seen['first'] is not None and spin_starts(t1 - 0.2, t3 - 0.3) == {seen['first'][1]}, sorted(spin_starts(t1 - 0.2, t3 - 0.3))))),
        (t4 + 0.75, run.at('on the ground the spin works and nothing is spent', lambda: (seen['ground'] is not None, seen))),
        (t4 + 0.75, run.at('a new jump allows one air spin again', lambda: (seen['again'] is not None, seen))),
        (t4 + 1.2, run.finish()), (t4 + 1.25, _finish(run, 'air_spin_once_per_airtime'))]
    _run(run, steps)
    return 'running'


def scenario_spin_opposite_cancel(client=1):
    """A clockwise ground spin, then a counter-clockwise rotation that ends on about frame 10 of the spin (D5, 14 SD.2): the spin stops early on
    every world (Spinning held for clearly less than the natural 0.5 s, and no new spin starts: the phase is None right after), the predicting
    client has no rollback from it, and a counter-clockwise rotation with no spin running is an ordinary spin again"""
    run = g._Run('spin_opposite_cancel', client)
    key = g.key_of_player(client)
    _spy(run, key)
    seen = {}

    def phase_at(name):
        def f():
            seen[name + '_rb'] = g.rollbacks(client)
            return name, seen[name + '_rb']
        return f

    def held_seconds(t0, t1, side):
        ts = [s['ms'] / 1000.0 for s in run._win(side, t0, t1, sim=True) if TAG_SPINNING in s['tags']]
        return (max(ts) - min(ts)) if ts else 0.0

    spin, t1 = spin_steps(client, 0.5, clockwise=True)
    # The second rotation starts right away: 9 steps of 35 ms end ~0.32 s later, past CancelFromFrame (6 frames, 0.1 s) of the first spin
    cancel, t2 = spin_steps(client, t1 + 0.02, clockwise=False)
    plain, t3 = spin_steps(client, t2 + 1.2, clockwise=False)
    steps = [(0.4, phase_at('before'))] + spin + cancel + [(t2 + 0.8, phase_at('cancelled'))] + plain + [
        (t3 + 0.1, run.at('the spin was held for less than its natural length on the authority and the client', lambda: (
            0.0 < held_seconds(t1 - 0.4, t2 + 0.5, 'srv') < 0.45 and 0.0 < held_seconds(t1 - 0.4, t2 + 0.5, 'cli') < 0.45,
            f"srv {held_seconds(t1 - 0.4, t2 + 0.5, 'srv'):.2f} s, cli {held_seconds(t1 - 0.4, t2 + 0.5, 'cli'):.2f} s"))),
        (t3 + 0.1, run.at('after the cancel no world shows a spin (the opposite rotation did not start one)', lambda: (
            run.spy.never(_is_spin, t2 + 0.7, t2 + 1.1), 'no Spinning reading on any world in the window'))),
        (t3 + 0.1, run.at('no rollback on the spinner from the cancel', lambda: (seen['cancelled_rb'] - seen['before_rb'] == 0, f"{seen['before_rb']} -> {seen['cancelled_rb']}"))),
        (t3 + 0.8, run.at('with no spin running the same rotation is a counter-clockwise spin', lambda: _seen_all(run, _is_spin, t3 - 0.1, t3 + 0.75))),
        (t3 + 1.0, run.finish()), (t3 + 1.05, _finish(run, 'spin_opposite_cancel'))]
    _run(run, steps)
    return 'running'


# ---- D8: the dive -------------------------------------------------------------------------------------------------------------------------------------
PHASE_DIVING, PHASE_LAG = 2, 6
TAG_DIVE_GRAVITY = 'Combat.State.DiveGravity'


def crouch_tap(client):
    return lambda: ('crouch', g.combat_tap(client, g.button_mask('Crouch')))


def crouch_hold(client):
    return lambda: ('hold crouch', g.combat_hold(client, g.button_mask('Crouch')))


def crouch_release(client):
    return lambda: ('release crouch', g.combat_clear(client))


def scenario_drill_ground_lag(client=1):
    """Jump, air spin, crouch held: the dive starts on every world (phase Diving, the dive gravity effect on, the speed along gravity near the plunge
    speed of 1800), reaches the ground, the lag phase follows (18 frames) and ends; no rollback on the diver from the dive"""
    run = g._Run('drill_ground_lag', client)
    key = g.key_of_player(client)
    _spy(run, key)
    seen = {}

    def at(name):
        def f():
            seen[name] = {wi: drill(wi, key) for wi in range(len(g.worlds()))}
            seen[name + '_rb'] = g.rollbacks(client)
            return name, seen[name]
        return f

    def down_speed(t0, t1, side='srv'):
        return [-s['vel'][2] for s in run._win(side, t0, t1, sim=True)]

    spin, t1 = spin_steps(client, 0.55, clockwise=True)   # as the latch rig: the turn ends near the top of the jump (the jump is on time since SD-08)
    steps = [(0.3, at('before'))] + jump_steps(client, 0.3) + spin + [
        (t1 + 0.05, crouch_hold(client)), (t1 + 0.07, crouch_tap(client)),
        (t1 + 2.0, at('after')), (t1 + 2.0, crouch_release(client)),
        (t1 + 2.05, run.at('every world showed the dive (first sighting in the dive window)', lambda: _seen_all(
            run, lambda r: r[0] == PHASE_DIVING, t1, t1 + 1.5))),
        (t1 + 2.05, run.at('the dive gravity effect was on during the dive (authority and client)', lambda: (
            run.saw_tag(TAG_DIVE_GRAVITY, t1, t1 + 1.0, 'srv') and run.saw_tag(TAG_DIVE_GRAVITY, t1, t1 + 1.0, 'cli'), ''))),
        (t1 + 2.05, run.at('the plunge reached about the plunge speed', lambda: (
            max(down_speed(t1, t1 + 1.0) or [0.0]) > 1500.0, f"max down speed {max(down_speed(t1, t1 + 1.0) or [0.0]):.0f}"))),
        (t1 + 2.05, run.at('the dive ended: no phase on any world 2 s later (the lag is over)', lambda: (
            all((d or (0,))[0] not in (PHASE_DIVING, PHASE_LAG) for d in seen['after'].values()), seen['after']))),
        (t1 + 2.05, run.at('no rollback on the diver', lambda: (seen['after_rb'] - seen['before_rb'] == 0, f"{seen['before_rb']} -> {seen['after_rb']}"))),
        (t1 + 2.6, run.finish()), (t1 + 2.65, _finish(run, 'drill_ground_lag'))]
    _run(run, steps)
    return 'running'


def scenario_drill_nondrillable_bounce(client=1, victim=2):
    """The diver lands its tip on another player marked not drillable (Combat.Immunity.Drill through the test effect; players are drillable
    since D9): the victim takes the tip's damage on the authority, the dive ends in a
    bounce (speed along up > 500 right after) on the authority and the diver's client, and the air spin is not given back by it"""
    vkey = g.key_of_player(victim)
    run = g._Run('drill_nondrillable_bounce', client, extra=[('vic', 0, vkey)])
    key = g.key_of_player(client)
    seen = {}

    def place_under():
        return 'victim under the diver', _teleport_under(key, vkey, client)

    def at(name):
        def f():
            vic = run.latest('vic')
            seen[name] = g.damage_of(0, vkey)
            return name, seen[name]
        return f

    def immune(on):
        return lambda: ('drill immunity', on, TestLib.server_set_drill_immunity(actor(0, vkey), on))

    spin, t1 = spin_steps(client, 0.55, clockwise=False)
    steps = [(0.1, immune(True)), (0.2, at('before'))] + jump_steps(client, 0.3) + [(t1, place_under)] + spin + [
        (t1 + 0.03, crouch_hold(client)), (t1 + 0.05, crouch_tap(client)), (t1 + 1.2, at('after')), (t1 + 1.2, crouch_release(client)),
        (t1 + 1.25, run.at("the victim took the tip's damage on the authority", lambda: (
            seen['before'] is not None and seen['after'] is not None and seen['after'] > seen['before'] + 1.0, f"{seen['before']} -> {seen['after']}"))),
        (t1 + 1.25, run.at('the diver bounced (rising after the dive) on the authority and its client', lambda: (
            max([s['vel'][2] for s in run._win('srv', t1, t1 + 1.0, sim=True)] or [0.0]) > 500.0
            and max([s['vel'][2] for s in run._win('cli', t1, t1 + 1.0, sim=True)] or [0.0]) > 500.0, ''))),
        (t1 + 1.25, run.at('the air spin stayed spent after the bounce', lambda: (run.saw_tag(TAG_AIR_SPENT, t1 + 0.2, t1 + 0.6, 'srv'), ''))),
        (t1 + 1.55, immune(False)), (t1 + 1.6, run.finish()), (t1 + 1.65, _finish(run, 'drill_nondrillable_bounce'))]
    _run(run, steps)
    return 'running'

# drill_tilted_gravity (plan D8): needs a level with a gravity volume the harness can place a pawn in; the dive and bounce vectors under any gravity
# are pinned by the unit tests (GSTNSpinDrill.Core.Math.LaunchAndDive, GSTNSpinDrill.Core.Spin.HopAndRefresh). Not written until such a level exists.


# ---- drill latch (plans D9, D10) ---------------------------------------------------------------------------------------------------------------------
# The diver jumps, air spins and dives with crouch held onto another player placed under it (players are drillable: no weight limit, spin-drill.md
# 13b). The tip's hit forms the drill latch (Seating, then Boring while crouch is held); the exit is a pop on the release, the launch with a jump press
# in the boost window, the launch when the target is gone, a detach when the driller is hit. Tuning defaults: seat 12 frames, a hit every 6, at most 90.
PHASE_SEATING, PHASE_BORING, PHASE_LAUNCHING = 3, 4, 5
LATCHED = (PHASE_SEATING, PHASE_BORING)
DRILL_STEPS_PER_LATCH = 3   # driller client rollbacks allowed per latch (D10 stop condition: more than 3 corrections per latch)
LAUNCH_VZ = 1000.0          # the launch is 1400 cm/s along the surface normal, the pop 400


def latch_info(wi, key):
    """(phase, latch frame, hits) of the pawn `key` as world `wi` shows it; (0, -1, 0) without a fragment"""
    r = read(wi, key)
    return r[0], r[4], r[5]


def _is_spin(r):
    return r[0] == PHASE_SPINNING


def _is_latched(r):
    return r[0] in LATCHED


def _all_latched(seen):
    rows = seen.values()
    return all(r[0] in LATCHED for r in rows) and len({r[1] for r in rows}) == 1


def _none_latched(seen):
    return all(r[0] not in LATCHED for r in seen.values())


class _DrillRig:
    """The common start of the latch scenarios: diver = world `client`'s pawn, target = world `victim`'s pawn, placed under the diver in the jump.
    t1 = the end of the stick turn; crouch is held from t1 + 0.03 (and tapped at t1 + 0.05) until a scenario releases it"""

    def __init__(self, name, client, victim, judge_reconciles=True):
        self.name, self.client, self.victim = name, client, victim
        self.key, self.vkey = g.key_of_player(client), g.key_of_player(victim)
        self.run = g._Run(name, client, extra=[('vic', 0, self.vkey)], judge_reconciles=judge_reconciles)
        self.seen = {}
        _spy(self.run, self.key)
        spin, self.t1 = spin_steps(client, 0.55, clockwise=True)
        # The target goes under the diver at the end of the stick turn (t1, ~0.55 s after the press, near the top of the jump: see _teleport_under)
        self.steps = [(0.1, self.immune(False)), (0.2, self.mark('before'))] + jump_steps(client, 0.3) + [(self.t1, self.place_under)] + spin + [
            (self.t1 + 0.03, crouch_hold(client)), (self.t1 + 0.05, crouch_tap(client)), (self.t1 + 0.45, self.mark('latched'))]

    def immune(self, on):
        return lambda: ('drill immunity', on, TestLib.server_set_drill_immunity(actor(0, self.vkey), on))

    def place_under(self):
        return 'target under the diver', _teleport_under(self.key, self.vkey, self.client)

    def mark(self, name):
        def f():
            vic = self.run.latest('vic')
            self.seen[name] = {wi: latch_info(wi, self.key) for wi in range(len(g.worlds()))}
            self.seen[name + '_rb'] = g.rollbacks(self.client)
            self.seen[name + '_hp'] = g.damage_of(0, self.vkey)
            self.seen[name + '_used'] = {wi: (drill(wi, self.key) or (0, 0, 0, 0))[2] for wi in range(len(g.worlds()))}
            return name, self.seen[name]
        return f

    def check_latched(self, t_exit):
        """Every world showed Seating or Boring with one latch frame at some point in [t1, t_exit] (the worlds see it 100-250 ms apart)"""
        return self.run.at('the tip formed the drill latch: every world shows Seating or Boring with one latch frame', lambda: _seen_all(
            self.run, _is_latched, self.t1, t_exit, same_latch=True))

    def rollbacks(self, a, b, limit=DRILL_STEPS_PER_LATCH):
        return self.run.at(f'at most {limit} rollbacks on the driller client during the latch', lambda: (
            self.seen[b + '_rb'] - self.seen[a + '_rb'] <= limit, f"{self.seen[a + '_rb']} -> {self.seen[b + '_rb']}"))

    def max_vz(self, t0, t1, side):
        return max([s['vel'][2] for s in self.run._win(side, t0, t1, sim=True)] or [0.0])

    def end(self, t):
        return [(t, crouch_release(self.client)), (t + 0.5, self.run.finish()), (t + 0.55, _finish(self.run, self.name))]


def scenario_drill_latch_static_multi_hit(client=1, victim=2):
    """The latch on a target that stands still, crouch held for 1 s: every world shows the latch with the same latch frame, the bore lands at least
    three hits (the authority's count, a hit every 6 frames after the 12-frame seat), the target's damage rises on the authority, the driller's client
    stays within the correction budget, and the release ends the latch on every world"""
    rig = _DrillRig('drill_latch_static_multi_hit', client, victim)
    run, t1 = rig.run, rig.t1
    steps = rig.steps + [
        (t1 + 1.1, rig.mark('bored')), (t1 + 1.12, crouch_release(client)), (t1 + 1.6, rig.mark('after')),
        (t1 + 1.65, rig.check_latched(t1 + 1.1)),
        (t1 + 1.65, run.at('still latched after 1 s of crouch on every world', lambda: (_all_latched(rig.seen['bored']), rig.seen['bored']))),
        (t1 + 1.65, run.at('the bore landed at least three hits (authority)', lambda: (rig.seen['bored'][0][2] >= 3, rig.seen['bored']))),
        (t1 + 1.65, run.at("the target's damage rose on the authority", lambda: (
            rig.seen['before_hp'] is not None and rig.seen['bored_hp'] is not None and rig.seen['bored_hp'] > rig.seen['before_hp'] + 1.0,
            f"{rig.seen['before_hp']} -> {rig.seen['bored_hp']}"))),
        (t1 + 1.65, rig.rollbacks('latched', 'bored')),
        (t1 + 1.65, run.at('the release ended the latch on every world', lambda: (_none_latched(rig.seen['after']), rig.seen['after'])))]
    _run(run, steps + rig.end(t1 + 1.7))
    return 'running'


def scenario_drill_release_early_pop(client=1, victim=2):
    """Crouch let go right after the seat: the latch ends on every world with the small pop out (rising, but well under the launch speed) on the
    authority and the driller's client"""
    rig = _DrillRig('drill_release_early_pop', client, victim)
    run, t1 = rig.run, rig.t1
    t_rel = t1 + 0.5
    steps = rig.steps + [
        (t_rel, crouch_release(client)), (t_rel + 0.5, rig.mark('after')),
        (t_rel + 0.55, rig.check_latched(t_rel + 0.1)),
        (t_rel + 0.55, run.at('the release ended the latch on every world', lambda: (_none_latched(rig.seen['after']), rig.seen['after']))),
        (t_rel + 0.55, run.at('the pop: the driller rises after the release, under the launch speed (authority and client)', lambda: (
            all(150.0 < rig.max_vz(t_rel, t_rel + 0.4, side) < LAUNCH_VZ for side in ('srv', 'cli')),
            f"max vz srv {rig.max_vz(t_rel, t_rel + 0.4, 'srv'):.0f}, cli {rig.max_vz(t_rel, t_rel + 0.4, 'cli'):.0f}"))),
        (t_rel + 0.55, rig.rollbacks('latched', 'after'))]
    _run(run, steps + rig.end(t_rel + 0.6))
    return 'running'


def scenario_drill_launch_after_bore(client=1, victim=2):
    """After 0.6 s of bore, jump is pressed and crouch let go two frames later (inside the 8-frame boost window): the exit is the launch, at about
    the launch speed on the authority and the driller's client, and it gives the air spin back"""
    rig = _DrillRig('drill_launch_after_bore', client, victim)
    run, t1 = rig.run, rig.t1
    t_rel = t1 + 0.8
    steps = rig.steps + [
        (t_rel - 0.035, lambda: ('jump button', g.combat_tap(client, g.button_mask('Jump')))), (t_rel, crouch_release(client)), (t_rel + 0.6, rig.mark('after')),
        (t_rel + 0.65, rig.check_latched(t_rel + 0.1)),
        (t_rel + 0.65, run.at('the launch: the driller leaves at about the launch speed (authority and client)', lambda: (
            all(rig.max_vz(t_rel - 0.05, t_rel + 0.4, side) > LAUNCH_VZ for side in ('srv', 'cli')),
            f"max vz srv {rig.max_vz(t_rel - 0.05, t_rel + 0.4, 'srv'):.0f}, cli {rig.max_vz(t_rel - 0.05, t_rel + 0.4, 'cli'):.0f}"))),
        (t_rel + 0.65, run.at('the launch gave the air spin back (every world)', lambda: (
            all(u == 0 for u in rig.seen['after_used'].values()) and _none_latched(rig.seen['after']), (rig.seen['after_used'], rig.seen['after'])))),
        (t_rel + 0.65, rig.rollbacks('latched', 'after'))]
    _run(run, steps + rig.end(t_rel + 0.7))
    return 'running'


def scenario_drill_target_destroyed(client=1, victim=2):
    """The latched target is knocked out by a small KO zone around it (not reaching the driller sitting on top) while crouch is still held: the latch
    ends on every world with the launch (target gone), on the authority and the driller's client. The KO is an out-of-band event: reconciles are
    budgeted, not judged"""
    rig = _DrillRig('drill_target_destroyed', client, victim, judge_reconciles=False)
    run, t1 = rig.run, rig.t1
    t_ko = t1 + 0.6

    def zone():
        v = actor(0, rig.vkey).get_actor_location()
        for w in g.worlds():
            unreal.GSTN_KOZone.spawn_zone(w, v, unreal.Vector(60.0, 60.0, 50.0))
        return 'KO zone on the target', (round(v.x), round(v.y), round(v.z))

    def clear():
        for w in g.worlds():
            unreal.GSTN_KOZone.destroy_all_zones(w)
        g.comps(g.worlds()[0])[rig.vkey].server_reset_test_state(unreal.Vector(0.0, 0.0, 0.0))
        return 'zones removed, target respawned'

    steps = rig.steps + [
        (t_ko, zone), (t_ko + 0.6, rig.mark('after')), (t_ko + 0.62, clear),
        (t_ko + 0.65, rig.check_latched(t_ko + 0.1)),
        (t_ko + 0.65, run.at('the latch ended on every world after the KO', lambda: (_none_latched(rig.seen['after']), rig.seen['after']))),
        (t_ko + 0.65, run.at('target gone: the exit is the launch (authority and client)', lambda: (
            all(rig.max_vz(t_ko, t_ko + 0.5, side) > LAUNCH_VZ for side in ('srv', 'cli')),
            f"max vz srv {rig.max_vz(t_ko, t_ko + 0.5, 'srv'):.0f}, cli {rig.max_vz(t_ko, t_ko + 0.5, 'cli'):.0f}"))),
        (t_ko + 0.65, rig.rollbacks('latched', 'after'))]
    _run(run, steps + rig.end(t_ko + 0.7))
    return 'running'


def scenario_drill_latch_moving_enemy(client=1, victim=2):
    """D10: the latched target walks forward for 0.8 s while the driller holds crouch: the latch holds on every world, the target moved on the authority
    (the driller rides it: the parked transform plus the per-latch anchor), and the driller's client stays within the correction budget of the latch"""
    rig = _DrillRig('drill_latch_moving_enemy', client, victim, judge_reconciles=False)
    run, t1 = rig.run, rig.t1
    pos = {}

    def where(name):
        def f():
            pos[name] = (actor(0, rig.vkey).get_actor_location(), actor(0, rig.key).get_actor_location())
            return name, 'positions'
        return f

    def walked():
        (v0, d0), (v1, d1) = pos['walk_start'], pos['walk_end']
        dv, dd = (v1 - v0).length(), (d1 - d0).length()
        return dv > 100.0 and abs(dv - dd) < 60.0, f'target moved {dv:.0f} cm, driller {dd:.0f} cm'

    steps = rig.steps + [
        (t1 + 0.5, where('walk_start')), (t1 + 0.5, lambda: ('walk', g.hold(victim, 'move', (0.0, 1.0, 0.0)))),
        (t1 + 1.3, rig.mark('walked')), (t1 + 1.3, where('walk_end')), (t1 + 1.3, lambda: ('stop', g.release(victim, 'move'))),
        (t1 + 1.35, rig.check_latched(t1 + 1.3)),
        (t1 + 1.35, run.at('still latched after the walk on every world', lambda: (_all_latched(rig.seen['walked']), rig.seen['walked']))),
        (t1 + 1.35, run.at('the target walked and the driller rode it (authority)', walked)),
        (t1 + 1.35, rig.rollbacks('latched', 'walked'))]
    _run(run, steps + rig.end(t1 + 1.4))
    return 'running'


def scenario_drill_hit_while_latched_detaches(client=1, victim=2, hitter=0):
    """The third pawn (the host) is placed next to the latched driller, facing it, and attacks: the hit detaches the driller (hitstun wins, 7.4),
    the latch ends on every world and the driller's damage rises on the authority. Hits by another player are out-of-band for the driller's
    client: reconciles are budgeted, not judged"""
    rig = _DrillRig('drill_hit_while_latched_detaches', client, victim, judge_reconciles=False)
    run, t1 = rig.run, rig.t1
    hkey = g.key_of_player(hitter)
    hp = {}
    t_hit = t1 + 0.6

    def place_hitter():
        d = actor(0, rig.key).get_actor_location()
        yaw = math.radians(g.snap(g.worlds()[0])[hkey]['yaw'])
        spot = unreal.Vector(d.x - 70.0 * math.cos(yaw), d.y - 70.0 * math.sin(yaw), d.z)
        hp['before'] = g.damage_of(0, rig.key)
        return 'hitter placed', g.comps(g.worlds()[0])[hkey].server_teleport_exact(spot)

    def driller_hp():
        hp['after'] = g.damage_of(0, rig.key)
        return 'driller hp', hp

    steps = rig.steps + [
        (t_hit, place_hitter), (t_hit + 0.03, lambda: ('attack', g.combat_tap(hitter, g.button_mask('Attack')))),
        (t_hit + 0.7, rig.mark('after')), (t_hit + 0.7, driller_hp),
        (t_hit + 0.75, rig.check_latched(t_hit + 0.1)),
        (t_hit + 0.75, run.at('the hit ended the latch on every world', lambda: (_none_latched(rig.seen['after']), rig.seen['after']))),
        (t_hit + 0.75, run.at("the driller's damage rose on the authority", lambda: (
            hp.get('before') is not None and hp.get('after') is not None and hp['after'] > hp['before'] + 1.0, hp))),
        (t_hit + 0.75, rig.rollbacks('latched', 'after'))]
    _run(run, steps + rig.end(t_hit + 0.8))
    return 'running'
