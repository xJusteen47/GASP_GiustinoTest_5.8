# Designer examples (Tools/GAS/setup_designer_examples.py): the example kit's three moves land with the damage written in their Blueprints, and the
# example backpack's dome comes up on its button. Run in the one-PIE suite driver (listen host + 2 clients on DefaultLevel):
#   R31_IMPORT=example_scenarios R31_ONLY=designer_examples, then Tools/GASNPPTest/m3_suite_driver.py (as Saved/UE59Load/suite.ps1 runs it).
# (The fresh-PIE batch driver crashed on UE 5.9 in the fork's NP unregister at a PIE restart, 2026-11-13, before this scenario ran.)
import unreal
import gasnpp_test as g

SUITE_KIT = ['designer_examples']
KIT = 'KIT_Example'
BACKPACK = '/Game/GASNPPTest/Examples/DA_Example_Backpack_Dome.DA_Example_Backpack_Dome'
# (logical button, damage written in the move's Blueprint, where the victim stands in the attacker's frame: x ahead, y left)
MOVES = [('Attack', 4.0, (110.0, 0.0)), ('Strong', 14.0, (130.0, 0.0)), ('Special', 8.0, (90.0, 0.0))]


def _server_loadout(key):
    pawn = g.comps(g.worlds()[0])[key].get_owner()
    for holder in (pawn, pawn.get_editor_property('player_state')):
        lc = holder.get_component_by_class(unreal.GSTN_LoadoutComponent) if holder else None
        if lc:
            return lc
    return None


def _add_backpack(key):
    """The applied loadout plus the example backpack, accepted on the server like the loadout screen's confirm. Returns (ok, detail)"""
    lc = _server_loadout(key)
    if not lc:
        return False, 'no loadout component'
    data = lc.get_editor_property('applied')
    ext = unreal.InstancedStruct()
    ext.import_text(f'/Script/GSTNBackpackCore.GSTN_LoadoutBackpackExt(Backpack="{BACKPACK}")')
    data.set_editor_property('extensions', list(data.get_editor_property('extensions')) + [ext])
    res = lc.accept_loadout(data)
    ok, errors = (res[0], res[1]) if isinstance(res, tuple) else (bool(res), [])
    return bool(ok), f'accepted={ok} errors={list(errors)}'


def scenario_designer_examples(client=1, with_backpack=False):
    """The client takes KIT_Example. Light (X), strong (RT) and special (Y) each hit the other client standing in reach: the victim's damage grows by the
    move's authored damage, on the authority and on the victim's own client. with_backpack: then the example backpack is added to the loadout (accepted
    by the server) and a tap of the dodge button with the stick at rest raises the dome. OFF by default: on UE 5.9 (2026-11-13) calling AcceptLoadout
    from Python on the running authority pawn re-made its table at revision 0 and crashed the editor (NP ensure ClientID/ServerID, then a BitArray
    check in UNpAbilitySystemComponent::EndPlay), twice; not investigated"""
    victim = 2 if client == 1 else 1
    vkey = g.key_of_player(victim)
    run = g._Run('designer_examples', client, judge_reconciles=False)
    key, st = run.key, {}

    def setup():
        g.hit_rules_setup()
        return 'kit', g._apply_loadout(key, KIT)

    def place(xy):
        return lambda: ('placed', g._place_near(client, vkey, xy))

    def before(name):
        def f():
            st[name] = (g.damage_of(0, vkey), g.damage_of(victim, vkey))
            g.combat_tap(client, g.button_mask(name))
            return name, st[name]
        return f

    def after(name, damage):
        def f():
            now = (g.damage_of(0, vkey), g.damage_of(victim, vkey))
            got = (now[0] - st[name][0], now[1] - st[name][1])
            return all(abs(x - damage) < 0.51 for x in got), f'{name}: damage +{got[0]:.1f} authority, +{got[1]:.1f} victim client (want +{damage})'
        return f

    def backpack():
        ok, detail = _add_backpack(key)
        st['backpack'] = ok
        return 'backpack', detail

    def dome():
        st['dome_t'] = run.now()
        g.combat_tap(client, g.button_mask('Dodge'))
        return 'dodge tap', st['dome_t']

    def dome_seen():
        t0 = st.get('dome_t', 0.0)
        seen = any('Backpack.State.Dome' in s['tags'] for s in run._win('srv', t0, t0 + 1.5, sim=True))
        return bool(st.get('backpack')) and seen, f'backpack accepted {st.get("backpack")}, dome tag on the authority {seen}'

    steps = [(0.3, setup)]
    t = 1.8
    for name, damage, xy in MOVES:
        steps += [(t, place(xy)), (t + 0.8, before(name)), (t + 2.3, run.at(f'{name} hits for {damage}', after(name, damage)))]
        t += 3.0
    if with_backpack:
        steps += [(t, backpack), (t + 1.5, dome), (t + 3.0, run.at('example backpack: dome on the dodge button', dome_seen))]
        t += 3.0
    steps += [(t + 0.5, run.finish())]
    run.run(steps)


# The one-PIE suite looks scenarios up on gasnpp_test
g.scenario_designer_examples = scenario_designer_examples
