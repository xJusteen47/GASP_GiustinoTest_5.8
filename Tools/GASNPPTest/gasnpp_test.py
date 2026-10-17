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


# ------------------------------------------------------------------------------------------------ GA_Jump / GA_WallSlide
WALL_SLIDING_TAG = 'State.Movement.WallSliding'


def _has_tag(actor, tag_name):
    asc = actor.get_component_by_class(unreal.NpAbilitySystemComponent)
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{tag_name}")')
    return bool(asc) and asc.get_gameplay_tag_count(tag) > 0


def _local_pawn(client_index):
    pc = unreal.GameplayStatics.get_player_controller(worlds()[client_index], 0)
    return pc, (pc.get_controlled_pawn() if pc else None)


def _steer_value(client_index, target):
    """IA_Move value (right, forward) that walks the client's pawn toward target (camera relative, from the control yaw)."""
    import math
    pc, pawn = _local_pawn(client_index)
    loc = pawn.get_actor_location()
    dx, dy = target[0] - loc.x, target[1] - loc.y
    n = max((dx * dx + dy * dy) ** 0.5, 1e-3)
    dx, dy = dx / n, dy / n
    yaw = math.radians(pc.get_control_rotation().yaw)
    fwd, right = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))
    return (dx * right[0] + dy * right[1], dx * fwd[0] + dy * fwd[1], 0)


_mode_log = {'handle': None, 'events': [], 'last': {}, 'start': 0.0}


def start_mode_log():
    """Records, for every pawn in every PIE world, each change of Mover mode or of the WallSliding tag."""
    stop_mode_log()
    _mode_log.update(events=[], last={}, start=time.time())

    def tick(_dt):
        t = time.time() - _mode_log['start']
        for i, w in enumerate(worlds()):
            for n, c in comps(w).items():
                a = c.get_owner()
                mover = a.get_component_by_class(unreal.CharacterMoverComponent)
                state = (str(mover.get_movement_mode_name()), _has_tag(a, WALL_SLIDING_TAG))
                key = (i, n)
                if _mode_log['last'].get(key) != state:
                    _mode_log['last'][key] = state
                    loc = a.get_actor_location()
                    _mode_log['events'].append(f'{t:6.2f}s w{i} {n} {state[0]}{" +WallSliding" if state[1] else ""} z={loc.z:.0f}')

    _mode_log['handle'] = unreal.register_slate_post_tick_callback(tick)


