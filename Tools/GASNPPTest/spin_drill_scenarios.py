# Spin and drill, plan D4: the spin scenarios (SUITE_SPIN, not in SUITE_DEFAULT). Ground spin ring, air spin hop and low gravity, one air spin per
# airtime. Same harness as defence_scenarios / link_scenarios: run inside the editor's Python console, PIE "Run Under One Process", listen server
# first (host + 2 clients), then 60/15 ms with and without 2 % loss.
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import spin_drill_scenarios as sd
#   sd.prepare_editor()                 # once, in the editor world BEFORE PIE: the spin bindings go on the test kit IN MEMORY (never saved)
#   sd.scenario_spin_ground_hits_ring() # in a running PIE
#   sd.scenario_spin_air_hop_low_gravity()
#   sd.scenario_air_spin_once_per_airtime()
#   sd.last()                           # (verdict, checks) of the last one
#   sd.restore_editor()                 # with PIE stopped: the kit's bindings as saved
#
# Needs the GSTNSpinDrill plugin enabled (EnabledByDefault false: enable it locally, never commit the .uproject change). What stays in memory:
# the eight default spin bindings (UGSTN_SpinDrillLibrary.add_spin_bindings_to_kit, slots inline, so neither the command set nor the input config
# changes). No shared Blueprint is edited. The stick rotation is driven through IA_Move like a pad: 9 positions 35 ms apart (a 360 in ~0.32 s,
# inside the 30-frame window of the Spin condition).
import math
import time

import unreal
import gasnpp_test as g

KIT_PATH = '/Game/GASNPPTest/Combat/' + g.HIT_KIT
SUITE_SPIN = ['spin_ground_hits_ring', 'spin_air_hop_low_gravity', 'air_spin_once_per_airtime']
TAG_SPINNING = 'Combat.State.Spinning'
TAG_AIR_SPENT = 'Combat.State.AirSpinSpent'
TAG_LOW_GRAVITY = 'Combat.State.SpinLowGravity'
PHASE_SPINNING = 1
STEP_S = 0.035
_saved = {}
_last = {'verdict': None, 'checks': []}
Lib = unreal.GSTN_SpinDrillLibrary


# ---- content, in memory ------------------------------------------------------------------------------------------------------------------------
def prepare_editor():
    """Adds the default spin bindings to the test kit (in memory). Run in the editor world before PIE: the loadout is compiled when a pawn starts"""
    kit = unreal.load_asset(KIT_PATH)
    _saved.setdefault('kit_bindings', list(kit.get_editor_property('kit_bindings')))
    return 'spin bindings added', Lib.add_spin_bindings_to_kit(kit, True)


def restore_editor():
    """Puts the kit's bindings back as they were before prepare_editor (with PIE stopped)"""
    if 'kit_bindings' not in _saved:
        return 'nothing to restore'
    kit = unreal.load_asset(KIT_PATH)
    kit.set_editor_property('kit_bindings', _saved.pop('kit_bindings'))
    return 'kit bindings restored'


def last():
    return _last['verdict'], _last['checks']


# ---- reads -----------------------------------------------------------------------------------------------------------------------------------------
def actor(wi, key):
    c = g.comps(g.worlds()[wi]).get(key)
    return c.get_owner() if c else None


def drill(wi, key):
    """(phase, phase start frame, air spins used, low gravity until) of the pawn `key` as world `wi` shows it, None without a fragment"""
    ok, phase, start, used, low = Lib.get_shown_drill_numbers(actor(wi, key))
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
    run.run(steps)
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
    run.run(steps)
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
    run.run(steps)
    return 'running'
