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
    """Same emulation on every PIE net driver (server and clients). Remembered: abilities_startup re-applies it after each PIE restart."""
    _cfg['net'] = (lag_ms, var_ms, loss_pct)
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
_timeline = {'handle': None}   # legacy name: kept None so callbacks of older module versions stay inert
_running = {'handle': None}


def run_timeline(steps):
    """steps: list of (seconds_from_now, callable). Runs on editor ticks without blocking."""
    if _running['handle'] is not None:
        # A previous timeline still running would otherwise leak its callback and, when it ends, unregister this one's handle.
        unreal.unregister_slate_post_tick_callback(_running['handle'])
        _running['handle'] = None
    start = time.time()
    pending = sorted(steps, key=lambda s: s[0])
    log = []
    handle = []

    def tick(_dt):
        now = time.time() - start
        while pending and pending[0][0] <= now:
            t, fn = pending.pop(0)
            try:
                log.append(f'{t:.1f}s {fn()}')
            except Exception as e:
                log.append(f'{t:.1f}s ERROR {e}')
        if not pending and handle:
            unreal.unregister_slate_post_tick_callback(handle.pop())
            if _running['handle'] is not None and not handle:
                _running['handle'] = None
            unreal.log('GASNPP timeline done: ' + ' | '.join(log))

    handle.append(unreal.register_slate_post_tick_callback(tick))
    _running['handle'] = handle[0]


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


def walk_back(client=1, x=680.0, y=-880.0):
    """Walks the client's pawn to x <= `x` (away from the wall), so scenario_jump_wall starts from a known distance. Poll
    the log for 'GASNPP walk_back done' before starting the next scenario."""
    ia_move = unreal.load_asset(IA['move'])
    handle = {'h': None}

    def tick(_dt):
        pc, pawn = _local_pawn(client)
        comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
        if pawn.get_actor_location().x > x:
            comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, (x - 100.0, y))))
        else:
            comp.stop_holding_input(ia_move)
            unreal.unregister_slate_post_tick_callback(handle['h'])
            unreal.log(f'GASNPP walk_back done x={pawn.get_actor_location().x:.0f}')

    handle['h'] = unreal.register_slate_post_tick_callback(tick)


def scenario_jump_wall(client=1, wall_x=1010.0, wall_y=-880.0, jump_distance=130.0, wall_time=0.7, ground_jump_first=True):
    """Ground jump in place, then walk to the wall (face at x=wall_x, normal -X), jump at it, hold into it, wall jump.
    The pawn must start further than jump_distance from the wall (walk_back()); a jump from 220 cm lands short of it.
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


_log_cache = {'path': None}


def _flush_log():
    """Pushes the editor's buffered log lines to disk (unreal.log_flush when this build exposes it)."""
    fn = getattr(unreal, 'log_flush', None)
    if fn is None:
        return False
    try:
        fn()
        return True
    except Exception:
        return False


def _tail(path, size=262144):
    with open(path, 'rb') as f:
        f.seek(0, os.SEEK_END)
        f.seek(max(0, f.tell() - size))
        return f.read().decode('utf-8', errors='replace')


def _log_path():
    """Log file of THIS editor process. Two editors on one project (Development and DebugGame) share Saved/Logs: the first owns
    <Game>.log, the second is renamed (<Game>_2.log...) because the first holds the file, so "<Game>.log" is not necessarily ours.
    Order of trust:
      1. -ABSLOG=<file> / -LOG=<file.log> on this process's command line: the process was told which file to write.
      2. A probe: write a line with a unique token (pid + uuid) through unreal.log, flush, and take the candidate
         <Game>*.log whose tail contains it. Only the file this process writes can contain a token it just generated, so this
         needs no assumption about the naming scheme, the number of editors or file locking (a "newest file" or "file that is
         open" guess is wrong while the other editor logs too). The probe blocks the game thread for at most ~1 s, once.
    The result is cached (the log file does not change during a session)."""
    import uuid
    cached = _log_cache['path']
    if cached and os.path.exists(cached):
        return cached
    cmd = unreal.SystemLibrary.get_command_line()
    log_dir = unreal.Paths.project_log_dir()
    for pattern, rel in ((r'-ABSLOG=("[^"]+"|\S+)', False), (r'-LOG=("[^"]+\.log"|\S+\.log)', True)):
        m = re.search(pattern, cmd, re.IGNORECASE)
        if m:
            p = m.group(1).strip('"')
            p = os.path.join(log_dir, p) if rel and not os.path.isabs(p) else p
            if os.path.exists(p):
                _log_cache['path'] = p
                return p
    base = unreal.SystemLibrary.get_game_name()
    token = f'GASNPP_LOG_PROBE pid={os.getpid()} {uuid.uuid4().hex}'
    unreal.log(token)
    for _ in range(10):
        _flush_log()
        candidates = [os.path.join(log_dir, f) for f in os.listdir(log_dir)
                      if f.startswith(base) and f.endswith('.log') and '-backup-' not in f]
        for p in candidates:
            try:
                if token in _tail(p):
                    _log_cache['path'] = p
                    return p
            except OSError:
                pass
        time.sleep(0.1)
    raise RuntimeError(f'could not find this editor process\'s log in {log_dir} (probe {token} not found in any {base}*.log)')


def mark_log():
    """Byte position of the end of the editor log: pass it to report_reconciles() to look only at what follows.
    Flushes first, otherwise lines still buffered in memory would land after the mark and be counted in the next window."""
    _flush_log()
    return os.path.getsize(_log_path())


def _read_log_since(since):
    """Text of the log after `since`, tolerant of lines not flushed yet: flush, then read until the size stops growing (at most
    ~0.5 s). A trailing line without newline is a half-written one and is dropped."""
    path = _log_path()
    last = -1
    for _ in range(6):
        _flush_log()
        size = os.path.getsize(path)
        if size == last:
            break
        last = size
        time.sleep(0.08)
    with open(path, 'rb') as f:
        f.seek(since)
        text = f.read().decode('utf-8', errors='replace')
    return text if text.endswith('\n') else text[:text.rfind('\n') + 1]


def _tick_key(stamp):
    """'[2026.10.23-01.49.34:967][397]' -> '2026.10.23-01.49.34][397' (the wall-clock second and the engine tick, no millisecond)"""
    return stamp.split(':')[0] + stamp[stamp.index(']'):]


_MODEL_MARKERS = (('[MoverDefaultSyncState]', 'mover'), ('Current period Time', 'asc'), ('Active Mappings', 'asc'), ('History:', 'combat'))


def report_reconciles(since=0):
    """Reconciles in the editor log after `since` (see mark_log): returns a dict and logs a summary.
    unexplained = state mismatches not on the tick of an offset change, with the model each one came from in
    unexplained_by_model. unexplained_ours leaves out the Mover model (position corrections under network lag are the
    movement prediction's own business); it is the number the 'no unexplained reconciles' checks require to be 0.
    max_burst = most mismatches logged within one wall-clock second (a startup storm shows here)."""
    text = _read_log_since(since)
    offset_frames, mismatches, forced = set(), [], 0
    models = []  # model of each mismatch, read from the first state block that follows its line
    for line in text.splitlines():
        if models and models[-1] is None:
            for marker, model in _MODEL_MARKERS:
                if marker in line:
                    models[-1] = model
                    break
        m = _LOG_LINE.match(line)
        if not m:
            continue
        stamp, category, _verbosity, message = m.groups()
        if category != 'LogNetworkPrediction':
            continue
        if message.startswith('ReconcileFromInputOffset'):
            # Every model of the entity logs its own mismatch a few ms after the offset line, inside the same engine tick:
            # match on (second, tick, new offset), not on the millisecond
            new_offset = re.search(r'New Offset (\d+)', message)
            offset_frames.add((_tick_key(stamp), new_offset.group(1) if new_offset else None))
        elif message.startswith('Reconcile required due to Sync/Aux mismatch'):
            mismatches.append((stamp, message))
            models.append(None)
        elif 'ForceReconcile' in message and message.startswith('Reconcile activated'):
            forced += 1
    def _on_offset_change(stamp, message):
        offset = re.search(r'Offset: (\d+)\.', message)
        return (_tick_key(stamp), offset.group(1) if offset else None) in offset_frames

    unexplained = [x for x in mismatches if not _on_offset_change(*x)]
    by_model = {}
    for x, model in zip(mismatches, models):
        if not _on_offset_change(*x):
            by_model[model or 'unknown'] = by_model.get(model or 'unknown', 0) + 1
    per_second = {}
    for stamp, _ in mismatches:
        per_second[stamp.split(':')[0]] = per_second.get(stamp.split(':')[0], 0) + 1
    result = {'offset_changes': len(offset_frames), 'mismatches': len(mismatches), 'on_offset_frames': len(mismatches) - len(unexplained),
              'unexplained': len(unexplained), 'unexplained_by_model': by_model,
              'unexplained_ours': len(unexplained) - by_model.get('mover', 0), 'forced': forced, 'max_burst': max(per_second.values(), default=0),
              'unexplained_lines': [x[1] for x in unexplained[:5]]}
    unreal.log('GASNPP reconciles: ' + json.dumps(result))
    return result


