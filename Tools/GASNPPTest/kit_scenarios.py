# M11 kit scenarios (kits on the ground, the kit swap on the grab button). Run with the M10/M11 driver (module=kit_scenarios) or
# defence_scenarios.defence_batch([...], module=kit_scenarios). K4 adds the full kit_pickup / kit_throw set; K1 ships the swap check.
import re

import unreal
import gasnpp_test as g

SUITE_KIT = ['kit_swap_hold', 'kit_throw_stick_forward', 'kit_throw_stick_back', 'kit_throw_lifted']
# M11 K3: the three hub archetypes (Tools/GAS/setup_test_kit_archetypes.py)
SUITE_ARCHETYPES = ['kit_archetype_stats', 'kit_archetype_throw_light', 'kit_archetype_throw_medium', 'kit_archetype_lift_heavy']
_SWAP = re.compile(r'KitSwap (\S+) frame=(\d+) (took|refused) kit=(\S+) left=(\S+) at (\S+) \((focus|nearest)\)')


def _host_actor(key):
    c = g.comps(g.worlds()[0]).get(key)
    return c.get_owner() if c else None


def _kit_of(wi, key):
    return str((g._loadout_view(wi, key)[0] or {}).get('kit'))


def scenario_kit_swap_hold(client=1, kit='KIT_Test_Link'):
    """M11 K1: a kit on the ground in front of the client's pawn; a short hold of the grab button (about 15 frames) swaps nothing; a hold longer
    than KitSwap.HoldSeconds (0.5 s) released swaps: the host log names the kit the probe framed (focus), the authority and the owner end on
    `kit`, and the pickup keeps the previous kit (KIT_Test_Hits). (The sampler keeps only Test.* tags, so Interact.Target.Kit is judged through
    the focus the host's swap reports, not sampled.)"""
    run = g._Run('kit_swap_hold', client, judge_reconciles=False)
    key = run.key
    interact = g.button_mask('Interact')
    st = {'log': g.mark_log()}

    def log():
        return g._read_log_since(st['log'])

    def spawn():
        pawn = _host_actor(key)
        loc, fwd = pawn.get_actor_location(), pawn.get_actor_forward_vector()
        where = unreal.Vector(loc.x + fwd.x * 90.0, loc.y + fwd.y * 90.0, loc.z - 70.0)
        st['pickup'] = g._srv(key).server_spawn_kit_pickup(kit, '', where, 0.0)
        return st['pickup']

    def no_swap():
        swaps = _SWAP.findall(log())
        return not swaps and kit not in _kit_of(0, key), f'swaps {swaps}, kit {_kit_of(0, key)}'

    def swapped():
        swaps = _SWAP.findall(log())
        ok = len(swaps) == 1 and swaps[0][2] == 'took' and kit in swaps[0][3] and 'KIT_Test_Hits' in swaps[0][4] and swaps[0][6] == 'focus'
        return ok, f'swaps {swaps}'

    def kit_everywhere():
        kits = [_kit_of(0, key), _kit_of(client, key)]
        return all(kit in k for k in kits), f'authority {kits[0]}, owner {kits[1]}'

    def cleanup():
        for a in unreal.GameplayStatics.get_all_actors_of_class(g.worlds()[0], unreal.GSTN_KitPickup):
            a.destroy_actor()
        return g.combat_clear(client)

    steps = [
        (0.3, lambda: ('start kit', g._apply_loadout(key, 'KIT_Test_Hits'))),
        (1.5, lambda: ('spawn kit', spawn())),
        (3.1, lambda: ('short hold', g.combat_hold(client, interact))),
        (3.35, lambda: ('release', g.combat_clear(client))),
        (4.3, run.at('a short hold swaps nothing', no_swap)),
        (4.5, lambda: ('long hold', g.combat_hold(client, interact))),
        (5.3, lambda: ('release', g.combat_clear(client))),
        (6.5, run.at('the long hold swapped the framed kit (host log)', swapped)),
        (6.6, run.at('authority and owner carry the new kit', kit_everywhere)),
        (6.7, lambda: ('cleanup', cleanup())),
        (7.5, run.finish()),
    ]
    run.run(steps)


