# GAS + NPP + Mover network test helpers (run inside the Unreal Editor Python console).
#
# Usage (PIE "Run Under One Process"; PRIMARY = listen server: the host plays and is world 0, clients are worlds 1..N):
#   import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import gasnpp_test as m
#   m.setup_editor(True)            # before Play: test GameMode + PIE settings = listen server, host + 2 clients (not saved), no CPU throttling
#   m.setup_editor(True, 'dedicated', 2)   # secondary check: dedicated server + 2 clients (world 0 = server with no local player)
#   m.net(60, 15, 2)                # lag ms, jitter ms, packet loss %
#   m.check_topology()              # after Play: are the roles what the mode promises?
#   m.reset_all(); m.scenario_abilities()   # all players (host included in listen); or scenario_forced_rollback / scenario_server_correction / scenario_accounting
#   print(m.report())               # counters + authoritative copy vs every other world's copy of each pawn
#   m.setup_editor(False)           # after Stop: restore GM_Sandbox, throttling and the PIE settings
# World index: 0 = host (listen) or dedicated server; 1.. = clients. In listen, hold/press/release(0, ...) drive the host's own pawn
# through the same Enhanced Input injection as the clients.
import unreal, json

CHAR = '/Game/GASNPPTest/BP_GASNPP_TestCharacter'
IA = {
    'move': '/Game/Input/IA_Move',
    'dash': '/Game/GASNPPTest/Input/IA_GASNPP_Dash',
    'buff': '/Game/GASNPPTest/Input/IA_GASNPP_Buff',
    'jump': '/Game/Input/IA_Jump',
}


_cfg = {'mode': None, 'prev': None}


def worlds():
    """[host or dedicated server, client1, client2] PIE worlds."""
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
    """Snapshot per pawn key. 'local' is added here (not in C++): this world's own player controls that pawn."""
    out = {}
    for n, c in comps(world).items():
        v = json.loads(c.get_state_snapshot())
        v['local'] = bool(c.get_owner().is_locally_controlled())
        out[n] = v
    return out


def local_comp(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = pc.get_controlled_pawn() if pc else None
    return pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent) if pawn else None


def _input_comp(index):
    """Test component of the pawn the local player of world `index` controls: the one that injects input. Index 0 only has one in
    listen mode (the host); on a dedicated server world 0 has no local player."""
    comp = local_comp(worlds()[index])
    if comp is None or not comp.get_owner().is_locally_controlled():
        raise RuntimeError(f'world {index} has no locally controlled test pawn (dedicated server world, or PIE not running)')
    return comp


def key_of_player(index):
    """Pawn key ('P<id>') of the pawn the local player of world `index` controls. Player ids depend on join order (listen:
    host first), so scenarios resolve targets through this instead of hard-coded keys."""
    return next(n for n, v in snap(worlds()[index]).items() if v['local'])


def detected_mode():
    """'listen' if world 0 has a locally controlled pawn, else 'dedicated'."""
    return 'listen' if any(v['local'] for v in snap(worlds()[0]).values()) else 'dedicated'


def all_players():
    """World indices that have a local player to drive: 0..N in listen (host included), 1..N on a dedicated server."""
    return tuple(range(0 if detected_mode() == 'listen' else 1, len(worlds())))


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
    c = _input_comp(client_index)
    return c.start_holding_input(unreal.load_asset(IA[action]), unreal.Vector(*value))


def release(client_index, action):
    c = _input_comp(client_index)
    return c.stop_holding_input(unreal.load_asset(IA[action]))


def press(client_index, action):
    c = _input_comp(client_index)
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


def _combat_history(c):
    """The history string with every frame older than FGSTN_InputHistory::MaxEventAge (254) written as '?'.
    The wire format sends 8-bit frame deltas, so a client receives such an event (the current stick, kept forever) as
    AncientFrame ('?') while the server still holds the real frame: same logical history, different strings."""
    latest = c.get('historyFrame')
    items = []
    for item in c['history'].split():
        frame, sep, rest = item.partition(':')
        if sep and latest is not None and frame.lstrip('-').isdigit() and latest - int(frame) > 254:
            frame = '?'
        items.append(frame + sep + rest)
    return ' '.join(items)


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
    # Combat input history (newest 8 events): only where both sides run the combat model, not on simulated proxies
    ca, cb = a.get('combat'), b.get('combat')
    if ca and cb and b.get('simTicks', 0) > 0 and a.get('simTicks', 0) > 0:
        ha, hb = _combat_history(ca), _combat_history(cb)
        if ha != hb:
            d.append(f"combat history '{ha}'!='{hb}'")
    return d


