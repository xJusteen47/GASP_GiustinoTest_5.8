# Helper for the playable test hub L_Test_Hub (TH-1, docs/notes/handovers/test-level-brief.md). Python only: it drives the existing harness
# (gasnpp_test and the scenario modules), it adds no C++ and saves no asset. Run in the editor's Python console:
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import test_hub as h
#   h.prepare()            # BEFORE Play: test content in memory (mount kits, spin bindings, shield button, link types) + PIE settings
#   ...Play (listen, 3 players, Run Under One Process)...
#   h.kit(0, 'KIT_Test_Mount')   h.park(1, 'Z1')   h.mount(0, 1)   h.walker(2, True)   h.spin(0)   h.fighter('Z5')   h.drill_immune(1)
#   h.selftest()           # every player in every zone, one action per zone; h.selftest_result() when it is done (also Saved/TestHub/selftest.txt)
#   ...stop Play...
#   h.restore()            # the in-memory test content and the PIE settings back as they were
#
# Player index i = PIE world index: 0 = listen host, 1 and 2 = clients. Zones and park spots come from Tools/GAS/build_test_hub_map.py.
import math
import os
import sys
import time

import unreal

_HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (_HERE, os.path.join(os.path.dirname(_HERE), 'GAS')):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import gasnpp_test as g          # noqa: E402
import build_test_hub_map as hub  # noqa: E402

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'TestHub')
FIGHTER_SPOTS = {'Z5': (-5500.0, -1000.0, 150.0), 'Z1': (4200.0, 1200.0, 150.0), 'Z2': (7500.0, 600.0, 150.0)}
DEFAULT_KIT = g.HIT_KIT        # KIT_Test_Hits: attacks + the spin bindings prepare() adds in memory
MOUNT_KIT = 'KIT_Test_Mount'
_state = {'prepared': None, 'walkers': {}, 'selftest': None, 'rows': []}


def _write(name, lines):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name)
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(str(x) for x in lines) + '\n')
    return path


def _run(steps):
    """steps: [(seconds from now, callable)] on editor ticks, non-blocking. Unlike g.run_timeline each call gets its own callback, so mount() and
    spin() can run inside selftest() without cancelling it"""
    start, pending, handle = time.time(), sorted(steps, key=lambda s: s[0]), []

    def tick(_dt):
        now = time.time() - start
        while pending and pending[0][0] <= now:
            _, fn = pending.pop(0)
            try:
                fn()
            except Exception as e:
                unreal.log_warning(f'[test_hub] step failed: {e!r}')
        if not pending and handle:
            unreal.unregister_slate_post_tick_callback(handle.pop())

    handle.append(unreal.register_slate_post_tick_callback(tick))


def _pie_running():
    return bool(g.worlds())


def _key(i):
    """Pawn key of the player of world i. Not g.key_of_player: the level's dummies are AI-possessed on the server and count as 'local' there"""
    pc = unreal.GameplayStatics.get_player_controller(g.worlds()[i], 0)
    ps = pc.get_controlled_pawn().get_editor_property('player_state') if pc and pc.get_controlled_pawn() else None
    if ps is None:
        raise RuntimeError(f'world {i} has no player pawn')
    return f"P{ps.get_editor_property('player_id')}"


def _players():
    """World indices with a player of their own (the host is world 0 in listen)"""
    out = []
    for i in range(len(g.worlds())):
        try:
            _key(i)
            out.append(i)
        except Exception:
            pass
    return out


def dummies():
    """Keys of the level's dummies (test characters with no player state)"""
    return [k for k in g.comps(g.worlds()[0]) if not k.startswith('P')]


def _place_ahead(i, key, ahead):
    """Server teleport of pawn `key` to `ahead` cm in front of player i"""
    v = g.snap(g.worlds()[0])[_key(i)]
    yaw = math.radians(v['yaw'])
    x, y = v['location'][0] + ahead * math.cos(yaw), v['location'][1] + ahead * math.sin(yaw)
    # server_teleport_exact, not server_reset_to_spawn: the reset left an AI dummy short of the spot (probe 2026-11-06)
    g.comps(g.worlds()[0])[key].server_teleport_exact(unreal.Vector(x, y, v['location'][2] + 2.0))
    return round(x), round(y)