# ------------------------------------------------------------------------------------------------ combat (GSTNCombat)
def combat_tap(client_index, mask):
    """One-frame press of the combat buttons in `mask` (bit i = button i of the pawn's GSTN_CombatSimComponent)."""
    _, pawn = _local_pawn(client_index)
    pawn.get_component_by_class(unreal.GSTN_CombatSimComponent).tap_scripted_buttons(mask)
    return True


# ---- test content names: buttons are resolved from the InputConfig asset, never hard-coded as indices
INPUT_CONFIG = '/Game/GASNPPTest/Input/IC_GASNPP_Test'
TAG_DASHING, TAG_ATTACKING, TAG_STANCE, TAG_BUFFED = 'Test.State.Dashing', 'Test.State.Attacking', 'Test.State.Stance', 'Test.State.Buffed'


def _tag_text(tag):
    try:
        return str(tag.get_editor_property('tag_name'))
    except Exception:
        m = re.search(r'TagName="?([^")]+)', str(tag))
        return m.group(1) if m else str(tag)


def button_index(name):
    """Index (= bit) of button `name` ('Attack' or 'Input.Button.Attack') in IC_GASNPP_Test, looked up at call time so that
    reordering the buttons in the asset does not break the scenarios. Raises when the asset has no such button."""
    want = name if name.startswith('Input.Button.') else f'Input.Button.{name}'
    names = [_tag_text(b.get_editor_property('tag')) for b in unreal.load_asset(INPUT_CONFIG).get_editor_property('buttons')]
    if want not in names:
        raise RuntimeError(f'{want} is not a button of {INPUT_CONFIG}: {names}')
    return names.index(want)


def button_mask(button):
    """Mask of a button given by name (resolved from the asset) or by raw index (kept for old callers)."""
    return 1 << (button if isinstance(button, int) else button_index(button))


def _combat_comp(client_index):
    _, pawn = _local_pawn(client_index)
    return pawn.get_component_by_class(unreal.GSTN_CombatSimComponent)


def _require_test_config(client_index):
    """The pawn must run the input config the names were resolved from; skipped when the property is not readable from Python."""
    try:
        cfg = str(_combat_comp(client_index).get_editor_property('input_config'))
    except Exception:
        return
    if cfg and 'None' not in cfg and 'IC_GASNPP_Test' not in cfg:
        raise RuntimeError(f'the pawn uses {cfg}, not IC_GASNPP_Test: button names would resolve against the wrong asset')


def _total_rollbacks():
    return sum(local_comp(w).get_editor_property('rollback_count') for w in worlds()[1:] if local_comp(w))


_last_run = {'name': None, 'results': [], 'verdict': None}


class _Run:
    """Sampler + assertions of one strict scenario. Every editor tick it records, for the authoritative copy (world 0) and the
    client's own copy of the player's pawn: the Test.* tags, stamina, cooldowns, mode, rollbacks and the combat request. Checks
    are timeline steps over windows of those samples ("tag seen between t0 and t1"), so they do not depend on the exact tick a
    state change lands on, and each scenario asserts a positive effect (tag / cooldown / stamina) before it asserts an absence,
    so it cannot pass vacuously. Verdict: unreal.log "PASS"/"FAIL" line, also in last_checks()."""

    def __init__(self, name, client, extra=None, judge_reconciles=True):
        """extra: more sampled copies as [(label, world_index, pawn_key)]; their samples are read like 'srv' / 'cli' (spike_stats uses
        it for the second pawn). judge_reconciles=False: the spike scenarios create reconciles on purpose (a grant, an unpredicted
        effect) and judge them per window themselves, so the whole-run 'no unexplained reconciles' check is left out."""
        self.name, self.client, self.key = name, client, key_of_player(client)
        self.extra, self.judge_reconciles = list(extra or []), judge_reconciles
        self.results, self.samples, self.handle = [], [], None
        self.rb0 = _total_rollbacks()
        self.log_mark = None
        try:
            self.log_mark = mark_log()
        except Exception as e:
            self.note(f'log not readable ({e}): reconcile check skipped')
        self.t0 = time.time()
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def now(self):
        return time.time() - self.t0

    def _tick(self, _dt):
        try:
            ws = worlds()
            s = {'t': self.now()}
            for side, wi, key in (('srv', 0, self.key), ('cli', self.client, self.key), *self.extra):
                c = comps(ws[wi]).get(key)
                if c is None:
                    s[side] = None
                    continue
                v = json.loads(c.get_state_snapshot())
                s[side] = {'tags': set(v.get('tags', [])), 'stamina': v['attributes'].get('Stamina'), 'cd': dict(v['cooldowns']),
                           'mode': v.get('mode'), 'req': tuple((v.get('combat') or {}).get('request', (0, -1))),
                           'loc': tuple(v['location']), 'vel': tuple(v.get('velocity', (0, 0, 0))), 'attr': dict(v['attributes']),
                           'probe': v.get('probe') or {}}
            self.samples.append(s)
        except Exception:
            pass

    def run(self, steps):
        """Restarts the clock and runs the timeline; the last step must be finish()."""
        self.t0 = time.time()
        self.samples.clear()
        run_timeline(steps)

    # -- queries over the samples
    def _win(self, side, t0, t1):
        return [s[side] for s in self.samples if t0 <= s['t'] <= t1 and s.get(side)]

    def saw_tag(self, tag, t0, t1, side='srv'):
        return any(tag in s['tags'] for s in self._win(side, t0, t1))

    def held_tag(self, tag, t0, t1, side='srv'):
        w = self._win(side, t0, t1)
        return bool(w) and all(tag in s['tags'] for s in w)

    def saw_cooldown(self, name, t0, t1, side='srv'):
        return any(x > 0 for s in self._win(side, t0, t1) for k, x in s['cd'].items() if name in k)

    def stamina_drop(self, t0, t1):
        """Stamina before t0 minus the lowest value in [t0, t1] (regen can only make it smaller, never fake a drop)."""
        before = [s['srv']['stamina'] for s in self.samples if s['t'] < t0 and s.get('srv') and s['srv']['stamina'] is not None]
        low = [s['stamina'] for s in self._win('srv', t0, t1) if s['stamina'] is not None]
        return (before[-1] - min(low)) if before and low else 0.0

    def new_bindings(self, t0, t1, side='srv'):
        """Bindings (index + 1; the combat sync state's RequestedBinding) of requests made in [t0, t1], in order. A request is
        new when its frame differs from the last sample before t0 (the sync state keeps the last one)."""
        before = [s[side]['req'] for s in self.samples if s['t'] < t0 and s.get(side)]
        seen, out = (before[-1][1] if before else None), []
        for s in self._win(side, t0, t1):
            if s['req'][1] != seen and s['req'][0] > 0:
                out.append(s['req'][0])
            seen = s['req'][1]
        return out

    def latest(self, side):
        return next((s[side] for s in reversed(self.samples) if s.get(side)), None)

    # -- results
    def note(self, text):
        self.results.append((None, text, ''))

    def check(self, label, ok, detail=''):
        self.results.append((bool(ok), label, detail))

    def at(self, label, fn):
        """Timeline step: fn() -> (ok, detail) evaluated at that time."""
        def step():
            try:
                ok, detail = fn()
            except Exception as e:
                ok, detail = False, f'exception {e}'
            self.check(label, ok, detail)
            return f"{'PASS' if ok else 'FAIL'} {label} {detail}"
        return step

    def finish(self):
        """Timeline step: stops sampling, adds the reconcile and forced-reconcile checks, logs the verdict."""
        def step():
            if self.handle is not None:
                unreal.unregister_slate_post_tick_callback(self.handle)
                self.handle = None
            if self.log_mark is not None and self.judge_reconciles:
                try:
                    r = report_reconciles(self.log_mark)
                    self.check('no unexplained reconciles', r['unexplained_ours'] == 0, f"{r['unexplained_ours']} unexplained {r['unexplained_by_model']} {r['unexplained_lines']}, forced={r['forced']}")
                except Exception as e:
                    self.note(f'reconcile check failed to run: {e}')
            if self.name in _FORCES_RECONCILES:
                gained = _total_rollbacks() - self.rb0
                self.check('forced reconciles happened (rollbacks went up)', gained > 0, f'+{gained} (np.ForceReconcile is one global cvar: another client may take it)')
            bad = [(l, d) for ok, l, d in self.results if ok is False]
            verdict = ('PASS' if not bad else 'FAIL') + f" {sum(1 for ok, _, _ in self.results if ok is not None)} checks"
            _last_run.update(name=self.name, results=list(self.results), verdict=verdict)
            text = f'GASNPP {self.name}: {verdict}' + ''.join(f"\n  {'PASS' if ok else 'FAIL' if ok is False else 'note'} {l} {d}" for ok, l, d in self.results)
            (unreal.log_error if bad else unreal.log)(text)
            return verdict
        return step


_FORCES_RECONCILES = ('combat_spike', 'combat_lock', 'spike_stats', 'spike_triggers')


def last_checks():
    """(verdict, [(ok, label, detail)]) of the last strict scenario (ok None = a note, not a check)."""
    return _last_run['verdict'], _last_run['results']