# ------------------------------------------------------------------------------------------------ M11 K2: kit throws picked by the stick
def _lifting(on):
    """Thrown enemies (GSTNHeld ThrownEnemies.bEnabled) on or off on the settings every world of the PIE process reads, so a standard fighter is
    grabbable but not liftable and the grab keeps the Smash throws. (A Combat.Held.NoLift loose tag added from Python does not reach the lift
    check: tried, the pawn shows no such tag.) Restored by the scenario's end"""
    settings = unreal.get_default_object(unreal.GSTN_HeldSettings)
    tuning = settings.get_editor_property('thrown_enemies')
    tuning.set_editor_property('enabled', on)
    settings.set_editor_property('thrown_enemies', tuning)
    return 'thrown enemies', on


def _kit_throw(name, direction, lifted, pairing, kit=None):
    """A grabs B (the link rig: the harness kit with the link test bindings, compiled with the project's ThrowInputMode, Stick by default), then
    pushes the stick toward B (forward) or away from B (back), with no button. Not lifted (thrown enemies off for the scenario): the stick alone throws,
    the link ends with reason Throw on every world (no watchdog break: not the grab's timeout escape). Lifted (B liftable, the default test weights): the held
    throw owns the stick, so the push throws nothing and the link holds (KitThrowsWhileLifting NotStickOnly); a press of Crouch then throws (the
    Smash down throw, not stick-only)"""
    import link_scenarios as ls
    rig = ls._GrabRig(name, pairing)
    run = rig.run
    st = {}

    def loc(key):
        return ls.actor(0, key).get_actor_location()

    def push():
        a, b = loc(rig.akey), loc(rig.bkey)
        st['a'], st['dir'] = a, unreal.Vector(b.x - a.x, b.y - a.y, 0.0).normal()
        target = (b.x, b.y) if direction == 'forward' else (2 * a.x - b.x, 2 * a.y - b.y)
        value = g._steer_value(rig.ia, target)
        return 'stick', direction, value, g.hold(rig.ia, 'move', value)

    def side():
        b = loc(rig.bkey)
        along = (b.x - st['a'].x) * st['dir'].x + (b.y - st['a'].y) * st['dir'].y
        ok = along > 120.0 if direction == 'forward' else along < -60.0
        return ok, f'B along the A->B line: {along:.0f} cm (stick {direction})'

    steps = [(0.2, rig.setup())]
    t0 = 1.2
    if kit:
        # K3: A swaps to an archetype after the rig's link kit (the server refuses a change within 30 frames of the last); its own carry
        # capacity decides the lift, so thrown enemies stay on
        def kit_on():
            kits = [_kit_of(0, rig.akey), _kit_of(rig.ia, rig.akey)]
            return all(kit in k for k in kits), f'authority {kits[0]}, owner {kits[1]}'
        steps += [(1.0, lambda: ('archetype on A', kit, g._apply_loadout(rig.akey, kit))), (1.9, run.at(f'A carries {kit} on the authority and its owner', kit_on))]
        t0 = 2.1
    elif not lifted:
        steps += [(0.6, lambda: _lifting(False))]
    grab, t = rig.grab_steps(t0)
    steps += grab
    def tags():
        rows = {wi: g._has_tag(ls.actor(wi, rig.akey), 'Combat.Held.CarryingEnemy') for wi in range(len(g.worlds()))}
        if kit:
            return rows[0] == lifted, f'A lifting per world: {rows} (expected {lifted}: carry capacity against the weight 100 of B)'
        return run.note(f'A lifting per world: {rows}')

    steps += [(t + 0.1, run.at('A lifts B only when its kit can carry B', tags) if kit else tags), (t + 0.2, push), (t + 0.6, lambda: ('stick released', g.release(rig.ia, 'move')))]
    if lifted:
        steps += [(t + 1.2, rig.formed('lifted: the stick alone threw nothing, the link holds on every world')),
                  (t + 1.3, lambda: ('crouch tap', g.combat_tap(rig.ia, g.button_mask('Crouch')))),
                  (t + 3.2, rig.gone('lifted: crouch (the down throw) ended the link on every world')), (t + 3.2, rig.reason('Throw'))]
        end = t + 3.3
    else:
        steps += [(t + 2.4, rig.gone('the stick alone threw: the link is gone on every world')), (t + 2.4, rig.reason('Throw')),
                  # A note, not a check: the harness kit binds the same test throw to every direction and the harness pawns have no knockback move
                  # effect, so B does not land on the stick's side; which throw a stick direction picks is GSTNLink.Core.KitThrows.StickPicksThrow
                  (t + 2.5, lambda: run.note(side()[1])),
                  (t + 2.6, rig.equal_texts('the link states are equal on every world after the throw')),
                  (t + 2.7, lambda: _lifting(True) if not kit else ('thrown enemies untouched',))]
        end = t + 2.8
    steps += [(end - 0.05, lambda: ('cleared', g.release(rig.ia, 'move'), g.combat_clear(rig.ia)))]
    steps += rig.end(end)
    run.run(steps)