def _role(v):
    return v['role'].split('::')[-1].replace('ROLE_', '')


def _kind(v):
    """host-local = the host's own pawn (authority AND locally controlled); server-copy = authoritative copy of a remote
    client's pawn (on the host or on a dedicated server); autonomous = a client's own pawn; simulated = a client's view of
    someone else's pawn (the host's included)."""
    r = _role(v)
    if r == 'Authority':
        return 'host-local' if v.get('local') else 'server-copy'
    return {'AutonomousProxy': 'autonomous', 'SimulatedProxy': 'simulated'}.get(r, r)


def check_topology():
    """Do the roles match the mode? Returns (mode, problems). Listen: w0 has one Authority+local pawn (the host) and the other pawns
    Authority non-local; every client has exactly one AutonomousProxy+local pawn and the rest SimulatedProxy (the host's pawn is one of
    those). Dedicated: w0 has no local pawn. Every world sees one pawn per player."""
    ws = worlds()
    snaps = [snap(w) for w in ws]
    mode = detected_mode()
    problems = []
    if _cfg['mode'] and _cfg['mode'] != mode:
        problems.append(f"configured {_cfg['mode']} but the PIE session behaves as {mode}")
    listen = mode == 'listen'
    players = len(ws) if listen else len(ws) - 1
    for i, s in enumerate(snaps):
        if len(s) != players:
            problems.append(f'w{i} has {len(s)} pawns, expected {players}: {sorted(s)}')
        kinds = sorted(_kind(v) for v in s.values())
        if i == 0:
            want = (['host-local'] if listen else []) + ['server-copy'] * (players - (1 if listen else 0))
        else:
            want = ['autonomous'] + ['simulated'] * (players - 1)
        if kinds != sorted(want):
            problems.append(f'w{i} kinds {kinds}, expected {sorted(want)}')
    return mode, problems


def report(pos_tol=2.0, proxy_pos_tol=None):
    """Counters per world + comparison of the authoritative copy of every pawn (w0: the host's own pawn in listen mode, the server copy
    of each remote client's pawn) with that pawn in every other world:
      host-local  vs simulated  = the host's pawn as a client sees it (listen only)
      server-copy vs autonomous = a client's own pawn vs the authoritative copy on the host/server
      server-copy vs simulated  = a client's pawn as another client sees it
    proxy_pos_tol: position tolerance for 'simulated' copies (default: pos_tol); they are interpolated, so a moving one can lag."""
    ws = worlds()
    snaps = [snap(w) for w in ws]
    mode, problems = check_topology()
    lines = [f"mode={mode} worlds={len(ws)}" + (f" TOPOLOGY PROBLEMS: {problems}" if problems else '')]
    for i, s in enumerate(snaps):
        for n, v in sorted(s.items()):
            lines.append(f"w{i} {n} {_kind(v):<11} rb={v['rollbacks']:<3} rbMs={v['rolledBackMs']:<6.0f} ticks={v['simTicks']:<5} resim={v['resimTicks']:<4} abResim={v['abilityResimTicks']:<4} "
                         f"hp={v['attributes'].get('Health')} st={v['attributes'].get('Stamina')} tags={v['tags']} cd={ {k[14:]: round(x, 2) for k, x in v['cooldowns'].items()} } eff={len(v['effects'])} mode={v.get('mode')}")
    total = bad = 0
    for n, sv in sorted(snaps[0].items()):
        for i in range(1, len(snaps)):
            cv = snaps[i].get(n)
            total += 1
            if cv is None:
                bad += 1
                lines.append(f"CMP {n} {_kind(sv)}(w0) vs w{i}: MISSING pawn")
                continue
            tol = proxy_pos_tol if (proxy_pos_tol is not None and _role(cv) == 'SimulatedProxy') else pos_tol
            diffs = _diff(sv, cv, tol)
            bad += bool(diffs)
            lines.append(f"CMP {n} {_kind(sv)}(w0) vs {_kind(cv)}(w{i}): {'OK' if not diffs else diffs}")
    lines.append(f"CMP RESULT: {total - bad}/{total} OK")
    return '\n'.join(lines)


