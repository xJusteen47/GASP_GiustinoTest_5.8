# GAS + NPP + Mover network test helpers (run inside the Unreal Editor Python console).
#
# Usage (PIE: dedicated server + 2 clients, "Run Under One Process"):
#   import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import gasnpp_test as m
#   m.setup_editor(True)            # before Play: test GameMode on the open level (not saved), no CPU throttling
#   m.net(60, 15, 2)                # lag ms, jitter ms, packet loss %
#   m.reset_all(); m.scenario_abilities((1, 2))   # or scenario_forced_rollback / scenario_server_correction / scenario_accounting
#   print(m.report())               # counters + server vs client state comparison
#   m.setup_editor(False)           # after Stop: restore GM_Sandbox and throttling
import unreal, json

CHAR = '/Game/GASNPPTest/BP_GASNPP_TestCharacter'
IA = {
    'move': '/Game/Input/IA_Move',
    'dash': '/Game/GASNPPTest/Input/IA_GASNPP_Dash',
    'buff': '/Game/GASNPPTest/Input/IA_GASNPP_Buff',
    'jump': '/Game/Input/IA_Jump',
}


def worlds():
    """[server, client1, client2] PIE worlds."""
    return list(unreal.EditorLevelLibrary.get_pie_worlds(True))


def comps(world):
    cls = unreal.load_asset(CHAR).generated_class()
    out = {}
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls):
        ps = a.get_editor_property('player_state')
        key = f"P{ps.get_editor_property('player_id')}" if ps else a.get_name()
        out[key] = a.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
    return out


def snap(world):
    return {n: json.loads(c.get_state_snapshot()) for n, c in comps(world).items()}


def local_comp(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = pc.get_controlled_pawn() if pc else None
    return pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent) if pawn else None


def reset_all():
    for w in worlds():
        for c in comps(w).values():
            c.reset_counters()


def net(lag_ms=0, var_ms=0, loss_pct=0):
    """Same emulation on every PIE net driver (server and clients)."""
    for w in worlds():
        for cmd in (f'NetEmulation.PktLag {lag_ms}', f'NetEmulation.PktLagVariance {var_ms}', f'NetEmulation.PktLoss {loss_pct}'):
            unreal.SystemLibrary.execute_console_command(w, cmd)


def hold(client_index, action, value):
    c = local_comp(worlds()[client_index])
    return c.start_holding_input(unreal.load_asset(IA[action]), unreal.Vector(*value))


def release(client_index, action):
    c = local_comp(worlds()[client_index])
    return c.stop_holding_input(unreal.load_asset(IA[action]))


def press(client_index, action):
    c = local_comp(worlds()[client_index])
    return c.press_input(unreal.load_asset(IA[action]))


import time
_timeline = {'handle': None}