def scenario_combat_spike(client=1, dash_button='Dash'):
    """Combat model spike on BP_GASNPP_TestCharacter: the combat model asks the ability system for the dash (Input.Button.<dash_button>,
    resolved from IC_GASNPP_Test), also across forced re-simulations. Strict: each dash must actually happen (Test.State.Dashing on
    the server and predicted on the client, dash cooldown, stamina spent); the second one only after the cooldown. Needs a
    Command.Dash slot in the standard slot set (see testing.md). The hitstop step uses the component's HitstopTestButton, when set.
    Verdict: last_checks() / the log; also compare report() and np.PrintReconciles 1."""
    _require_test_config(client)
    dash = button_mask(dash_button)
    try:
        hs = _combat_comp(client).get_editor_property('hitstop_test_button')
    except Exception:
        hs = -1
    run = _Run('combat_spike', client)

    def dash_checks(t0, t1):
        return [
            (t1, run.at(f'dash at {t0}s: Test.State.Dashing on the server', lambda: (run.saw_tag(TAG_DASHING, t0, t1), ''))),
            (t1, run.at(f'dash at {t0}s: predicted on the client', lambda: (run.saw_tag(TAG_DASHING, t0, t1, 'cli'), ''))),
            (t1, run.at(f'dash at {t0}s: cooldown applied', lambda: (run.saw_cooldown('Dash', t0, t1), ''))),
            (t1, run.at(f'dash at {t0}s: stamina spent', lambda: (run.stamina_drop(t0, t1) > 1.0, f'drop {run.stamina_drop(t0, t1):.1f}'))),
        ]

    steps = [
        (0.0, lambda: ('move', hold(client, 'move', (0, 1, 0)))),
        (1.0, lambda: ('combat dash', combat_tap(client, dash))),
        (1.1, lambda: ('force reconcile across the request frame', force_reconcile(client, 15))),
        *dash_checks(1.0, 1.9),
        (3.4, lambda: ('combat dash after cooldown', combat_tap(client, dash))),
        *dash_checks(3.4, 4.3),
        (4.4, lambda: ('stop', release(client, 'move'))),
        (4.8, run.finish()),
    ]
    if hs >= 0:
        steps += [(1.6, lambda: ('hitstop', combat_tap(client, 1 << hs))),
                  (1.7, lambda: ('force reconcile during hitstop', force_reconcile(client, 10)))]
    else:
        run.note('hitstop step skipped: HitstopTestButton is unset on the combat component')
    run.run(steps)


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


# ------------------------------------------------------------------------------------------------ combat locks (M2 S2c)
def combat_hold(client_index, mask):
    """Replaces the sampled combat buttons with `mask` (bit i = button i) until combat_clear. Taps still add presses."""
    _, pawn = _local_pawn(client_index)
    pawn.get_component_by_class(unreal.GSTN_CombatSimComponent).set_scripted_buttons(mask)
    return True


def combat_clear(client_index):
    _, pawn = _local_pawn(client_index)
    pawn.get_component_by_class(unreal.GSTN_CombatSimComponent).clear_scripted_buttons()
    return True


def scenario_combat_lock(client=1, attack_button='Attack', stance_button='Special', dash_button='Dash'):
    """Command locks (M2 section 6b/8) on the test content (GSTN_GA_TestStance on the Special command: owns Test.State.Stance and
    Input.Lock.Command.Attack while the trigger button is held; GSTN_GA_TestAttack on the Attack commands: Test.State.Attacking;
    GSTN_GA_TestDash on Command.Dash). Buttons are names resolved from IC_GASNPP_Test (an int is still taken as a raw index).
    Strict, in order (every absence is asserted only together with the stance being active, so it cannot be vacuous):
      0.5s  stance held                 -> Test.State.Stance on the server (and predicted on the client)
      1.0s  dash tap under the stance   -> dashes (another root, not locked): Dashing + cooldown + stamina spent
      2.0s  attack tap under the stance -> no Test.State.Attacking while the stance holds (locked; the press expires in the buffer)
      3.0s  stance + attack held, 3.2s stance released, attack still held -> Attacking comes out after the release, not before
      4.0s  stance held again, 4.2s forced reconcile -> stance continuous across it (the lock survives the re-simulation)
    Needs a Command.Dash slot (testing.md, open item); without it the dash check fails visibly. Also: no unexplained reconciles."""
    _require_test_config(client)
    stance, attack, dash = button_mask(stance_button), button_mask(attack_button), button_mask(dash_button)
    run = _Run('combat_lock', client)
    steps = [
        (0.0, lambda: ('move', hold(client, 'move', (0, 1, 0)))),
        (0.5, lambda: ('stance held', combat_hold(client, stance))),
        (0.95, run.at('stance activated (server)', lambda: (run.saw_tag(TAG_STANCE, 0.5, 0.95), ''))),
        (0.95, run.at('stance predicted (client)', lambda: (run.saw_tag(TAG_STANCE, 0.5, 0.95, 'cli'), ''))),
        (1.0, lambda: ('dash under the attack lock', combat_tap(client, dash))),
        (1.9, run.at('dash not locked: Dashing', lambda: (run.saw_tag(TAG_DASHING, 1.0, 1.9), ''))),
        (1.9, run.at('dash not locked: cooldown + stamina', lambda: (run.saw_cooldown('Dash', 1.0, 1.9) and run.stamina_drop(1.0, 1.9) > 1.0, f'drop {run.stamina_drop(1.0, 1.9):.1f}'))),
        (2.0, lambda: ('attack tap under the lock', combat_tap(client, attack))),
        (2.9, run.at('attack locked: stance held the whole time', lambda: (run.held_tag(TAG_STANCE, 2.0, 2.9), ''))),
        (2.9, run.at('attack locked: no Attacking', lambda: (not run.saw_tag(TAG_ATTACKING, 2.0, 2.9), ''))),
        (3.0, lambda: ('stance + attack held', combat_hold(client, stance | attack))),
        (3.2, lambda: ('stance released, attack still held', combat_hold(client, attack))),
        (3.15, run.at('attack still locked right before the release', lambda: (run.held_tag(TAG_STANCE, 3.0, 3.1) and not run.saw_tag(TAG_ATTACKING, 3.0, 3.15), ''))),
        (3.6, lambda: ('release all', combat_clear(client))),
        (3.95, run.at('held attack comes out after the stance ends', lambda: (run.saw_tag(TAG_ATTACKING, 3.2, 3.95), ''))),
        (3.95, run.at('stance ended on release', lambda: (not run.saw_tag(TAG_STANCE, 3.6, 3.95), ''))),
        (4.0, lambda: ('stance held again', combat_hold(client, stance))),
        (4.2, lambda: ('force reconcile while the stance is held', force_reconcile(client, 10))),
        (4.75, run.at('stance continuous across the reconcile', lambda: (run.held_tag(TAG_STANCE, 4.45, 4.75), ''))),
        (4.8, lambda: ('release all', combat_clear(client))),
        (5.5, lambda: ('stop', release(client, 'move'))),
        (5.9, run.finish()),
    ]
    run.run(steps)


def scenario_stance_lock(client=1, **kw):
    """C9: the stance of the lock scenario now runs on UGSTN_GA_Base (bEndOnTriggerRelease, the trigger button captured into sync
    variables by the base), so this is scenario_combat_lock over it: held stance, attack locked, release ends it, the stance survives
    a forced reconcile (the trigger button is restored with the ability's sync vars)."""
    return scenario_combat_lock(client, **kw)


def scenario_camera_aim(client=1, yaw=123.0, pitch=-20.0, stance_button='Special'):
    """C9 camera aim (combat-m3 7.10). IC_GASNPP_Test gets bSendCameraAim on IN MEMORY (never saved), the client's control rotation is set to
    (pitch, yaw), the stance is pressed and its GA_Base logs the aim it read on activation (GASNPP_AIM, one line per machine that
    activates it). Strict: the server and the predicting client both activated it, both read the aim within 0.02 degree of the control
    rotation, and the activation context carried the Special button on both. The flag is switched back off at the end."""
    _require_test_config(client)
    cfg = unreal.load_asset(INPUT_CONFIG)
    cfg.set_editor_property('send_camera_aim', True)
    stance = button_mask(stance_button)
    run = _Run('camera_aim', client, judge_reconciles=False)
    state = {}

    def lines():
        text = _read_log_since(state['mark'])
        out = []
        for l in text.splitlines():
            m = re.search(r'GASNPP_AIM role=(\d+) local=(\d+) yaw=([-\d.]+) pitch=([-\d.]+) button=(-?\d+) source=(\d+)', l)
            if m:
                out.append((int(m.group(1)), int(m.group(2)), float(m.group(3)), float(m.group(4)), int(m.group(5)), int(m.group(6))))
        return out

    def near(a, b):
        return abs(((a - b) + 180.0) % 360.0 - 180.0) < 0.02

    def check_lines():
        ls = lines()
        srv = [x for x in ls if x[0] == 3]      # ROLE_Authority
        cli = [x for x in ls if x[1] == 1 and x[0] == 2]   # autonomous proxy
        ok = bool(srv) and bool(cli) and all(near(x[2], yaw) and near(x[3], pitch) for x in srv + cli)
        btn = button_index(stance_button)
        ok = ok and all(x[4] == btn and x[5] == 0 for x in srv + cli)
        return ok, f'server {srv[:2]} client {cli[:2]}'

    steps = [
        (0.0, lambda: ('mark', state.update(mark=mark_log()))),
        (0.1, lambda: ('control rotation', _set_pitch_yaw(client, pitch, yaw))),
        (0.5, lambda: ('stance held', combat_hold(client, stance))),
        (1.2, run.at('aim read by the stance on the server and the client, trigger button in the context', check_lines)),
        (1.3, lambda: ('release', combat_clear(client))),
        (2.0, run.at('stance ended on release', lambda: (not run.saw_tag(TAG_STANCE, 1.75, 2.0), ''))),
        (2.05, lambda: ('flag off', cfg.set_editor_property('send_camera_aim', False))),
        (2.15, run.finish()),
    ]
    run.run(steps)