def stop_mode_log():
    if _mode_log['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_mode_log['handle'])
        _mode_log['handle'] = None
    return '\n'.join(_mode_log['events'])


_steer = {'handle': None}


def scenario_jump_wall(client=1, wall_x=1010.0, wall_y=-880.0, jump_distance=220.0, wall_time=0.7, ground_jump_first=True):
    """Ground jump in place, then walk to the wall (face at x=wall_x, normal -X), jump at it, hold into it, wall jump.
    Steering re-aims the move input every tick. Run start_mode_log() first and read stop_mode_log() + report() after."""
    ia_move = unreal.load_asset(IA['move'])
    target = (wall_x + 200.0, wall_y)
    st = {'phase': 'ground_jump' if ground_jump_first else 'walk', 't0': time.time(), 'log': []}

    def note(msg):
        st['log'].append(f"{time.time() - st['t0']:.2f}s {msg}")

    def tick(_dt):
        try:
            now = time.time() - st['t0']
            pc, pawn = _local_pawn(client)
            comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
            mode = str(pawn.get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name())
            if st['phase'] == 'ground_jump':
                if now > 0.3:
                    note(f'ground jump press={press(client, "jump")} mode={mode}')
                    st.update(phase='ground_wait', t1=now)
            elif st['phase'] == 'ground_wait':
                if now - st['t1'] > 1.5:
                    note(f'ground jump done mode={mode}')
                    st['phase'] = 'walk'
            elif st['phase'] == 'walk':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, target)))
                if wall_x - pawn.get_actor_location().x < jump_distance and mode == 'Walking':
                    note(f'jump at wall press={press(client, "jump")} x={pawn.get_actor_location().x:.0f}')
                    st.update(phase='air', t1=now)
            elif st['phase'] == 'air':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, target)))
                if mode == 'WallSliding':
                    note(f'on wall tag={_has_tag(pawn, WALL_SLIDING_TAG)}')
                    st.update(phase='wall', t1=now)
                elif now - st['t1'] > 2.0:
                    note(f'never reached the wall, mode={mode}')
                    st['phase'] = 'done'
            elif st['phase'] == 'wall':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, target)))
                if now - st['t1'] > wall_time:
                    # Away from the wall: the wall jump pushes off it
                    comp.stop_holding_input(ia_move)
                    note(f'wall jump press={press(client, "jump")} mode={mode} tag={_has_tag(pawn, WALL_SLIDING_TAG)}')
                    st.update(phase='after', t1=now)
            elif st['phase'] == 'after':
                if now - st['t1'] > 2.0:
                    note(f'end mode={mode} tag={_has_tag(pawn, WALL_SLIDING_TAG)}')
                    st['phase'] = 'done'
            if st['phase'] == 'done':
                comp.stop_holding_input(ia_move)
                unreal.unregister_slate_post_tick_callback(_steer['handle'])
                _steer['handle'] = None
                unreal.log('GASNPP jump/wall: ' + ' | '.join(st['log']))
        except Exception as e:
            unreal.unregister_slate_post_tick_callback(_steer['handle'])
            _steer['handle'] = None
            unreal.log_error(f'GASNPP jump/wall error: {e} | ' + ' | '.join(st['log']))

    if _steer['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_steer['handle'])
    _steer['handle'] = unreal.register_slate_post_tick_callback(tick)


def scenario_server_dash(client=1, player_key='P256'):
    """Client walks forward; the server applies two dashes the client did not predict.
    Each must cost the owning client about one rollback (not a chain over the whole dash)."""
    steps = [
        (0.0, lambda: ('move', hold(client, 'move', (0, 1, 0)))),
        (1.0, lambda: ('server dash 1', server_apply(player_key, 'GSTN_GE_Dash'))),
        (2.5, lambda: ('server dash 2', server_apply(player_key, 'GSTN_GE_Dash'))),
        (3.5, lambda: ('stop', release(client, 'move'))),
    ]
    run_timeline(steps)


# ------------------------------------------------------------------------------------------------ visual smoothness
_smooth = {'handle': None}


def measure_smoothness(seconds=3.0, world_index=0, settle=1.0):
    """Walks the local player forward and records, every rendered frame, the mesh and camera positions.
    Reports the per-frame speed variation (stddev / mean, in %): ~0 is smooth, large values mean steps (stutter).
    world_index: the PIE world with the local player. Result in the log ('GASNPP smoothness')."""
    import statistics
    w = worlds()[world_index]
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    pawn = pc.get_controlled_pawn()
    mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    cam = pc.player_camera_manager
    ia = unreal.load_asset(IA['move'])
    forward = unreal.Vector(0.0, 1.0, 0.0)
    test_comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
    if test_comp:
        # Test pawn: its component injects through this player's own input subsystem
        test_comp.start_holding_input(ia, forward)
        inject = lambda: None
    else:
        # Python has no local player subsystem getter: take the live instance (single local player only, e.g. standalone).
        # InputActionValue can't be built from Python either: inject the vector every frame
        sub = next(s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if not s.get_name().startswith('Default__'))
        inject = lambda: sub.inject_input_vector_for_action(ia, forward, [], [])
    st = {'t0': time.time(), 'samples': []}

    def tick(dt):
        t = time.time() - st['t0']
        inject()
        if t > settle:
            st['samples'].append((dt, mesh.get_world_location(), cam.get_camera_location()))
        if t > settle + seconds:
            unreal.unregister_slate_post_tick_callback(_smooth['handle'])
            _smooth['handle'] = None
            if test_comp:
                test_comp.stop_holding_input(ia)
            out = []
            for k, name in ((1, 'mesh'), (2, 'camera')):
                speeds = []
                for (dt0, *a), (dt1, *b) in zip(st['samples'], st['samples'][1:]):
                    p0, p1 = a[k - 1], b[k - 1]
                    d = ((p1.x - p0.x) ** 2 + (p1.y - p0.y) ** 2) ** 0.5
                    speeds.append(d / max(dt1, 1e-4))
                m = statistics.mean(speeds)
                out.append(f'{name}: mean {m:.0f} cm/s, variation {100 * statistics.pstdev(speeds) / max(m, 1e-3):.1f}%, '
                           f'frames with no movement {sum(1 for s in speeds if s < 0.05 * m)}/{len(speeds)}')
            fps = len(st['samples']) / seconds
            unreal.log(f'GASNPP smoothness ({fps:.0f} fps): ' + ' | '.join(out))

    if _smooth['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_smooth['handle'])
    _smooth['handle'] = unreal.register_slate_post_tick_callback(tick)