def scenario_abilities(clients=None, stagger=0.3):
    """Each player: walk forward, dash, buff, dash on cooldown, dash again, stop. Players start `stagger` seconds apart.
    clients: world indices (0 = the host, listen only); default = every player (all_players())."""
    clients = all_players() if clients is None else clients
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
    """Forces a reconcile on a client world. The host/server (world 0) is authoritative and never reconciles: refused."""
    if client_index == 0:
        raise RuntimeError('world 0 is the host/server: it has nothing to reconcile, pick a client (index >= 1)')
    w = worlds()[client_index]
    unreal.SystemLibrary.execute_console_command(w, f'np.ForceReconcileExtraFrames {extra_frames}')
    unreal.SystemLibrary.execute_console_command(w, 'np.ForceReconcile 1')
    return True


def server_apply(player_key, effect_class_name):
    """Apply an effect on the authoritative copy only (not predicted by the owning client; on the host's own pawn it is a plain
    local application)."""
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


def scenario_server_correction(player_key=None, client=1):
    """Server-side unpredicted dash/buff/cost on the pawn of world `client` (default client 1; 0 = the host's own pawn)."""
    player_key = player_key or key_of_player(client)
    steps = [
        (0.0, lambda: ('server dash (unpredicted move)', server_apply(player_key, 'GSTN_GE_Dash'))),
        (1.0, lambda: ('server buff (unpredicted effect)', server_apply(player_key, 'GSTN_GE_Buff'))),
        (1.2, lambda: ('server dash cost', server_apply(player_key, 'GSTN_GE_DashCost'))),
    ]
    run_timeline(steps)


_PIE_PROPS = ('play_net_mode', 'play_number_of_clients', 'play_net_dedicated', 'run_under_one_process')


def configure_pie(mode='listen', clients=2):
    """PIE play settings for the harness (in memory; previous values kept for setup_editor(False)). clients = remote clients.
    listen: net mode PIE_ListenServer, players = host + clients. dedicated: dedicated server + `clients` clients. Always one process.
    Returns what was set / what failed; a property not reachable from Python has to be set by hand in Play > Advanced settings."""
    _cfg['mode'] = mode
    if not hasattr(unreal, 'LevelEditorPlaySettings'):
        # Not exposed to Python in 5.8 (checked live). Start PIE through the play tool instead (e.g. Rider MCP ue_play:
        # netMode=listen, players=clients+1, runUnderOneProcess=true; dedicated: netMode=client, dedicatedServer=true,
        # players=clients) or set Play > Advanced settings by hand.
        return {'play_settings': f"unreal.LevelEditorPlaySettings not available: start PIE with {'listen, players=%d' % (clients + 1) if mode == 'listen' else 'dedicated server, players=%d' % clients}, one process"}
    s = unreal.get_default_object(unreal.LevelEditorPlaySettings)
    if _cfg['prev'] is None:
        _cfg['prev'] = {}
        for k in _PIE_PROPS:
            try:
                _cfg['prev'][k] = s.get_editor_property(k)
            except Exception:
                pass
    listen = mode == 'listen'
    want = {
        'play_net_mode': unreal.PlayNetMode.PIE_LISTEN_SERVER if listen else unreal.PlayNetMode.PIE_CLIENT,
        'play_number_of_clients': clients + 1 if listen else clients,
        'play_net_dedicated': not listen,
        'run_under_one_process': True,
    }
    result = {}
    for k, v in want.items():
        try:
            s.set_editor_property(k, v)
            result[k] = str(v)
        except Exception as e:
            result[k] = f'FAILED {e}'
    _cfg['mode'] = mode
    return result