def _tag_span(run, side, tag, t0, t1):
    """(first, last) sample times in [t0, t1] at which `side` owned `tag`, None if never. The server's copy lags the client's
    clock by the input lead, so the checks of timeline_cancel compare durations and order, never absolute times."""
    ts = [s['t'] for s in run.samples if t0 <= s['t'] <= t1 and s.get(side) and tag in s[side]['tags']]
    return (ts[0], ts[-1]) if ts else None


def scenario_timeline_cancel(client=1, attack_button='Attack', stance_button='Special'):
    """C10 timeline abilities (combat-m3 7.2, 8.3) on UGSTN_GA_TestAttack, a UGSTN_GA_BaseCombat of 20 frames (0.32 s), startup override
    window of 8 frames (0.13 s), cancel window [8, 14) into Command.Special, free from frame 14, and the stance (Special, a higher tier).
    Three cases 1.5 s apart, judged on the server's copy and the predicting client's by how long Attacking lasts and in which order the
    tags come (the natural length of the attack is 0.32 s, sampled every ~40 ms):
      0.5s  startup override: attack tap, the stance held 0.08 s later (frame ~5, inside the window) -> Stance comes and Attacking ends
            well before its natural end (the replaced timeline is cancelled)
      2.0s  cancel window: attack tap, the stance held 0.17 s later (frame ~10, in [8, 14)) -> Stance comes and Attacking ends before 0.28 s
      3.5s  not cancellable: attack tap, a second attack tap 0.05 s later (same tier, before the first actionable frame) -> Attacking lasts
            its natural 0.28 to 0.38 s (a restart would make it 0.42 s or more)
    Also: no unexplained reconciles."""
    _require_test_config(client)
    attack, stance = button_mask(attack_button), button_mask(stance_button)
    run = _Run('timeline_cancel', client)
    global _tc_run
    _tc_run = run

    def case(label, a0, a1, side, lo, hi, with_stance):
        def f():
            att = _tag_span(run, side, TAG_ATTACKING, a0, a1)
            if att is None:
                return False, f'{side}: Attacking never seen'
            dur = att[1] - att[0] + 0.04      # one sample of resolution
            ok = lo <= dur <= hi
            detail = f'{side}: Attacking {dur:.2f}s (want {lo}..{hi})'
            if with_stance:
                st = _tag_span(run, side, TAG_STANCE, a0, a1)
                ok = ok and st is not None and st[0] >= att[0]
                detail += f', stance {"seen after the attack started" if ok else "missing or before the attack"}'
            return ok, detail
        return f

    steps = [
        # case 1: startup override
        (0.5, lambda: ('attack', combat_tap(client, attack))),
        (0.58, lambda: ('stance held (inside the override window)', combat_hold(client, stance))),
        (1.0, lambda: ('release', combat_clear(client))),
        (1.4, run.at('override: server', case('', 0.4, 1.4, 'srv', 0.0, 0.2, True))),
        (1.4, run.at('override: client', case('', 0.4, 1.4, 'cli', 0.0, 0.2, True))),
        # case 2: cancel window
        (2.0, lambda: ('attack', combat_tap(client, attack))),
        (2.17, lambda: ('stance held (inside the cancel window)', combat_hold(client, stance))),
        (2.6, lambda: ('release', combat_clear(client))),
        (3.1, run.at('cancel window: server', case('', 1.9, 3.1, 'srv', 0.1, 0.28, True))),
        (3.1, run.at('cancel window: client', case('', 1.9, 3.1, 'cli', 0.1, 0.28, True))),
        # case 3: not cancellable by the same tier
        (3.5, lambda: ('attack', combat_tap(client, attack))),
        (3.55, lambda: ('second attack (same tier, before the first actionable frame)', combat_tap(client, attack))),
        (4.6, run.at('no cancel: server', case('', 3.4, 4.6, 'srv', 0.28, 0.38, False))),
        (4.6, run.at('no cancel: client', case('', 3.4, 4.6, 'cli', 0.28, 0.38, False))),
        (4.7, run.finish()),
    ]
    run.run(steps)


def _set_pitch_yaw(client_index, pitch, yaw):
    pc, _ = _local_pawn(client_index)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    return (pitch, yaw)


# ------------------------------------------------------------------------------------------------ combat input resolution (M2 S2b)
# Strict scenarios over the test content (IC_GASNPP_Test + SS_GASNPP_Standard + the binding set on BP_GASNPP_TestCharacter): the
# button names come from the asset, an activation is proven by tags/cooldown/stamina, a resolved binding by the combat sync state's
# request (binding index + 1, sampled every tick). Not coverable with the current content (no binding/slot for it): buffer,
# motion, chord, spin, the "airborne-less command" part of the context test, light-tilt (aim zone) and "crouch 2 frames later"
# (the editor tick cannot place a press with frame accuracy). See testing.md.
def _set_yaw(client_index, yaw):
    pc, _ = _local_pawn(client_index)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    return yaw


_main_runs = {}


def scenario_combat_main_inputs(client=1, camera_yaw=0.0):
    """The four standard attack commands from stick + crouch: attack alone (Neutral), attack + stick forward (Directional), crouch held +
    attack (CrouchNeutral), crouch held + stick + attack (CrouchDirectional), each 1.2 s apart. Each must start Test.State.Attacking
    and resolve a new binding; the four bindings must be four different ones. `camera_yaw` rotates the control rotation first: run it
    again with another yaw (e.g. 90) and the bindings must be identical (the stick is relative to the camera/facing, not to the world)."""
    _require_test_config(client)
    attack, crouch = button_mask('Attack'), button_mask('Crouch')
    run = _Run('combat_main_inputs', client)
    cases = [('neutral', False, False), ('directional', True, False), ('crouch_neutral', False, True), ('crouch_directional', True, True)]
    found = {}

    def setup(tilt, crouching):
        if tilt:
            hold(client, 'move', (0, 1, 0))
        if crouching:
            combat_hold(client, crouch)
        else:
            combat_clear(client)
        return f'tilt={tilt} crouch={crouching}'

    def teardown(tilt):
        if tilt:
            release(client, 'move')
        combat_clear(client)
        return 'released'

    def check(name, a, b):
        def f():
            bs = run.new_bindings(a, b)
            found[name] = bs[0] if bs else None
            return run.saw_tag(TAG_ATTACKING, a, b) and bool(bs), f'binding {found[name]}'
        return run.at(f'{name}: Attacking + a binding resolved', f)

    def distinct():
        vals = [found.get(n) for n, _, _ in cases]
        _main_runs[camera_yaw] = dict(found)
        others = {y: r for y, r in _main_runs.items() if y != camera_yaw}
        return (None not in vals and len(set(vals)) == len(vals) and all(r == found for r in others.values()),
                f'bindings {found}; other camera yaws {others}')

    steps, t = [(0.0, lambda: ('camera yaw', _set_yaw(client, camera_yaw)))], 0.5
    for name, tilt, crouching in cases:
        steps += [(t, lambda tilt=tilt, c=crouching, name=name: (name, setup(tilt, c))),
                  (t + 0.3, lambda: ('attack tap', combat_tap(client, attack))),
                  (t + 1.1, check(name, t + 0.3, t + 1.1)),
                  (t + 1.15, lambda tilt=tilt: teardown(tilt))]
        t = round(t + 1.2, 2)
    steps += [(t, run.at('four distinct bindings, same for every camera yaw', distinct)), (t + 0.1, run.finish())]
    run.run(steps)