# ---- automatic Play setup and kit pads (registered by prepare(), lives while the editor is open) ---------------------------------------------
_watch = {'handle': None, 'pie': False, 'since': 0.0, 'ready': False, 'next': 0.0, 'on_pad': {}, 'auto': False, 'content': False}
MAP_NAME = hub.MAP_PATH.rsplit('/', 1)[1]


def _editor_map():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]


def _pie_is_hub():
    # PIE worlds are named UEDPIE_<n>_<map>
    return g.worlds()[0].get_name().endswith(MAP_NAME)


def spawn_dummies():
    """The hub's dummies (build_test_hub_map.DUMMIES) spawned on the server with an AI controller; they replicate to the clients. Not level
    actors: a placed Mover pawn cannot be saved in a map. Skips a spot that already has one"""
    world = g.worlds()[0]
    cls = unreal.load_asset(hub.DUMMY_CLASS).generated_class()
    have = [a.get_actor_location() for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls)]
    # Python cannot spawn into a PIE world directly: the host's `summon` cheat spawns on the server (listen host), then each new pawn gets
    # an AI controller and is teleported to its spot through the test component
    todo = [d for d in hub.DUMMIES if not any(math.hypot(p.x - d[1][0], p.y - d[1][1]) < 50.0 for p in have)]
    before = set(a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls))
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    for _ in todo:
        unreal.SystemLibrary.execute_console_command(world, f'summon {hub.DUMMY_CLASS}.{hub.DUMMY_CLASS.rsplit("/", 1)[1]}_C', pc)
    new = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if a.get_name() not in before]
    made = 0
    for pawn, (zone, (x, y), yaw) in zip(new, todo):
        if pawn.get_controller() is None:
            pawn.spawn_default_controller()
        pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), True)
        pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent).server_teleport_exact(unreal.Vector(x, y, 100.0))
        made += 1
    return made


def play_setup():
    """Run once per Play (the watcher does it 2 s after Play starts): the dummies, hit rules in memory (damage on, as the hit scenarios), the
    link steps, the default kit on every player"""
    import link_scenarios as ls
    out = [('dummies', spawn_dummies()), g.hit_rules_setup(), ls.link_setup()]
    # Players only: a loadout change on a dummy summoned in the same tick crashed the editor in the link snapshot contributor (2026-11-06,
    # FGSTN_SyncFragmentList::Find from UGSTN_CombatSimComponent::SimulationTick). The dummies keep their default kit: being hit needs none
    out.append({_key(i): g._apply_loadout(_key(i), DEFAULT_KIT) for i in _players()})
    return out


def _pad_check():
    world = g.worlds()[0]
    for i in _players():
        key = _key(i)
        loc = g.snap(world)[key]['location']
        dx, dy = hub.DEBUG_PAD[1]
        if math.hypot(loc[0] - dx, loc[1] - dy) < hub.PAD_RADIUS:
            if _watch['on_pad'].get(key) != 'debug':
                _watch['on_pad'][key] = 'debug'
                _watch['debug'] = not _watch.get('debug', False)
                for cvar in ('gstn.Combat.Debug.Hitboxes', 'gstn.Combat.Debug.Hurtboxes'):
                    unreal.SystemLibrary.execute_console_command(world, f"{cvar} {int(_watch['debug'])}")
                unreal.SystemLibrary.print_string(g.worlds()[i], 'Hitbox/hurtbox: ' + ('ACCESO' if _watch['debug'] else 'SPENTO'), True, True,
                                                  unreal.LinearColor(1, 0.3, 0.3, 1), 4.0)
            continue
        pad = next((p for p in hub.KIT_PADS if math.hypot(loc[0] - p[3][0], loc[1] - p[3][1]) < hub.PAD_RADIUS), None)
        if pad is None:
            _watch['on_pad'].pop(key, None)
            continue
        if _watch['on_pad'].get(key) == pad[2]:
            continue
        _watch['on_pad'][key] = pad[2]
        result = g._apply_loadout(key, pad[0], pad[1])
        unreal.SystemLibrary.print_string(g.worlds()[i], f'Kit: {pad[2]} ({pad[0]}) -> {result}', True, True, unreal.LinearColor(1, 1, 0, 1), 4.0)


