# M11 kit scenarios (kits on the ground, the kit swap on the grab button). Run with the M10/M11 driver (module=kit_scenarios) or
# defence_scenarios.defence_batch([...], module=kit_scenarios). K4 adds the full kit_pickup / kit_throw set; K1 ships the swap check.
import re

import unreal
import gasnpp_test as g

SUITE_KIT = ['kit_swap_hold']
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