def setup_editor(enable=True, mode='listen', clients=2):
    """Temporary editor-only setup (never saved): test GameMode override on the open level, PIE settings for `mode` ('listen' =
    primary, host + `clients` clients; 'dedicated' = secondary check), no background CPU throttling. enable=False restores all."""
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    gm = '/Game/GASNPPTest/GM_GASNPPTest' if enable else '/Game/Blueprints/GM_Sandbox'
    w.get_world_settings().set_editor_property('default_game_mode', unreal.load_asset(gm).generated_class())
    perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
    perf.set_editor_property('bThrottleCPUWhenNotForeground', not enable)
    if enable:
        pie = configure_pie(mode, clients)
    else:
        pie = {}
        s = unreal.get_default_object(unreal.LevelEditorPlaySettings) if hasattr(unreal, 'LevelEditorPlaySettings') else None
        for k, v in ((_cfg['prev'] or {}).items() if s else ()):
            try:
                s.set_editor_property(k, v)
                pie[k] = 'restored'
            except Exception as e:
                pie[k] = f'FAILED {e}'
        _cfg.update(mode=None, prev=None)
    return w.get_world_settings().get_editor_property('default_game_mode').get_name(), perf.get_editor_property('bThrottleCPUWhenNotForeground'), pie


def server_remove(player_key, effect_class_name):
    comp = comps(worlds()[0])[player_key]
    asc = comp.get_owner().get_component_by_class(unreal.NpAbilitySystemComponent)
    cls = unreal.load_class(None, f'/Script/GASP_Giustino2.{effect_class_name}')
    asc.remove_active_gameplay_effect_by_source_effect(cls, None, -1)
    return True


def scenario_accounting(client=2, player_key=None):
    """No regen: two successful dashes (+1 blocked by cooldown) and a buff, with forced rollbacks in between (client >= 1).
    Expected final: Stamina 50, Health 80 (from 50), no tags."""
    player_key = player_key or key_of_player(client)
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


_force_on_mode = {'handle': None, 'log': []}