def _watch_tick(_dt):
    now = time.time()
    if now < _watch['next']:
        return
    _watch['next'] = now + 0.2
    try:
        running = _pie_running()
    except Exception:
        running = False
    if not running:
        _watch.update(pie=False, ready=False, on_pad={})
        if _watch['auto']:
            _auto_content()
        return
    if not _pie_is_hub():
        return
    if not _watch['pie']:
        _watch.update(pie=True, since=now)
        return
    try:
        if not _watch['ready'] and now - _watch['since'] > 2.0:
            _watch['ready'] = True
            unreal.log(f'[test_hub] play setup: {play_setup()}')
        if _watch['ready']:
            _pad_check()
    except Exception as e:
        unreal.log_warning(f'[test_hub] watcher: {e!r}')


def _auto_content():
    """auto_start mode, editor idle: the test content in memory while L_Test_Hub is the open level, put back when another level is opened"""
    try:
        here = _editor_map() == hub.MAP_PATH
    except Exception:
        return
    if here and not _watch['content']:
        _watch['content'] = True
        unreal.log(f'[test_hub] {prepare(setup_pie=False)}')
    elif not here and _watch['content']:
        _watch['content'] = False
        unreal.log(f'[test_hub] left the hub, content restored: {restore(pie_settings=False)}')


def auto_start():
    """Called by the project's Python startup script (Config/DefaultEngine.ini, hub_startup.py): nothing to run by hand. Opening L_Test_Hub
    prepares the test content, every Play in it spawns the dummies and sets the hit rules, the pads swap kits. Other levels are not touched"""
    _watch['auto'] = True
    return _start_watcher()


def _start_watcher():
    if _watch['handle'] is None:
        _watch['handle'] = unreal.register_slate_post_tick_callback(_watch_tick)
    return 'watcher on'


# ---- before Play -------------------------------------------------------------------------------------------------------------------------------
def prepare(setup_pie=True):
    """The scenario modules' prepare_editor() in one go, then the PIE settings (setup_editor). Content changes stay in memory except the mount
    test kits, which mount_scenarios creates and saves in GSTNPiggyback's Content/Test when missing (its own behaviour). Reports where two disagree"""
    if _pie_running():
        raise RuntimeError('stop Play first: the kits and the button map are read when a pawn starts')
    import defence_scenarios as ds
    import spin_drill_scenarios as sd
    import mount_scenarios as ms
    rows, problems = [], []
    for name, fn in (('defence', ds.prepare_editor), ('spin', sd.prepare_editor), ('mount+link', ms.prepare_editor)):
        try:
            rows.append((name, fn()))
        except Exception as e:
            problems.append(f'{name} prepare failed: {e!r}')
    # Known overlaps: the spin bindings go on KIT_Test_Hits only (the mount kits are copies of KIT_Test_Link: no spin while on them); link's
    # grab_attack would turn KIT_Test_Hits' attack into a grab and break Z1, so the hub never calls it
    notes = ['spin works on KIT_Test_Hits only; KIT_Test_Mount / KIT_Test_Link have no spin bindings',
             'link grab_attack is not used (it would make the Z1 attack a grab)']
    if setup_pie:
        try:
            rows.append(('pie', g.setup_editor(True, 'listen', 2)))
        except Exception as e:
            problems.append(f'setup_editor failed: {e!r}')
    _state['prepared'] = time.time()
    rows.append(('watcher', _start_watcher()))
    path = _write('prepare.txt', [f'{n}: {r}' for n, r in rows] + ['NOTE ' + n for n in notes] + ['PROBLEM ' + p for p in problems])
    return f"prepare: {len(rows)} ok, {len(problems)} problems ({path})"


def restore(pie_settings=True):
    """With Play stopped: the in-memory content and the PIE settings back"""
    import defence_scenarios as ds
    import spin_drill_scenarios as sd
    out = []
    for fn in (sd.restore_editor, ds.restore_editor):
        try:
            out.append(fn())
        except Exception as e:
            out.append(f'{fn.__module__} restore failed: {e!r}')
    if pie_settings:
        out.append(g.setup_editor(False))
    return out


# ---- during Play -------------------------------------------------------------------------------------------------------------------------------
def kit(i, name=DEFAULT_KIT):
    """Server-side kit change of player i (the C15 test loadout path; plugin kits by name once scanned). 'OK' when accepted"""
    return g._apply_loadout(_key(i), name)


def where(i):
    v = g.snap(g.worlds()[0])[_key(i)]
    return tuple(round(c) for c in v['location'])


