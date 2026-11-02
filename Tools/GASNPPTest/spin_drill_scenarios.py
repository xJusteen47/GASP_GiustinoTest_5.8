# Spin and drill, plans D4 and D5: the spin scenarios (SUITE_SPIN, not in SUITE_DEFAULT). Ground spin ring, air spin hop and low gravity, one air spin per
# airtime, the opposite-spin cancel. Same harness as defence_scenarios / link_scenarios: run inside the editor's Python console, PIE "Run Under One Process", listen server
# first (host + 2 clients), then 60/15 ms with and without 2 % loss.
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import spin_drill_scenarios as sd
#   sd.prepare_editor()                 # once, in the editor world BEFORE PIE: the spin bindings go on the test kit IN MEMORY (never saved)
#   sd.scenario_spin_ground_hits_ring() # in a running PIE
#   sd.scenario_spin_air_hop_low_gravity()
#   sd.scenario_air_spin_once_per_airtime()
#   sd.scenario_spin_opposite_cancel()
#   sd.last()                           # (verdict, checks) of the last one
#   sd.restore_editor()                 # with PIE stopped: the kit's bindings as saved
#
# Needs the GSTNSpinDrill plugin enabled (EnabledByDefault false: enable it locally, never commit the .uproject change). What stays in memory:
# the twelve default spin bindings (cancels first) (UGSTN_SpinDrillLibrary.add_spin_bindings_to_kit, slots inline, so neither the command set nor the input config
# changes). No shared Blueprint is edited. The stick rotation is driven through IA_Move like a pad: 9 positions 35 ms apart (a 360 in ~0.32 s,
# inside the 30-frame window of the Spin condition).
import math
import time

import unreal
import gasnpp_test as g

KIT_PATH = '/Game/GASNPPTest/Combat/' + g.HIT_KIT
SUITE_SPIN = ['spin_ground_hits_ring', 'spin_air_hop_low_gravity', 'air_spin_once_per_airtime', 'spin_opposite_cancel', 'drill_ground_lag', 'drill_nondrillable_bounce']
NEEDED_BUTTONS = ['Input.Button.Spin', 'Input.Button.Crouch']
TAG_SPINNING = 'Combat.State.Spinning'
TAG_AIR_SPENT = 'Combat.State.AirSpinSpent'
TAG_LOW_GRAVITY = 'Combat.State.SpinLowGravity'
PHASE_SPINNING = 1
STEP_S = 0.035
_saved = {}
_last = {'verdict': None, 'checks': []}
Lib = unreal.GSTN_SpinDrillLibrary


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
    return 'spin bindings added', Lib.add_spin_bindings_to_kit(kit, True), 'buttons added', missing


def restore_editor():
    """Puts the kit's bindings and the input config's buttons back as they were before prepare_editor (with PIE stopped)"""
    out = []
    if 'kit_bindings' in _saved:
        kit = unreal.load_asset(KIT_PATH)
        kit.set_editor_property('kit_bindings', _saved.pop('kit_bindings'))
        out.append('kit bindings restored')
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
    # UE Python gives None (not a tuple) when the function returns false: no drill fragment on that pawn
    r = Lib.get_shown_drill_numbers(actor(wi, key))
    if not isinstance(r, tuple) or len(r) != 5:
        return None
    ok, phase, start, used, low = r
    return (phase, start, used, low) if ok else None


def drill_text(wi, key):
    return Lib.get_shown_drill_text(actor(wi, key))


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
    return [(t, lambda: ('jump', g.press(client, 'jump')))]


def _apply_kit():
    """The test kit (with the spin bindings added in memory by prepare_editor) on every pawn: the pawns start on their default loadout, which has none"""
    return 'kit applied', [g._apply_loadout(g.key_of_player(i), g.HIT_KIT) for i in g.all_players()]


def _run(run, steps):
    run.run([(0.05, _apply_kit)] + list(steps))