def run_timeline(steps):
    """steps: list of (seconds_from_now, callable). Runs on editor ticks without blocking."""
    start = time.time()
    pending = sorted(steps, key=lambda s: s[0])
    log = []

    def tick(_dt):
        now = time.time() - start
        while pending and pending[0][0] <= now:
            t, fn = pending.pop(0)
            try:
                log.append(f'{t:.1f}s {fn()}')
            except Exception as e:
                log.append(f'{t:.1f}s ERROR {e}')
        if not pending and _timeline['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(_timeline['handle'])
            _timeline['handle'] = None
            unreal.log('GASNPP timeline done: ' + ' | '.join(log))

    _timeline['handle'] = unreal.register_slate_post_tick_callback(tick)


def _diff(a, b, pos_tol=2.0):
    d = []
    la, lb = a['location'], b['location']
    dist = sum((x - y) ** 2 for x, y in zip(la, lb)) ** 0.5
    if dist > pos_tol:
        d.append(f'pos {dist:.1f}cm')
    if a.get('mode') != b.get('mode'):
        d.append(f"mode {a.get('mode')}!={b.get('mode')}")
    for k, v in a.get('attributes', {}).items():
        bv = b.get('attributes', {}).get(k)
        if bv is None or abs(v - bv) > 0.01:
            d.append(f'{k} {v}!={bv}')
    if sorted(a.get('tags', [])) != sorted(b.get('tags', [])):
        d.append(f"tags {a.get('tags')}!={b.get('tags')}")
    return d


def report():
    """Counters per world + server/autonomous/simulated state comparison for every pawn."""
    ws = worlds()
    snaps = [snap(w) for w in ws]
    lines = []
    for i, s in enumerate(snaps):
        for n, v in sorted(s.items()):
            lines.append(f"w{i} {n} {v['role'][5:]:<15} rb={v['rollbacks']:<3} rbMs={v['rolledBackMs']:<6.0f} ticks={v['simTicks']:<5} resim={v['resimTicks']:<4} abResim={v['abilityResimTicks']:<4} "
                         f"hp={v['attributes'].get('Health')} st={v['attributes'].get('Stamina')} tags={v['tags']} cd={ {k[14:]: round(x, 2) for k, x in v['cooldowns'].items()} } eff={len(v['effects'])} mode={v.get('mode')}")
    server = snaps[0]
    for n, sv in sorted(server.items()):
        for i in range(1, len(snaps)):
            cv = snaps[i].get(n)
            if cv is None:
                continue
            diffs = _diff(sv, cv)
            lines.append(f"CMP {n} server vs w{i}({cv['role'][5:]}): {'OK' if not diffs else diffs}")
    return '\n'.join(lines)


def scenario_abilities(clients=(1,), stagger=0.3):
    """Each client: walk forward, dash, buff, dash on cooldown, dash again, stop. Clients start `stagger` seconds apart."""
    steps = []
    for n, c in enumerate(clients):
        o = n * stagger
        steps += [
            (0.0 + o, lambda c=c: (f'c{c} move', hold(c, 'move', (0, 1, 0)))),
            (1.0 + o, lambda c=c: (f'c{c} dash1', press(c, 'dash'))),
            (1.6 + o, lambda c=c: (f'c{c} buff', press(c, 'buff'))),
            (2.0 + o, lambda c=c: (f'c{c} dash2-oncooldown', press(c, 'dash'))),
            (3.4 + o, lambda c=c: (f'c{c} dash3', press(c, 'dash'))),
            (4.5 + o, lambda c=c: (f'c{c} stop', release(c, 'move'))),
        ]
    run_timeline(steps)


def force_reconcile(client_index, extra_frames=10):
    w = worlds()[client_index]
    unreal.SystemLibrary.execute_console_command(w, f'np.ForceReconcileExtraFrames {extra_frames}')
    unreal.SystemLibrary.execute_console_command(w, 'np.ForceReconcile 1')
    return True


def server_apply(player_key, effect_class_name):
    """Apply an effect on the server copy only (not predicted by the owning client)."""
    comp = comps(worlds()[0])[player_key]
    asc = comp.get_owner().get_component_by_class(unreal.NpAbilitySystemComponent)
    cls = unreal.load_class(None, f'/Script/GASP_Giustino2.{effect_class_name}')
    h = asc.apply_gameplay_effect_to_self(cls, 1.0, asc.make_effect_context())
    return str(h)


def scenario_forced_rollback(client=1):
    steps = [
        (0.0, lambda: ('move', hold(client, 'move', (0, 1, 0)))),
        (1.0, lambda: ('dash', press(client, 'dash'))),
        (1.1, lambda: ('force reconcile during dash', force_reconcile(client, 10))),
        (1.6, lambda: ('buff', press(client, 'buff'))),
        (1.9, lambda: ('force reconcile during buff', force_reconcile(client, 15))),
        (2.6, lambda: ('force reconcile while walking', force_reconcile(client, 20))),
        (3.0, lambda: ('stop', release(client, 'move'))),
    ]
    run_timeline(steps)


def scenario_server_correction(player_key='P256'):
    steps = [
        (0.0, lambda: ('server dash (unpredicted move)', server_apply(player_key, 'GSTN_GE_Dash'))),
        (1.0, lambda: ('server buff (unpredicted effect)', server_apply(player_key, 'GSTN_GE_Buff'))),
        (1.2, lambda: ('server dash cost', server_apply(player_key, 'GSTN_GE_DashCost'))),
    ]
    run_timeline(steps)


def setup_editor(enable=True):
    """Temporary editor-only setup (never saved): test GameMode override on the open level, no background CPU throttling."""
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    gm = '/Game/GASNPPTest/GM_GASNPPTest' if enable else '/Game/Blueprints/GM_Sandbox'
    w.get_world_settings().set_editor_property('default_game_mode', unreal.load_asset(gm).generated_class())
    perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
    perf.set_editor_property('bThrottleCPUWhenNotForeground', not enable)
    return w.get_world_settings().get_editor_property('default_game_mode').get_name(), perf.get_editor_property('bThrottleCPUWhenNotForeground')


def server_remove(player_key, effect_class_name):
    comp = comps(worlds()[0])[player_key]
    asc = comp.get_owner().get_component_by_class(unreal.NpAbilitySystemComponent)
    cls = unreal.load_class(None, f'/Script/GASP_Giustino2.{effect_class_name}')
    asc.remove_active_gameplay_effect_by_source_effect(cls, None, -1)
    return True


def scenario_accounting(client=2, player_key='P257'):
    """No regen: two successful dashes (+1 blocked by cooldown) and a buff, with forced rollbacks in between.
    Expected final: Stamina 50, Health 80 (from 50), no tags."""
    steps = [
        (0.0, lambda: ('server removes regen', server_remove(player_key, 'GSTN_GE_StaminaRegen'))),
        (1.0, lambda: ('dash1', press(client, 'dash'))),
        (1.15, lambda: ('force reconcile', force_reconcile(client, 12))),
        (1.5, lambda: ('dash2 (cooldown, must fail)', press(client, 'dash'))),
        (1.7, lambda: ('buff', press(client, 'buff'))),
        (2.3, lambda: ('force reconcile', force_reconcile(client, 20))),
        (3.0, lambda: ('dash3', press(client, 'dash'))),
        (3.1, lambda: ('force reconcile', force_reconcile(client, 8))),
    ]
    run_timeline(steps)