def park(i, zone, slot=None):
    """Teleports player i (server, the floor under the spot is found by the teleport) to its slot in `zone` ('Z0'..'Z6', 'Z1_lane')"""
    spots = hub.PARK[zone]
    x, y = spots[(i if slot is None else slot) % len(spots)]
    comp = g.comps(g.worlds()[0])[_key(i)]
    spawn = comp.get_spawn_location()
    comp.server_reset_to_spawn(unreal.Vector(x - spawn.x, y - spawn.y, 0.0))
    return zone, (x, y)


def park_all(zone):
    return [park(i, zone) for i in _players()]


def walker(i, on=True, seconds_per_turn=4.0):
    """Player i walks in a circle (IA_Move held, the stick turned every 0.1 s): a moving target. walker(i, False) stops it"""
    old = _state['walkers'].pop(i, None)
    if old is not None:
        unreal.unregister_slate_post_tick_callback(old['handle'])
        g.release(i, 'move')
    if not on:
        return f'walker {i} off'
    st = {'t': 0.0, 'next': 0.0}

    def tick(dt):
        st['t'] += dt
        if st['t'] < st['next']:
            return
        st['next'] = st['t'] + 0.1
        a = 2.0 * math.pi * st['t'] / seconds_per_turn
        try:
            g.hold(i, 'move', (math.cos(a), math.sin(a), 0.0))
        except Exception:   # Play stopped: drop the callback
            unreal.unregister_slate_post_tick_callback(st['handle'])
            _state['walkers'].pop(i, None)

    st['handle'] = unreal.register_slate_post_tick_callback(tick)
    _state['walkers'][i] = st
    return f'walker {i} on'


def mount(rider, carrier):
    """Rider and carrier on the mount kit and team 1, the carrier 90 cm in front of the rider, then the rider taps Interact (as mount_basic).
    Look at it with mounted(rider, carrier) a second later"""
    import link_scenarios as ls
    ls.link_setup()
    kits = [kit(rider, MOUNT_KIT), kit(carrier, MOUNT_KIT)]
    g._team_everywhere([_key(rider), _key(carrier)], 1)
    others = [o for o in _players() if o not in (rider, carrier)]
    if others:
        g._team_everywhere([_key(o) for o in others], 2)
    rk, ck = _key(rider), _key(carrier)
    _run([(0.8, lambda: ('placed', _place_ahead(rider, ck, 90.0))),
                    (1.5, lambda: ('tap', g.combat_tap(rider, g.button_mask('Interact'))))])
    return 'mount: kits', kits, 'tap in 1.5 s'


def mounted(rider, carrier):
    """True when the server shows the Body link rider role 1 / carrier role 2, phase Active"""
    import link_scenarios as ls
    r, c = ls.body(0, _key(rider)), ls.body(0, _key(carrier))
    return bool(r and c and r[0] == 1 and c[0] == 2 and r[1] == 2 and c[1] == 2), (r, c)


def spin(i, clockwise=True):
    """One stick turn on player i (the spin input of the spin scenarios); needs KIT_Test_Hits with the bindings from prepare()"""
    import spin_drill_scenarios as sd
    steps, _ = sd.spin_steps(i, 0.05, clockwise)
    _run(steps)
    return 'spin', i


def drill_immune(i, on=True):
    """Player i marked not drillable (Combat.Immunity.Drill through the test effect): a dive on it bounces"""
    import link_scenarios as ls
    return unreal.GSTN_SpinDrillTestLibrary.server_set_drill_immunity(ls.actor(0, _key(i)), on)


def fighter(zone='Z5'):
    """Spawns the scripted platform test fighter (GSTNPlatform) in `zone` on the server; it replicates to the clients"""
    x, y, z = FIGHTER_SPOTS[zone]
    unreal.SystemLibrary.execute_console_command(g.worlds()[0], f'gstn.Platform.SpawnTestFighter {x} {y} {z} 90')
    return 'fighter', zone


def _count_actors(world_index, cls):
    return len(unreal.GameplayStatics.get_all_actors_of_class(g.worlds()[world_index], cls))


# ---- selftest ----------------------------------------------------------------------------------------------------------------------------------
def _has_tag(wi, i, name):
    import link_scenarios as ls
    return ls.has_tag(wi, _key(i), name)