def scenario_combat_context(client=1, wait_after_jump=0.3):
    """Same input (Attack, no stick) on the ground and in the air: the Grounded and the Airborne binding of Command.Attack.Neutral
    must be two different bindings, both activating Test.State.Attacking. The jump is real (GA_Jump); the scenario asserts that the
    pawn is not Walking when the airborne attack is sent, so a failed jump fails the scenario instead of testing the ground twice.
    Not covered: a command without an airborne binding / a command with a slot and no binding (no such content)."""
    _require_test_config(client)
    attack = button_mask('Attack')
    run = _Run('combat_context', client)
    got = {}

    def binding(key, a, b):
        def f():
            bs = run.new_bindings(a, b)
            got[key] = bs[0] if bs else None
            return run.saw_tag(TAG_ATTACKING, a, b) and bool(bs), f'binding {got[key]}'
        return f

    def airborne():
        mode = (run.latest('cli') or {}).get('mode')
        return mode not in (None, 'Walking'), f'mode {mode}'

    t_air = round(1.6 + wait_after_jump, 2)
    steps = [
        (0.3, lambda: ('ground attack', combat_tap(client, attack))),
        (1.3, run.at('grounded attack: Attacking + binding', binding('ground', 0.3, 1.3))),
        (1.6, lambda: ('jump', press(client, 'jump'))),
        (t_air, run.at('airborne at the attack tap', airborne)),
        (t_air, lambda: ('air attack', combat_tap(client, attack))),
        (t_air + 1.0, run.at('airborne attack: Attacking + binding', binding('air', t_air, t_air + 1.0))),
        (t_air + 1.0, run.at('grounded and airborne bindings differ', lambda: (None not in got.values() and len(set(got.values())) == 2, f'{got}'))),
        (t_air + 1.5, run.finish()),
    ]
    run.run(steps)


def scenario_combat_activation(client=1):
    """Every button of IC_GASNPP_Test that has a command in the standard slot set starts its test ability (activation from the
    InputConfig + SlotBindings): Attack -> Attacking, Special (held) -> Stance and gone after the release, Dash -> Dashing + cooldown +
    stamina, Buff -> Buffed. Dash and Buff have no slot in SS_GASNPP_Standard yet (testing.md): those two checks fail until one exists."""
    _require_test_config(client)
    attack, special, dash, buff = (button_mask(n) for n in ('Attack', 'Special', 'Dash', 'Buff'))
    run = _Run('combat_activation', client)

    def dash_ok():
        drop = run.stamina_drop(3.0, 4.0)
        return run.saw_tag(TAG_DASHING, 3.0, 4.0) and run.saw_cooldown('Dash', 3.0, 4.0) and drop > 1.0, f'drop {drop:.1f}'

    steps = [
        (0.3, lambda: ('attack', combat_tap(client, attack))),
        (1.2, run.at('Attack -> Attacking', lambda: (run.saw_tag(TAG_ATTACKING, 0.3, 1.2), ''))),
        (1.4, lambda: ('special held', combat_hold(client, special))),
        (2.0, run.at('Special held -> Stance', lambda: (run.held_tag(TAG_STANCE, 1.7, 2.0), ''))),
        (2.1, lambda: ('special released', combat_clear(client))),
        (2.8, run.at('Special released -> Stance gone', lambda: (not run.saw_tag(TAG_STANCE, 2.5, 2.8), ''))),
        (3.0, lambda: ('dash', combat_tap(client, dash))),
        (4.0, run.at('Dash -> Dashing + cooldown + stamina', dash_ok)),
        (4.2, lambda: ('buff', combat_tap(client, buff))),
        (5.2, run.at('Buff -> Buffed', lambda: (run.saw_tag(TAG_BUFFED, 4.2, 5.2), ''))),
        (5.4, run.finish()),
    ]
    run.run(steps)


# ------------------------------------------------------------------------------------------------ startup burst check
_startup = {'handle': None, 'results': [], 'status': 'idle'}