def scenario_kit_throw_stick_forward(pairing='client_grabs_client'):
    _kit_throw('kit_throw_stick_forward', 'forward', False, pairing)


def scenario_kit_throw_stick_back(pairing='client_grabs_client'):
    _kit_throw('kit_throw_stick_back', 'back', False, pairing)


def scenario_kit_throw_lifted(pairing='client_grabs_client'):
    _kit_throw('kit_throw_lifted', 'forward', True, pairing)


# ------------------------------------------------------------------------------------------------ M11 K3: the three hub archetypes
ARCHETYPE_WALK = {'KIT_Test_Light': 760.0, 'KIT_Test_Medium': 600.0, 'KIT_Test_Heavy': 430.0, 'KIT_Test_Hits': 600.0}


def scenario_kit_archetype_stats(client=1):
    """M11 K3: the client's pawn takes the light, the medium and the heavy kit, then KIT_Test_Hits again (1.5 s apart: the server refuses a change
    within 30 frames of the last). After each: the authority and the owner carry the kit, with the kit's WalkSpeed attribute and the same table
    checksum (the archetype's weapon jab, special and throws compiled the same on both)"""
    run = g._Run('kit_archetype_stats', client, judge_reconciles=False)
    key = run.key

    def check(kit):
        def f():
            rows = []
            ok = True
            for wi in (0, client):
                loadout, combat, stats = g._loadout_view(wi, key)
                walk = stats.get('walkSpeedStat')
                ok &= kit in str(loadout.get('kit')) and walk is not None and abs(walk - ARCHETYPE_WALK[kit]) < 0.5
                rows.append((loadout.get('kit'), walk, combat.get('tableCrc')))
            ok &= rows[0][2] == rows[1][2]
            return ok, f'(kit, WalkSpeed, table) authority {rows[0]}, owner {rows[1]}'
        return f

    steps = []
    t = 0.3
    for kit in ('KIT_Test_Light', 'KIT_Test_Medium', 'KIT_Test_Heavy', 'KIT_Test_Hits'):
        steps += [(t, (lambda k: lambda: ('apply', k, g._apply_loadout(key, k)))(kit)), (t + 1.3, run.at(f'{kit}: kit, WalkSpeed and table on both', check(kit)))]
        t += 1.6
    steps += [(t, run.finish())]
    run.run(steps)


def scenario_kit_archetype_throw_light(pairing='client_grabs_client'):
    """The light kit (carry 60) cannot lift a standard fighter (weight 100): the stick alone throws forward with its own throw"""
    _kit_throw('kit_archetype_throw_light', 'forward', False, pairing, kit='KIT_Test_Light')


def scenario_kit_archetype_throw_medium(pairing='client_grabs_client'):
    """The medium kit (carry 90) cannot lift a standard fighter either: the stick alone throws back"""
    _kit_throw('kit_archetype_throw_medium', 'back', False, pairing, kit='KIT_Test_Medium')


def scenario_kit_archetype_lift_heavy(pairing='client_grabs_client'):
    """The heavy kit (carry 200) lifts a standard fighter: the stick throws nothing (the held throw owns it), Crouch does the heavy's down throw"""
    _kit_throw('kit_archetype_lift_heavy', 'forward', True, pairing, kit='KIT_Test_Heavy')