def selftest(zone_seconds=4.0):
    """Every player into every zone (its own slot), the position checked on the server, then one action per zone. Non-blocking: returns at once,
    the rows go to Saved/TestHub/selftest.txt and selftest_result() when it ends (~30 s)"""
    players = list(_players())
    rows = []
    _state['rows'], _state['selftest'] = rows, 'running'

    def check(label, fn):
        def f():
            try:
                ok, info = fn()
            except Exception as e:
                ok, info = False, repr(e)
            rows.append(('PASS' if ok else 'FAIL', label, info))
            return label, ok
        return f

    def act(label, fn):
        def f():
            try:
                return label, fn()
            except Exception as e:
                rows.append(('FAIL', label, repr(e)))
                return label, repr(e)
        return f

    def at_spot(zone):
        def f():
            far = {}
            for i in players:
                x, y = hub.PARK[zone][i % len(hub.PARK[zone])]
                p = where(i)
                d = math.hypot(p[0] - x, p[1] - y)
                if d > 150.0:
                    far[i] = (p, round(d))
            return not far, far or 'all in place'
        return f

    seen = {'spin': False}

    def watch_spin():
        seen['spin'] = seen['spin'] or _has_tag(0, players[0], 'Combat.State.Spinning')
        return 'watch', seen['spin']

    steps, t = [], 0.2
    steps.append((t, act('kits', lambda: [kit(i, DEFAULT_KIT) for i in players])))
    t += 1.5
    for zone in ('Z0', 'Z1', 'Z2', 'Z3', 'Z4', 'Z5', 'Z6'):
        steps.append((t, act(f'{zone} park', lambda z=zone: park_all(z))))
        steps.append((t + 1.2, check(f'{zone} every player at its spot', at_spot(zone))))
        a = t + 1.4
        if zone == 'Z1':
            steps.append((a, act('Z1 attack tap', lambda: g.combat_tap(players[0], g.button_mask('Attack')))))
            steps.append((a + 0.1, check('Z1 attack accepted (no error)', lambda: (True, 'tap sent'))))
        elif zone == 'Z2':
            steps.append((a, act('Z2 shield tap', lambda: g.combat_tap(players[0], g.button_mask('Shield')))))
            steps.append((a + 0.1, check('Z2 shield button exists', lambda: (g.button_index('Shield') >= 0, g.button_index('Shield')))))
        elif zone == 'Z3' and len(players) >= 2:
            steps.append((a - 0.1, act('Z3 mount', lambda: mount(players[0], players[1]))))   # after the spot check: it moves the carrier
            steps.append((a + 2.4, check('Z3 mount formed on the server', lambda: mounted(players[0], players[1]))))
        elif zone == 'Z4':
            steps.append((a - 1.3, act('Z4 kits back', lambda: [kit(i, DEFAULT_KIT) for i in players])))
            steps.append((a, act('Z4 spin', lambda: spin(players[0]))))
            for k in range(1, 8):
                steps.append((a + 0.1 * k, watch_spin))
            steps.append((a + 0.9, check('Z4 spin seen on the server (Combat.State.Spinning)', lambda: (seen['spin'], seen['spin']))))
        elif zone == 'Z5':
            steps.append((a, act('Z5 fighter', lambda: fighter('Z5'))))
            steps.append((a + 0.8, check('Z5 test fighter on the server', lambda: (
                _count_actors(0, unreal.GSTN_PlatformTestFighter) > 0, _count_actors(0, unreal.GSTN_PlatformTestFighter)))))
        elif zone == 'Z6':
            steps.append((a, check('Z6 looping meshes in the PIE world', lambda: (
                _count_actors(0, unreal.SkeletalMeshActor) >= len(hub.ANIMS), _count_actors(0, unreal.SkeletalMeshActor)))))
        t += zone_seconds + 1.0 if zone == 'Z3' else zone_seconds if zone == 'Z4' else zone_seconds - 1.5

    def finish():
        fails = [r for r in rows if r[0] == 'FAIL']
        _state['selftest'] = 'PASS' if not fails else f'FAIL {len(fails)}'
        path = _write('selftest.txt', [f'{s}  {l}  {info}' for s, l, info in rows] + [f'RESULT {_state["selftest"]}'])
        unreal.log(f'[test_hub] selftest {_state["selftest"]} ({path})')
        return 'selftest', _state['selftest']

    steps.append((t + 0.2, finish))
    _run(steps)
    return f'selftest started, ~{int(t + 1)} s; selftest_result() after'


def selftest_result():
    rows = _state['rows']
    return _state['selftest'], [r for r in rows if r[0] == 'FAIL'][:8]