def force_reconcile_on_mode(client=1, mode='WallSliding', extra_frames=15):
    """Forces a reconcile on the client while its pawn is in `mode` (e.g. on the wall, with GA_WallSlide active): the
    re-simulation must activate the ability again on the same frame. Read _force_on_mode['log'] after.

    np.ForceReconcile is one global cvar: under "Run Under One Process" the first client world that runs its rollback
    service takes it, which is not always `client`. It is set again every frame until `client`'s rollback counter goes
    up, so the other client may get an extra reconcile too."""
    if client == 0:
        raise RuntimeError('world 0 is the host/server: it never reconciles, pick a client (index >= 1)')
    _force_on_mode['log'] = []
    st = {'start_rb': None}

    def tick(_dt):
        try:
            comp = local_comp(worlds()[client])
            _, pawn = _local_pawn(client)
            in_mode = pawn and str(pawn.get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name()) == mode
            if st['start_rb'] is not None and comp.get_editor_property('rollback_count') > st['start_rb']:
                _force_on_mode['log'].append(f"c{client} reconciled (rollbacks {st['start_rb']} -> {comp.get_editor_property('rollback_count')})")
            elif in_mode:
                if st['start_rb'] is None:
                    st['start_rb'] = comp.get_editor_property('rollback_count')
                    _force_on_mode['log'].append(f'reconcile requested in {mode} tag={_has_tag(pawn, WALL_SLIDING_TAG)}')
                force_reconcile(client, extra_frames)
                return
            elif st['start_rb'] is None:
                return
            else:
                _force_on_mode['log'].append(f'c{client} left {mode} before reconciling')
        except Exception as e:
            _force_on_mode['log'].append(f'ERROR {e}')
        unreal.unregister_slate_post_tick_callback(_force_on_mode['handle'])
        _force_on_mode['handle'] = None

    if _force_on_mode['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_force_on_mode['handle'])
    _force_on_mode['handle'] = unreal.register_slate_post_tick_callback(tick)


def scenario_server_dash(client=1, player_key=None):
    """Client walks forward; the server applies two dashes the client did not predict.
    Each must cost the owning client about one rollback (not a chain over the whole dash)."""
    player_key = player_key or key_of_player(client)
    steps = [
        (0.0, lambda: ('move', hold(client, 'move', (0, 1, 0)))),
        (1.0, lambda: ('server dash 1', server_apply(player_key, 'GSTN_GE_Dash'))),
        (2.5, lambda: ('server dash 2', server_apply(player_key, 'GSTN_GE_Dash'))),
        (3.5, lambda: ('stop', release(client, 'move'))),
    ]
    run_timeline(steps)


# ------------------------------------------------------------------------------------------------ reconcile log check
# Network Prediction resynchronises the client's frame offset now and then (log: "ReconcileFromInputOffset Old Offset X, New
# Offset Y", every few seconds in PIE "Run Under One Process", also with no network emulation). The client's simulation
# time jumps with it, so a state mismatch on that frame is expected (an idle pawn shows it in the period timer of a periodic
# effect). A run is clean when no "Sync/Aux mismatch" reconcile happens outside the frames of an offset change; forced
# reconciles (np.ForceReconcile) are counted apart. Remove the exception when the offset resyncs are fixed in the fork.
import os, re

_LOG_LINE = re.compile(r'^(\[\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d:\d+\]\[\s*\d+\])(\w+): (?:(\w+): )?(.*)$')


def _log_path():
    return os.path.join(unreal.Paths.project_log_dir(), unreal.SystemLibrary.get_game_name() + '.log')


def mark_log():
    """Byte position of the end of the editor log: pass it to report_reconciles() to look only at what follows."""
    return os.path.getsize(_log_path())


def report_reconciles(since=0):
    """Reconciles in the editor log after `since` (see mark_log): returns a dict and logs a summary.
    unexplained = state mismatches not on the frame of an offset change (the number that must be 0)."""
    with open(_log_path(), 'rb') as f:
        f.seek(since)
        text = f.read().decode('utf-8', errors='replace')
    offset_frames, mismatches, forced = set(), [], 0
    for line in text.splitlines():
        m = _LOG_LINE.match(line)
        if not m:
            continue
        stamp, category, _verbosity, message = m.groups()
        if category != 'LogNetworkPrediction':
            continue
        if message.startswith('ReconcileFromInputOffset'):
            offset_frames.add(stamp)
        elif message.startswith('Reconcile required due to Sync/Aux mismatch'):
            mismatches.append((stamp, message))
        elif 'ForceReconcile' in message and message.startswith('Reconcile activated'):
            forced += 1
    unexplained = [x for x in mismatches if x[0] not in offset_frames]
    result = {'offset_changes': len(offset_frames), 'mismatches': len(mismatches), 'on_offset_frames': len(mismatches) - len(unexplained),
              'unexplained': len(unexplained), 'forced': forced, 'unexplained_lines': [x[1] for x in unexplained[:5]]}
    unreal.log('GASNPP reconciles: ' + json.dumps(result))
    return result


# ------------------------------------------------------------------------------------------------ combat (GSTNCombat)
def combat_tap(client_index, mask):
    """One-frame press of the combat buttons in `mask` (bit i = button i of the pawn's GSTN_CombatSimComponent)."""
    _, pawn = _local_pawn(client_index)
    pawn.get_component_by_class(unreal.GSTN_CombatSimComponent).tap_scripted_buttons(mask)
    return True


def scenario_combat_spike(client=1):
    """Combat model spike on BP_GASNPP_TestCharacter (button 0 -> test dash, button 1 -> hitstop window):
    the combat model asks the ability system for the dash, also across forced re-simulations.
    Clean = server/client state equal and no reconciles besides the forced ones (np.PrintReconciles 1 to check)."""
    steps = [
        (0.0, lambda: ('move', hold(client, 'move', (0, 1, 0)))),
        (1.0, lambda: ('combat dash', combat_tap(client, 1))),
        (1.1, lambda: ('force reconcile across the request frame', force_reconcile(client, 15))),
        (1.6, lambda: ('hitstop', combat_tap(client, 2))),
        (1.7, lambda: ('force reconcile during hitstop', force_reconcile(client, 10))),
        (3.4, lambda: ('combat dash after cooldown', combat_tap(client, 1))),
        (4.0, lambda: ('stop', release(client, 'move'))),
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