def abilities_startup(runs=3, run_seconds=14.0, burst_threshold=5, ready_timeout=60.0, settle_seconds=3.0):
    """Restarts PIE `runs` times and runs scenario_abilities as soon as every pawn has its input ready, to see whether a burst of
    Sync/Aux mismatch reconciles shows up at startup (26 in one burst on client 2 were seen once). The restart uses the current PIE
    settings and re-applies the last m.net() values. Non-blocking: poll abilities_startup_result(). Per run: mismatches before the
    scenario started (startup) and during it, unexplained ones, the largest per-second burst, topology problems and the CMP line.
    The scenario starts `settle_seconds` after every pawn has its input ready: the fork's startup burst (Offset 0, the late client's
    local Mover mode still None, "RollbackFrame N AHEAD of PendingFrame 0", so the rollback does nothing) falls before it and is only
    reported (`startup_*` keys, `startup_max_burst` is informational), not judged.
    Verdict (log): FAIL when a run has max_burst >= burst_threshold or unexplained mismatches in the scenario window, a failing CMP, or an error."""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not all(hasattr(les, n) for n in ('editor_request_begin_play', 'editor_request_end_play', 'is_in_play_in_editor')):
        raise RuntimeError('LevelEditorSubsystem has no begin/end play in this build: restart PIE by hand (ue_play) between runs')
    if _startup['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_startup['handle'])
    expected = len(worlds()) or 3
    st = {'run': 0, 'phase': 'end', 't': time.time(), 'ending': False, 'mark_start': None, 'mark_run': None, 'ready_at': None}
    _startup.update(results=[], status=f'run 1/{runs}')

    def finish(status):
        unreal.unregister_slate_post_tick_callback(_startup['handle'])
        _startup['handle'] = None
        _startup['status'] = status
        bad = status != 'done' or any(r.get('error') or r.get('max_burst', 0) >= burst_threshold or r.get('unexplained', 0)
                                      or not r.get('cmp', '').endswith(tuple(f'{n}/{n} OK' for n in range(1, 100)))
                                      for r in _startup['results'])
        (unreal.log_error if bad else unreal.log)(f"GASNPP abilities_startup {status}: {'FAIL' if bad else 'PASS'} " + json.dumps(_startup['results']))

    def ready():
        ws = worlds()
        if len(ws) < expected:
            return False
        for w in ws:
            # Only the pawn a world's local player controls ever has its input ready (the others are authority or simulated copies),
            # so every world must hold all the pawns and have exactly that one ready.
            c = comps(w)
            if len(c) < expected or not any(json.loads(x.get_state_snapshot()).get('inputReady') for x in c.values()):
                return False
        return True

    def tick(_dt):
        try:
            elapsed = time.time() - st['t']
            if st['phase'] == 'end':
                if les.is_in_play_in_editor():
                    if not st['ending']:
                        les.editor_request_end_play()
                        st['ending'] = True
                else:
                    st.update(mark_start=mark_log(), phase='wait_ready', t=time.time(), ending=False)
                    les.editor_request_begin_play()
            elif st['phase'] == 'wait_ready':
                if les.is_in_play_in_editor() and ready():
                    st.update(phase='settle', t=time.time())
                elif elapsed > ready_timeout:
                    _startup['results'].append({'run': st['run'] + 1, 'error': f'pawns not ready after {ready_timeout}s'})
                    finish('error')
            elif st['phase'] == 'settle':
                if elapsed >= settle_seconds:
                    if _cfg.get('net'):
                        net(*_cfg['net'])
                    st.update(mark_run=mark_log(), phase='run', t=time.time())
                    reset_all()
                    scenario_abilities()
            elif st['phase'] == 'run' and elapsed > run_seconds:
                whole, during = report_reconciles(st['mark_start']), report_reconciles(st['mark_run'])
                _, problems = check_topology()
                _startup['results'].append({
                    'run': st['run'] + 1, 'startup_mismatches': whole['mismatches'] - during['mismatches'],
                    'startup_unexplained': whole['unexplained'] - during['unexplained'], 'startup_max_burst': whole['max_burst'],
                    'run_mismatches': during['mismatches'], 'unexplained': during['unexplained'], 'max_burst': during['max_burst'],
                    'offset_changes': during['offset_changes'], 'topology': problems, 'cmp': report().splitlines()[-1]})
                st['run'] += 1
                if st['run'] >= runs:
                    finish('done')
                else:
                    _startup['status'] = f"run {st['run'] + 1}/{runs}"
                    st.update(phase='end', t=time.time())
        except Exception as e:
            _startup['results'].append({'run': st['run'] + 1, 'error': str(e)})
            finish('error')

    _startup['handle'] = unreal.register_slate_post_tick_callback(tick)


def abilities_startup_result():
    return _startup['status'], _startup['results']


# ------------------------------------------------------------------------------------------------ M3 spikes (grants, stats, triggers)
# Probes on UGSTN_GASNPPTestComponent (see GSTN_TestProbeAbilities.h). Run ONE scenario at a time and wait for its verdict
# (last_checks() / the log): they share the probe abilities and the editor timeline. Judge them at m.net(60, 15, 0): packet loss
# causes reconciles of its own (see "Known exception" above). Each scenario takes the world index of the pawn it drives
# (default 1); index 0 is the host's own pawn in listen mode (no reconcile there, the owner is the authority).
def _srv(key):
    """Test component of pawn `key` on world 0 (the host or the dedicated server): the one whose Server* functions run."""
    return comps(worlds()[0])[key]


def _view(world_index, key):
    """Snapshot (dict) of pawn `key` as world `world_index` sees it, or None."""
    c = comps(worlds()[world_index]).get(key)
    return json.loads(c.get_state_snapshot()) if c else None


def _probe(view, name):
    """Activation counters of probe `name` in a snapshot (zeros when it never ran there)."""
    zero = dict(attempts=0, attemptsRestoring=0, activations=0, activationsResim=0, activationsRestoring=0, lastFrame=-1, endFrame=-1)
    return {**zero, **(((view or {}).get('probe') or {}).get('triggers') or {}).get(name, {})}


def _others(client):
    """Client worlds that are neither the host/server nor the pawn's owner: they see it as a simulated proxy."""
    return [i for i in range(1, len(worlds())) if i != client]


def _set_stats_probe(keys, on):
    """Stats probe on/off on every copy of the pawns `keys`, in every world (set it before the attribute changes, not mid-flight)."""
    for w in worlds():
        for k in keys:
            c = comps(w).get(k)
            if c is not None:
                c.set_stats_probe_enabled(on)


def _mark(run, marks, name):
    """Timeline step: remembers the log position under `name` (None when the log is not readable)."""
    def step():
        try:
            marks[name] = mark_log()
        except Exception:
            marks[name] = None
        return f'mark {name}'
    return step


def _reconciles_since(marks, name):
    """report_reconciles() over [mark `name`, now], or None when the log was not readable."""
    return report_reconciles(marks[name]) if marks.get(name) is not None else None


def _reconcile_check(run, marks, name, label, lo, hi, field='mismatches'):
    """Timeline step: the reconciles logged since mark `name` are between lo and hi (`field` of report_reconciles)."""
    def fn():
        r = _reconciles_since(marks, name)
        if r is None:
            return True, 'log not readable: skipped'
        return lo <= r[field] <= hi, f"{r[field]} {field} (want {lo}..{hi}), offset changes {r['offset_changes']}, forced {r['forced']} {r['unexplained_lines']}"
    return run.at(label, fn)


def _first_time(run, side, pred, t0):
    """Time of the first sample of `side` at or after t0 that satisfies pred(sample), or None."""
    return next((s['t'] for s in run.samples if s['t'] >= t0 and s.get(side) and pred(s[side])), None)


def _latest(run, side):
    s = run.latest(side)
    return s if s is not None else {'probe': {}, 'attr': {}, 'tags': set(), 'vel': (0, 0, 0), 'loc': (0, 0, 0)}


def _log_text_since(mark):
    return _read_log_since(mark) if mark is not None else ''


def scenario_spike_grants(client=1, count=40, bound=2.0):
    """Spike 1 (L1-L3): a loadout-sized batch of ability specs granted on the server, outside the simulation, to a client's pawn.
    Steps: grant `count` distinct ability classes (30 predicted + 10 ServerOnly when count is 40) and an infinite effect in ONE
    game thread call; 5 s idle; revoke them; grant a different batch (the second half of the classes); revoke. Strict, per phase:
      - the server owns the batch at once; the owner finds it with FindAbilitySpecFromClass within `bound` seconds (the time is
        reported); the pawn as a simulated proxy (the other clients) finds NONE of it;
      - the grant (and each revoke / re-grant) costs 1..2 logged reconciles on the owner's connection (0 for the host's own pawn);
        5 idle seconds after it cost none;
      - the infinite effect's tag is on the server and the owner; no 'GiveAbility on simulated proxy' error mentions the batch.
    NOT measured here (the harness cannot read them): bytes per send and the one-frame cost of creating the instances; read the
    bytes with `stat net` in the owner's world (Out Bytes) before and after the grant, and the cost in Insights."""
    run = _Run('spike_grants', client, judge_reconciles=False)
    key, others, marks, st = run.key, _others(client), {}, {}
    want = (0, 0) if client == 0 else (1, 2)
    half = count // 2

    def owns(n):
        return lambda p: (p.get('probe') or {}).get('ownedBulkSpecs') == n

    def remote_none(label):
        def fn():
            seen = {i: (_view(i, key) or {}).get('probe', {}).get('ownedBulkSpecs') for i in others}
            return bool(seen) and all(v == 0 for v in seen.values()), f'owned specs per remote client world {seen}'
        return run.at(label, fn)

    def owner_gets(label, n, t_key):
        def fn():
            t = _first_time(run, 'cli', owns(n), st[t_key])
            return t is not None and t - st[t_key] <= bound, 'never' if t is None else f'{t - st[t_key]:.2f}s after the call (bound {bound}s)'
        return run.at(label, fn)

    def server_owns(label, n):
        return run.at(label, lambda: (owns(n)(_latest(run, 'srv')), f"server finds {_latest(run, 'srv')['probe'].get('ownedBulkSpecs')}"))

    def action(name, fn):
        def step():
            st['t_' + name] = run.now()
            st[name] = fn()
            return name, st[name]
        return step

    steps = [
        (0.0, _mark(run, marks, 'start')),
        (0.2, run.at('precondition: the snapshot carries the probe data and nothing is granted yet', lambda: (
            'ownedBulkSpecs' in _latest(run, 'srv')['probe'] and owns(0)(_latest(run, 'srv')) and owns(0)(_latest(run, 'cli')), f"{_latest(run, 'srv')['probe']}"))),
        (0.4, _mark(run, marks, 'grant')),
        (0.5, action('grant', lambda: (_srv(key).server_grant_bulk(0, count), _srv(key).server_set_bulk_effect(True)))),
        (3.5, run.at(f'grant: {count} specs granted and the effect applied', lambda: (st['grant'] == (count, True), f"{st['grant']}"))),
        (3.5, server_owns(f'grant: the server owns {count}', count)),
        (3.5, owner_gets(f'grant: the owner finds {count} within {bound}s', count, 't_grant')),
        (3.5, remote_none('grant: the other clients find none')),
        (3.5, run.at('grant: the infinite effect (Test.Probe.Bulk) is on the server and the owner', lambda: (
            'Test.Probe.Bulk' in _latest(run, 'srv')['tags'] and 'Test.Probe.Bulk' in _latest(run, 'cli')['tags'], f"srv {sorted(_latest(run, 'srv')['tags'])} cli {sorted(_latest(run, 'cli')['tags'])}"))),
        (3.5, _reconcile_check(run, marks, 'grant', 'grant: reconciles for the batch', *want)),
        (3.6, _mark(run, marks, 'idle')),
        (8.6, _reconcile_check(run, marks, 'idle', 'idle after the grant: no reconcile', 0, 0, 'unexplained')),
        (8.6, owner_gets('idle: the owner still finds the batch', count, 't_grant')),
        (8.7, _mark(run, marks, 'revoke')),
        (8.8, action('revoke', lambda: (_srv(key).server_revoke_bulk(), _srv(key).server_set_bulk_effect(False)))),
        (11.8, run.at(f'revoke: {count} specs cleared', lambda: (st['revoke'] == (count, True), f"{st['revoke']}"))),
        (11.8, server_owns('revoke: the server owns none', 0)),
        (11.8, owner_gets('revoke: the owner finds none within the bound', 0, 't_revoke')),
        (11.8, _reconcile_check(run, marks, 'revoke', 'revoke: reconciles for the removal', *want)),
        (11.9, _mark(run, marks, 'regrant')),
        (12.0, action('regrant', lambda: _srv(key).server_grant_bulk(count - half, half))),
        (15.0, run.at(f're-grant: {half} different specs granted', lambda: (st['regrant'] == half, f"{st['regrant']}"))),
        (15.0, server_owns(f're-grant: the server owns {half}', half)),
        (15.0, owner_gets(f're-grant: the owner finds {half} within {bound}s', half, 't_regrant')),
        (15.0, remote_none('re-grant: the other clients find none')),
        (15.0, _reconcile_check(run, marks, 'regrant', 're-grant: reconciles for the new batch', *want)),
        (15.1, run.at('no GiveAbility error on a simulated proxy for the batch', lambda: (
            not [l for l in _log_text_since(marks['start']).splitlines() if 'GiveAbility called on ability GSTN_GA_ProbeGrant' in l],
            'log scan of the whole scenario'))),
        (15.2, action('cleanup', lambda: _srv(key).server_revoke_bulk())),
        (18.0, owner_gets('cleanup: the owner finds none again', 0, 't_cleanup')),
        (18.1, run.finish()),
    ]
    run.note('bytes per send and the instance creation cost are not measured by this scenario (stat net / Insights)')
    run.run(steps)


def _horizontal_peak(run, side, t0, t1):
    return max((sum(x * x for x in s['vel'][:2]) ** 0.5 for s in run._win(side, t0, t1)), default=0.0)


def _apex(run, side, t_base, t0, t1):
    """Highest z in [t0, t1] minus the z of the last sample before t_base."""
    before = [s[side]['loc'][2] for s in run.samples if s['t'] < t_base and s.get(side)]
    zs = [s['loc'][2] for s in run._win(side, t0, t1)]
    return (max(zs) - before[-1]) if before and zs else 0.0


def scenario_spike_stats(client_a=1, client_b=2):
    """Spike 2 (L4-L5): Mover's MaxSpeed and gravity rewritten every tick from attributes, driven by infinite effects applied on
    the server from outside the simulation. Pawn A gets preset 1 (speed x2, gravity x0.5), pawn B preset 2 (speed x0.5,
    gravity x2), so the two pawns must keep different values at the same time (per-component settings). Forced reconciles across
    the change and across the jump apexes. Strict:
      - the attribute scales, MaxSpeed and gravity of both pawns are what the presets say, on the server AND the owning client;
      - the pawns really move at those speeds (peak horizontal speed) and jump to different heights (apex ratio);
      - after the change there is no reconcile that is not forced or an offset resync; at the end server and owner agree
        (position, mode, attributes, MaxSpeed, gravity).
    The settings are written by the test component from Mover's pre-simulation tick (not a Mover movement modifier): the write
    is a pure function of the restored attribute, which is what the spike needs to show. Queueing a persistent modifier from
    outside the simulation (L5) is NOT covered. If the speeds do not follow the scale, the walking mode may use MaxSpeedOverride."""
    key_a, key_b = key_of_player(client_a), key_of_player(client_b)
    run = _Run('spike_stats', client_a, extra=[('srvB', 0, key_b), ('cliB', client_b, key_b)], judge_reconciles=False)
    marks, st = {}, {}
    pawns = [('A', client_a, key_a, 'srv', 'cli', 1), ('B', client_b, key_b, 'srvB', 'cliB', 2)]

    def stat(side, field):
        return (_latest(run, side)['probe'].get('stats') or {}).get(field)

    def close(a, b, tol):
        return a is not None and b is not None and abs(a - b) <= tol

    def scales():
        out, ok = [], True
        for name, _, _, srv, cli, preset in pawns:
            sp, gr = (2.0, 0.5) if preset == 1 else (0.5, 2.0)
            for side in (srv, cli):
                a = _latest(run, side)['attr']
                good = close(a.get('ProbeSpeedScale'), sp, 0.01) and close(a.get('ProbeGravityScale'), gr, 0.01)
                ok &= good
                out.append(f"{name}/{side} speed {a.get('ProbeSpeedScale')} gravity {a.get('ProbeGravityScale')}")
        return ok, '; '.join(out)

    def written():
        ok, out = True, []
        base_a = stat('srv', 'baseMaxSpeed')
        for name, _, _, srv, cli, preset in pawns:
            sp = 2.0 if preset == 1 else 0.5
            good = bool(base_a) and close(stat(srv, 'maxSpeed'), base_a * sp, 0.5) and close(stat(cli, 'maxSpeed'), stat(srv, 'maxSpeed'), 0.5) \
                and close(stat(cli, 'gravityZ'), stat(srv, 'gravityZ'), 0.5)
            ok &= good
            out.append(f"{name} maxSpeed srv {stat(srv, 'maxSpeed')} cli {stat(cli, 'maxSpeed')} (base {stat(srv, 'baseMaxSpeed')}), gravityZ srv {stat(srv, 'gravityZ')} cli {stat(cli, 'gravityZ')}")
        ga, gb = stat('srv', 'gravityZ'), stat('srvB', 'gravityZ')
        ratio = gb / ga if ga and gb else None
        ratio_ok = ratio is not None and close(ratio, 4.0, 0.05)
        out.append(f'gravity B/A {ratio} (want 4)')
        return ok and ratio_ok, '; '.join(out)

    def speeds():
        va, vb = _horizontal_peak(run, 'srv', 3.4, 5.7), _horizontal_peak(run, 'srvB', 3.4, 5.7)
        ma, mb = stat('srv', 'maxSpeed') or 0.0, stat('srvB', 'maxSpeed') or 0.0
        good = vb > 1.0 and va > 2.5 * vb and 0.6 * ma <= va <= 1.05 * ma and 0.6 * mb <= vb <= 1.05 * mb
        return good, f'peak speed A {va:.0f} (max {ma:.0f}), B {vb:.0f} (max {mb:.0f})'

    def agree():
        out, ok = [], True
        for name, ci, key, _, _, _ in pawns:
            sv, ov = _view(0, key), _view(ci, key)
            if sv is None or ov is None:
                ok = False
                out.append(f'{name}: missing view')
                continue
            d = _diff(sv, ov)
            same = close((sv['probe']['stats'] or {}).get('maxSpeed'), (ov['probe']['stats'] or {}).get('maxSpeed'), 0.01) and \
                close((sv['probe']['stats'] or {}).get('gravityZ'), (ov['probe']['stats'] or {}).get('gravityZ'), 0.01)
            ok &= not d and same
            out.append(f"{name}: diff {d or 'none'}, MaxSpeed/gravity {'same' if same else 'DIFFERENT'}")
        return ok, '; '.join(out)

    def force(index, extra):
        return lambda: (f'force reconcile on client {index}', force_reconcile(index, extra) if index != 0 else 'host: nothing to reconcile')

    steps = [
        (0.0, lambda: ('stats probe on', _set_stats_probe([key_a, key_b], True))),
        (0.3, run.at('precondition: both pawns start with scale 1 and no preset', lambda: (
            all(close(_latest(run, s)['attr'].get('ProbeSpeedScale'), 1.0, 0.01) for s in ('srv', 'srvB', 'cli', 'cliB')), f"{[_latest(run, s)['attr'] for s in ('srv', 'srvB')]}"))),
        (0.5, _mark(run, marks, 'change')),
        (0.6, lambda: ('presets', (_srv(key_a).server_set_stats_preset(1), _srv(key_b).server_set_stats_preset(2)))),
        (0.8, force(client_a, 20)),
        (1.4, force(client_b, 20)),
        (3.0, run.at('the scales are what the presets say, on the server and the owners', scales)),
        (3.0, run.at('MaxSpeed and gravity were written from the scales, owner = server', written)),
        (3.0, _mark(run, marks, 'steady')),
        (3.2, lambda: ('walk', (hold(client_a, 'move', (0, 1, 0)), hold(client_b, 'move', (0, 1, 0))))),
        (5.7, run.at('the pawns move at their own speeds', speeds)),
        (5.8, lambda: ('stop', (release(client_a, 'move'), release(client_b, 'move')))),
        (6.5, lambda: ('jump', (press(client_a, 'jump'), press(client_b, 'jump')))),
        (6.75, force(client_b, 10)),
        (7.5, force(client_a, 10)),
        (10.5, run.at('the pawns jump to different heights', lambda: (lambda a, b: (b > 5.0 and a > 1.8 * b, f'apex A {a:.0f} cm, B {b:.0f} cm (want A > 1.8 B)'))(
            _apex(run, 'srv', 6.5, 6.5, 10.0), _apex(run, 'srvB', 6.5, 6.5, 10.0)))),
        (10.6, run.at('server and owner agree at the end', agree)),
        (10.6, _reconcile_check(run, marks, 'steady', 'no reconcile after the change besides the forced ones and offset resyncs', 0, 0, 'unexplained')),
        (10.7, lambda: ('cleanup', (_srv(key_a).server_set_stats_preset(0), _srv(key_b).server_set_stats_preset(0)))),
        (12.2, lambda: ('stats probe off', _set_stats_probe([key_a, key_b], False))),
        (12.8, run.finish()),
    ]
    run.note('pawn B runs on the same Mover settings class as A but on its own component instance; L5 (modifier queued from outside) is not covered')
    run.run(steps)


def scenario_spike_triggers(client=1, fork_patched=True, resim_frames=15):
    """Spike 3 (L6-L8): triggered abilities. Needs the fork patch in UNpAbilitySystemComponent::MonitoredTagChanged (PATCHES.md, 1.
    OwnedTag triggers); fork_patched=False turns the 'nothing happens while a frame is restored' checks into notes, to see what
    the unpatched fork does. Presses are IA_GASNPP_Buff releases (the raiser abilities activate on Completed; the test buff, on
    Started, fires too). Phases, each with its own counters (reset between phases) and ProbeEventCount / ProbeTagCount (instant
    effects applied by the triggered abilities, so the synced state shows a double activation):
      E: GameplayEvent trigger, raiser granted AFTER the target (order A). One press, then one press with a forced reconcile
         across the event frame. Server: 1 activation per press. Owner: 1 forward activation per press, >= 1 re-fire in the forced
         resim, none while restoring. Other clients: no activation (a simulated proxy does not run abilities).
      E swap: the same with the raiser granted BEFORE the target (order B). The activation frame is the same; the ability lives
         one frame less when it is ticked in its activation frame. Server and owner must agree on both, whatever the order.
      T: OwnedTagAdded trigger. Press (raiser applies the 1 s tag effect), press with a forced reconcile across the tag
         transition: same expectations, and no CanActivate attempt while restoring.
      T server-only: the tag effect applied by the server outside the simulation (the owner never predicts it): the owner
         reconciles and RestoreTags raises the tag 0 -> 1 INSIDE the restore: no forward activation on the owner, counters equal.
    End state of each phase, on the server, the owner and the other clients: same counter attribute, same Test.* tags, same
    active probe abilities (none). Listen: run it with client=1 and with client=0 (the host's own pawn)."""
    run = _Run('spike_triggers', client, judge_reconciles=False)
    key, others, marks, st = run.key, _others(client), {}, {}
    owner_is_server = client == 0

    def reset_counters():
        for w in worlds():
            c = comps(w).get(key)
            if c is not None:
                c.reset_probe_counters()
        return 'counters reset'

    def baseline(attr):
        def step():
            st['base_' + attr] = (_view(0, key)['attributes'] or {}).get(attr, 0.0)
            return attr, st['base_' + attr]
        return step

    def regrant(order):
        def step():
            _srv(key).server_revoke_probes()
            st['granted_' + '+'.join(order)] = [_srv(key).server_grant_probe(n) for n in order]
            return order, st['granted_' + '+'.join(order)]
        return step

    def specs_equal(label):
        def fn():
            s, o = (_view(0, key) or {}).get('probe', {}).get('specs'), (_view(client, key) or {}).get('probe', {}).get('specs')
            return s is not None and s == o, f'activatable specs: server {s}, owner {o}'
        return run.at(label, fn)

    def verify(label, probe, attr, presses, forced, server_forward=None, owner_forward=None, owner_restore_legit=False):
        """Counters of `probe` and the end state, after a phase. forced: forced reconciles across the activation in the phase.
        server_forward / owner_forward: expected forward activations (default `presses`)."""
        sf = presses if server_forward is None else server_forward
        of = presses if owner_forward is None else owner_forward

        def fn():
            views = {i: _view(i, key) for i in range(len(worlds()))}
            if any(v is None for v in views.values()):
                return False, f'missing views {[i for i, v in views.items() if v is None]}'
            bad = []
            for i, v in views.items():
                got = v['attributes'].get(attr)
                if got is None or abs(got - (st['base_' + attr] + presses)) > 0.01:
                    bad.append(f"w{i} {attr}={got}, want {st['base_' + attr] + presses}")
            s = _probe(views[0], probe)
            if s['activations'] != sf or s['activationsResim'] or s['activationsRestoring'] or s['attemptsRestoring']:
                bad.append(f'server counters {s}, want {sf} forward and nothing re-simulated or restored')
            if not owner_is_server:
                o = _probe(views[client], probe)
                if o['activations'] != of:
                    bad.append(f"owner forward activations {o['activations']}, want {of}")
                if forced and not 1 <= o['activationsResim'] <= 2 * forced:
                    bad.append(f"owner re-fired {o['activationsResim']} times in {forced} forced resim(s), want 1..{2 * forced}")
                if not forced and o['activationsResim']:
                    bad.append(f"owner re-fired {o['activationsResim']} times without a forced resim")
                restoring = []
                if o['attemptsRestoring']:
                    restoring.append(f"{o['attemptsRestoring']} CanActivate call(s) while restoring")
                if o['activationsRestoring'] and not owner_restore_legit:
                    restoring.append(f"{o['activationsRestoring']} activation(s) while restoring")
                if restoring and fork_patched:
                    bad.append('owner: ' + ' and '.join(restoring))
                elif restoring:
                    run.note(f'{label}: unpatched fork, owner made ' + ' and '.join(restoring))
            for i in others:
                p = _probe(views[i], probe)
                if p['activations'] or p['activationsResim']:
                    bad.append(f'w{i} (simulated proxy) activated {p}')
            # End state: the same on every machine
            names = lambda v: [n for n in (v['probe']['activeAbilities']) if n.startswith('GSTN_GA_Probe')]
            tags = lambda v: sorted(t for t in v['tags'] if t.startswith('Test.Probe'))
            ref = views[0]
            for i, v in views.items():
                if names(v) != names(ref) or tags(v) != tags(ref):
                    bad.append(f'w{i} active {names(v)} tags {tags(v)} differ from the server {names(ref)} {tags(ref)}')
            return not bad, '; '.join(bad) or f"{probe} {s}" + ('' if owner_is_server else f" owner {_probe(views[client], probe)}")
        return run.at(label, fn)

    def saw_active(label, name):
        def fn():
            srv = _first_time(run, 'srv', lambda p: name in (p['probe'].get('activeAbilities') or []), 0.0) is not None
            cli = owner_is_server or _first_time(run, 'cli', lambda p: name in (p['probe'].get('activeAbilities') or []), 0.0) is not None
            return srv and cli, f'{name} was active on the server: {srv}, on the owner: {cli}'
        return run.at(label, fn)

    def timing(label, order):
        """Activation frame and lifetime of the target relative to the raiser, server = owner."""
        def fn():
            out, ok = {}, True
            for who, i in (('server', 0), ('owner', client)):
                v = _view(i, key)
                t, r = _probe(v, 'EventTarget'), _probe(v, 'EventRaiser')
                out[who] = (t['lastFrame'] - r['lastFrame'], t['endFrame'] - t['lastFrame'])
            ok = out['server'] == out['owner'] and out['server'][1] > 0
            st['timing_' + order] = out['server']
            return ok, f"order {order}: (target activation - raiser activation, target lifetime) in frames {out}"
        return run.at(label, fn)

    steps = [
        (0.0, _mark(run, marks, 'start')),
        (0.1, regrant(('EventTarget', 'EventRaiser'))),
        (0.2, run.at('E: probe abilities granted', lambda: (all(st['granted_EventTarget+EventRaiser']), f"{st['granted_EventTarget+EventRaiser']}"))),
        (2.7, specs_equal('E: the owner has the granted specs') if not owner_is_server else (lambda: 'host owns them')),
        (2.8, lambda: reset_counters()),
        (2.8, baseline('ProbeEventCount')),
        (3.0, lambda: ('E press 1', press(client, 'buff'))),
        (4.5, saw_active('E: the target was active on the server and on the owner', 'GSTN_GA_ProbeEventTarget')),
        (4.6, verify('E1: one press, one activation, everywhere', 'EventTarget', 'ProbeEventCount', 1, 0)),
        (5.5, lambda: ('E press 2', press(client, 'buff'))),
        (5.6, lambda: ('force reconcile across the event frame', force_reconcile(client, resim_frames) if client != 0 else 'host: nothing to reconcile')),
        (7.2, verify('E2: second press with a forced resim over the event frame', 'EventTarget', 'ProbeEventCount', 2, 0 if client == 0 else 1)),
        (7.2, timing('E: tick order A (target before raiser in the spec array)', 'A')),
        (7.4, regrant(('EventRaiser', 'EventTarget'))),
        (7.5, run.at('E swap: probe abilities granted in the other order', lambda: (all(st['granted_EventRaiser+EventTarget']), f"{st['granted_EventRaiser+EventTarget']}"))),
        (10.0, specs_equal('E swap: the owner has the granted specs') if not owner_is_server else (lambda: 'host owns them')),
        (10.1, lambda: reset_counters()),
        (10.1, baseline('ProbeEventCount')),
        (10.3, lambda: ('E swap press', press(client, 'buff'))),
        (10.4, lambda: ('force reconcile across the event frame', force_reconcile(client, resim_frames) if client != 0 else 'host: nothing to reconcile')),
        (12.0, verify('E3: raiser first in the spec array, forced resim over the event frame', 'EventTarget', 'ProbeEventCount', 1, 0 if client == 0 else 1)),
        (12.0, timing('E: tick order B (raiser before target in the spec array)', 'B')),
        (12.1, run.at('E: the two orders were measured (informational: same frame or the next one)', lambda: (
            'timing_A' in st and 'timing_B' in st, f"A {st.get('timing_A')} B {st.get('timing_B')} (lifetime shorter by one frame in B = the target was ticked in its activation frame)"))),
        (12.3, regrant(('TagTarget', 'TagRaiser'))),
        (12.4, run.at('T: probe abilities granted', lambda: (all(st['granted_TagTarget+TagRaiser']), f"{st['granted_TagTarget+TagRaiser']}"))),
        (14.9, specs_equal('T: the owner has the granted specs') if not owner_is_server else (lambda: 'host owns them')),
        (15.0, lambda: reset_counters()),
        (15.0, baseline('ProbeTagCount')),
        (15.2, lambda: ('T press 1', press(client, 'buff'))),
        (17.0, saw_active('T: the target was active on the server and on the owner', 'GSTN_GA_ProbeTagTarget')),
        (17.0, verify('T1: one press, one tag activation, everywhere', 'TagTarget', 'ProbeTagCount', 1, 0)),
        (17.5, lambda: ('T press 2', press(client, 'buff'))),
        (17.6, lambda: ('force reconcile across the tag transition', force_reconcile(client, resim_frames) if client != 0 else 'host: nothing to reconcile')),
        (20.0, verify('T2: second press with a forced resim over the tag transition', 'TagTarget', 'ProbeTagCount', 2, 0 if client == 0 else 1)),
        (20.1, _mark(run, marks, 'server_only')),
        (20.1, lambda: reset_counters()),
        (20.1, baseline('ProbeTagCount')),
        (20.2, lambda: ('T server-only tag effect', server_apply(key, 'GSTN_GE_ProbeTagGrant'))),
        (23.0, verify('T3: tag effect applied by the server only: no forward activation on the owner, counters equal', 'TagTarget', 'ProbeTagCount', 1, 0,
                      server_forward=1, owner_forward=0, owner_restore_legit=True)),
        (23.0, _reconcile_check(run, marks, 'server_only', 'T3: the owner reconciled, the tag came in through a restore', 0 if client == 0 else 1, 99)),
        (23.1, lambda: ('cleanup', _srv(key).server_revoke_probes())),
        (24.5, run.finish()),
    ]
    run.note('forced reconciles use np.ForceReconcile (one global cvar): the re-fire bound is 1..2 per forced reconcile')
    run.run(steps)