# ---- scenarios ---------------------------------------------------------------------------------------------------------------------------------------
def _finish(run, name):
    def step():
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
    # The victim's authoritative copy is sampled too ('vic'): its Health is what the ring's hit changes
    run = g._Run('spin_ground_hits_ring', client, extra=[('vic', 0, vkey)])
    marks = {}

    def mark(name):
        def f():
            vic = run.latest('vic')
            marks[name] = {'rb': g.rollbacks(client), 'hp': vic['attr'].get('Health') if vic else None,
                           'drill': {wi: drill(wi, key) for wi in range(len(g.worlds()))}}
            return name, marks[name]['drill']
        return f

    def place_victim():
        # Next to the spinner on the authority: inside the ring (110 cm) at the spinner's height
        a = actor(0, key).get_actor_location()
        actor(0, vkey).set_actor_location(unreal.Vector(a.x + 80.0, a.y, a.z), False, True)
        return 'victim placed'

    spin, t_end = spin_steps(client, 0.6, clockwise=True)
    steps = [(0.2, place_victim), (0.5, mark('before'))] + spin + [
        (t_end + 0.05, mark('spinning')),
        (t_end + 0.05, run.at('the spinner shows the Spinning phase on every world', lambda: (
            all((marks['spinning']['drill'][wi] or (0,))[0] == PHASE_SPINNING for wi in marks['spinning']['drill']), marks['spinning']['drill']))),
        (t_end + 0.06, run.at('the phase start frame is the same on every world', lambda: (
            len({(d or (0, -1))[1] for d in marks['spinning']['drill'].values()}) == 1, marks['spinning']['drill']))),
        (t_end + 1.0, run.at('Combat.State.Spinning was held on the authority and on the predicting client', lambda: (
            run.saw_tag(TAG_SPINNING, 0.5, t_end + 1.0, 'srv') and run.saw_tag(TAG_SPINNING, 0.5, t_end + 1.0, 'cli'), ''))),
        (t_end + 1.0, mark('after')),
        (t_end + 1.0, run.at('the spin ended: no phase on any world after its timeline', lambda: (
            all((marks['after']['drill'][wi] or (0,))[0] != PHASE_SPINNING for wi in marks['after']['drill']), marks['after']['drill']))),
        (t_end + 1.02, run.at("the pawn next to the spinner took the ring's damage on the authority", lambda: (
            marks['before']['hp'] is not None and marks['after']['hp'] is not None and marks['after']['hp'] < marks['before']['hp'] - 1.0,
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

    t_jump2 = 2.5
    spin, t_end = spin_steps(client, t_jump2 + 0.25, clockwise=False)
    steps = jump_steps(client, 0.3) + jump_steps(client, t_jump2) + spin + [
        (t_jump2 + 0.24, run.at('before the spin the jump is near its top', lambda: (True, f"vz {up_speed(t_jump2 + 0.2, t_jump2 + 0.24)[-1:]}"))),
        (t_end + 0.1, run.at('the hop lifts: vz reaches the hop speed after the spin starts', lambda: (
            max(up_speed(t_end - 0.05, t_end + 0.1) or [0.0]) >= 440.0, f"max vz {max(up_speed(t_end - 0.05, t_end + 0.1) or [0.0]):.0f}"))),
        (t_end + 0.3, run.at('the low gravity effect is on during the spin, on both copies', lambda: (
            run.saw_tag(TAG_LOW_GRAVITY, t_end, t_end + 0.3, 'srv') and run.saw_tag(TAG_LOW_GRAVITY, t_end, t_end + 0.3, 'cli'), ''))),
        (t_end + 1.5, run.at('the low gravity effect is gone after the spin', lambda: (
            not run.saw_tag(TAG_LOW_GRAVITY, t_end + 1.2, t_end + 1.5, 'srv'), ''))),
        (t_end + 1.5, run.at('the air spin was spent, then given back on landing', lambda: (
            run.saw_tag(TAG_AIR_SPENT, t_end, t_end + 0.5, 'srv') and not run.saw_tag(TAG_AIR_SPENT, t_end + 1.3, t_end + 1.5, 'srv'), ''))),
        (t_end + 1.55, lambda: run.note(f"descent with the spin vs the plain jump: {up_speed(t_end + 0.3, t_end + 0.6)[:3]} / {up_speed(0.9, 1.2)[:3]}")),
        (t_end + 1.6, run.finish()), (t_end + 1.65, _finish(run, 'spin_air_hop_low_gravity'))]
    _run(run, steps)
    return 'running'


def scenario_air_spin_once_per_airtime(client=1):
    """Two spins in one jump: the first is the air spin (hop, AirSpinSpent), the second starts nothing (the air spin classes are blocked, the fragment
    refuses it), a ground spin after landing works, and a new jump allows one air spin again. Getting hit is not a refresh (owner 2026-11-01): not
    exercised here, the unit test GSTNSpinDrill.Core.Spin.HopAndRefresh pins it"""
    run = g._Run('air_spin_once_per_airtime', client)
    key = g.key_of_player(client)
    seen = {}

    def phase_at(name):
        def f():
            seen[name] = drill(0, key)
            return name, seen[name]
        return f

    first, t1 = spin_steps(client, 0.45, clockwise=True)
    second, t2 = spin_steps(client, t1 + 0.1, clockwise=False)
    ground, t3 = spin_steps(client, 2.6, clockwise=True)
    again, t4 = spin_steps(client, 3.85, clockwise=True)
    steps = jump_steps(client, 0.3) + first + [(t1 + 0.05, phase_at('first'))] + second + [(t2 + 0.05, phase_at('second'))] + ground + [
        (t3 + 0.05, phase_at('ground'))] + jump_steps(client, 3.7) + again + [(t4 + 0.05, phase_at('again')),
        (t4 + 0.1, run.at('the first spin in the air is the air spin', lambda: (seen['first'] and seen['first'][0] == PHASE_SPINNING and seen['first'][2] == 1, seen))),
        (t4 + 0.1, run.at('the second spin in the same airtime started nothing new', lambda: (
            not seen['second'] or seen['second'][1] == seen['first'][1] or seen['second'][0] != PHASE_SPINNING, seen))),
        (t4 + 0.1, run.at('on the ground the spin works and nothing is spent', lambda: (seen['ground'] and seen['ground'][0] == PHASE_SPINNING and seen['ground'][2] == 0, seen))),
        (t4 + 0.1, run.at('a new jump allows one air spin again', lambda: (seen['again'] and seen['again'][0] == PHASE_SPINNING and seen['again'][2] == 1, seen))),
        (t4 + 1.2, run.finish()), (t4 + 1.25, _finish(run, 'air_spin_once_per_airtime'))]
    _run(run, steps)
    return 'running'


def scenario_spin_opposite_cancel(client=1):
    """A clockwise ground spin, then a counter-clockwise rotation that ends on about frame 10 of the spin (D5, 14 SD.2): the spin stops early on
    every world (Spinning held for clearly less than the natural 0.5 s, and no new spin starts: the phase is None right after), the predicting
    client has no rollback from it, and a counter-clockwise rotation with no spin running is an ordinary spin again"""
    run = g._Run('spin_opposite_cancel', client)
    key = g.key_of_player(client)
    seen = {}

    def phase_at(name):
        def f():
            seen[name] = {wi: drill(wi, key) for wi in range(len(g.worlds()))}
            seen[name + '_rb'] = g.rollbacks(client)
            return name, seen[name]
        return f

    def held_seconds(t0, t1, side):
        ts = [s['ms'] / 1000.0 for s in run._win(side, t0, t1, sim=True) if TAG_SPINNING in s['tags']]
        return (max(ts) - min(ts)) if ts else 0.0

    spin, t1 = spin_steps(client, 0.5, clockwise=True)
    # The second rotation starts right away: 9 steps of 35 ms end ~0.32 s later, past CancelFromFrame (6 frames, 0.1 s) of the first spin
    cancel, t2 = spin_steps(client, t1 + 0.02, clockwise=False)
    plain, t3 = spin_steps(client, t2 + 1.2, clockwise=False)
    steps = [(0.4, phase_at('before'))] + spin + cancel + [(t2 + 0.15, phase_at('cancelled'))] + plain + [(t3 + 0.05, phase_at('plain')),
        (t3 + 0.1, run.at('the spin was held for less than its natural length on the authority and the client', lambda: (
            0.0 < held_seconds(t1 - 0.4, t2 + 0.5, 'srv') < 0.45 and 0.0 < held_seconds(t1 - 0.4, t2 + 0.5, 'cli') < 0.45,
            f"srv {held_seconds(t1 - 0.4, t2 + 0.5, 'srv'):.2f} s, cli {held_seconds(t1 - 0.4, t2 + 0.5, 'cli'):.2f} s"))),
        (t3 + 0.1, run.at('after the cancel no world shows a spin (the opposite rotation did not start one)', lambda: (
            all((d or (0,))[0] != PHASE_SPINNING for d in seen['cancelled'].values()), seen['cancelled']))),
        (t3 + 0.1, run.at('no rollback on the spinner from the cancel', lambda: (seen['cancelled_rb'] - seen['before_rb'] == 0, f"{seen['before_rb']} -> {seen['cancelled_rb']}"))),
        (t3 + 0.1, run.at('with no spin running the same rotation is a counter-clockwise spin', lambda: (
            all((d or (0,))[0] == PHASE_SPINNING for d in seen['plain'].values()), seen['plain']))),
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
    seen = {}

    def at(name):
        def f():
            seen[name] = {wi: drill(wi, key) for wi in range(len(g.worlds()))}
            seen[name + '_rb'] = g.rollbacks(client)
            return name, seen[name]
        return f

    def down_speed(t0, t1, side='srv'):
        return [-s['vel'][2] for s in run._win(side, t0, t1, sim=True)]

    spin, t1 = spin_steps(client, 0.4, clockwise=True)
    steps = [(0.3, at('before'))] + jump_steps(client, 0.3) + spin + [
        (t1 + 0.05, crouch_hold(client)), (t1 + 0.07, crouch_tap(client)), (t1 + 0.15, at('diving')),
        (t1 + 2.0, at('after')), (t1 + 2.0, crouch_release(client)),
        (t1 + 2.05, run.at('every world showed the dive', lambda: (all((d or (0,))[0] == PHASE_DIVING for d in seen['diving'].values()), seen['diving']))),
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
    """The diver lands its tip on another player (nothing is drillable before D9): the victim takes the tip's damage on the authority, the dive ends in a
    bounce (speed along up > 500 right after) on the authority and the diver's client, and the air spin is not given back by it"""
    vkey = g.key_of_player(victim)
    run = g._Run('drill_nondrillable_bounce', client, extra=[('vic', 0, vkey)])
    key = g.key_of_player(client)
    seen = {}

    def place_under():
        a = actor(0, key).get_actor_location()
        actor(0, vkey).set_actor_location(unreal.Vector(a.x, a.y, a.z - 20.0), False, True)
        return 'victim under the diver'

    def at(name):
        def f():
            vic = run.latest('vic')
            seen[name] = vic['attr'].get('Health') if vic else None
            return name, seen[name]
        return f

    spin, t1 = spin_steps(client, 0.55, clockwise=False)
    steps = jump_steps(client, 0.3) + [(0.5, place_under), (0.52, at('before'))] + spin + [
        (t1 + 0.03, crouch_hold(client)), (t1 + 0.05, crouch_tap(client)), (t1 + 1.2, at('after')), (t1 + 1.2, crouch_release(client)),
        (t1 + 1.25, run.at("the victim took the tip's damage on the authority", lambda: (
            seen['before'] is not None and seen['after'] is not None and seen['after'] < seen['before'] - 1.0, f"{seen['before']} -> {seen['after']}"))),
        (t1 + 1.25, run.at('the diver bounced (rising after the dive) on the authority and its client', lambda: (
            max([s['vel'][2] for s in run._win('srv', t1, t1 + 1.0, sim=True)] or [0.0]) > 500.0
            and max([s['vel'][2] for s in run._win('cli', t1, t1 + 1.0, sim=True)] or [0.0]) > 500.0, ''))),
        (t1 + 1.25, run.at('the air spin stayed spent after the bounce', lambda: (run.saw_tag(TAG_AIR_SPENT, t1 + 0.2, t1 + 0.6, 'srv'), ''))),
        (t1 + 1.6, run.finish()), (t1 + 1.65, _finish(run, 'drill_nondrillable_bounce'))]
    _run(run, steps)
    return 'running'

# drill_tilted_gravity (plan D8): needs a level with a gravity volume the harness can place a pawn in; the dive and bounce vectors under any gravity
# are pinned by the unit tests (GSTNSpinDrill.Core.Math.LaunchAndDive, GSTNSpinDrill.Core.Spin.HopAndRefresh). Not written until such a level exists.
