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


_pawn_override = {'prev': None}


def _find_blueprint(name):
    """Soft path ('/Game/.../Name.Name') of the Blueprint asset called `name`, searched through the asset registry; None when there is none."""
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game', recursive=True):
        if str(a.asset_name) == name and 'Blueprint' in str(a.asset_class_path.asset_name):
            return str(a.package_name) + '.' + name
    return None


def set_pawn_override(pawn=None):
    """Swaps the default pawn class of GM_GASNPPTest in memory (never saved) for the Blueprint called `pawn` (e.g. 'SandboxCharacter_Mover'),
    resolved by soft path: no Blueprint is edited. None restores the class that was replaced. The scenarios that sample the test component
    (_Run) need the harness pawn: use this for log based scenarios such as slide_jump(bare=True)."""
    gm_class = unreal.load_asset('/Game/GASNPPTest/GM_GASNPPTest').generated_class()
    cdo = unreal.get_default_object(gm_class)
    if pawn is None:
        if _pawn_override['prev'] is not None:
            cdo.set_editor_property('default_pawn_class', _pawn_override['prev'])
            _pawn_override['prev'] = None
        return 'pawn restored'
    path = _find_blueprint(pawn)
    if path is None:
        raise RuntimeError(f'no Blueprint called {pawn} under /Game')
    if _pawn_override['prev'] is None:
        _pawn_override['prev'] = cdo.get_editor_property('default_pawn_class')
    cdo.set_editor_property('default_pawn_class', unreal.load_object(None, path + '_C'))
    return f'pawn class {path}_C'


def setup_editor(enable=True, mode='listen', clients=2, pawn=None):
    """Temporary editor-only setup (never saved): test GameMode override on the open level, PIE settings for `mode` ('listen' =
    primary, host + `clients` clients; 'dedicated' = secondary check), no background CPU throttling. enable=False restores all."""
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    gm = '/Game/GASNPPTest/GM_GASNPPTest' if enable else '/Game/Blueprints/GM_Sandbox'
    w.get_world_settings().set_editor_property('default_game_mode', unreal.load_asset(gm).generated_class())
    perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
    perf.set_editor_property('bThrottleCPUWhenNotForeground', not enable)
    set_pawn_override(pawn if enable else None)
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
    Expected final: Stamina 50, no tags (Health was 100 in the C19 run; this text used to say 80), authority and owner equal."""
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
# Mover mode the wall scenarios expect on the harness wall. None (the default) reads it from the pawn: the mode its Falling wall entry
# transition grabs on an unmarked wall (wall_mode()), so the scenarios follow Tools/GAS/setup_wall_slide_modes.py --use v2|legacy|v2climb
# and whatever wall_ab / surface_scenarios point the transitions at in memory. Set a name ('WallSliding' = legacy slide, 'WallSlidingV2' =
# slide v2) to force it. A hard-coded 'WallSliding' made every wall scenario report "never on the wall" once SandboxCharacter_Mover's
# Falling transition became the surface-driven one (it grabs WallSlidingV2).
WALL_MODE = None


def wall_mode(world_index=0):
    """The Mover mode the wall scenarios wait for: WALL_MODE when set, else the target of the first pawn's Falling wall entry transition
    (a surface-driven transition's unknown_surface_mode_name, a plain one's target_mode_name), read from the live pawn in `world_index`.
    Falls back to 'WallSliding' when PIE has no pawn with such a transition."""
    if WALL_MODE:
        return WALL_MODE
    surface_classes = tuple(getattr(unreal, n) for n in ('GSTN_WallSurfaceModeTransition', 'GSTN_AbilityWallSurfaceModeTransition') if hasattr(unreal, n))
    ws = worlds()
    for c in (comps(ws[world_index]).values() if len(ws) > world_index else ()):
        mover = c.get_owner().get_component_by_class(unreal.MoverComponent) if c else None
        falling = mover.get_editor_property('movement_modes').get('Falling') if mover else None
        for tr in (falling.get_editor_property('transitions') if falling else ()):
            if surface_classes and isinstance(tr, surface_classes):
                return str(tr.get_editor_property('surface_modes').get_editor_property('unknown_surface_mode_name'))
            if isinstance(tr, unreal.GSTN_WallEntryTransition):
                return str(tr.get_editor_property('target_mode_name'))
    return 'WallSliding'


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
    wm = wall_mode()
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
                if mode == wm:
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


def force_reconcile_on_mode(client=1, mode=None, extra_frames=15):
    """Forces a reconcile on the client while its pawn is in `mode` (e.g. on the wall, with GA_WallSlide active): the
    re-simulation must activate the ability again on the same frame. Read _force_on_mode['log'] after.

    np.ForceReconcile is one global cvar: under "Run Under One Process" the first client world that runs its rollback
    service takes it, which is not always `client`. It is set again every frame until `client`'s rollback counter goes
    up, so the other client may get an extra reconcile too."""
    if client == 0:
        raise RuntimeError('world 0 is the host/server: it never reconciles, pick a client (index >= 1)')
    mode = mode or wall_mode()
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


def report_reconciles(since=0, ignore_own_changes=True):
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

    def _own_change(message):
        recv = re.search(r'Recv Frame: (\d+)', message)
        return bool(ignore_own_changes and recv and any(f <= int(recv.group(1)) <= f + OWN_CHANGE_FRAMES for f in _server_changes))

    unexplained = [x for x in mismatches if not _on_offset_change(*x) and not _own_change(x[1])]
    by_model = {}
    for x, model in zip(mismatches, models):
        if not _on_offset_change(*x) and not _own_change(x[1]):
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

    def __init__(self, name, client, extra=None, judge_reconciles=True, sample=True):
        """extra: more sampled copies as [(label, world_index, pawn_key)]; their samples are read like 'srv' / 'cli' (spike_stats uses
        it for the second pawn). judge_reconciles=False: the spike scenarios create reconciles on purpose (a grant, an unpredicted
        effect) and judge them per window themselves, so the whole-run 'no unexplained reconciles' check is left out."""
        self.name, self.client, self.sample = name, client, sample
        self.key = key_of_player(client) if sample else None
        self.extra, self.judge_reconciles = list(extra or []), judge_reconciles
        self.results, self.samples, self.sim, self.handle = [], [], [], None
        self.rb0 = _total_rollbacks()
        self.log_mark = None
        try:
            self.log_mark = mark_log()
        except Exception as e:
            self.note(f'log not readable ({e}): reconcile check skipped')
        self.t0 = time.time()
        _last_run['obj'] = self    # for post-mortems of a failed check (samples)
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def now(self):
        return time.time() - self.t0

    def _tick(self, _dt):
        if not self.sample:
            return
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
                           'probe': v.get('probe') or {}, 'jump': (v.get('combat') or {}).get('jump') or {},
                           'intl': bool((v.get('combat') or {}).get('inTimeline', False))}
            self._add_sim_samples(ws, s)
            self.samples.append(s)
            self.sim.sort(key=lambda x: x['t'])
        except Exception:
            pass

    def _add_sim_samples(self, ws, editor_sample):
        """Adds one entry to self.sim per simulation tick the pawn ran since the last editor tick (UGSTN_GASNPPTestComponent.DrainSimHistory),
        so heights, speeds and tag spans are judged on the simulation tick and not on how many sim steps one editor frame held. An entry
        holds only what the simulation owns (loc, vel, mode, tags) and its sim time 'ms' (exact: use differences of it for durations); 't'
        is this tick's time minus the sim time still to come (the newest sim tick is 'now'), good to ~one editor tick. Resimulated ticks
        are in it too. Editor-tick fields (stamina, cooldowns, combat) stay in self.samples."""
        for side, wi, key in (('srv', 0, self.key), ('cli', self.client, self.key), *self.extra):
            c, base = comps(ws[wi]).get(key), editor_sample.get(side)
            if c is None or base is None:
                continue
            ticks = json.loads(c.drain_sim_history())
            if not ticks:
                continue
            newest = ticks[-1]['t']
            for e in ticks:
                self.sim.append({'t': editor_sample['t'] - (newest - e['t']) / 1000.0, side: {'ms': e['t'], 'tags': set(e['tags']), 'loc': tuple(e['loc']),
                                                                                              'vel': tuple(e['vel']), 'mode': e['mode']}})

    def run(self, steps):
        """Restarts the clock and runs the timeline; the last step must be finish()."""
        self.t0 = time.time()
        self.samples.clear()
        self.sim.clear()
        run_timeline(steps)

    # -- queries over the samples
    def _win(self, side, t0, t1, sim=False):
        """Samples of `side` in [t0, t1]: the editor ticks, or with sim=True the simulation ticks (loc / vel / mode / tags / ms only)."""
        return [s[side] for s in (self.sim if sim else self.samples) if t0 <= s['t'] <= t1 and s.get(side)]

    def saw_tag(self, tag, t0, t1, side='srv'):
        return any(tag in s['tags'] for s in self._win(side, t0, t1, sim=True))

    def held_tag(self, tag, t0, t1, side='srv'):
        w = self._win(side, t0, t1, sim=True)
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


_FORCES_RECONCILES = ('combat_spike', 'combat_lock', 'spike_stats', 'spike_triggers', 'hitstop_montage')


def last_checks():
    """(verdict, [(ok, label, detail)]) of the last strict scenario (ok None = a note, not a check)."""
    return _last_run['verdict'], _last_run['results']


def spread_in(run, side, t0, t1):
    """Largest distance between two simulation-tick positions of `side` in [t0, t1] (cm); 1e9 without samples."""
    pts = [x['loc'] for x in run._win(side, t0, t1, sim=True)]
    return max((sum((a - b) ** 2 for a, b in zip(p, q)) ** 0.5 for p in pts for q in pts), default=1e9)


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
        hs_frames = int(_combat_comp(client).get_editor_property('hitstop_test_frames'))
    except Exception:
        hs, hs_frames = -1, 8
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
        # The window is real (H7): the pawn is held still by the Mover freeze, the same on both machines, and moves again afterwards
        window = hs_frames / 60.0
        steps += [(2.3, run.at('hitstop: the pawn stood still in the window (owner)', lambda: (spread_in(run, 'cli', 1.6 + 0.05, 1.6 + window - 0.02) < 2.0, f"spread {spread_in(run, 'cli', 1.6 + 0.05, 1.6 + window - 0.02):.1f} cm")))]
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
    ts = [s[side]['ms'] / 1000.0 for s in run.sim if t0 <= s['t'] <= t1 and s.get(side) and tag in s[side]['tags']]
    return (min(ts), max(ts)) if ts else None


def scenario_timeline_cancel(client=1, attack_button='Attack', stance_button='Special'):
    """C10 timeline abilities (combat-m3 7.2, 8.3) on UGSTN_GA_TestAttack, a UGSTN_GA_BaseCombat of 20 frames (0.32 s), startup override
    window of 8 frames (0.13 s), cancel window [8, 14) into Command.Special, free from frame 14, and the stance (Special, a higher tier).
    Three cases 1.5 s apart, judged on the server's copy and the predicting client's by how long Attacking lasts and in which order the
    tags come (the natural length of the attack is 0.32 s, sampled every ~40 ms):
      0.5s  startup override: attack tap, the stance held 0.08 s later (frame ~5, inside the window) -> Stance comes and Attacking ends
            well before its natural end (the replaced timeline is cancelled)
      2.0s  cancel window: attack tap, the stance held 0.17 s later (frame ~10, in [8, 14)) -> Stance comes and Attacking ends before 0.28 s
      3.5s  not cancellable: attack tap, a second attack tap 0.05 s later (same tier, before the first actionable frame) -> Attacking lasts
            its natural 0.28 to 0.38 s (checked from 0.25: a sample is quantised by the editor tick) (a restart would make it 0.42 s or more)
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
            dur = att[1] - att[0] + 0.02      # one sim tick of resolution
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
        (4.6, run.at('no cancel: server', case('', 3.4, 4.6, 'srv', 0.25, 0.38, False))),
        (4.6, run.at('no cancel: client', case('', 3.4, 4.6, 'cli', 0.25, 0.38, False))),
        (4.7, run.finish()),
    ]
    run.run(steps)


TAG_TRIGGERED, TAG_AWAITING = 'Test.State.Triggered', 'Combat.Ability.AwaitingAdoption'


def scenario_triggered_timeline(client=1, resim_frames=15):
    """C11 adoption (combat-m3 7.4) on UGSTN_GA_TestTriggeredTimeline, a 12 frame timeline ability (0.2 s) that no command starts: the tag
    raiser probe (a press of the buff button) applies the effect that grants Test.Probe.TagTrigger, the owned-tag trigger activates the
    ability, it waits with Combat.Ability.AwaitingAdoption and the combat tick adopts it one frame later. Two presses, 2 s apart, the second
    with a forced reconcile across the activation and adoption frames. Judged on the server's copy and the owner's:
      - Test.State.Triggered (owned while the adopted timeline runs) shows on both and lasts about the 0.2 s of the timeline
      - Combat.Ability.AwaitingAdoption never stays: gone on every machine at the end of each press
      - the authority published exactly one TimelineStarted(Adopted) per press (clients publish nothing), none from a command
      - nothing of the ability is left active anywhere
    Also: no unexplained reconciles."""
    # The cleanup revokes the probe abilities on the server only, which makes the owner reconcile on purpose: judge the window before it
    run = _Run('triggered_timeline', client, judge_reconciles=False)
    key = run.key
    st, marks = {}, {}

    def no_unexplained():
        r = report_reconciles(marks['start']) if marks.get('start') is not None else None
        if r is None:
            return False, 'the log was not readable'
        return r['unexplained_ours'] == 0, f"{r['unexplained_ours']} unexplained {r['unexplained_by_model']} {r['unexplained_lines']}, forced={r['forced']}"

    def regrant():
        _srv(key).server_revoke_probes()
        st['granted'] = [_srv(key).server_grant_probe(n) for n in ('TimelineTarget', 'TagRaiser')]
        return st['granted']

    def specs_equal():
        s, o = (_view(0, key) or {}).get('probe', {}).get('specs'), (_view(client, key) or {}).get('probe', {}).get('specs')
        return s is not None and s == o, f'activatable specs: server {s}, owner {o}'

    def started(n_adopted):
        def f():
            counts = (((_view(0, key) or {}).get('combat') or {}).get('timelineStarted')) or {}
            return counts.get('adopted', 0) == n_adopted and not counts.get('command', 0) and not counts.get('forced', 0), f'TimelineStarted published by the authority: {counts}'
        return f

    def span(t0, t1):
        def f():
            out, ok = [], True
            for side in ('srv', 'cli'):
                s = _tag_span(run, side, TAG_TRIGGERED, t0, t1)
                if s is None:
                    return False, f'{side}: {TAG_TRIGGERED} never seen'
                dur = s[1] - s[0] + 0.04
                ok = ok and 0.1 <= dur <= 0.45
                out.append(f'{side} {dur:.2f}s')
            return ok, ', '.join(out) + ' (want 0.1..0.45, the timeline is 0.2 s)'
        return f

    def clean():
        bad = []
        for i in range(len(worlds())):
            v = _view(i, key) or {}
            if TAG_AWAITING in v.get('tags', []):
                bad.append(f'w{i} still has {TAG_AWAITING}')
            if TAG_TRIGGERED in v.get('tags', []):
                bad.append(f'w{i} still has {TAG_TRIGGERED}')
            if 'GSTN_GA_TestTriggeredTimeline' in ((v.get('probe') or {}).get('activeAbilities') or []):
                bad.append(f'w{i} still has the ability active')
        return not bad, '; '.join(bad) or 'no waiting tag, no timeline tag, no active ability on any machine'

    steps = [
        (0.6, _mark(run, marks, 'start')),   # after the grant: the server-only revoke and grant at 0.1 s is a deliberate one-off ASC difference
        (0.1, lambda: ('grant', regrant())),
        (0.2, run.at('probes granted', lambda: (all(st['granted']), f"{st['granted']}"))),
        (2.5, run.at('the owner has the granted specs', specs_equal)),
        (2.6, lambda: ('reset counters', [c.reset_probe_counters() for w in worlds() for c in [comps(w).get(key)] if c is not None] and 'reset')),
        (3.0, lambda: ('press 1', press(client, 'buff'))),
        (4.4, run.at('press 1: the adopted timeline ran on the server and the owner', span(3.0, 4.4))),
        (4.4, run.at('press 1: exactly one TimelineStarted(Adopted)', started(1))),
        (4.4, run.at('press 1: nothing left over', clean)),
        (5.5, lambda: ('press 2', press(client, 'buff'))),
        (5.6, lambda: ('force reconcile across the adoption', force_reconcile(client, resim_frames) if client != 0 else 'host: nothing to reconcile')),
        (7.0, run.at('press 2: the adopted timeline ran on the server and the owner', span(5.5, 7.0))),
        (7.0, run.at('press 2: exactly one more TimelineStarted(Adopted), none re-published by the resimulation', started(2))),
        (7.0, run.at('press 2: nothing left over', clean)),
        (7.05, run.at('no unexplained reconciles (before the cleanup)', no_unexplained)),
        (7.1, lambda: ('cleanup', _srv(key).server_revoke_probes())),
        (7.8, run.finish()),
    ]
    run.run(steps)


def scenario_weapon_switch(client=1, resim_frames=15):
    """C12 weapon switch (combat-m3 7.7, 10.2): UGSTN_GA_TestWeaponSwitch (a 12 frame timeline with a weapon switch track at frame 3,
    adopted like the triggered timeline: a press of the buff button) swaps the stats effect primary <-> support while the real
    FGSTN_StatsMovementModifier follows the WalkSpeed stat. The primary effect is +300 (x1.5 -> run speed 600), the support -300 (x0.5 -> 200).
    Two presses; the first with a forced reconcile across the swap frame, the second too. Judged on the server and the owner:
      - the weapon index in the combat state and the WalkSpeed stat are those of the weapon, on both, after each press
      - the authority published exactly one WeaponSwitched per press (a re-simulation publishes none)
      - the peak walking speed is the primary's before the first press, the support's after it
      - no unexplained reconcile of ours (ability system / combat) in the window"""
    run = _Run('weapon_switch', client, judge_reconciles=False)
    key = run.key
    st, marks = {}, {}

    def stat(side, field):
        return (_latest(run, side)['probe'].get('stats') or {}).get(field)

    def grant():
        _srv(key).server_revoke_probes()
        st['granted'] = [_srv(key).server_grant_probe(n) for n in ('SwitchTarget', 'TagRaiser')]
        st['equipped'] = _srv(key).server_equip_primary_weapon(True)
        return st['granted'] + [st['equipped']]

    def peak(t0, t1, want):
        def f():
            v = _horizontal_peak(run, 'srv', t0, t1)
            return 0.85 * want <= v <= 1.05 * want, f'peak speed {v:.0f} (want about {want})'
        return f

    def weapon(index, walk_speed, run_speed):
        def f():
            out, ok = [], True
            for i, side in ((0, 'srv'), (client, 'cli')):
                v = _view(i, key) or {}
                w = ((v.get('combat') or {}).get('activeWeapon'))
                ws, rs, has = stat(side, 'walkSpeedStat'), stat(side, 'gaitRunSpeed'), stat(side, 'hasCombatSet')
                good = has and w == index and ws is not None and abs(ws - walk_speed) < 0.5 and rs is not None and abs(rs - run_speed) < 1.0
                ok &= bool(good)
                out.append(f'{side}: weapon {w}, WalkSpeed {ws}, run speed {rs}')
            return ok, '; '.join(out) + f' (want weapon {index}, WalkSpeed {walk_speed}, run {run_speed})'
        return f

    def switched(n):
        def f():
            c = (((_view(0, key) or {}).get('combat') or {}).get('weaponSwitched'))
            return c == n, f'WeaponSwitched published by the authority: {c} (want {n})'
        return f

    def agree():
        sv, ov = _view(0, key), _view(client, key)
        d = [x for x in _diff(sv, ov) if not str(x).startswith(('Health', 'Stamina'))] if sv and ov else 'missing view'   # the periodic regen of the test set lags by a period on the owner
        return not d, f'server vs owner diff: {d or "none"}'

    def no_unexplained():
        r = report_reconciles(marks['start']) if marks.get('start') is not None else None
        if r is None:
            return False, 'the log was not readable'
        return r['unexplained_ours'] == 0, f"{r['unexplained_ours']} unexplained of ours {r['unexplained_by_model']}, forced={r['forced']} {r['unexplained_lines']}"

    steps = [
        (0.6, _mark(run, marks, 'start')),   # after the grant: the server-only revoke and grant at 0.1 s is a deliberate one-off ASC difference
        (0.1, lambda: ('grant and equip the primary', grant())),
        (0.2, run.at('probes granted and the primary equipped', lambda: (all(st['granted']) and st['equipped'], f"{st['granted']} {st['equipped']}"))),
        (2.5, run.at('start: the primary weapon, WalkSpeed 900, run 600', weapon(0, 900.0, 600.0))),
        (2.6, lambda: ('reset counters', [c.reset_probe_counters() for w in worlds() for c in [comps(w).get(key)] if c is not None] and 'reset')),
        (2.8, lambda: ('walk', hold(client, 'move', (0, 1, 0)))),
        (5.0, run.at('walking with the primary', peak(3.6, 5.0, 600))),
        (5.1, lambda: ('stop', release(client, 'move'))),
        (5.6, lambda: ('press 1: switch', press(client, 'buff'))),
        (5.7, lambda: ('force reconcile across the swap', force_reconcile(client, resim_frames) if client != 0 else 'host: nothing to reconcile')),
        (7.0, run.at('press 1: the support weapon, WalkSpeed 300, run 200', weapon(1, 300.0, 200.0))),
        (7.0, run.at('press 1: exactly one WeaponSwitched', switched(1))),
        (7.2, lambda: ('walk', hold(client, 'move', (0, 1, 0)))),
        (9.6, run.at('walking with the support weapon', peak(8.2, 9.6, 200))),
        (9.7, lambda: ('stop', release(client, 'move'))),
        (10.2, lambda: ('press 2: switch back', press(client, 'buff'))),
        (10.3, lambda: ('force reconcile across the swap', force_reconcile(client, resim_frames) if client != 0 else 'host: nothing to reconcile')),
        (11.6, run.at('press 2: the primary again', weapon(0, 900.0, 600.0))),
        (11.6, run.at('press 2: exactly one more WeaponSwitched, none from the re-simulation', switched(2))),
        (11.7, run.at('server and owner agree', agree)),
        (11.75, run.at('no unexplained reconciles of ours (before the cleanup)', no_unexplained)),
        (11.8, lambda: ('cleanup', (_srv(key).server_revoke_probes(), _srv(key).server_equip_primary_weapon(False)))),
        (12.6, run.finish()),
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
# Server frames at which a scenario made a deliberate server-only change (a grant, a revoke, a kit or loadout change): each is an out-of-band
# change of the ability system, so the owner reconciles once, by design. report_reconciles does not count a mismatch whose received frame lies in
# [frame, frame + OWN_CHANGE_FRAMES] (about 1.5 s: lag and the resend) as unexplained; loadout_change judges its own changes separately.
_server_changes = []
OWN_CHANGE_FRAMES = 90


class _ServerProxy:
    """The authority's test component; every server_* call it receives is recorded in _server_changes with the server frame."""
    def __init__(self, comp):
        self._c = comp

    def __getattr__(self, name):
        attr = getattr(self._c, name)
        if name.startswith('server_') and callable(attr):
            def call(*args, **kwargs):
                try:
                    frame = json.loads(self._c.get_state_snapshot()).get('serverFrame')
                except Exception:
                    frame = None
                if frame is not None:
                    _server_changes.append(frame)
                return attr(*args, **kwargs)
            return call
        return attr


def _srv(key):
    """Test component of pawn `key` on world 0 (the host or the dedicated server): the one whose Server* functions run."""
    return _ServerProxy(comps(worlds()[0])[key])


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
    want = (0, 0) if client == 0 else (0, 2)  # the owner's cost of an out-of-simulation grant: C19 measured 0 (the fork no longer mismatches on it), the spike's 1..2 stays the bound
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
    return max((sum(x * x for x in s['vel'][:2]) ** 0.5 for s in run._win(side, t0, t1, sim=True)), default=0.0)


def _apex(run, side, t_base, t0, t1):
    """Highest z in [t0, t1] minus the z of the last sample before t_base."""
    before = [s[side]['loc'][2] for s in run.sim if s['t'] < t_base and s.get(side)]
    zs = [s['loc'][2] for s in run._win(side, t0, t1, sim=True)]
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
        # The GASP walking mode takes its speed from the gait variables (run for a full stick), not from MaxSpeed
        ma, mb = stat('srv', 'gaitRunSpeed') or 0.0, stat('srvB', 'gaitRunSpeed') or 0.0
        good = vb > 1.0 and va > 2.5 * vb and 0.6 * ma <= va <= 1.05 * ma and 0.6 * mb <= vb <= 1.05 * mb
        return good, f"peak speed A {va:.0f} (run {ma:.0f}), B {vb:.0f} (run {mb:.0f}); MaxSpeedOverride A {stat('srv', 'maxSpeedOverride')} B {stat('srvB', 'maxSpeedOverride')}"

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



# ---- C13 jump scenarios (combat-m3 7.9). The test pawn is given the platform jump kit by the harness (server_set_jump_kit), which the test binding set
# binds to Command.Movement.Jump (Press of Input.Button.Jump) and Command.Movement.FastFall (Press of Input.Button.Crouch).

def _jump_tap(client, hold):
    """One press of the jump button; with hold > 0 the button then stays held until combat_clear (the caller schedules it)."""
    mask = button_mask('Jump')
    combat_tap(client, mask)
    if hold > 0:
        combat_hold(client, mask)
    return f'tap jump, hold {hold}s'


def _apex_up(run, side, t_base, t0, t1, up=(0.0, 0.0, 1.0)):
    """Highest point along `up` reached in [t0, t1], measured from the last sample before t_base: (height, displacement vector at the apex)."""
    before = [s[side]['loc'] for s in run.sim if s['t'] < t_base and s.get(side)]
    if not before:
        return 0.0, None
    b, best, at = before[-1], 0.0, None
    for s in run._win(side, t0, t1, sim=True):
        d = tuple(s['loc'][i] - b[i] for i in range(3))
        h = sum(d[i] * up[i] for i in range(3))
        if h > best:
            best, at = h, d
    return best, at


def _extreme_up_speed(run, side, t0, t1, up=(0.0, 0.0, 1.0), lowest=True):
    speeds = [sum(s['vel'][i] * up[i] for i in range(3)) for s in run._win(side, t0, t1, sim=True)]
    return (min(speeds) if lowest else max(speeds)) if speeds else 0.0


def _jump_field(run, side, field):
    return (_latest(run, side).get('jump') or {}).get(field)


def _both(run, fn):
    """(ok, text) of fn(side) on the authority ('srv') and the owner ('cli'): both must hold, the text shows both."""
    a, b = fn('srv'), fn('cli')
    return bool(a[0] and b[0]), f'srv: {a[1]}; cli: {b[1]}'


def _field_is(run, field, want, label, span=1.0):
    """Timeline check: a field of the jump state was `want` on the authority and on the owner at some sample of the last `span` seconds. The
    authority simulates a frame later in wall time than the owner (input travel), so a point test would compare different frames."""
    def f():
        t1 = run.now()
        def side(sd):
            seen = [s[sd].get('jump', {}).get(field) for s in run.samples if t1 - span <= s['t'] <= t1 and s.get(sd)]
            return want in seen, f'{field} seen {sorted(set(map(str, seen)))} (want {want})'
        return _both(run, side)
    return run.at(label, f)


def _field_never(run, field, bad, label, t0, t1):
    """Timeline check: a field of the jump state was never `bad` on either side in [t0, t1] (scenario time, so mind the authority's delay)."""
    def f():
        def side(sd):
            hit = [s['t'] for s in run.samples if t0 <= s['t'] <= t1 and s.get(sd) and s[sd].get('jump', {}).get(field) == bad]
            return not hit, f'{field} == {bad} at {hit[:1]}'
        return _both(run, side)
    return run.at(label, f)


def _apex_pair(run, store, label, t_base, t0, t1, up=(0.0, 0.0, 1.0), tol=1.0, floor=20.0):
    """Timeline check: the apex height of this jump on the authority and on the owner, kept in store[label], within `tol` cm of each other."""
    def f():
        hs, ds = _apex_up(run, 'srv', t_base, t0, t1, up)
        hc, dc = _apex_up(run, 'cli', t_base, t0, t1, up)
        store[label] = (hs, hc, ds, dc)
        return hs > floor and abs(hs - hc) <= tol, f'{label}: apex srv {hs:.1f} cm, owner {hc:.1f} cm (delta {abs(hs - hc):.2f}, want <= {tol})'
    return run.at(f'{label}: apex height, authority vs owner', f)


def scenario_variable_jump(client=1, holds=(0.0, 0.12, 0.30), spacing=3.0):
    """C13 variable jump: the longer the jump button is held (anywhere between the minimum hop and MaxJumpHoldFrames) the higher the jump.
    One jump per hold time, each from standing; judged on the authority and the owner: apex heights strictly increase with the hold time,
    authority and owner agree within 1 cm, no unexplained reconcile of ours. The hold times are well apart in frames (a hold is quantised by the
    editor tick that releases it, about one frame): 0.12 s lands between the minimum hop and the cap (0.10 s sometimes gave the apex of a tap, 0.16 s sometimes the cap: the window between 4 and 12 frames is too narrow for a third mid hold), 0.30 s the cap."""
    run = _Run('variable_jump', client)
    key, apex = run.key, {}
    # The server-only grant and revoke of the kit are reconciles of their own (an unpredicted ASC change): the reconcile judgement window is
    # re-marked after the grant and closed before the revoke.
    def remark():
        run.log_mark = mark_log()
        return 'judge window starts after the grant', True
    steps = [(0.3, lambda: ('grant the jump kit', _srv(key).server_set_jump_kit(True))), (1.0, remark)]
    for i, hold in enumerate(holds):
        t = 1.5 + i * spacing
        steps.append((t, lambda hold=hold: ('jump', _jump_tap(client, hold))))
        if hold > 0:
            steps.append((t + hold, lambda: ('release', combat_clear(client))))
        steps.append((t + spacing - 0.3, _apex_pair(run, apex, f'hold {hold}s', t - 0.05, t, t + spacing - 0.3)))

    def increasing():
        hs = [apex[f'hold {h}s'][0] for h in holds if f'hold {h}s' in apex]
        return len(hs) == len(holds) and all(b > a + 2.0 for a, b in zip(hs, hs[1:])), 'authority apex heights ' + ', '.join(f'{x:.1f}' for x in hs) + ' (each > the previous + 2 cm)'

    end = 1.5 + len(holds) * spacing
    steps += [
        (end, run.at('apex heights strictly increase with the hold time', increasing)),
        (end + 0.1, run.finish()),
        (end + 0.2, lambda: ('cleanup', (combat_clear(client), _srv(key).server_set_jump_kit(False)))),
    ]
    run.run(steps)



def scenario_jump_squat(client=1):
    """C19 jump squat: KIT_Test_Squat binds UGSTN_GA_TestJumpSquat (SquatFrames 6, data on the ability) to the jump command. The ability owns
    Test.State.JumpSquat from the request to the take-off. Judged on the sim ticks of the authority and the owner: six frames with the tag and no
    vertical speed in them, the first rising tick comes 7 to 10 ticks after the first tag tick (6 of squat plus the 1 to 4 ticks the velocity sample takes to show a
    queued impulse; the pure state machine test pins the take-off frame exactly), a second jump press inside the squat is refused (one request only), and both machines agree to a tick."""
    run = _Run('jump_squat', client, judge_reconciles=False)
    key, st, marks = run.key, {}, {}
    base = (_loadout_view(0, key)[0].get('revision') or 0)
    tag, jump, frame_ms = 'Test.State.JumpSquat', button_mask('Jump'), 1000.0 / 60.0

    def ticks(side):
        # one entry per sim tick (a resimulated tick keeps its last version), in sim order
        by_ms = {x[side]['ms']: x[side] for x in run.sim if 2.0 <= x['t'] <= 4.5 and x.get(side)}
        return [by_ms[k] for k in sorted(by_ms)]

    def squat_shape(side):
        def f():
            w = ticks(side)
            tagged = [s for s in w if tag in s['tags']]
            rise = next((s for s in w if s['vel'][2] > 100.0), None)
            if not tagged or rise is None:
                return False, f'{side}: tagged ticks {len(tagged)}, rising tick {rise is not None}'
            flat = all(abs(s['vel'][2]) < 50.0 for s in tagged)
            delay = w.index(rise) - w.index(tagged[0])
            return (len(tagged) == 6 and flat and 7 <= delay <= 10), f'{side}: {len(tagged)} tagged ticks (want 6: the ability ends on the take-off frame), flat in the squat {flat}, first tag tick to first rising tick {delay} sim ticks (want 7..10: 6 of squat plus the 1 to 4 sim ticks the velocity sample takes to show the impulse: it moves in steps of 2 to 3 ticks, as after a plain jump)'
        return run.at(f'squat shape on the {side}', f)

    def same_delay():
        d = {}
        for side in ('srv', 'cli'):
            w = ticks(side)
            tagged = [i for i, s in enumerate(w) if tag in s['tags']]
            rise = next((i for i, s in enumerate(w) if s['vel'][2] > 100.0), None)
            d[side] = (rise - tagged[0]) if tagged and rise is not None else None
        return d['srv'] is not None and d['cli'] is not None and abs(d["srv"] - d["cli"]) <= 3, f"sim ticks from the request to the first rising tick: authority {d['srv']}, owner {d['cli']} (want within 3: the velocity sample moves in steps of 2 to 3 ticks and the two copies sample on different phases)"

    def same_requests():
        s, c = run.new_bindings(1.95, 4.4, 'srv'), run.new_bindings(1.95, 4.4, 'cli')
        return s == c, f'request bindings authority {s}, owner {c} (want equal; the refused press shows in the squat tag count of 6, a second squat would run longer)'

    steps = [
        (0.3, _mark(run, marks, 'start')),
        (0.4, lambda: ('clear', combat_clear(client))),
        (0.6, _c18_apply(key, st, 'S', 'KIT_Test_Squat')),
        (1.8, _c18_accepted(run, st, 'S')),
        (1.9, _c18_state(run, key, client, 'KIT_Test_Squat', base + 1, 'Squat kit applied on both, same table')),
        (2.0, lambda: ('jump', combat_tap(client, jump))),
        (2.04, lambda: ('second press inside the squat', combat_tap(client, jump))),
        (4.4, squat_shape('srv')),
        (4.4, squat_shape('cli')),
        (4.4, run.at('the squat jump takes off at the same delay (within the sample step) on both', same_delay)),
        (4.4, run.at('the same requests on both', same_requests)),
        (4.5, lambda: ('cleanup', combat_clear(client))),
        (4.6, run.finish()),
    ]
    run.run(steps)

def scenario_air_jump_fastfall(client=1):
    """C13 air jump and fast fall. A: a plain jump (reference apex and fall speed). B: a fast fall request while still rising is refused (no
    request), one while falling sets the fast fall and the descent is faster than A. C: a jump and an air jump (one used, higher than A), and
    a second air jump request is refused (none left). Authority and owner agree within 1 cm on every apex."""
    run = _Run('air_jump_fastfall', client)
    key, apex = run.key, {}
    jump, crouch = button_mask('Jump'), button_mask('Crouch')
    TA, TB, TC = 1.5, 5.0, 8.5

    def refused(t0, t1, label):
        def f():
            s, c = run.new_bindings(t0, t1, 'srv'), run.new_bindings(t0, t1, 'cli')
            return not s and not c, f'requests srv {s} cli {c} (want none)'
        return run.at(label, f)

    def remark():
        run.log_mark = mark_log()
        return 'judge window starts after the grant', True

    def descent(label, t0):
        """Seconds of sim time from the apex to the landing (z back within 2 cm of the take-off height), on the sim ticks of the authority."""
        w = [x['srv'] for x in run.sim if t0 <= x['t'] <= t0 + 3.0 and x.get('srv')]
        z0, top = w[0]['loc'][2], max(range(len(w)), key=lambda i: w[i]['loc'][2])
        land = next((x['ms'] for x in w[top:] if x['loc'][2] <= z0 + 2.0), None)
        return None if land is None else (land - w[top]['ms']) / 1000.0

    def faster_fall():
        a, b = descent('A', TA), descent('B', TB)
        ok = a is not None and b is not None and b < a - 0.03
        return ok, f'apex-to-landing time: A {a}, B (fast fall requested while falling) {b} (B must be at least 0.03 s shorter: a time is exact on the sim tick, a landing speed is not)'

    def air_higher():
        a, c = apex['A'][0], apex['C'][0]
        return c > a + 30.0, f'apex A {a:.1f}, C (with the air jump) {c:.1f} (C > A + 30)'

    steps = [
        (0.3, lambda: ('grant the jump kit', _srv(key).server_set_jump_kit(True))),
        (1.0, remark),  # the server-only grant / revoke are reconciles of their own: judged after the grant, closed before the revoke
        # A: a plain jump
        (TA, lambda: ('A: jump', _jump_tap(client, 0.3))),
        (TA + 0.3, lambda: ('release', combat_clear(client))),
        (TA + 3.0, _apex_pair(run, apex, 'A', TA - 0.05, TA, TA + 3.0)),
        # B: fast fall refused while rising, accepted while falling
        (TB, lambda: ('B: jump', _jump_tap(client, 0.3))),
        (TB + 0.3, lambda: ('release', combat_clear(client))),
        (TB + 0.2, lambda: ('B: fast fall while rising (refused)', combat_tap(client, crouch))),
        (TB + 0.55, _field_never(run, 'fastFalling', True, 'B: no fast fall while rising', TB, TB + 0.55)),
        (TB + 0.66, lambda: ('B: fast fall while falling', combat_tap(client, crouch))),
        (TB + 1.6, _field_is(run, 'fastFalling', True, 'B: fast falling', span=1.0)),
        (TB + 3.0, run.at('B: the fast fall descent is faster than the plain one', faster_fall)),
        # C: an air jump, and a second one refused
        (TC, lambda: ('C: jump', _jump_tap(client, 0.3))),
        (TC + 0.3, lambda: ('release', combat_clear(client))),
        (TC + 0.4, lambda: ('C: air jump', combat_tap(client, jump))),
        (TC + 1.0, _field_is(run, 'airJumpsUsed', 1, 'C: one air jump used', span=0.8)),
        (TC + 0.7, lambda: ('C: a second air jump (refused)', combat_tap(client, jump))),
        (TC + 1.2, _field_never(run, 'airJumpsUsed', 2, 'C: never a second air jump', TC, TC + 1.2)),
        (TC + 3.0, _apex_pair(run, apex, 'C', TC - 0.05, TC, TC + 1.8)),
        (TC + 3.0, run.at('the air jump goes higher than the plain jump', air_higher)),
        (TC + 3.8, _field_is(run, 'airJumpsUsed', 0, 'C: landing resets the air jumps', span=0.5)),
        (TC + 3.9, run.finish()),
        (TC + 4.0, lambda: ('cleanup', (combat_clear(client), _srv(key).server_set_jump_kit(False)))),
    ]
    run.run(steps)


def _set_gravity_everywhere(key, accel, enable):
    """Mover's gravity override on every machine's copy of pawn `key`: it is a component setting, so every copy must hold the same value."""
    for w in worlds():
        c = comps(w).get(key)
        if c is not None:
            c.set_test_gravity(unreal.Vector(*accel), enable)
    return f'gravity {accel if enable else "default"} set on {len(worlds())} worlds'


def _unit(v):
    n = sum(x * x for x in v) ** 0.5
    return tuple(x / n for x in v)


def _angle_deg(a, b):
    import math
    ua, ub = _unit(a), _unit(b)
    return math.degrees(math.acos(max(-1.0, min(1.0, sum(x * y for x, y in zip(ua, ub))))))


def scenario_tilted_gravity_jump(client=1, tilt_deg=30.0):
    """C13 under tilted gravity (owner rule 1): gravity 980 cm/s^2 tilted `tilt_deg` from straight down; every "up" is -gravity. A jump, an air jump
    and a fast fall go along local up: the launch velocity is within 3 degrees of local up, the displacement at the apex is along it, the
    fast fall descends along it, and authority and owner agree on the apex within 1 cm. The tilt is kept small enough for the world floor to
    stay walkable under it."""
    import math
    t = math.radians(tilt_deg)
    gravity = (980.0 * math.sin(t), 0.0, -980.0 * math.cos(t))
    up = _unit(tuple(-g for g in gravity))
    run = _Run('tilted_gravity_jump', client)
    key, apex = run.key, {}
    jump, crouch = button_mask('Jump'), button_mask('Crouch')
    T = 3.0

    def fast_samples(side, t0, t1, speed):
        return [s['vel'] for s in run._win(side, t0, t1, sim=True) if sum(x * x for x in s['vel']) > speed ** 2]

    def launch_direction():
        def f(side):
            fast = fast_samples(side, T + 0.02, T + 0.7, 200.0)
            if not fast:
                return False, 'no fast sample'
            a = _angle_deg(fast[0], up)
            return a < 3.0, f'launch velocity {a:.2f} deg from local up'
        return _both(run, f)

    def displacement_along_up():
        def f(side):
            h, d = _apex_up(run, side, T - 0.05, T, T + 2.0, up)
            if d is None:
                return False, 'no apex'
            lateral = sum((d[i] - h * up[i]) ** 2 for i in range(3)) ** 0.5
            return lateral < 0.08 * h and h > 50.0, f'apex {h:.1f} cm along up, {lateral:.1f} cm sideways of it (want < 8%)'
        return _both(run, f)

    def descent_along_up():
        def f(side):
            fast = fast_samples(side, T + 1.35, T + 2.6, 400.0)
            if not fast:
                return False, 'no fast descent sample'
            a = _angle_deg(fast[0], tuple(-x for x in up))
            return a < 6.0, f'fast descent {a:.2f} deg from local down'
        return _both(run, f)

    def standing():
        def f(side):
            s = _latest(run, side)
            return abs(s['vel'][2]) < 20.0 and abs(s['vel'][0]) < 20.0, f'mode {s["mode"]}, velocity {tuple(round(x) for x in s["vel"])}'
        return _both(run, f)

    steps = [
        (0.3, lambda: ('grant the jump kit', _srv(key).server_set_jump_kit(True))),
        (0.6, lambda: ('tilt gravity', _set_gravity_everywhere(key, gravity, True))),
        (T - 0.5, run.at('standing still under the tilted gravity', standing)),
        (T, lambda: ('jump', _jump_tap(client, 0.3))),
        (T + 0.3, lambda: ('release', combat_clear(client))),
        (T + 0.8, run.at("the launch goes along local up", launch_direction)),
        (T + 0.45, lambda: ('air jump', combat_tap(client, jump))),
        (T + 1.0, _field_is(run, 'airJumpsUsed', 1, 'one air jump used')),
        (T + 1.3, lambda: ('fast fall', combat_tap(client, crouch))),
        (T + 1.9, _field_is(run, 'fastFalling', True, 'fast falling')),
        (T + 3.5, _apex_pair(run, apex, 'tilted jump', T - 0.05, T, T + 2.0, up)),
        (T + 3.5, run.at('the displacement at the apex is along local up', displacement_along_up)),
        (T + 3.5, run.at('the fast fall descends along local up', descent_along_up)),
        (T + 3.6, lambda: ('restore gravity', _set_gravity_everywhere(key, gravity, False))),
        (T + 3.7, lambda: ('cleanup', (combat_clear(client), _srv(key).server_set_jump_kit(False)))),
        (T + 4.7, run.finish()),
    ]
    run.note(f'gravity {tuple(round(g, 1) for g in gravity)}, local up {tuple(round(x, 3) for x in up)}')
    run.run(steps)


def scenario_external_launch_jump(client=1, launch_speed=900.0):
    """C13 external launch (owner rule 2): a launch layered move through the GE path (the Launcher probe ability, triggered by a press of the buff
    button like the triggered timeline) that is not the combatant's own jump. Judged on the authority and the owner:
      A (in the air): jump, air jump (none left), the launch refills the air jumps (default rules), the next air jump works again;
      B (from the ground): the launch ends the coyote window, so a jump request right after leaving the ground is an air jump (airJumpsUsed 1);
      the launch rises with plain gravity (no hold phase): apex about speed^2 / (2 g), authority and owner within 1 cm."""
    run = _Run('external_launch_jump', client)
    key, apex = run.key, {}
    jump = button_mask('Jump')
    TA, TB = 1.5, 6.0

    def launch_apex():
        hs = apex['B'][0]
        want = launch_speed ** 2 / (2.0 * 980.0)
        return abs(hs - want) < 0.06 * want, f'apex {hs:.1f} cm, plain gravity predicts {want:.1f}'

    steps = [
        (0.3, lambda: ('grant the jump kit and the launcher', (_srv(key).server_set_jump_kit(True), _srv(key).server_revoke_probes(), [_srv(key).server_grant_probe(n) for n in ('Launcher', 'TagRaiser')]))),
        # A: both jumps used, then a launch in the air
        (TA, lambda: ('A: jump', _jump_tap(client, 0.3))),
        (TA + 0.3, lambda: ('release', combat_clear(client))),
        (TA + 0.4, lambda: ('A: air jump', combat_tap(client, jump))),
        (TA + 0.85, _field_is(run, 'airJumpsUsed', 1, 'A: the air jump is used')),
        (TA + 0.7, lambda: ('A: a second air jump (refused)', combat_tap(client, jump))),
        (TA + 0.8, _field_is(run, 'airJumpsUsed', 1, 'A: no air jump left')),
        (TA + 0.9, lambda: ('A: the launch', press(client, 'buff'))),
        (TA + 1.1, _field_is(run, 'airJumpsUsed', 0, 'A: the launch refilled the air jumps')),
        (TA + 1.5, _field_is(run, 'phase', 3, 'A: launched phase (3), plain gravity')),
        (TA + 1.2, lambda: ('A: air jump again', combat_tap(client, jump))),
        (TA + 1.4, _field_is(run, 'airJumpsUsed', 1, 'A: the air jump works again after the launch')),
        (TA + 4.0, _field_is(run, 'airJumpsUsed', 0, 'A: landed, reset')),
        # B1: a launch from the ground and nothing else: plain gravity all the way
        (TB, lambda: ('B1: the launch from the ground', press(client, 'buff'))),
        (TB + 3.0, _apex_pair(run, apex, 'B', TB - 0.05, TB, TB + 2.5)),
        (TB + 3.1, run.at('the launch rises with plain gravity', launch_apex)),
        # B2: the same launch, then a jump request right after leaving the ground
        (TB + 4.0, lambda: ('B2: the launch from the ground', press(client, 'buff'))),
        (TB + 4.25, lambda: ('B2: jump request in the air', combat_tap(client, jump))),
        (TB + 4.95, _field_is(run, 'airJumpsUsed', 1, 'B2: the launch ended the coyote window: the jump was an air jump')),
        (TB + 8.0, lambda: ('cleanup', (combat_clear(client), _srv(key).server_revoke_probes(), _srv(key).server_set_jump_kit(False)))),
        (TB + 9.0, run.finish()),
    ]
    run.run(steps)


IA.setdefault('sprint', '/Game/Input/IA_Sprint')
IA.setdefault('crouch', '/Game/Input/IA_Crouch')


def _bare_input(client, action, value=(1.0, 0.0, 0.0), start=True, pulse=False):
    """Input on a pawn without the test component (SandboxCharacter_Mover through set_pawn_override): the local player's enhanced input
    subsystem injects the action (UGSTN_TestSlotUtils.inject_input / stop_input)."""
    pc = unreal.GameplayStatics.get_player_controller(worlds()[client], 0)
    act = unreal.load_asset(IA[action])
    utils = unreal.GSTN_TestSlotUtils
    ok = utils.inject_input(pc, act, unreal.Vector(*value), pulse) if start else utils.stop_input(pc, act)
    return f'{"start" if start else "stop"} {action}: {ok}'


def scenario_slide_jump(client=1, repeats=10, bare=False):
    """Owner bug report 2026-10-23: sprint, crouch to slide, jump during the slide, and at some point the client is corrected. Repeats the sequence
    `repeats` times with np.LogReconcileField on and attributes the reconciles of the whole run: mismatches, unexplained of ours, by model
    (mover / asc / combat) and the logged differing fields (see the lines in the report). Strict check: no unexplained reconcile of ours; the Mover
    corrections are listed, not judged (the cause is in the Blueprint modes if they show a movement mode / slide state field).
    bare=True: the pawn is not the harness pawn (set_pawn_override('SandboxCharacter_Mover') before PIE): no sampling, inputs through the
    player's enhanced input subsystem, the verdict is the reconcile report only."""
    run = _Run('slide_jump', client, judge_reconciles=False, sample=not bare)
    if bare:
        press = lambda c, a: _bare_input(c, a, pulse=True)
        hold = lambda c, a, v: _bare_input(c, a, v)
        release = lambda c, a: _bare_input(c, a, start=False)
    else:
        from_globals = globals()
        press, hold, release = from_globals['press'], from_globals['hold'], from_globals['release']
    marks = {}
    for w in worlds():
        unreal.SystemLibrary.execute_console_command(w, 'np.LogReconcileField 1')
    steps = [(0.5, _mark(run, marks, 'start'))]
    t = 1.0
    for i in range(repeats):
        steps += [
            (t, lambda: ('walk + sprint', (hold(client, 'move', (0, 1, 0)), hold(client, 'sprint', (1, 0, 0))))),
            (t + 1.3, lambda: ('crouch: slide', hold(client, 'crouch', (1, 0, 0)))),
            (t + 1.7, lambda: ('jump during the slide', press(client, 'jump'))),
            (t + 2.0, lambda: ('release crouch', release(client, 'crouch'))),
            (t + 2.4, lambda: ('release', (release(client, 'sprint'), release(client, 'move')))),
        ]
        t += 3.6
    def attribute():
        r = report_reconciles(marks['start']) if marks.get('start') is not None else None
        if r is None:
            return False, 'log not readable'
        run.note(f"reconciles: {r['mismatches']} mismatches, unexplained of ours {r['unexplained_ours']} {r['unexplained_by_model']}, forced {r['forced']}, offset changes {r['offset_changes']}")
        for line in (r['unexplained_lines'] or [])[:12]:
            run.note(f'  {line}')
        return r['unexplained_ours'] == 0, f"{r['unexplained_ours']} unexplained of ours {r['unexplained_by_model']}; mover-side total mismatches {r['mismatches']}"
    steps += [
        (t, run.at('no unexplained reconcile of ours over the repeats', attribute)),
        (t + 0.5, run.finish()),
    ]
    run.run(steps)



# ---- C14 movement kit scenarios (combat-m3 section 3). The harness grants the plugin dash / dodge abilities (server_set_move_kit); the test binding
# set binds them to Command.Movement.Dash (Press of Input.Button.Sprint), Command.Defence.Dodge (Press of Input.Button.Dodge, ground / air /
# escape variants by context and tag) and Command.Interact.Grab / PickUp (Release of Input.Button.Interact, tap / hold).

def _kit(key, dash=True, dodge=True, interact=True):
    return _srv(key).server_set_move_kit(dash, dodge, interact)


def _moved(run, side, t_base, t1):
    """Displacement (x, y, z) of `side` between the last sample before t_base and the latest sample up to t1."""
    before = [s[side]['loc'] for s in run.sim if s['t'] < t_base and s.get(side)]
    after = [s[side]['loc'] for s in run.sim if s['t'] <= t1 and s.get(side)]
    if not before or not after:
        return None
    return tuple(after[-1][i] - before[-1][i] for i in range(3))


def _norm(v):
    return sum(x * x for x in v) ** 0.5


def _landing_pair(run, store, label, t_base, t1, up=(0.0, 0.0, 1.0), tol=1.0, floor=30.0):
    """Timeline check: where the move left the pawn (displacement from before t_base), authority vs owner within `tol` cm, and it moved at least `floor`."""
    def f():
        ds, dc = _moved(run, 'srv', t_base, t1), _moved(run, 'cli', t_base, t1)
        if ds is None or dc is None:
            return False, 'no samples'
        store[label] = (ds, dc)
        delta = _norm(tuple(ds[i] - dc[i] for i in range(3)))
        start = [s['srv'] for s in run.sim if s['t'] < t_base and s.get('srv')][-1]
        return _norm(ds) > floor and delta <= tol, f'{label}: displacement srv {tuple(round(x, 1) for x in ds)}, owner {tuple(round(x, 1) for x in dc)} (delta {delta:.2f} cm, want <= {tol}, moved >= {floor}; authority from {tuple(round(x) for x in start["loc"])} in {start["mode"]})'
    return run.at(f'{label}: landing position, authority vs owner', f)


def _no_move(run, label, t_base, t1, limit=5.0):
    """Timeline check: neither copy moved more than `limit` cm between t_base and t1 (a blocked or missing move does nothing)."""
    def f():
        ds, dc = _moved(run, 'srv', t_base, t1), _moved(run, 'cli', t_base, t1)
        if ds is None or dc is None:
            return False, 'no samples'
        return _norm(ds) <= limit and _norm(dc) <= limit, f'displacement srv {_norm(ds):.1f} cm, owner {_norm(dc):.1f} cm (want <= {limit})'
    return run.at(label, f)


def _in_timeline_seen(run, label, t0, t1, want=True):
    def f():
        def side(sd):
            seen = any(s['t'] >= t0 and s['t'] <= t1 and s.get(sd) and s[sd].get('intl') for s in run.samples)
            return seen == want, f'timeline seen {seen} (want {want})'
        return _both(run, side)
    return run.at(label, f)


def _standing_after(run, label, t):
    def f():
        def side(sd):
            s = _latest(run, sd)
            return (not s.get('intl')) and abs(s['vel'][2]) < 20.0 and (s['vel'][0] ** 2 + s['vel'][1] ** 2) ** 0.5 < 40.0, f'mode {s.get("mode")}, in timeline {s.get("intl")}, velocity {tuple(round(x) for x in s["vel"])}'
        return _both(run, side)
    return run.at(label, f)


def scenario_dash(client=1, up=(0.0, 0.0, 1.0), gravity=None, name='dash'):
    """C14 dash: a press of the sprint button throws the pawn along its facing in the plane perpendicular to local up. Judged on the authority and
    the owner: the pawn moved, the peak speed is perpendicular to up (within 4 degrees), the timeline ran and ended, and both copies end within 1 cm."""
    run = _Run(name, client)
    key, st = run.key, {}
    sprint = button_mask('Sprint')
    T = 2.0

    def peak_direction():
        def f(side):
            fast = [s['vel'] for s in run._win(side, T, T + 1.0, sim=True) if _norm(s['vel']) > 500.0]
            if not fast:
                return False, 'no fast sample'
            a = _angle_deg(fast[0], up)
            return abs(a - 90.0) < 4.0, f'peak velocity {a:.2f} deg from local up (want 90 +- 4)'
        return _both(run, f)

    steps = [(0.3, lambda: ('grant the movement kit', _kit(key)))]
    if gravity is not None:
        steps += [(0.6, lambda: ('tilt gravity', _set_gravity_everywhere(key, gravity, True)))]
    steps += [
        (T - 0.3, _standing_after(run, 'standing before the dash', T)),
        (T, lambda: ('dash', combat_tap(client, sprint))),
        (T + 0.8, _in_timeline_seen(run, 'the dash timeline ran', T, T + 0.8)),
        (T + 1.2, run.at('the peak speed is perpendicular to local up', peak_direction)),
        (T + 3.0, _landing_pair(run, st, 'dash', T - 0.05, T + 3.0, floor=100.0)),
        (T + 3.0, _standing_after(run, 'back to actionable, at rest', T)),
    ]
    if gravity is not None:
        steps += [(T + 3.1, lambda: ('restore gravity', _set_gravity_everywhere(key, gravity, False)))]
    steps += [
        (T + 3.2, lambda: ('cleanup', (combat_clear(client), _kit(key, False, False, False)))),
        (T + 4.2, run.finish()),
    ]
    run.run(steps)


def scenario_dash_tilted_gravity(client=1, tilt_deg=30.0):
    """C14 dash under gravity tilted `tilt_deg` from straight down (owner rule 1): the same checks as scenario_dash with the up of the tilted frame."""
    import math
    t = math.radians(tilt_deg)
    gravity = (980.0 * math.sin(t), 0.0, -980.0 * math.cos(t))
    scenario_dash(client, up=_unit(tuple(-g for g in gravity)), gravity=gravity, name='dash_tilted_gravity')


def scenario_ground_dodge(client=1):
    """C14 ground dodge: a press of the dodge button on the ground throws the pawn into a low arc (long jump) and it lands standing. Judged on the authority
    and the owner: the apex of the arc and the landing position agree within 1 cm, the apex is near the prediction of the default data (46 cm), the
    timeline ended on landing (actionable again, no lying state: mode walking, speed ~0)."""
    run = _Run('ground_dodge', client)
    key, st, apex = run.key, {}, {}
    dodge = button_mask('Dodge')
    T = 2.0
    steps = [
        (0.3, lambda: ('grant the movement kit', _kit(key))),
        (T, lambda: ('dodge', combat_tap(client, dodge))),
        (T + 0.4, _in_timeline_seen(run, 'the dodge timeline ran', T, T + 0.4)),
        (T + 3.0, _apex_pair(run, apex, 'ground dodge', T - 0.05, T, T + 1.5, tol=1.0, floor=20.0)),
        (T + 3.0, _landing_pair(run, st, 'ground dodge', T - 0.05, T + 3.0, floor=200.0)),
        (T + 3.0, _standing_after(run, 'landed standing, actionable', T)),
        (T + 3.1, lambda: ('cleanup', (combat_clear(client), _kit(key, False, False, False)))),
        (T + 4.1, run.finish()),
    ]
    run.run(steps)


def scenario_air_dodge(client=1):
    """C14 air dodge (dive roll): jump, press dodge in the air. The dive goes forward and down, the timeline ends on landing (plus the landing lag),
    and the landing position agrees between the authority and the owner within 1 cm."""
    run = _Run('air_dodge', client)
    key, st = run.key, {}
    dodge, jump = button_mask('Dodge'), button_mask('Jump')
    T = 2.0
    steps = [
        (0.3, lambda: ('grant the movement and jump kits', (_kit(key), _srv(key).server_set_jump_kit(True)))),
        (T, lambda: ('jump', _jump_tap(client, 0.3))),
        (T + 0.3, lambda: ('release', combat_clear(client))),
        (T + 0.55, lambda: ('air dodge', combat_tap(client, dodge))),
        (T + 0.9, _in_timeline_seen(run, 'the air dodge timeline ran', T + 0.55, T + 0.9)),
        (T + 4.0, _landing_pair(run, st, 'air dodge', T - 0.05, T + 4.0, floor=100.0)),
        (T + 4.0, _standing_after(run, 'landed, actionable again', T)),
        (T + 4.1, lambda: ('cleanup', (combat_clear(client), _kit(key, False, False, False), _srv(key).server_set_jump_kit(False)))),
        (T + 5.1, run.finish()),
    ]
    run.run(steps)


def scenario_dodge_blocked_in_hitstun(client=1):
    """C14 owner rule 3, real since M4 H9c. `client` is the attacker (like every hit scenario); the dodger is the other client, a pawn that carries no
    loadout (the suite runs this after hit_basic, whose kit sits on the attacker). A: the attacker's hit puts the dodger in hitstun (a long one: the
    harness raises the hitstun floor), the dodge pressed in it does nothing (the blocked tags of the ability: Combat.State.Hitstun is projected from the
    real state). B: the hitstun over, the dodge works. C: hitstun on through the harness hook and the kit has the escape dodge (blocks dropped as data): it
    works; the hook does not touch the combat state, so the combat gate (commands are refused in a real hitstun) does not stand in the way."""
    dodger = 2 if client == 1 else 1
    run = _Run('dodge_blocked_in_hitstun', dodger, judge_reconciles=False)
    key, akey, st = run.key, key_of_player(client), {}
    dodge, attack = button_mask('Dodge'), button_mask('Attack')
    TH, TA, TB, TC = 2.0, 2.9, 5.8, 10.5

    def back_to_lane():
        # B's dodge (5.8 to 8.8 s) slides the pawn 2 to 3 m and C's goes on from there; back on the lane, C starts from the same pose as B.
        lane = sorted(comps(worlds()[0]), key=str).index(key)
        return 'back to the lane', comps(worlds()[0])[key].server_reset_to_spawn(lane_offset(lane))

    def prepare():
        _kit(key, False, True, False)
        hit_rules_setup()
        set_tuning(min_hitstun_frames=150)
        st['kit'] = _apply_loadout(akey, HIT_KIT)
        _hit_logging(True)
        return 'movement kit, rules, long hitstun, attacker kit', st['kit']

    def mark_hit():
        st['F'] = _latest_frame(0, akey)
        return 'mark', st['F']

    def in_hitstun():
        hits = [r for r in _hit_log(0, akey) if r[0] >= st['F']]
        seen = {sd: run.saw_tag('Combat.State.Hitstun', TA - 0.3, TA + 0.1, sd) for sd in ('srv', 'cli')}
        return len(hits) == 1 and all(seen.values()), f'hits {hits}, Combat.State.Hitstun on the dodger {seen}'

    def hitstun_over():
        seen = {sd: run.saw_tag('Combat.State.Hitstun', TB - 0.4, TB - 0.05, sd) for sd in ('srv', 'cli')}
        return not any(seen.values()), f'Combat.State.Hitstun just before B {seen}'

    def remark():
        run.log_mark = mark_log()
        return 'judge window starts after the teleport', True

    def cleanup():
        combat_clear(dodger)
        _srv(key).server_set_hitstun(False)
        _srv(key).server_set_escape_kit(False)
        _kit(key, False, False, False)
        set_damage(key, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'cleanup'

    steps = [
        (0.3, prepare),
        (0.5, lambda: ('dodger in front of the attacker', _place_near(client, key, (205.0, 0.0)))),
        (TH - 0.05, mark_hit),
        (TH, lambda: ('hit', combat_tap(client, attack))),
        (TA - 0.05, run.at('A: the hit landed and the dodger is in hitstun', in_hitstun)),
        (TA, lambda: ('A: dodge in hitstun', combat_tap(dodger, dodge))),
        (TA + 1.0, _no_move(run, 'A: blocked in hitstun, nothing moved', TA - 0.05, TA + 1.0)),
        (TA + 1.0, _in_timeline_seen(run, 'A: no timeline started', TA, TA + 1.0, want=False)),
        (TB - 0.05, run.at('B: the hitstun is over', hitstun_over)),
        (TB, lambda: ('B: dodge after the hitstun', combat_tap(dodger, dodge))),
        (TB + 3.0, _landing_pair(run, st, 'B: free dodge', TB - 0.05, TB + 3.0, floor=200.0, tol=2.0)),
        (TB + 3.1, back_to_lane),
        (TB + 3.2, remark),
        (TC - 1.5, lambda: ('hitstun on, escape kit on', (_srv(key).server_set_hitstun(True), _srv(key).server_set_escape_kit(True)))),
        (TC, lambda: ('C: dodge in hitstun with the escape kit', combat_tap(dodger, dodge))),
        (TC + 3.0, _landing_pair(run, st, 'C: escape dodge in hitstun', TC - 0.05, TC + 3.0, floor=200.0, tol=2.0)),
        (TC + 3.1, cleanup),
        (TC + 3.2, run.at('the attacker kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (TC + 4.1, run.finish()),
    ]
    run.run(steps)


def scenario_kit_without_dodge(client=1):
    """C14: a kit without a dodge (the abilities are not granted; the command is bound but has nothing to start). A press of the dodge button does
    nothing and does not reserve the press: no request, no movement, and a dash pressed right after still works."""
    run = _Run('kit_without_dodge', client)
    key, st = run.key, {}
    dodge, sprint = button_mask('Dodge'), button_mask('Sprint')
    T = 2.0
    steps = [
        (0.3, lambda: ('grant dash and interact only', _kit(key, True, False, True))),
        (T, lambda: ('dodge (no such ability)', combat_tap(client, dodge))),
        (T + 1.0, _no_move(run, 'no dodge: nothing moved', T - 0.05, T + 1.0)),
        (T + 1.0, run.at('no request was made for the dodge press', lambda: (not run.new_bindings(T - 0.05, T + 1.0, 'srv') and not run.new_bindings(T - 0.05, T + 1.0, 'cli'),
                                                                              f'bindings srv {run.new_bindings(T - 0.05, T + 1.0, "srv")}, owner {run.new_bindings(T - 0.05, T + 1.0, "cli")}'))),
        (T + 1.2, lambda: ('dash right after', combat_tap(client, sprint))),
        (T + 4.0, _landing_pair(run, st, 'the dash still works', T + 1.15, T + 4.0, floor=100.0)),
        (T + 4.1, lambda: ('cleanup', (combat_clear(client), _kit(key, False, False, False)))),
        (T + 5.1, run.finish()),
    ]
    run.run(steps)


def scenario_interact_tap_hold(client=1):
    """C14 interact: the default movement slots give a tap = grab and a hold = pick up (the placeholder abilities own Test.State.Grab / PickUp).
    A: tap -> grab only. B: a press held 0.5 s -> pick up only. C: a press that is pending (no command yet) and released after 7 frames -> grab, so
    a pending hold never swallows the tap. Judged on the authority and the owner. (The swapped data set is a Core test: GSTNCombat.Core.MovementSlots.InteractTapHold.)"""
    run = _Run('interact_tap_hold', client)
    key = run.key
    interact = button_mask('Interact')
    TA, TB, TC = 2.0, 5.0, 8.0

    def tags(sd, t0, t1):
        return set().union(*[s[sd]['tags'] for s in run.sim if t0 <= s['t'] <= t1 and s.get(sd)] or [set()])

    def only(label, want, other, t0, t1):
        def f(sd):
            seen = tags(sd, t0, t1)
            return want in seen and other not in seen, f'seen {sorted(t for t in seen if t.startswith("Test.State.") and t[11:] in ("Grab", "PickUp"))} (want {want} only)'
        return run.at(label, lambda: _both(run, f))

    def pending():
        def f(sd):
            seen = tags(sd, TC + 0.02, TC + 0.09)
            return 'Test.State.Grab' not in seen and 'Test.State.PickUp' not in seen, 'no interact command while the press is pending'
        return _both(run, f)

    steps = [
        (0.3, lambda: ('grant the interact kit', _kit(key, False, False, True))),
        (TA, lambda: ('A: tap', combat_tap(client, interact))),
        (TA + 1.0, only('A: a tap is a grab', 'Test.State.Grab', 'Test.State.PickUp', TA, TA + 1.0)),
        (TB, lambda: ('B: press and hold', combat_hold(client, interact))),
        (TB + 0.5, lambda: ('B: release', combat_clear(client))),
        (TB + 1.5, only('B: a hold is a pick up', 'Test.State.PickUp', 'Test.State.Grab', TB, TB + 1.5)),
        (TC, lambda: ('C: press', combat_hold(client, interact))),
        (TC + 0.09, run.at('C: nothing fires while the press may still become a hold', pending)),
        (TC + 0.12, lambda: ('C: release after about 7 frames', combat_clear(client))),
        (TC + 1.2, only('C: the tap was not swallowed by the pending hold', 'Test.State.Grab', 'Test.State.PickUp', TC + 0.1, TC + 1.2)),
        (TC + 1.3, lambda: ('cleanup', (combat_clear(client), _kit(key, False, False, False)))),
        (TC + 2.3, run.finish()),
    ]
    run.run(steps)


# ---- C15 loadout and hitstop scenarios. The test kits / weapons are /Game/GASNPPTest/Combat/{KIT_Test_Alpha, KIT_Test_Beta, WPN_Test_*}
# (Tools/GAS/setup_gas_assets.py --combat-content). The loadout component is put on the PlayerState at run time by the test component
# (ServerApplyTestLoadout), so no shared Blueprint is edited. A loadout stays applied until the PIE session ends: run loadout_change LAST in a
# session (the kit stats override WalkSpeed / JumpVelocity of every scenario after it).

def _apply_loadout(key, kit, support=''):
    return _srv(key).server_apply_test_loadout(kit, support)


def _loadout_view(i, key):
    v = _view(i, key) or {}
    return v.get('loadout') or {}, v.get('combat') or {}, ((v.get('probe') or {}).get('stats') or {})


def scenario_loadout_change(client=1):
    """C15 kit changes by pickup (combat-m3 loadout direction): ServerApplyLoadoutChange on the authority, judged on the authority and the owner.
      A  Alpha: loadout revision 1 on both, same table checksum, the kit stats (WalkSpeed 600, no weapon effect) and the jump of the kit
      B  an attack in progress, then Beta: the timeline is cancelled at once, revision 2, WalkSpeed 900 (600 + the spear's 300), a higher jump
      C  a refused loadout (unknown kit; a primary weapon as the additional weapon) changes nothing: revision stays, the running state too
      D  Alpha + the support weapon: accepted, the kit stats stay (support weapons ship without a stats effect), the weapon is listed
    Reconciles: every accepted change is one out-of-band ability system change (like a grant), so they are listed, not judged."""
    run = _Run('loadout_change', client, judge_reconciles=False)
    key, st, apex, marks = run.key, {}, {}, {}
    # The loadout outlives a scenario (until the PIE session ends): the revisions are counted from the one the authority has at the start
    base = (_loadout_view(0, key)[0].get('revision') or 0)
    jump = button_mask('Jump')
    attack = button_mask('Attack')

    def apply(kit, support, label):
        def f():
            st[label] = _apply_loadout(key, kit, support)
            return label, st[label]
        return f

    def accepted(label):
        return run.at(f'{label}: accepted', lambda: (st.get(label) == 'OK', f'ServerApplyTestLoadout -> {st.get(label)}'))

    def refused(label):
        return run.at(f'{label}: refused with a finding', lambda: (st.get(label) not in (None, 'OK'), f'ServerApplyTestLoadout -> {st.get(label)}'))

    def state(label, kit, revision, walk, weapons=()):
        def f():
            out, ok, crcs = [], True, []
            for i, side in ((0, 'srv'), (client, 'cli')):
                lo, co, stats = _loadout_view(i, key)
                good = (lo.get('kit') == kit and lo.get('revision') == revision and co.get('loadoutRevision') == revision and lo.get('kitResolved')
                        and abs((stats.get('walkSpeedStat') or -1) - walk) < 0.5 and tuple(lo.get('additionalWeapons') or ()) == tuple(weapons))
                ok &= bool(good)
                crcs.append(lo.get('tableCrc'))
                out.append(f"{side}: kit {lo.get('kit')} rev {lo.get('revision')} (combat state {co.get('loadoutRevision')}) WalkSpeed {stats.get('walkSpeedStat')} weapons {lo.get('additionalWeapons')}")
            same = crcs[0] is not None and crcs[0] == crcs[1]
            return ok and same, '; '.join(out) + f' table checksums {crcs} (want equal; kit {kit}, revision {revision}, WalkSpeed {walk}, weapons {list(weapons)})'
        return run.at(label, f)

    def unchanged(label, kit, revision):
        def f():
            lo, co, _ = _loadout_view(0, key)
            return lo.get('kit') == kit and lo.get('revision') == revision, f"authority kit {lo.get('kit')} revision {lo.get('revision')} (want {kit}, {revision})"
        return run.at(label, f)

    def apexes():
        a, b = apex.get('alpha'), apex.get('beta')
        return a is not None and b is not None and b[0] > a[0] + 30.0 and abs(a[0] - a[1]) <= 1.0 and abs(b[0] - b[1]) <= 1.0, \
            f'apex Alpha {a}, Beta {b} (authority, owner; Beta must be > Alpha + 30 cm, owner within 1 cm)'

    def apex_step(label, t0, t1):
        def f():
            hs, _ = _apex_up(run, 'srv', t0 - 0.05, t0, t1)
            hc, _ = _apex_up(run, 'cli', t0 - 0.05, t0, t1)
            apex[label] = (hs, hc)
            return hs > 20.0, f'{label}: authority {hs:.1f} cm, owner {hc:.1f} cm'
        return run.at(f'{label}: a jump gives an apex', f)

    def timeline_cancelled():
        def f():
            out, ok = [], True
            for i, side in ((0, 'srv'), (client, 'cli')):
                v = _view(i, key) or {}
                co = v.get('combat') or {}
                ok &= not co.get('inTimeline')
                out.append(f"{side}: inTimeline {co.get('inTimeline')}")
            return ok, '; '.join(out)
        return run.at('B: the running timeline is gone on both after the change', f)

    def reconciles():
        r = report_reconciles(marks['start'], ignore_own_changes=False)
        by = r['unexplained_by_model']
        # three accepted changes: each is one out-of-band write of the combat state and of the ability system, so at most 3 of each model
        ok = by.get('combat', 0) <= 3 and by.get('asc', 0) <= 3 and by.get('mover', 0) == 0
        return ok, f"unexplained by model {by} (at most one combat and one asc reconcile per accepted change = 3 each, no Mover), forced={r['forced']}"

    def table_swapped(label, revision):
        """The simulation swapped to the loadout's table set: the combat state's table revision follows the demanded loadout revision on both"""
        def f():
            out, ok = [], True
            for i, side in ((0, 'srv'), (client, 'cli')):
                co = _loadout_view(i, key)[1]
                ok &= co.get('tableRevision') == revision and co.get('loadoutRevision') == revision
                out.append(f"{side}: table revision {co.get('tableRevision')} loadout revision {co.get('loadoutRevision')} bindings {co.get('tableBindings')}")
            return ok, '; '.join(out) + f' (want both {revision})'
        return run.at(label, f)

    def combat_table_crc(label):
        def f():
            crcs = [_loadout_view(i, key)[1].get('tableCrc') for i in (0, client)]
            return bool(crcs[0]) and crcs[0] == crcs[1], f'combat table checksums (authority, owner) {crcs}, want equal'
        return run.at(label, f)

    def dash_requests(label, t0, t1, want):
        """A press of the dash button: a request is made (want True) or none is, and the press is not reserved (want False)"""
        def f():
            srv, cli = run.new_bindings(t0, t1, 'srv'), run.new_bindings(t0, t1, 'cli')
            return (bool(srv) and bool(cli)) if want else (not srv and not cli), f'dash press: bindings authority {srv}, owner {cli} (want {"a request" if want else "none"})'
        return run.at(label, f)

    def gate_open():
        closed = _loadout_view(0, key)[1].get('tableGateClosed')
        return closed == 0, f'frames the authority ran with the table gate closed: {closed} (want 0)'

    sprint = button_mask('Sprint')
    steps = [
        (0.4, _mark(run, marks, 'start')),
        (0.5, lambda: ('clear a stale press', combat_clear(client))),
        (0.6, apply('KIT_Test_Alpha', '', 'A')),
        (1.8, accepted('A')),
        (1.9, state('A: Alpha applied on both (revision base+1, WalkSpeed 600, same table)', 'KIT_Test_Alpha', base + 1, 600.0)),
        (1.92, table_swapped('A: the simulation swapped to the loadout table (table revision == loadout revision)', base + 1)),
        (1.92, combat_table_crc('A: the combat table checksum is the same on the authority and the owner')),
        (1.95, lambda: ('A: dash press (Alpha has no dash command)', combat_tap(client, sprint))),
        (2.35, dash_requests('A: the dash command does not exist after Alpha: no request, no reserved press', 1.93, 2.35, False)),
        (2.5, lambda: ('A: jump', _jump_tap(client, 0.3))),
        (2.8, lambda: ('release', combat_clear(client))),
        (5.5, apex_step('alpha', 2.5, 5.5)),
        # B: an attack in progress, then the kit change
        (6.0, lambda: ('B: attack', combat_tap(client, attack))),
        (6.1, run.at('B: the attack is running', lambda: (any((_view(i, key) or {}).get('combat', {}).get('inTimeline') for i in (0, client)), 'inTimeline on the authority or the owner'))),
        (6.12, apply('KIT_Test_Beta', '', 'B')),
        (7.2, accepted('B')),
        (7.3, timeline_cancelled()),
        (7.4, state('B: Beta applied on both (revision base+2, WalkSpeed 900, same table)', 'KIT_Test_Beta', base + 2, 900.0)),
        (7.45, lambda: ('B: dash press (Beta has the dash)', combat_tap(client, sprint))),
        (7.85, dash_requests('B: after Beta the dash command exists: the press makes a request', 7.43, 7.85, True)),
        (8.0, lambda: ('B: jump', _jump_tap(client, 0.3))),
        (8.3, lambda: ('release', combat_clear(client))),
        (11.0, apex_step('beta', 8.0, 11.0)),
        (11.1, run.at('B: the Beta jump is higher', apexes)),
        # C: refusals change nothing
        (11.5, apply('KIT_Missing', '', 'C1')),
        (11.6, apply('KIT_Test_Beta', 'WPN_Test_Blade', 'C2')),
        (12.2, refused('C1')),
        (12.2, refused('C2')),
        (12.3, unchanged('C: nothing changed after the refusals', 'KIT_Test_Beta', base + 2)),
        # D: the support weapon
        (12.5, apply('KIT_Test_Alpha', 'WPN_Test_Support', 'D')),
        (13.8, accepted('D')),
        (13.9, state('D: Alpha + support weapon (revision base+3, WalkSpeed 600 kept)', 'KIT_Test_Alpha', base + 3, 600.0, ('WPN_Test_Support',))),
        (14.0, run.at('reconciles: one per accepted change and model', reconciles)),
        (14.05, run.at('the table gate stayed open on the authority', gate_open)),
        (14.1, run.finish()),
    ]
    run.run(steps)


def scenario_hitstop(client=1):
    """C15 hitstop (combat-m3 7.3): a hitstop window opened during a running attack pauses its timeline and the combat clock for exactly
    HitstopTestFrames (8) frames, the same on the authority and the owner, and the attack still finishes. The test component sets the
    combat component's HitstopTestButton to bit 20 (bound to no button), tapped as a scripted button. A: an attack without a hitstop gives the
    plain duration. B: the same attack with a hitstop 5 frames in lasts longer by the window, and the paused frames rise by exactly 7 on both (the window
    opened on frame F pauses from F+1)."""
    run = _Run('hitstop', client, judge_reconciles=False)
    key, seen = run.key, {}
    attack, stop = button_mask('Attack'), 1 << 20

    def paused(side_i):
        return ((_view(side_i, key) or {}).get('combat') or {}).get('pausedTotal')

    def span(label, t0, t1):
        def f():
            att = _tag_span(run, 'srv', TAG_ATTACKING, t0, t1)     # sim time: a duration of 7 frames must not depend on the editor tick
            seen[label] = (att[1] - att[0] + 1 / 60.0) if att else 0.0
            return bool(att), f'{label}: the attack was Attacking for {seen[label]:.3f} s of sim time on the authority'
        return run.at(f'{label}: the attack ran', f)

    def mark(label):
        def f():
            seen[label] = (paused(0), paused(client))
            return label, seen[label]
        return f

    def rose():
        a0, a1 = seen['before'], seen['after']
        d = (a1[0] - a0[0], a1[1] - a0[1])
        return d == (7, 7), f'paused frames rose by {d} on (authority, owner), want (7, 7): the window opened on frame F pauses from F+1, so 8 - 1 frames'

    def longer():
        a, b = seen.get('A'), seen.get('B')
        return a is not None and b is not None and b > a + 0.07, f'duration without {a} s, with {b} s (want at least 0.07 s longer: 7 paused frames are 0.117 s at 60 Hz)'

    steps = [
        (0.5, lambda: ('clear', combat_clear(client))),
        (1.0, lambda: ('A: attack', combat_tap(client, attack))),
        (2.5, span('A', 0.9, 2.5)),
        (3.0, mark('before')),
        (3.2, lambda: ('B: attack', combat_tap(client, attack))),
        (3.2 + 5 / 60.0, lambda: ('B: hitstop', combat_tap(client, stop))),
        (5.0, span('B', 3.1, 5.0)),
        (5.1, mark('after')),
        (5.2, run.at('B: the paused frames rose by exactly the window on both', rose)),
        (5.3, run.at('B: the hitstop attack lasted longer', longer)),
        (5.4, run.finish()),
    ]
    run.run(steps)


# ---- C18 table swap scenarios (loadout-table-swap-design.md, session 2). The kits are /Game/GASNPPTest/Combat/KIT_Test_{Gamma, Delta, Sigma, Omega}
# (Tools/GAS/setup_gas_assets.py --combat-content); each scenario applies its own kit first. A loadout stays applied after the scenario: they run
# before loadout_change, after hitstop (every kit binds the blade's attack and the platform jump, so the earlier press-driven scenarios still work).

def _c18_apply(key, st, label, kit, supers=''):
    def f():
        st[label] = _srv(key).server_apply_test_loadout_with_supers(kit, '', supers)
        return label, st[label]
    return f


def _c18_state(run, key, client, kit, revision, label, bindings=None):
    """The kit is the loadout on both machines, the simulation swapped to its table (table revision == loadout revision), same table checksum
    and binding count on both (`bindings`: and that count)."""
    def f():
        out, ok, crcs, counts = [], True, [], []
        for i, side in ((0, 'srv'), (client, 'cli')):
            lo, co, _ = _loadout_view(i, key)
            ok &= bool(lo.get('kit') == kit and lo.get('revision') == revision and co.get('loadoutRevision') == revision
                       and co.get('tableRevision') == revision and lo.get('kitResolved'))
            crcs.append(co.get('tableCrc'))
            counts.append(co.get('tableBindings'))
            out.append(f"{side}: kit {lo.get('kit')} rev {lo.get('revision')} table rev {co.get('tableRevision')} bindings {co.get('tableBindings')}")
        ok &= bool(crcs[0]) and crcs[0] == crcs[1] and counts[0] == counts[1] and (bindings is None or counts[0] == bindings)
        return ok, '; '.join(out) + f' table checksums {crcs} (want equal; kit {kit}, revision {revision}' + (f', {bindings} bindings)' if bindings is not None else ')')
    return run.at(label, f)


def _c18_accepted(run, st, label):
    return run.at(f'{label}: accepted', lambda: (st.get(label) == 'OK', f'ServerApplyTestLoadout -> {st.get(label)}'))


def _c18_both_saw(run, tag, t0, t1, label, want=True):
    """The authority and the owner owned `tag` somewhere in [t0, t1] (want) or neither did"""
    def f():
        s, c = run.saw_tag(tag, t0, t1, 'srv'), run.saw_tag(tag, t0, t1, 'cli')
        return (s and c) if want else (not s and not c), f'{tag}: authority {s}, owner {c} (want {"both" if want else "neither"})'
    return run.at(label, f)


def _c18_requests(run, t0, t1, label, count):
    """The same number of requests (`count`) on the authority and the owner in [t0, t1]"""
    def f():
        s, c = run.new_bindings(t0, t1, 'srv'), run.new_bindings(t0, t1, 'cli')
        return s == c and len(s) == count, f'request bindings authority {s}, owner {c} (want equal, {count})'
    return run.at(label, f)


def _c18_meters(key, i):
    st = ((_view(i, key) or {}).get('probe') or {}).get('stats') or {}
    return st.get('exMeter'), st.get('superMeter'), st.get('hasMeterSet')


def _c18_meter_is(run, key, client, field, want, label):
    """ExMeter (field 0) or SuperMeter (field 1) equals `want` on the authority and the owner"""
    def f():
        got = [_c18_meters(key, i)[field] for i in (0, client)]
        return all(g is not None and abs(g - want) < 0.5 for g in got), f'{"ex" if field == 0 else "super"} meter authority/owner {got}, want {want}'
    return run.at(label, f)


def _c18_reconciles(marks, max_combat, max_asc):
    def f():
        r = report_reconciles(marks['start'], ignore_own_changes=False)
        by = r['unexplained_by_model']
        return by.get('combat', 0) <= max_combat and by.get('asc', 0) <= max_asc and by.get('mover', 0) == 0, \
            f"unexplained by model {by} (at most combat {max_combat}, asc {max_asc}, no Mover), forced={r['forced']}"
    return f


def _c18_gate_open(key):
    closed = _loadout_view(0, key)[1].get('tableGateClosed')
    return closed == 0, f'frames the authority ran with the table gate closed: {closed} (want 0)'


def scenario_command_set(client=1):
    """C18 command set (design session 2, item 1): KIT_Test_Gamma removes the dash command from the shared set and adds Command.Test.Gamma on the
    same button (Sprint). Judged on the authority and the owner: the press starts the added command and nothing else (no dash, no movement, one request
    on both), the attack pressed after it is not swallowed, a forced reconcile across the press changes nothing, and the same press under Beta
    (which binds the dash) dashes and starts no added command. Reconciles: one combat and one ability system change per accepted loadout (2)."""
    run = _Run('command_set', client, judge_reconciles=False)
    key, st, marks, store = run.key, {}, {}, {}
    base = (_loadout_view(0, key)[0].get('revision') or 0)
    sprint, attack = button_mask('Sprint'), button_mask('Attack')
    steps = [
        (0.3, _mark(run, marks, 'start')),
        (0.4, lambda: ('clear', combat_clear(client))),
        (0.6, _c18_apply(key, st, 'G', 'KIT_Test_Gamma')),
        (1.8, _c18_accepted(run, st, 'G')),
        (1.9, _c18_state(run, key, client, 'KIT_Test_Gamma', base + 1, 'Gamma applied on both, same table')),
        (2.0, lambda: ('Gamma: press the removed dash button', combat_tap(client, sprint))),
        (2.8, _c18_both_saw(run, 'Test.State.GammaMove', 1.95, 2.8, 'Gamma: the press started the added command on both')),
        (2.8, _c18_both_saw(run, TAG_DASHING, 1.95, 2.8, 'Gamma: no dash', want=False)),
        (2.8, _no_move(run, 'Gamma: nothing moved', 1.95, 2.8)),
        (2.8, _c18_requests(run, 1.95, 2.8, 'Gamma: exactly one request, the added command, on both', 1)),
        (3.0, lambda: ('Gamma: attack right after (not swallowed by the removed command)', combat_tap(client, attack))),
        (3.8, _c18_both_saw(run, TAG_ATTACKING, 2.95, 3.8, 'Gamma: the attack came out on both')),
        (4.2, lambda: ('Gamma: press again, with a forced reconcile across it', combat_tap(client, sprint))),
        (4.22, lambda: ('force reconcile', force_reconcile(client, 12))),
        (5.2, _c18_both_saw(run, 'Test.State.GammaMove', 4.15, 5.2, 'Gamma: the second press started the added command on both')),
        (5.2, _c18_requests(run, 4.15, 5.2, 'Gamma: one request for the second press on both', 1)),
        (5.4, _c18_apply(key, st, 'B', 'KIT_Test_Beta')),
        (6.6, _c18_accepted(run, st, 'B')),
        (6.7, _c18_state(run, key, client, 'KIT_Test_Beta', base + 2, 'Beta applied on both, same table')),
        (6.8, lambda: ('Beta: the same press', combat_tap(client, sprint))),
        (8.0, _c18_both_saw(run, 'Test.State.GammaMove', 6.75, 8.0, 'Beta: no added command', want=False)),
        (8.0, _landing_pair(run, store, 'Beta: the same press dashes', 6.75, 8.0, floor=100.0)),
        (8.0, _c18_requests(run, 6.75, 8.0, 'Beta: one request (the dash) on both', 1)),
        (8.1, run.at('reconciles: one combat and one asc per accepted change', _c18_reconciles(marks, 2, 2))),
        (8.15, run.at('the table gate stayed open on the authority', lambda: _c18_gate_open(key))),
        (8.2, lambda: ('cleanup', combat_clear(client))),
        (8.3, run.finish()),
    ]
    run.run(steps)


def scenario_ex_meter(client=1):
    """C18 EX meter (design session 2, item 3): KIT_Test_Delta has a plain special (Press Special) and an EX chord (Special + Buff, priority +100,
    falls through when unavailable) whose ability costs 50 ExMeter. A: no meter, the EX chord gives the plain special and no EX, meter untouched.
    B: after a server grant of 60, the chord gives the EX and spends 50 (10 left). C: 10 left is short again, the chord falls through to the plain
    special and the meter stays. Always the same on the authority and the owner."""
    run = _Run('ex_meter', client, judge_reconciles=False)
    key, st, marks = run.key, {}, {}
    base = (_loadout_view(0, key)[0].get('revision') or 0)
    chord = button_mask('Special') | button_mask('Buff')
    plain, ex = 'Test.State.Special', 'Test.State.SpecialEx'

    def grant():
        st['grant'] = _srv(key).server_grant_meter(0)
        return 'grant 60 ExMeter', st['grant']

    steps = [
        (0.3, _mark(run, marks, 'start')),
        (0.4, lambda: ('clear', combat_clear(client))),
        (0.6, _c18_apply(key, st, 'D', 'KIT_Test_Delta')),
        (1.8, _c18_accepted(run, st, 'D')),
        (1.9, _c18_state(run, key, client, 'KIT_Test_Delta', base + 1, 'Delta applied on both, same table')),
        (1.95, run.at('the pawn has the meter set and an empty ExMeter', lambda: (all(_c18_meters(key, i)[2] and abs(_c18_meters(key, i)[0]) < 0.5 for i in (0, client)),
                                                                                  f'meters authority {_c18_meters(key, 0)}, owner {_c18_meters(key, client)}'))),
        (2.0, lambda: ('A: the EX chord without meter', combat_tap(client, chord))),
        (2.9, _c18_both_saw(run, plain, 1.95, 2.9, 'A: the plain special came out on both')),
        (2.9, _c18_both_saw(run, ex, 1.95, 2.9, 'A: no EX', want=False)),
        (2.9, _c18_requests(run, 1.95, 2.9, 'A: one request on both (the fall-through, not two)', 1)),
        (2.9, _c18_meter_is(run, key, client, 0, 0.0, 'A: ExMeter untouched')),
        (3.2, grant),
        (4.0, run.at('B: the grant landed (60) on both', lambda: (st.get('grant') is True and all(abs((_c18_meters(key, i)[0] or -1) - 60.0) < 0.5 for i in (0, client)),
                                                                f'grant {st.get("grant")}, meters authority {_c18_meters(key, 0)}, owner {_c18_meters(key, client)}'))),
        (4.1, lambda: ('B: the EX chord with 60', combat_tap(client, chord))),
        (5.0, _c18_both_saw(run, ex, 4.05, 5.0, 'B: the EX came out on both')),
        (5.0, _c18_both_saw(run, plain, 4.05, 5.0, 'B: not the plain special', want=False)),
        (5.0, _c18_requests(run, 4.05, 5.0, 'B: one request on both', 1)),
        (5.0, _c18_meter_is(run, key, client, 0, 10.0, 'B: the EX spent 50 on both')),
        (5.3, lambda: ('C: the EX chord with 10 left', combat_tap(client, chord))),
        (6.2, _c18_both_saw(run, plain, 5.25, 6.2, 'C: the plain special again on both')),
        (6.2, _c18_both_saw(run, ex, 5.25, 6.2, 'C: no EX', want=False)),
        (6.2, _c18_meter_is(run, key, client, 0, 10.0, 'C: the meter stayed at 10')),
        (6.3, run.at('reconciles: loadout change and grant (combat 1, asc 2)', _c18_reconciles(marks, 1, 2))),
        (6.35, run.at('the table gate stayed open on the authority', lambda: _c18_gate_open(key))),
        (6.4, lambda: ('cleanup', combat_clear(client))),
        (6.5, run.finish()),
    ]
    run.run(steps)


def scenario_super_code(client=1):
    """C18 super code (design session 2, item 2): KIT_Test_Sigma has two supers entered as codes (PerSuperCode: Special, then Dodge+Interact for A,
    Interact+Dodge for B), 100 SuperMeter each. Wrong code with meter: nothing, meter kept. Code A: super A, meter spent. Code B with the meter
    empty: refused, nothing. After a grant, code B with a forced reconcile between its buttons: the history is sync state, so the code resumes and
    super B comes out. Every result identical on the authority and the owner."""
    run = _Run('super_code', client, judge_reconciles=False)
    key, st, marks = run.key, {}, {}
    base = (_loadout_view(0, key)[0].get('revision') or 0)
    special, dodge, interact = button_mask('Special'), button_mask('Dodge'), button_mask('Interact')
    A, B = 'Test.State.SuperA', 'Test.State.SuperB'

    def grant(label):
        def f():
            st[label] = _srv(key).server_grant_meter(1)
            return label, st[label]
        return f

    def code(t, buttons, tag, reconcile_after=None):
        """Steps that enter special + buttons from time t, 0.15 s apart"""
        out = [(t, lambda: (f'{tag}: special (code entry)', combat_tap(client, special)))]
        for n, mask in enumerate(buttons, 1):
            out.append((t + 0.15 * n, lambda m=mask: ('code button', combat_tap(client, m))))
            if reconcile_after == n:
                out.append((t + 0.15 * n + 0.02, lambda: ('force reconcile mid-code', force_reconcile(client, 12))))
        return out

    steps = [
        (0.3, _mark(run, marks, 'start')),
        (0.4, lambda: ('clear', combat_clear(client))),
        (0.6, _c18_apply(key, st, 'S', 'KIT_Test_Sigma', 'SUP_Test_A,SUP_Test_B')),
        (1.8, _c18_accepted(run, st, 'S')),
        (1.9, _c18_state(run, key, client, 'KIT_Test_Sigma', base + 1, 'Sigma applied on both, same table (jump, the blade attack, two supers = 4 bindings)', bindings=4)),
        (2.0, grant('g1')),
        (3.0, _c18_meter_is(run, key, client, 1, 100.0, 'the first grant landed: SuperMeter 100 on both')),
        # wrong code: neither super's pattern
        *code(3.2, [interact, interact], 'wrong code', None),
        (4.4, _c18_both_saw(run, A, 3.1, 4.4, 'wrong code: no super A', want=False)),
        (4.4, _c18_both_saw(run, B, 3.1, 4.4, 'wrong code: no super B', want=False)),
        (4.4, _c18_meter_is(run, key, client, 1, 100.0, 'wrong code: meter kept')),
        # code A
        *code(5.0, [dodge, interact], 'code A', None),
        (6.2, _c18_both_saw(run, A, 4.95, 6.2, 'code A: super A came out on both')),
        (6.2, _c18_both_saw(run, B, 4.95, 6.2, 'code A: not super B', want=False)),
        (6.2, _c18_requests(run, 4.95, 6.2, 'code A: one request on both', 1)),
        (6.2, _c18_meter_is(run, key, client, 1, 0.0, 'code A: the cost was spent (0 left) on both')),
        # code B with the meter empty
        *code(7.0, [interact, dodge], 'code B, no meter', None),
        (8.2, _c18_both_saw(run, B, 6.95, 8.2, 'code B without meter: refused, no super B', want=False)),
        (8.2, _c18_meter_is(run, key, client, 1, 0.0, 'code B without meter: meter still 0')),
        # grant, code B with a forced reconcile between its buttons
        (8.4, grant('g2')),
        (9.2, _c18_meter_is(run, key, client, 1, 100.0, 'the second grant landed: SuperMeter 100 on both')),
        *code(9.4, [interact, dodge], 'code B, reconcile mid-code', 1),
        (10.8, _c18_both_saw(run, B, 9.35, 10.8, 'code B with a reconcile mid-code: super B came out on both')),
        (10.8, _c18_meter_is(run, key, client, 1, 0.0, 'code B: the cost was spent on both')),
        (10.9, run.at('reconciles: loadout change and two grants (combat 1, asc 3)', _c18_reconciles(marks, 1, 3))),
        (10.95, run.at('the table gate stayed open on the authority', lambda: _c18_gate_open(key))),
        (11.0, lambda: ('cleanup', combat_clear(client))),
        (11.1, run.finish()),
    ]
    run.run(steps)


def scenario_wait_frames(client=1):
    """C18 wait frames (design session 2, item 4; the C11 leftover): KIT_Test_Omega binds a timeline ability (Command.Special.Neutral) that waits 20
    combat frames and then strikes (Test.State.Strike). The timeline is in the registry of the swapped table, not the Blueprint's. A: no hitstop,
    the strike comes 20 frames after the start. B: a hitstop window (8 frames, the M0 debug button) opened 5 frames into the wait, with a forced
    reconcile inside the wait: the strike comes 7 frames later than in A (the window opened on frame F pauses from F+1). The same on both machines."""
    run = _Run('wait_frames', client, judge_reconciles=False)
    key, st, marks, delay = run.key, {}, {}, {}
    base = (_loadout_view(0, key)[0].get('revision') or 0)
    special, stop = button_mask('Special'), 1 << 20
    frame_ms = 1000.0 / 60.0

    def first_ms(side, tag, t0, t1):
        ms = [s[side]['ms'] for s in run.sim if t0 <= s['t'] <= t1 and s.get(side) and tag in s[side]['tags']]
        return min(ms) if ms else None

    def strike_delay(label, t0, t1):
        def f():
            out, ok = [], True
            for side in ('srv', 'cli'):
                a, b = first_ms(side, 'Test.State.Windup', t0, t1), first_ms(side, 'Test.State.Strike', t0, t1)
                d = (b - a) if a is not None and b is not None else None
                delay.setdefault(label, {})[side] = d
                ok &= d is not None
                out.append(f'{side}: windup at {a} ms, strike at {b} ms, delay {d}')
            return ok, '; '.join(out)
        return run.at(f'{label}: the windup ran and the strike came, on both', f)

    def delays():
        a, b = delay.get('A', {}), delay.get('B', {})
        if None in (a.get('srv'), a.get('cli'), b.get('srv'), b.get('cli')):
            return False, f'delays A {a}, B {b}'
        want_a, want_b = 20 * frame_ms, 27 * frame_ms
        ok = all(abs(a[s] - want_a) <= 2 * frame_ms for s in a) and all(abs(b[s] - want_b) <= 2 * frame_ms for s in b) \
            and all(abs((b[s] - a[s]) - 7 * frame_ms) <= 1.01 * frame_ms for s in a)
        return ok, f'delays A {a}, B {b} ms; want A ~{want_a:.0f} (20 frames), B ~{want_b:.0f} (27 frames), B - A = 7 frames ({7 * frame_ms:.0f} ms), each within a frame or two'

    steps = [
        (0.3, _mark(run, marks, 'start')),
        (0.4, lambda: ('clear', combat_clear(client))),
        (0.6, _c18_apply(key, st, 'O', 'KIT_Test_Omega')),
        (1.8, _c18_accepted(run, st, 'O')),
        (1.9, _c18_state(run, key, client, 'KIT_Test_Omega', base + 1, 'Omega applied on both, same table')),
        (2.0, lambda: ('A: the wait-strike timeline', combat_tap(client, special))),
        (3.6, strike_delay('A', 1.95, 3.6)),
        (3.6, _c18_requests(run, 1.95, 3.6, 'A: one request on both', 1)),
        (5.0, lambda: ('B: the wait-strike timeline', combat_tap(client, special))),
        (5.0 + 5 / 60.0, lambda: ('B: hitstop 5 frames into the wait', combat_tap(client, stop))),
        (5.0 + 8 / 60.0, lambda: ('B: force reconcile inside the wait', force_reconcile(client, 10))),
        (6.9, strike_delay('B', 4.95, 6.9)),
        (6.9, run.at('the hitstop delayed the strike by exactly the window (7 frames), on both', delays)),
        (7.0, run.at('reconciles: the loadout change and the forced one (combat 1, asc 1)', _c18_reconciles(marks, 1, 1))),
        (7.05, run.at('the table gate stayed open on the authority', lambda: _c18_gate_open(key))),
        (7.1, lambda: ('cleanup', combat_clear(client))),
        (7.2, run.finish()),
    ]
    run.run(steps)


# ---- M4 H7 hitstop and freeze scenarios (combat-m4-hits 8). The debug hitstop button now opens the real window: the combat tick adds the
# paused frames, pauses a playing synced montage, and asks the ability system for a Mover freeze (FGSTN_LayeredMove_Freeze) as long as the window.
MONTAGE_BENCH = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Bench/M_interaction_bench_into_BR_LFoot_Montage'


def _set_hitstop_debug(client, frames, montage=None):
    """Window length and montage of the debug hitstop button, on the authority's copy and the owner's copy of the client's pawn (UFUNCTION setters:
    a live component must not be edited with set_editor_property). montage None clears it."""
    key = key_of_player(client)
    for wi in sorted({0, client}):
        c = comps(worlds()[wi]).get(key)
        if c is None:
            continue
        combat = c.get_owner().get_component_by_class(unreal.GSTN_CombatSimComponent)
        combat.set_hitstop_test_frames(frames)
        combat.set_hitstop_test_montage(montage)


def _montage_info(wi, key):
    """(playing, time, latestFrame, hitstopStart, hitstopEnd) of the synced montage on world wi's copy of the pawn, or None"""
    c = comps(worlds()[wi]).get(key)
    if c is None:
        return None
    p = c.get_owner().get_component_by_class(unreal.GSTN_CombatSimComponent).get_hitstop_montage_info().split(',')
    return int(p[0]), float(p[1]), int(p[2]), int(p[3]), int(p[4])


def scenario_hitstop_montage(client=1, frames=40):
    """H0c as a scenario: the debug hitstop button starts a synced montage on the frame it opens a window of `frames` frames. The montage holds
    through the window and resumes after it; a reconcile forced inside the window leaves the montage time of the owner equal to the authority's
    for every frame both have simulated (the per-frame trace of the combat component, GetHitstopMontageTrace; the window itself is read per editor tick)."""
    run = _Run('hitstop_montage', client, judge_reconciles=False)
    key, rec, h = run.key, [], {'h': None}
    stop = 1 << 20

    def sample(_dt):
        try:
            rec.append((run.now(), _montage_info(0, key), _montage_info(client, key)))
        except Exception:
            pass

    def stop_sampling():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        return 'sampling stopped'

    def begin():
        _set_hitstop_debug(client, frames, unreal.load_asset(MONTAGE_BENCH))
        h['h'] = unreal.register_slate_post_tick_callback(sample)
        return 'montage set'

    def trace(wi):
        """{frame: montage time} the combat component recorded for every simulated frame (a re-simulated frame overwrites its entry)"""
        c = comps(worlds()[wi]).get(key)
        text = c.get_owner().get_component_by_class(unreal.GSTN_CombatSimComponent).get_hitstop_montage_trace()
        return {int(f): float(t) for f, t in (e.split(':') for e in text.split(';') if e)}

    def window():
        for _, a, _b in rec:
            if a and a[3] != -1 and a[4] > a[3] and a[0]:
                return a[3], a[4]
        return None

    def both_played():
        w = window()
        return w is not None, f'authority window {w}'

    def equal():
        a, b = trace(0), trace(client)
        common = sorted(set(a) & set(b))
        worst = max((abs(a[f] - b[f]) for f in common), default=1e9)
        return len(common) >= 40 and worst <= 0.001, f'{len(common)} frames compared, max difference {worst:.4f} s'

    def held():
        w, a = window(), trace(0)
        if w is None:
            return False, 'no window'
        inside = [a[f] for f in sorted(a) if w[0] < f < w[1]]
        after = [a[f] for f in sorted(a) if f >= w[1] + 3]
        ok = len(inside) >= 20 and max(inside) - min(inside) <= 0.001 and bool(after) and max(after) > max(inside) + 0.03
        return ok, f'window {w}: {len(inside)} frames inside, span {(max(inside) - min(inside)) if inside else -1:.4f} s, after the window up to {max(after) if after else -1:.3f} s'

    def never_back():
        bad = []
        for name, d in (('authority', trace(0)), ('owner', trace(client))):
            ts = [d[f] for f in sorted(d) if d[f] >= 0]
            bad += [name for x, y in zip(ts, ts[1:]) if y < x - 0.0005]
        return not bad, f'{len(bad)} backward steps ({set(bad)}) in the authority and owner montage times'

    steps = [
        (0.3, lambda: ('clear', combat_clear(client))),
        (0.5, begin),
        (1.0, lambda: ('hitstop + montage', combat_tap(client, stop))),
        (1.0 + 0.25, lambda: ('force reconcile inside the window', force_reconcile(client, 15))),
        (1.0 + frames / 60.0 + 1.0, stop_sampling),
        (1.0 + frames / 60.0 + 1.2, run.at('the window opened with a montage playing', both_played)),
        (1.0 + frames / 60.0 + 1.2, run.at('the montage held through the window and resumed after it (authority)', held)),
        (1.0 + frames / 60.0 + 1.2, run.at('owner and authority agree on the montage time per frame after the forced reconcile', equal)),
        (1.0 + frames / 60.0 + 1.2, run.at('the montage time never went back', never_back)),
        (1.0 + frames / 60.0 + 1.4, lambda: ('restore', _set_hitstop_debug(client, 8, None))),
        (1.0 + frames / 60.0 + 1.5, run.finish()),
    ]
    run.run(steps)


def scenario_hit_on_wall(client=1, wall_x=1010.0, wall_y=-880.0, frames=90):
    """First half of the wall scenario of the hits milestone: a hitstop freeze while the pawn is on the wall. The pawn is put in front of the wall
    (the geometry of scenario_jump_wall), jumps at it and, as soon as it is WallSliding, the debug window of `frames` frames opens. In the window
    the stick is held away from the wall and jump is pressed: without the wall lock that is a pull-away detach and a wall jump. Wanted: the pawn stays
    WallSliding and does not move through the window on both machines, is still WallSliding after it, and both copies stand in the same place."""
    run = _Run('hit_on_wall', client, judge_reconciles=False)
    wm = wall_mode()
    stop, ia_move = 1 << 20, unreal.load_asset(IA['move'])
    st, h, win = {'phase': 'idle', 'tw': None, 'reached': False}, {'h': None}, frames / 60.0

    def setup():
        _set_hitstop_debug(client, frames, None)
        c = comps(worlds()[0])[run.key]
        sp = c.get_spawn_location()
        c.server_reset_to_spawn(unreal.Vector(680.0 - sp.x, wall_y - sp.y, 0.0))
        return 'in front of the wall'

    def finish_driver(comp):
        comp.stop_holding_input(ia_move)
        st['phase'] = 'done'
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None

    def drive(_dt):
        try:
            now = run.now()
            _, pawn = _local_pawn(client)
            comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
            mode = str(pawn.get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name())
            x, ph = pawn.get_actor_location().x, st['phase']
            into = (wall_x + 200.0, wall_y)
            if ph == 'walk':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                if wall_x - x < 130.0 and mode == 'Walking':
                    press(client, 'jump')
                    st.update(phase='air', t1=now)
                elif now - st['t1'] > 6.0:
                    finish_driver(comp)
            elif ph == 'air':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                if mode == wm:
                    st.update(phase='window', tw=now, reached=True)
                    combat_tap(client, stop)
                elif now - st['t1'] > 2.5:
                    finish_driver(comp)
            elif ph == 'window':
                dt = now - st['tw']
                if dt > 0.4:
                    comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, (wall_x - 400.0, wall_y))))     # away from the wall
                if dt > 0.7 and not st.get('jumped'):
                    st['jumped'] = True
                    press(client, 'jump')
                if dt > win + 0.1:
                    comp.stop_holding_input(ia_move)
                    st['phase'] = 'after'
            elif ph == 'after':
                if now - st['tw'] > win + 0.8:
                    finish_driver(comp)
        except Exception as e:
            unreal.log_error(f'GASNPP hit_on_wall driver: {e}')
            st['phase'] = 'done'

    def start_driver():
        st.update(phase='walk', t1=run.now())
        h['h'] = unreal.register_slate_post_tick_callback(drive)
        return 'driving'

    def locs(side, t0, t1):
        return [s['loc'] for s in run._win(side, t0, t1, sim=True)]

    def spread(pts):
        return max((sum((a - b) ** 2 for a, b in zip(p, q)) ** 0.5 for p in pts for q in pts), default=1e9) if pts else 1e9

    def on_wall():
        return st['reached'], f"reached the wall: {st['reached']}"

    def held_mode():
        tw = st['tw']
        if tw is None:
            return False, 'never on the wall'
        out, ok = [], True
        for side in ('srv', 'cli'):
            modes = [s['mode'] for s in run._win(side, tw + 0.45, tw + win - 0.1, sim=True)]
            good = bool(modes) and all(m == wm for m in modes)
            ok &= good
            out.append(f'{side}: {len(modes)} samples, modes {sorted(set(modes))}')
        return ok, '; '.join(out)

    def held_still():
        tw = st['tw']
        if tw is None:
            return False, 'never on the wall'
        a, b = spread(locs('srv', tw + 0.45, tw + win - 0.1)), spread(locs('cli', tw + 0.45, tw + win - 0.1))
        return a < 3.0 and b < 6.0, f'spread of the position in the window: authority {a:.1f} cm, owner {b:.1f} cm'

    def still_on_wall_after():
        tw = st['tw']
        if tw is None:
            return False, 'never on the wall'
        out, ok = [], True
        for side in ('srv', 'cli'):
            modes = [s['mode'] for s in run._win(side, tw + win + 0.05, tw + win + 0.5, sim=True)]
            ok &= bool(modes) and all(m == wm for m in modes)
            out.append(f'{side}: modes after the window {sorted(set(modes))}')
        return ok, '; '.join(out)

    def same_place():
        tw = st['tw']
        if tw is None:
            return False, 'never on the wall'
        a, b = locs('srv', tw + win - 0.3, tw + win - 0.1), locs('cli', tw + win - 0.3, tw + win - 0.1)
        d = sum((x - y) ** 2 for x, y in zip(a[-1], b[-1])) ** 0.5 if a and b else 1e9
        return d < 6.0, f'authority {a[-1] if a else None}, owner {b[-1] if b else None}: {d:.1f} cm apart'

    t_end = 1.5 + 6.0 + win + 1.5
    steps = [
        (0.3, lambda: ('clear', combat_clear(client))),
        (0.5, setup),
        (2.5, start_driver),
        (t_end, lambda: ('restore', _set_hitstop_debug(client, 8, None))),
        (t_end, run.at('the pawn reached the wall', on_wall)),
        (t_end, run.at('WallSliding through the whole window on both, away-stick and jump press included', held_mode)),
        (t_end, run.at('the pawn did not move in the window', held_still)),
        (t_end, run.at('still WallSliding after the window', still_on_wall_after)),
        (t_end, run.at('both copies stand in the same place at the end of the window', same_place)),
        (t_end + 0.3, run.finish()),
    ]
    run.run(steps)



def scenario_hit_on_wall_hit(client=1, wall_x=1010.0, wall_y=-880.0, tap_delay=1.1):
    """Second half of the wall scenario of the hits milestone (M4 H9c): a real hit while the pawn is wall sliding. `client` is the climber, the other
    client the attacker: it stands on the floor beside the wall (clear of the climber's path) and spins the moment the climber is WallSliding. The spin is a
    sphere around the attacker, so it reaches the sliding pawn from below. Wanted, on the authority and the owner: the hit lands once on the climber while it
    is WallSliding; its hitstop window opens on the frame of the hit and, through the window, the pawn stays WallSliding and does not move; the tags
    Combat.State.Hitstop / Hitstun are projected; the damage of the hit is taken on both copies; and the two copies agree on the pawn's place at the end of
    the window. What the wall pawn does after its hitstun is reported (the stagger in place keeps it where it is)."""
    att = 2 if client == 1 else 1
    run = _Run('hit_on_wall_hit', client, judge_reconciles=False)
    wm = wall_mode()
    key, akey = run.key, key_of_player(att)
    ia_move, attack = unreal.load_asset(IA['move']), button_mask('Attack')
    st, h = {'phase': 'idle', 'tw': None, 'reached': False, 'rec': []}, {'h': None, 'v': None}

    def vsampler(_dt):
        try:
            st['rec'].append((run.now(), victim_info(0, key), victim_info(client, key)))
        except Exception:
            pass

    def setup():
        hit_rules_setup()
        _hit_logging(True)
        st['kit'] = _apply_loadout(akey, HIT_KIT)
        h['v'] = unreal.register_slate_post_tick_callback(vsampler)
        c = comps(worlds()[0])[key]
        sp = c.get_spawn_location()
        c.server_reset_to_spawn(unreal.Vector(680.0 - sp.x, wall_y - sp.y, 0.0))
        a = comps(worlds()[0])[akey]
        sa = a.get_spawn_location()
        a.server_reset_to_spawn(unreal.Vector(wall_x - 50.0 - sa.x, wall_y + 76.0 - sa.y, 0.0))
        return 'climber in front of the wall, attacker beside it'

    def finish_driver(comp):
        comp.stop_holding_input(ia_move)
        st['phase'] = 'done'
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None

    def drive(_dt):
        try:
            now = run.now()
            _, pawn = _local_pawn(client)
            comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
            mode = str(pawn.get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name())
            x, ph = pawn.get_actor_location().x, st['phase']
            into = (wall_x + 200.0, wall_y)
            if ph == 'walk':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                if wall_x - x < 130.0 and mode == 'Walking':
                    press(client, 'jump')
                    st.update(phase='air', t1=now)
                elif now - st['t1'] > 6.0:
                    finish_driver(comp)
            elif ph == 'air':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                if mode == wm:
                    st.update(phase='hit', tw=now, reached=True)
                elif now - st['t1'] > 2.5:
                    finish_driver(comp)
            elif ph == 'hit':
                # straight into the wall from where the pawn is: a fixed vector would slide it along the wall
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, (wall_x + 200.0, pawn.get_actor_location().y))))
                if now - st['tw'] > tap_delay - 0.6 and 'moved' not in st:
                    # The pawn drifts along the wall while it slides: put the attacker where it will be when the spin opens (0.33 s of the move plus
                    # the latency after the tap), on the side it drifts toward so the two capsules never touch on the way
                    st['moved'] = True
                    v = snap(worlds()[0])[key]
                    horizon = 0.6 + 0.5
                    px, py = v['location'][0] + v['velocity'][0] * horizon, v['location'][1] + v['velocity'][1] * horizon
                    a = comps(worlds()[0])[akey]
                    sa = a.get_spawn_location()
                    side = -1.0 if v['velocity'][1] < -5.0 else 1.0
                    a.server_reset_to_spawn(unreal.Vector(wall_x - 55.0 - sa.x, py + side * 76.0 - sa.y, 0.0))
                # Tap on the way down, a little after the top of the rise, and at the latest at tap_delay. The v2 slide keeps the entry momentum
                # (wall-slide-v2.md 6.2): the climber rises to about 375 cm and is back on the floor about 0.8 s after the top, so the fixed 1.1 s tap
                # of the legacy slide put the spin on the landing. From the tap the spin opens about 30 frames later on the authority (20 frames of the
                # move plus the attacker's latency), and the lag-compensated hit sees the climber where the attacker saw it, some frames earlier
                # (higher): at 200 cm/s down (about 20 cm under the top) the spin falls in the middle of the frames that are both in its reach as seen
                # and still on the wall. The legacy slide comes down slower and taps at tap_delay
                vz = snap(worlds()[0])[key]['velocity'][2]
                if 'moved' in st and 'F' not in st and (now - st['tw'] > tap_delay or (now - st['tw'] > 0.6 and vz < -200.0)):
                    st['F'] = _latest_frame(0, akey)
                    st['d0'] = (damage_of(0, key), damage_of(client, key))
                    v = snap(worlds()[0])
                    st['p_tap'] = (tuple(round(x) for x in v[key]['location']), tuple(round(x) for x in v[akey]['location']))
                    combat_tap(att, attack)
                if now - st['tw'] > tap_delay + 0.3 and 'p_late' not in st:
                    v = snap(worlds()[0])
                    st['p_late'] = (tuple(round(x) for x in v[key]['location']), tuple(round(x) for x in v[akey]['location']), sorted(t for t in v[akey].get('tags', []) if 'Test' in t))
                if now - st['tw'] > 3.0:
                    finish_driver(comp)
        except Exception as e:
            unreal.log_error(f'GASNPP hit_on_wall_hit driver: {e}')
            st['phase'] = 'done'

    def start_driver():
        st.update(phase='walk', t1=run.now())
        h['h'] = unreal.register_slate_post_tick_callback(drive)
        return 'driving'

    def hits():
        return [r for r in _hit_log(0, akey) if r[0] >= st.get('F', 10 ** 9)]

    def win():
        hs = hits()
        if len(hs) != 1:
            return None
        auth, own = _victim_windows(st['rec'], 1, hs[0][0])
        return hs[0][0], auth, own

    def landed_on_wall():
        hs, w = hits(), win()
        if not st['reached'] or len(hs) != 1 or not w:
            return False, f"reached the wall {st['reached']}, hits {hs}, (climber, attacker) at the tap {st.get('p_tap')}, 0.3 s later {st.get('p_late')}"
        t0 = st['tw']
        modes = {sd: sorted({s['mode'] for s in run._win(sd, t0, t0 + 0.2, sim=True)}) for sd in ('srv', 'cli')}
        return hs[0][2] == 3, f'hit {hs[0]}, climber modes right after the attack {modes}'

    def window_on_wall():
        w = win()
        if not w:
            return False, f'hits {hits()}'
        fr, auth, own = w
        ok = len(auth) == 1 and auth[0][0] == fr and auth[0][1] - auth[0][0] >= HIT_ATTACKER_HITSTOP and own[-1:] == [auth[0][:2]]
        return ok, f'hit frame {fr}, window authority {auth}, owner {own}'

    def held_in_window():
        w = win()
        if not w or not w[1]:
            return False, f'hits {hits()}'
        lo, hi = w[1][0][0], w[1][0][1]
        out, ok = [], True
        for sd in ('srv', 'cli'):
            ticks = [s for s in run.sim if s.get(sd) and s[sd]['tags'] and 'Combat.State.Hitstop' in s[sd]['tags']]
            modes = sorted({s[sd]['mode'] for s in ticks})
            pts = [s[sd]['loc'] for s in ticks]
            spread = max((math.dist(p, q) for p in pts for q in pts), default=1e9)
            good = bool(ticks) and modes == [wm] and spread < (5.0 if sd == 'srv' else 8.0)
            ok &= good
            out.append(f'{sd}: {len(ticks)} hitstop ticks, modes {modes}, spread {spread:.1f} cm')
        return ok, '; '.join(out)

    def tags_and_damage():
        seen = {sd: {t for s in run._win(sd, st['tw'], st['tw'] + 3.0, sim=True) for t in s['tags'] if t.startswith('Combat.State.')} for sd in ('srv', 'cli')}
        d = (damage_of(0, key) - st['d0'][0], damage_of(client, key) - st['d0'][1])
        ok = all({'Combat.State.Hitstop', 'Combat.State.Hitstun'} <= t for t in seen.values()) and d[0] == d[1] > 0
        return ok, f'tags srv {sorted(seen["srv"])}, owner {sorted(seen["cli"])}, damage change {d}'

    def after():
        seq = {sd: [] for sd in ('srv', 'cli')}
        for sd in seq:
            for s in run._win(sd, st['tw'], st['tw'] + 3.0, sim=True):
                if not seq[sd] or seq[sd][-1] != s['mode']:
                    seq[sd].append(s['mode'])
        zs = [round(s['loc'][2]) for s in run._win('srv', st['tw'], st['tw'] + 3.0, sim=True)]
        return True, f'mode sequence after the attack: authority {seq["srv"]}, owner {seq["cli"]}; height of the climber every 10th tick {zs[::10]}'

    def end():
        for hd in ('v',):
            if h[hd] is not None:
                unreal.unregister_slate_post_tick_callback(h[hd])
                h[hd] = None
        set_damage(key, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'restored'

    t_end = 2.5 + 6.0 + 4.5
    steps = [
        (0.3, lambda: ('clear', combat_clear(client))),
        (0.5, setup),
        (2.5, start_driver),
        (t_end, run.at('the attacker kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (t_end, run.at('the pawn reached the wall and the spin hit it once while it was wall sliding', landed_on_wall)),
        (t_end, run.at('the hitstop window opens on the frame of the hit, equal on the owner', window_on_wall)),
        (t_end, run.at('WallSliding through the whole window on both, and the pawn did not move', held_in_window)),
        (t_end, run.at('Hitstop and Hitstun projected, the damage taken on both copies', tags_and_damage)),
        (t_end, run.at('what the pawn does after its hitstun (reported)', after)),
        (t_end + 0.2, end),
        (t_end + 0.4, run.finish()),
    ]
    run.run(steps)


# ---- M4 H8: the attacker side of hits (combat-m4-hits 6). The attacker plays UGSTN_GA_TestHitMove (KIT_Test_Hits; the geometry is in its header),
# the victims are the other pawns, with the hurtbox set HBS_Test_Pawn. The victim side (damage, reaction, hitstun) is H9: the checks that need it are
# marked "pending H9" in each docstring. What is judged here is what the attacker side decides and writes: which frame a hitbox touches a
# hurtbox, the once-per-group rule, the confirm flag (cancel on hit), the attacker's own hitstop window, and the hit reaching the victim's inbox.
# The component keeps logs while gstn.Combat.Debug.HitLog is 1: the hits it made (GetHitLog), the world centre of every open hitbox per
# frame (GetHitboxTrace) and what its inbox received (GetInboxLog); a re-simulated frame replaces its entry, so an owner's logs compare with the
# authority's frame by frame.
import math

HIT_KIT = 'KIT_Test_Hits'
# The hurtboxes of HBS_Test_Pawn: (radius, half height with the hemispheres, local z, priority). Mirrors Tools/GAS/setup_gas_assets.hits_content_specs
HIT_HURTBOXES = ((35.0, 55.0, 15.0, 1), (22.0, 22.0, 68.0, 2), (30.0, 30.0, -62.0, 0))
# The four hitbox tracks of the test move in order: (hit group, sphere radius, swept between frames, first timeline frame of the track)
HIT_BLOCKS = ((0, 50.0, True, 4), (1, 45.0, False, 10), (2, 60.0, False, 16), (3, 140.0, False, 20))
HIT_ATTACKER_HITSTOP = 6
TAG_SPECIAL = 'Test.State.Special'     # the plain special of the kit (GSTN_GA_TestSpecial), the cancel target of the hit move


def _hit_comp(wi, key):
    return comps(worlds()[wi])[key].get_owner().get_component_by_class(unreal.GSTN_CombatSimComponent)


def _hit_logging(on):
    """gstn.Combat.Debug.HitLog is one process-wide cvar: every PIE world shares it"""
    unreal.SystemLibrary.execute_console_command(worlds()[0], f'gstn.Combat.Debug.HitLog {1 if on else 0}')


def _int_rows(text):
    return [tuple(int(x) for x in row.split(':')) for row in text.split(';') if row]


def _hit_log(wi, key):
    """[(frame, victim id, group, hitbox, hurtbox, ricochet, attacker hitstop)] the attacker side made on world wi's copy of pawn `key`"""
    return _int_rows(_hit_comp(wi, key).get_hit_log())


def _inbox_log(wi, key):
    """[(drain frame, attacker id, hitbox, hurtbox, hurt state, ricochet, relationship index, damage)] the inbox of pawn `key` received on world wi"""
    return _int_rows(_hit_comp(wi, key).get_inbox_log())


def _hit_trace(wi, key):
    """[(frame, space, (x, y, z))] world centre of every open hitbox"""
    out = []
    for row in _hit_comp(wi, key).get_hitbox_trace().split(';'):
        if row:
            frame, _idx, space, xyz = row.split(':')
            out.append((int(frame), int(space), tuple(float(v) for v in xyz.split(','))))
    return out


def _trace_blocks(rows):
    """Splits the trace of one execution into its hitbox tracks (runs of consecutive frames): [[(frame, centre)]]"""
    blocks, last = [], None
    for frame, _space, centre in rows:
        if last is None or frame != last + 1:
            blocks.append([])
        blocks[-1].append((frame, centre))
        last = frame
    return blocks


def _dist_point_segment(p, a, b):
    ab = [b[i] - a[i] for i in range(3)]
    t = sum((p[i] - a[i]) * ab[i] for i in range(3)) / max(sum(c * c for c in ab), 1e-9)
    t = max(0.0, min(1.0, t))
    return math.dist(p, [a[i] + ab[i] * t for i in range(3)])


def _touched_hurtboxes(point, radius, pawn):
    """Indices of the hurtboxes of a pawn standing at `pawn` that a sphere at `point` touches (vertical capsules, no rotation)"""
    out = []
    for i, (r, half, z, _prio) in enumerate(HIT_HURTBOXES):
        seg = half - r
        a, b = (pawn[0], pawn[1], pawn[2] + z - seg), (pawn[0], pawn[1], pawn[2] + z + seg)
        if _dist_point_segment(point, a, b) <= radius + r:
            out.append(i)
    return out


def _first_touch(block, radius, swept, pawn):
    """(frame, picked hurtbox index) of the first frame the sphere of a trace block touches the pawn, or None. A swept box is tried along the
    way from the previous frame (8 samples, finer than the 4 sub-steps of the simulation: the placements keep 10 cm of margin)"""
    prev = None
    for frame, centre in block:
        pts = [centre]
        if swept and prev is not None:
            pts = [tuple(prev[i] + (centre[i] - prev[i]) * k / 8.0 for i in range(3)) for k in range(9)]
        touched = sorted({i for p in pts for i in _touched_hurtboxes(p, radius, pawn)})
        if touched:
            return frame, min(touched, key=lambda i: (-HIT_HURTBOXES[i][3], i))
        prev = centre
    return None


def _place_near(att_index, victim_key, local_xy):
    """Teleports the victim to a spot given in the attacker's frame (x ahead, y to its left); the floor under it is found by the teleport"""
    a = snap(worlds()[0])[key_of_player(att_index)]
    yaw = math.radians(a['yaw'])
    x = a['location'][0] + local_xy[0] * math.cos(yaw) - local_xy[1] * math.sin(yaw)
    y = a['location'][1] + local_xy[0] * math.sin(yaw) + local_xy[1] * math.cos(yaw)
    spawn = comps(worlds()[0])[victim_key].get_spawn_location()
    comps(worlds()[0])[victim_key].server_reset_to_spawn(unreal.Vector(x - spawn.x, y - spawn.y, 0.0))
    return x, y


def _rot(v, yaw_deg):
    c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    return (v[0] * c - v[1] * s, v[0] * s + v[1] * c)


def _out_of_reach(q):
    """True when a pawn standing at q (the attacker's frame) is clear, with 20 cm of margin, of the three travelling boxes of the test move (not the
    spin): the Root sphere 80 -> 170 ahead, the BakedSocket sphere along (100, 260 -> 140) and the WorldAtMoveStart sphere at (200, -160)"""
    pts = [((80.0 + 30.0 * k / 8.0 * 3.0, 0.0), 50.0) for k in range(9)] + [((100.0, 260.0 - 15.0 * k), 45.0) for k in range(9)] + [((200.0, -160.0), 60.0)]
    return all(math.dist(c, q) > r + 35.0 + 20.0 for c, r in pts)


def _side_spot(a_index, b_key, radius=120.0):
    """A spot at `radius` from the attacker, in its frame, where neither pawn is reached by the other's Root, BakedSocket or WorldAtMoveStart box whatever
    the two facings are (the pawns keep the yaw earlier scenarios left them with): only the spin of the test move connects"""
    v = snap(worlds()[0])
    a, b = v[key_of_player(a_index)], v[b_key]
    for deg in range(0, 360, 15):
        p = (radius * math.cos(math.radians(deg)), radius * math.sin(math.radians(deg)))
        world = _rot(p, a['yaw'])
        a_in_b = _rot((-world[0], -world[1]), -b['yaw'])
        if _out_of_reach(p) and _out_of_reach(a_in_b):
            return p
    return (0.0, radius)


def _latest_frame(wi, key):
    return _montage_info(wi, key)[2]


def _hit_window(wi, key):
    """(start, end) of the hitstop window on world wi's copy of the pawn"""
    info = _montage_info(wi, key)
    return info[3], info[4]


def _team_everywhere(keys, team):
    for w in worlds():
        for k in keys:
            c = comps(w).get(k)
            if c is not None:
                c.get_owner().get_component_by_class(unreal.GSTN_CombatSimComponent).set_test_team(team)


def _tag(name):
    """A GameplayTag struct from its name (TagName is read-only from Python: the text form)"""
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


_PROBE_TAGS = ('Combat.State.Hitstun', 'Combat.State.Tumble', 'Combat.State.Hitstop', 'Combat.Lock.Movement')


class _VictimProbe:
    """Per editor tick, the victim's combat state on the given worlds: (time, [(victim info, location, velocity, mode, hit tags)] per world).
    victim info = "frame,action,hitstopStart,hitstopEnd,hitstunEnd,pendingReaction+1" (GetVictimInfo). Debug aid of the H9 scenarios and the
    judge of the numbers they check (frames are the simulation's, the time only orders the rows)"""

    def __init__(self, key, world_indices):
        self.key, self.worlds_, self.rows, self.t0, self.handle = key, tuple(world_indices), [], time.time(), None
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def _tick(self, _dt):
        try:
            row = [time.time() - self.t0]
            for wi in self.worlds_:
                c = comps(worlds()[wi])[self.key]
                cc = c.get_owner().get_component_by_class(unreal.GSTN_CombatSimComponent)
                v = json.loads(c.get_state_snapshot())
                info = [int(x) for x in cc.get_victim_info().split(',')]
                asc = c.get_owner().get_component_by_class(unreal.AbilitySystemComponent)
                tags = list(v.get('tags', [])) + [n for n in _PROBE_TAGS if asc.has_matching_gameplay_tag(_tag(n))]
                row.append((info, tuple(v['location']), tuple(v.get('velocity', (0, 0, 0))), v.get('mode'), tags))
            self.rows.append(row)
        except Exception:
            pass

    def stop(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        return self.rows


_probe = {}


def probe_start(key, world_indices=(0,)):
    if _probe.get('p'):
        _probe['p'].stop()
    _probe['p'] = _VictimProbe(key, world_indices)
    return 'probing'


def probe_rows(compact=True):
    p = _probe.get('p')
    if not p:
        return []
    out, last = [], None
    for row in p.rows:
        key = tuple((e[0][1], e[0][2], e[0][3], e[0][4], e[0][5], e[3], tuple(sorted(t for t in e[4] if t.startswith('Combat.')))) for e in row[1:])
        if key != last or not compact:
            out.append((round(row[0], 2), [(e[0][0], e[0][1], e[0][2], e[0][3], e[0][4], e[0][5], tuple(round(x) for x in e[1]), tuple(round(x) for x in e[2]), e[3], tuple(sorted(t[7:] for t in e[4] if t.startswith('Combat.')))) for e in row[1:]]))
        last = key
    return out


# ---- M4 H9: the rules, the hurtbox set and the victim for the hit scenarios. The rules object (GetCombatRulesForTest) and the hurtbox set asset are shared
# by every pawn of the process, so a change is the same on all machines; make it between hits, never while one is in flight. The standard rules ship no
# damage effect (the pending GSTN_CombatRules_Standard asset): the harness sets the native one.
TUNING_DEFAULT = {'push_back_threshold': 540.0, 'launch_threshold': 1080.0, 'tumble_threshold': 1800.0, 'min_hitstun_frames': 31}


def hit_rules(wi=0):
    """The combat rules of world wi's pawns (any pawn: they share one object)"""
    return _hit_comp(wi, next(iter(comps(worlds()[wi])))).get_combat_rules_for_test()


def hit_rules_setup():
    """Damage through the native damage effect, the standard thresholds, no friendly fire, the hurtbox set at its defaults. Returns what it set"""
    r = hit_rules()
    r.set_editor_property('damage_effect', unreal.GSTN_GE_Damage)
    r.set_editor_property('damage_mode', unreal.GSTN_DamageMode.PERCENT)
    r.set_editor_property('max_health', 100.0)
    set_tuning(**TUNING_DEFAULT)
    set_hurtbox(stagger=-1.0, infinite=False, max_reaction=unreal.GSTN_HitReaction.LAUNCH)
    return 'rules ready'


def set_tuning(**values):
    """Sets fields of the knockback model's tuning (push_back_threshold, launch_threshold, tumble_threshold, min_hitstun_frames...)"""
    km = hit_rules().get_editor_property('knockback_model')
    t = km.get_editor_property('tuning')
    for k, v in values.items():
        t.set_editor_property(k, v)
    km.set_editor_property('tuning', t)
    return values


def set_hurtbox(stagger=None, infinite=None, max_reaction=None):
    """The hurtbox set of the test pawns (HBS_Test_Pawn): the stagger threshold and the cap of the reaction tier. In memory only, never saved"""
    hs = unreal.load_asset('/Game/GASNPPTest/Combat/HBS_Test_Pawn')
    if stagger is not None or infinite is not None:
        th = hs.get_editor_property('stagger_threshold')
        if stagger is not None:
            th.set_editor_property('value', stagger)
        if infinite is not None:
            th.set_editor_property('infinite', infinite)
        hs.set_editor_property('stagger_threshold', th)
    if max_reaction is not None:
        hs.set_editor_property('max_reaction', max_reaction)
    return 'hurtbox set'


def set_damage(vkey, percent):
    """The damage percent of a pawn (server write; the owner reconciles once)"""
    return comps(worlds()[0])[vkey].server_set_damage_percent(percent)


def victim_info(wi, key):
    """(frame, action, hitstopStart, hitstopEnd, hitstunEnd, pending reaction + 1) of world wi's copy of a pawn"""
    return tuple(int(x) for x in _hit_comp(wi, key).get_victim_info().split(','))


def event_log(key, wi=0):
    """[(role, type, frame, damage, set of tags)] the authority published about a pawn (role I = instigator, T = target)"""
    out = []
    for row in comps(worlds()[wi])[key].get_hit_event_log().split(';'):
        if row:
            role, kind, frame, damage, tags = row.split(':', 4)
            out.append((role, kind, int(frame), float(damage), set(tags.split(','))))
    return out


def damage_of(wi, key):
    return json.loads(comps(worlds()[wi])[key].read_snapshot_entry(-1))['damagePercent']


def rollbacks(wi):
    """Rollbacks of the pawn world wi controls"""
    return local_comp(worlds()[wi]).get_editor_property('rollback_count')


def sim_rows(key, wi=0, limit=60, drain=True):
    """The simulation ticks of a pawn since the last drain, condensed to the ticks where velocity, mode or the combat tags changed:
    [(sim ms, velocity, location, mode, combat tags)]"""
    ticks = json.loads(comps(worlds()[wi])[key].drain_sim_history()) if drain else []
    out, last = [], None
    for e in ticks:
        row = (tuple(round(x) for x in e['vel']), e['mode'], tuple(sorted(t[7:] for t in e['tags'] if t.startswith('Combat.'))))
        if row != last:
            out.append((e['t'], row[0], tuple(round(x, 1) for x in e['loc']), row[1], row[2]))
        last = row
    return out[:limit]


def windows_seen(rec, which):
    """The distinct (hitstop start, hitstop end, hitstun end) of a victim's windows in the order they appeared, from samples
    rec = [(t, info on the authority, info on the owner)] (victim_info tuples); which 1 = authority, 2 = owner"""
    out = []
    for row in rec:
        info = row[which]
        w = (info[2], info[3], info[4])
        if w[0] >= 0 and (not out or out[-1] != w):
            out.append(w)
    return out


def victim_window_checks(run, label, hit_frames, rec, hitstop=8, lock=31):
    """Timeline checks of the victim half of a series of hits (hit frames of the attacker's log): one window per hit that starts on the frame of the
    hit and is `hitstop` frames long, hitstun `lock` frames after it (the floor of the rules for a stagger in place), the same on the owner's copy"""
    def f():
        frames = hit_frames() if callable(hit_frames) else hit_frames
        want = [(fr, fr + hitstop, fr + hitstop + lock) for fr in frames]
        # Windows of earlier runs stay in the state (absolute frames) and the owner's copy drops an expired hitstun end (canonical form): compare the
        # windows of these hits, the owner on the window and, while it still holds one, the hitstun end
        auth = [w for w in windows_seen(rec, 1) if frames and w[0] >= min(frames)]
        own = [w for w in windows_seen(rec, 2) if frames and w[0] >= min(frames)]
        own_pairs = []
        for w in own:
            if not own_pairs or own_pairs[-1] != w[:2]:
                own_pairs.append(w[:2])
        ok = auth == want and own_pairs == [w[:2] for w in want] and all(w in want or w[2] == -1 for w in own)
        return ok, f'authority {auth}, owner {own}, wanted {want}'
    return run.at(f'{label}: hitstop window on the frame of each hit ({hitstop} frames), hitstun {lock} frames after it, equal on the owner', f)


def hit_events_check(run, label, vkey, count, extra_tags=()):
    """Timeline check: the authority published exactly `count` HitLanded and `count` HitReceived about the victim, once per hit, each with the
    hit type, the relationship and the given tags"""
    def f():
        ev = event_log(vkey)
        landed = [e for e in ev if e[0] == 'T' and e[1] == 'HitLanded']
        received = [e for e in ev if e[0] == 'T' and e[1] == 'HitReceived']
        base = {'Combat.HitType.Normal'}
        ok = len(landed) == count and len(received) == count and all(base <= e[4] and all(t in e[4] for t in extra_tags) for e in landed + received)
        frames = [e[2] for e in landed]
        return ok and len(set(frames)) == count, f'HitLanded {len(landed)}, HitReceived {len(received)} (want {count} each), frames {frames}, first tags {sorted(received[0][4]) if received else None}'
    return run.at(f'{label}: HitLanded and HitReceived once per hit with their tags', f)


def scenario_hit_basic(client=1, step=3.0):
    """M4 H8 attacker side, the client `client` attacks the other client. Control, then the three hitbox spaces one by one (the victim is placed
    where only one track of the move can reach it, see UGSTN_GA_TestHitMove), judged on the authority's copy of the attacker and the attacker's own:
      - the frame of the hit equals the frame the hitbox first touches the hurtbox, computed here from the world centres the simulation traced
        (Root, BakedSocket, WorldAtMoveStart), the same frame and the same hurtbox on the authority and the owner
      - one hit per group and execution (the box stays over the victim for 4 frames)
      - the attacker's hitstop window opens on the frame of the hit with the authored 6 frames, on both, and the owner has it no later (predicted)
      - the hit reaches the victim's inbox on the authority the frame after, with the hurtbox and the hitbox it was made with
      - cancel on hit: the special (a command of the cancel window, OnHit) held 0.25 s after the attack starts cancels it only when the hit confirmed (control without a victim: it does not)
      - the hits cost the attacking client no rollback of their own (judged in the second after each attack against the control attack: at most one more over the three, the link drops packets)
    The victim half (M4 H9), on the victim's authority copy and its own client: the hit opens a hitstop window on the frame of the hit (8 frames) and a
    hitstun 31 frames after it (the floor of the rules, a stagger in place), the tags Combat.State.Hitstop / Hitstun are projected from it, the damage is
    8 per hit on both copies, HitLanded and HitReceived are published once per hit, and the victim's client rolls back at most once per hit."""
    victim_index = 2 if client == 1 else 1
    vkey = key_of_player(victim_index)
    run = _Run('hit_basic', client, judge_reconciles=False, extra=[('v_srv', 0, vkey), ('v_cli', victim_index, vkey)])
    key = run.key
    attack, special = button_mask('Attack'), button_mask('Special')
    st, rec, h = {'F': {}, 'vloc': {}}, [], {'h': None}
    rb = {}   # rollbacks of the attacking client are counted from the control on: the loadout change before it reconciles once, by design
    rbv = {}  # rollbacks of the victim's client per phase, from after the placement settled to just before the next one
    vrec = []
    arb, taps = [], {}   # (time, rollbacks of the attacking client) per editor tick, and the time of each attack

    def sampler(_dt):
        try:
            rec.append((run.now(), _hit_window(0, key), _hit_window(client, key)))
            vrec.append((run.now(), victim_info(0, vkey), victim_info(victim_index, vkey)))
            arb.append((run.now(), rollbacks(client)))
        except Exception:
            pass

    def atk(name):
        def f():
            taps[name] = run.now()
            combat_tap(client, attack)
            return 'attack', name
        return f

    def attacker_rollbacks():
        # What an attack costs the attacker's client with no victim in reach is the baseline (the pose a scenario leaves: the move origin stamped at the
        # start of a timeline can differ between the machines by the floor height, 12 cm on a lane of the full suite, and every attack then rolls back
        # once whether it hits or not). The hits must add nothing to it: at most one more over the three (the link drops 2 % of the packets, an idle pawn
        # rolls back about once per 25 s)
        def count(name):
            w = [c for t, c in arb if taps[name] <= t <= taps[name] + 1.0]
            return (w[-1] - w[0]) if w else 99
        base, out = count('c'), {n: count(n) for n in ('p1', 'p2', 'p3')}
        extra = sum(max(0, v - base) for v in out.values())
        return extra <= 1, f"rollbacks of the attacking client in the second after an attack: control (no victim) {base}, hits {out}; whole run {rb['start']} -> {rb['end']}"

    def rbmark(name):
        def f():
            rbv[name] = rollbacks(victim_index)
            return name, rbv[name]
        return f

    def setup():
        hit_rules_setup()
        _hit_logging(True)
        st['kit'] = _apply_loadout(key, HIT_KIT)
        h['h'] = unreal.register_slate_post_tick_callback(sampler)
        return 'kit', st['kit']

    def mark(name):
        def f():
            st['F'][name] = _latest_frame(0, key)
            return name, st['F'][name]
        return f

    def place(name, xy):
        def f():
            _place_near(client, vkey, xy)
            return 'placed', name
        return f

    def locate(name):
        def f():
            st['vloc'][name] = tuple(snap(worlds()[0])[vkey]['location'])
            return name, st['vloc'][name]
        return f

    def stop():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        rb['end'] = local_comp(worlds()[client]).get_editor_property('rollback_count')
        return 'stopped'

    def phase_rows(wi, name, nxt):
        lo, hi = st['F'][name], st['F'].get(nxt, 10 ** 9)
        return [r for r in _hit_log(wi, key) if lo <= r[0] < hi], [r for r in _hit_trace(wi, key) if lo <= r[0] < hi]

    def phase_check(name, nxt, block, victim_xy):
        def f():
            group, radius, swept, offset = HIT_BLOCKS[block]
            hits, trace = phase_rows(0, name, nxt)
            owner_hits, _ = phase_rows(client, name, nxt)
            blocks = _trace_blocks(trace)
            if len(blocks) < block + 1 or len(hits) != 1:
                return False, f'{len(blocks)} trace blocks, hits {hits}'
            expect = _first_touch(blocks[block], radius, swept, st['vloc'][name])
            start = blocks[0][0][0] - HIT_BLOCKS[0][3]
            frame, victim, g, hitbox, hurtbox, ricochet, hitstop = hits[0]
            ok = (expect is not None and frame == expect[0] and hurtbox == expect[1] and g == group and ricochet == 0 and hitstop == HIT_ATTACKER_HITSTOP
                  and owner_hits == hits and frame - start >= offset)
            return ok, f'hit {hits[0]}, expected first touch {expect}, move started {start}, owner saw {owner_hits}'
        return f

    def hit_reached_victim():
        out = []
        for name, nxt, block in (('p1', 'p2', 0), ('p2', 'p3', 1), ('p3', None, 2)):
            hits, _ = phase_rows(0, name, nxt)
            if len(hits) != 1:
                return False, f'{name}: hits {hits}'
            inbox = [e for e in _inbox_log(0, vkey) if e[0] == hits[0][0] + 1]
            out.append((name, inbox))
            if len(inbox) != 1 or inbox[0][2] != 0 or inbox[0][3] != hits[0][4] or inbox[0][4] != 0 or inbox[0][5] != 0 or inbox[0][6] != 3 or inbox[0][7] != 8:
                return False, f'{name}: hit {hits[0]}, inbox {inbox}'
        return True, f'{out}'

    def window_on_hit():
        hits, _ = phase_rows(0, 'p3', None)
        if len(hits) != 1:
            return False, f'hits {hits}'
        a, o = _hit_window(0, key), _hit_window(client, key)
        ok = a == o and a[0] == hits[0][0] and a[1] - a[0] == HIT_ATTACKER_HITSTOP
        return ok, f'hit frame {hits[0][0]}, window authority {a}, owner {o}'

    def window_predicted():
        hits, _ = phase_rows(0, 'p3', None)
        if len(hits) != 1:
            return False, f'hits {hits}'
        want = hits[0][0]
        t_auth = next((t for t, a, o in rec if a[0] == want and a[1] > a[0]), None)
        t_own = next((t for t, a, o in rec if o[0] == want and o[1] > o[0]), None)
        return t_auth is not None and t_own is not None and t_own <= t_auth, f'the window of frame {want} first seen: owner {t_own}, authority {t_auth}'

    def stance_cancels():
        # Phase 1 holds the stance 0.25 s into the attack with a confirmed hit; the control holds it at the same moment without one
        ctrl = run.saw_tag(TAG_SPECIAL, 2.25, 3.0)
        hit = run.saw_tag(TAG_SPECIAL, 4.9, 5.6)
        return hit and not ctrl, f'special seen after a hit: {hit}; in the control without one: {ctrl}'

    steps = [
        (0.3, setup),
        # control: no victim in reach; the stance held into the cancel window of the move is refused until the move is free
        (2.0, mark('c')),
        (2.0, lambda: ('rollbacks counted from here', rb.__setitem__('start', local_comp(worlds()[client]).get_editor_property('rollback_count')))),
        (2.0, atk('c')),
        (2.25, lambda: ('stance held', combat_hold(client, special))),
        (2.4, lambda: ('stance released', combat_clear(client))),
        (3.1, run.at('control: the move ran and nothing was hit', lambda: (run.saw_tag('Test.State.HitMove', 2.0, 2.6) and not [r for r in _hit_log(0, key) if r[0] >= st['F']['c']], f"hits {[r for r in _hit_log(0, key) if r[0] >= st['F']['c']]}"))),
        # phase 1: Root
        (step + 0.3, place('p1', (205.0, 0.0))),
        (step + 1.6, locate('p1')),
        (step + 1.65, rbmark('p1s')),
        (step + 1.7, mark('p1')),
        (step + 1.7, atk('p1')),
        (step + 1.95, lambda: ('stance held', combat_hold(client, special))),
        (step + 2.1, lambda: ('stance released', combat_clear(client))),
        # phase 2: BakedSocket
        (step + 3.5, rbmark('p1e')),
        (step + 3.6, place('p2', (100.0, 170.0))),
        (step + 4.9, locate('p2')),
        (step + 4.95, rbmark('p2s')),
        (step + 5.0, mark('p2')),
        (step + 5.0, atk('p2')),
        # phase 3: WorldAtMoveStart
        (step + 7.1, rbmark('p2e')),
        (step + 7.2, place('p3', (200.0, -110.0))),
        (step + 8.5, locate('p3')),
        (step + 8.55, rbmark('p3s')),
        (step + 8.6, mark('p3')),
        (step + 8.6, atk('p3')),
        (step + 10.1, rbmark('p3e')),
        (step + 10.2, stop),
        (step + 10.3, run.at('the kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (step + 10.3, run.at('Root: first touch frame, hurtbox, group, hitstop; owner == authority', phase_check('p1', 'p2', 0, (205.0, 0.0)))),
        (step + 10.3, run.at('BakedSocket: first touch frame, hurtbox, group, hitstop; owner == authority', phase_check('p2', 'p3', 1, (100.0, 170.0)))),
        (step + 10.3, run.at('WorldAtMoveStart: first touch frame, hurtbox, group, hitstop; owner == authority', phase_check('p3', None, 2, (200.0, -110.0)))),
        (step + 10.3, run.at('the hits reached the victim inbox on the authority the frame after', hit_reached_victim)),
        (step + 10.3, run.at('the attacker hitstop window opens on the frame of the hit, 6 frames, equal on both', window_on_hit)),
        (step + 10.3, run.at('the owner has the window no later than the authority (predicted)', window_predicted)),
        (step + 10.3, run.at('cancel on hit: the stance cancels the move after a hit and not without one', stance_cancels)),
        (step + 10.3, run.at('the hits cost the attacking client nothing beyond what an attack with no victim costs it (at most one rollback more over the three)', attacker_rollbacks)),
        # the victim half
        (step + 10.3, victim_window_checks(run, 'victim', lambda: [phase_rows(0, n, nx)[0][0][0] if len(phase_rows(0, n, nx)[0]) == 1 else -1 for n, nx in (('p1', 'p2'), ('p2', 'p3'), ('p3', None))], vrec)),
        (step + 10.3, run.at('victim: Combat.State.Hitstop and Hitstun projected on the authority and on the owner', lambda: (
            all(run.saw_tag(t, 0.0, 99.0, sd) for t in ('Combat.State.Hitstop', 'Combat.State.Hitstun') for sd in ('v_srv', 'v_cli')),
            f"authority {sorted({t for s in run._win('v_srv', 0, 99, sim=True) for t in s['tags'] if 'Combat' in t})}, owner {sorted({t for s in run._win('v_cli', 0, 99, sim=True) for t in s['tags'] if 'Combat' in t})}"))),
        (step + 10.3, run.at('victim: 24 damage (3 x 8) on the authority and on the owner', lambda: (
            damage_of(0, vkey) == 24 and damage_of(victim_index, vkey) == 24, f'authority {damage_of(0, vkey)}, owner {damage_of(victim_index, vkey)}'))),
        (step + 10.3, hit_events_check(run, 'victim', vkey, 3, ('Combat.Event.Hit.Staggered', 'Combat.Event.Hit.Reaction.StaggerInPlace'))),
        (step + 10.3, run.at('victim: at most one rollback per hit on the victim\'s client', lambda: (
            all(rbv[n + 'e'] - rbv[n + 's'] <= 1 for n in ('p1', 'p2', 'p3')), f"rollbacks per hit {[(n, rbv[n + 'e'] - rbv[n + 's']) for n in ('p1', 'p2', 'p3')]}"))),
        (step + 10.5, lambda: ('logging off', _hit_logging(False))),
        (step + 10.6, run.finish()),
    ]
    run.run(steps)


def _victim_windows(rec, col, lo=0, hi=10 ** 9):
    """Windows (hitstop start, end, hitstun end) of one pawn from the samples rec = [(t, ..., auth info, owner info, ...)]: col is the index of the
    authority's info, the owner's is the next one. Returns (authority windows, owner (start, end) pairs) whose start frame is in [lo, hi)"""
    rows = [(r[0], r[col], r[col + 1]) for r in rec]
    auth = [w for w in windows_seen(rows, 1) if lo <= w[0] < hi]
    own = []
    for w in windows_seen(rows, 2):
        if lo <= w[0] < hi and (not own or own[-1] != w[:2]):
            own.append(w[:2])
    return auth, own


def scenario_hit_trade(client=1):
    """M4 H8 + H9c, a trade: the two clients stand side by side (neither is in front of the other, so only the spin of the move, a sphere around the
    attacker, connects) and attack at the same moment. Two rounds on a clean link (the taps land on frames up to two apart under emulated jitter, and a
    hit cancels the victim's move one frame later, so a trade is only a trade when the frames meet):
      - round 1, the hurtboxes never stagger (a flinch): neither move is cancelled, so both hits land (group 3; the first hit freezes its victim for 8 frames and a frozen move does not advance, so the second comes 0 to 8 frames later) and both victims
        react the same way: a hitstop window from the frame of their hit (merged with their own attacker window), the damage of their inbox entry on
        the authority and on the owner, HitReceived and HitLanded once for each of the two hits, both inbox entries delivered the frame after
      - round 2, the standard staggering: whatever the taps' frames made of it (a trade, or the first hit cancelling the other move), every hit that
        landed has its victim reacting from the frame of the hit with hitstun 31 frames after the window, the damage of the inbox entry taken, events
        once; a move cancelled before its hit made no hit and its pawn lost nothing it was not dealt. The outcome is reported."""
    a_index, b_index = (1, 2) if client == 1 else (2, 1)
    run = _Run('hit_trade', client, judge_reconciles=False, extra=[('b_srv', 0, key_of_player(b_index)), ('b_cli', b_index, key_of_player(b_index))])
    akey, bkey = key_of_player(a_index), key_of_player(b_index)
    attack = button_mask('Attack')
    st, h = {'rec': [], 'F': {}, 'd0': {}}, {'h': None}

    def sampler(_dt):
        try:
            st['rec'].append((run.now(), victim_info(0, akey), victim_info(a_index, akey), victim_info(0, bkey), victim_info(b_index, bkey)))
        except Exception:
            pass

    def setup():
        st['net'] = _cfg.get('net', (0, 0, 0))
        net(0, 0, 0)
        hit_rules_setup()
        _hit_logging(True)
        st['kits'] = (_apply_loadout(akey, HIT_KIT), _apply_loadout(bkey, HIT_KIT))
        h['h'] = unreal.register_slate_post_tick_callback(sampler)
        return 'kits', st['kits']

    def both_tap(rnd):
        def f():
            st['F'][rnd] = _latest_frame(0, akey)
            st['d0'][rnd] = (damage_of(0, akey), damage_of(a_index, akey), damage_of(0, bkey), damage_of(b_index, bkey))
            combat_tap(a_index, attack)
            combat_tap(b_index, attack)
            return 'both attack', rnd
        return f

    def hits(rnd):
        lo = st['F'][rnd]
        hi = st['F'].get(rnd + 1, 10 ** 9)
        return [r for r in _hit_log(0, akey) if lo <= r[0] < hi], [r for r in _hit_log(0, bkey) if lo <= r[0] < hi]

    def inbox_damage(key, frame):
        return next((e[7] for e in _inbox_log(0, key) if e[0] == frame + 1), None)

    def trade():
        # The first hit freezes its victim for the 8 frames of the window, and a frozen move does not advance: the second hit of a flinch trade comes up to
        # 8 frames after the first (the same frame when the taps meet)
        a, b = hits(1)
        if len(a) != 1 or len(b) != 1:
            return False, f'A hits {a}, B hits {b}'
        ia, ib = _inbox_log(0, akey), _inbox_log(0, bkey)
        ok = (a[0][2] == 3 and b[0][2] == 3 and a[0][1] != b[0][1] and abs(a[0][0] - b[0][0]) <= 10
              and any(e[0] == b[0][0] + 1 and e[2] == 0 for e in ia) and any(e[0] == a[0][0] + 1 and e[2] == 0 for e in ib))
        return ok, f'A hit {a[0]}, B hit {b[0]}, inbox of A {ia[-1:]}, inbox of B {ib[-1:]}'

    def has_window(wins, frame, length=6):
        return any(w[0] == frame and w[1] - w[0] >= length for w in wins)

    def attacker_windows():
        a, b = hits(1)
        if len(a) != 1 or len(b) != 1:
            return False, f'A hits {a}, B hits {b}'
        lo, hi = st['F'][1], st['F'].get(2, 10 ** 9)
        wa, wb = window_of(1, lo, hi)[0], window_of(3, lo, hi)[0]
        return has_window(wa, a[0][0], HIT_ATTACKER_HITSTOP) and has_window(wb, b[0][0], HIT_ATTACKER_HITSTOP), f'windows of A {wa}, of B {wb}, own hit frames A {a[0][0]}, B {b[0][0]}'

    def window_of(col, lo, hi=10 ** 9):
        return _victim_windows(st['rec'], col, lo, hi)

    def flinch_both():
        a, b = hits(1)
        if len(a) != 1 or len(b) != 1:
            return False, f'A hits {a}, B hits {b}'
        out, ok = [], True
        lo, hi = st['F'][1], st['F'].get(2, 10 ** 9)
        for name, col, fr in (('A', 1, b[0][0]), ('B', 3, a[0][0])):
            auth, own = window_of(col, lo, hi)
            # The owner's state shows the window of its latest frame only: a window of 8 frames that ends before the correction arrives (the second
            # hit comes up to 8 frames later) is never on its screen, so the owner is compared on the last window
            good = has_window(auth, fr, 8) and own[-1:] == [auth[-1][:2]]
            ok &= good
            out.append(f'{name} (hit on it at {fr}): authority {auth}, owner {own}')
        return ok, '; '.join(out)

    def damage(rnd, want_both=True):
        a, b = hits(rnd)
        da = inbox_damage(akey, b[0][0]) if b else None
        db = inbox_damage(bkey, a[0][0]) if a else None
        d0 = st['d0'][rnd]
        got = (damage_of(0, akey) - d0[0], damage_of(a_index, akey) - d0[1], damage_of(0, bkey) - d0[2], damage_of(b_index, bkey) - d0[3])
        if rnd == 1:
            got = (got[0], got[1], got[2], got[3])
        want = (da or 0, da or 0, db or 0, db or 0)
        return got == want and (not want_both or (da and db)), f'inbox damage A {da}, B {db}; change on (A authority, A owner, B authority, B owner) {got}'

    def events(rnd):
        a, b = hits(rnd)
        out, ok = [], True
        for key, got_hit, dealt in ((akey, b, a), (bkey, a, b)):
            ev = [e for e in event_log(key) if st['F'][rnd] <= e[2] < st['F'].get(rnd + 1, 10 ** 9)]
            n = (len([e for e in ev if e[0] == 'T' and e[1] == 'HitReceived']), len([e for e in ev if e[0] == 'I' and e[1] == 'HitLanded']))
            ok &= n == (len(got_hit), len(dealt))
            out.append(n)
        return ok, f'(HitReceived as target, HitLanded as instigator) for A and B: {out}'

    def stagger_round():
        a, b = hits(2)
        out, ok = [], True
        if not a and not b:
            return False, 'no hit at all'
        for name, col, hit_on_it, other in (('A', 1, b, 'B'), ('B', 3, a, 'A')):
            auth, own = window_of(col, st['F'][2])
            if hit_on_it:
                fr = hit_on_it[0][0]
                good = bool(auth) and any(w[0] == fr and w[2] - w[1] == 31 for w in auth) and any(o[0] == fr for o in own)
            else:
                good = True
            ok &= bool(good)
            out.append(f'{name}: hit on it {[r[0] for r in hit_on_it]}, windows {auth}')
        kind = 'a trade (both hits landed)' if a and b else 'the first hit cancelled the other move'
        return ok, f'{kind}: A dealt {[r[0] for r in a]}, B dealt {[r[0] for r in b]}; ' + '; '.join(out)

    def end():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        set_damage(akey, 0)
        set_damage(bkey, 0)
        hit_rules_setup()
        net(*st['net'])
        return 'restored'

    steps = [
        (0.3, setup),
        (0.5, lambda: ('hurtboxes never stagger', set_hurtbox(infinite=True))),
        (2.0, lambda: ('B beside A', _place_near(a_index, bkey, _side_spot(a_index, bkey)))),
        (3.3, both_tap(1)),
        (5.0, run.at('round 1: each attacker hit the other once with the spin; inbox entries delivered; the hits at most 10 frames apart', trade)),
        (5.0, run.at('round 1: both attackers opened their hitstop window on their hit', attacker_windows)),
        (5.0, run.at('round 1: both victims reacted: a window from the frame of their hit, equal on the owner (a flinch)', flinch_both)),
        (5.0, run.at('round 1: both lost the damage of their inbox entry on the authority and on the owner', lambda: damage(1))),
        (5.0, run.at('round 1: HitReceived and HitLanded once for each of the two hits', lambda: events(1))),
        (5.1, lambda: ('standard hurtboxes', set_hurtbox(stagger=-1.0, infinite=False))),
        (5.5, lambda: ('B beside A', _place_near(a_index, bkey, _side_spot(a_index, bkey)))),
        (7.5, both_tap(2)),
        (9.5, run.at('round 2: every hit that landed staggered its victim from the frame of the hit, hitstun 31 frames after the window', stagger_round)),
        (9.5, run.at('round 2: the damage dealt was taken on both copies', lambda: damage(2, want_both=False))),
        (9.5, run.at('round 2: HitReceived and HitLanded once per hit that landed', lambda: events(2))),
        (9.6, end),
        (9.7, run.at('the kits were accepted', lambda: (st.get('kits') == ('OK', 'OK'), f'ServerApplyTestLoadout -> {st.get("kits")}'))),
        (9.8, lambda: ('logging off', _hit_logging(False))),
        (9.9, run.finish()),
    ]
    run.run(steps)


def scenario_friendly_fire(client=1):
    """M4 H8 + H9c, relationships through the spin of the move (it has an Ally variant: damage 3; no variant: 6). The two clients are put on the same
    team (the harness knob SetTestTeam), side by side:
      - friendly fire off: the ally is not hit (the move ran: Test.State.HitMove seen), and the victim does not react: no damage, no window, no events
      - friendly fire on: the ally is hit, the inbox entry says Ally and carries the variant's damage 3; the victim loses 3, opens its window from the
        frame of the hit, is staggered, and the events say Ally
      - different teams: an Enemy hit with the move's own damage 6, the victim loses 6 and reacts, the events say Enemy
    Judged on the victim's authority copy and its client."""
    a_index, b_index = (1, 2) if client == 1 else (2, 1)
    run = _Run('friendly_fire', client, judge_reconciles=False)
    akey, bkey = run.key, key_of_player(b_index)
    attack = button_mask('Attack')
    st = {'F': {}, 'd': {}, 'rec': []}
    h = {'h': None}

    def sampler(_dt):
        try:
            st['rec'].append((run.now(), victim_info(0, bkey), victim_info(b_index, bkey)))
        except Exception:
            pass

    def setup():
        hit_rules_setup()
        _hit_logging(True)
        st['kit'] = _apply_loadout(akey, HIT_KIT)
        h['h'] = unreal.register_slate_post_tick_callback(sampler)
        return 'kit', st['kit']

    def friendly(on):
        def f():
            _hit_comp(0, akey).set_test_friendly_fire(on)
            return 'friendly fire', on
        return f

    def teams(a, b):
        def f():
            _team_everywhere([akey], a)
            _team_everywhere([bkey], b)
            return 'teams', (a, b)
        return f

    def mark(name):
        def f():
            st['F'][name] = _latest_frame(0, akey)
            st['d'][name] = (damage_of(0, bkey), damage_of(b_index, bkey))
            return name, st['F'][name]
        return f

    def rows(lo, hi=10 ** 9):
        return [r for r in _hit_log(0, akey) if lo <= r[0] < hi]

    def events(lo, hi=10 ** 9):
        return [e for e in event_log(bkey) if e[0] == 'T' and e[1] == 'HitReceived' and lo <= e[2] < hi]

    def damage_since(name, nxt=None):
        end = st['d'][nxt] if nxt else (damage_of(0, bkey), damage_of(b_index, bkey))
        return end[0] - st['d'][name][0], end[1] - st['d'][name][1]

    def windows(lo, hi):
        return _victim_windows(st['rec'], 1, lo, hi)

    def ally_off():
        lo, hi = st['F']['off'], st['F']['on']
        auth, own = windows(lo, hi)
        ok = not rows(lo, hi) and run.saw_tag('Test.State.HitMove', 3.4, 4.2) and not events(lo, hi) and damage_since('off', 'on') == (0, 0) and not auth and not own
        return ok, f'hits {rows(lo, hi)}, events {events(lo, hi)}, damage change {damage_since("off", "on")}, windows {auth} / {own}'

    def ally_on():
        hits = rows(st['F']['on'], st['F']['enemy'])
        inbox = [e for e in _inbox_log(0, bkey) if hits and e[0] == hits[0][0] + 1]
        ok = len(hits) == 1 and hits[0][2] == 3 and len(inbox) == 1 and inbox[0][6] == 1 and inbox[0][7] == 3
        return ok, f'hits {hits}, inbox {inbox}'

    def enemy():
        hits = rows(st['F']['enemy'])
        inbox = [e for e in _inbox_log(0, bkey) if hits and e[0] == hits[0][0] + 1]
        ok = len(hits) == 1 and hits[0][2] == 3 and len(inbox) == 1 and inbox[0][6] == 2 and inbox[0][7] == 6
        return ok, f'hits {hits}, inbox {inbox}'

    def victim(name, lo, hi, damage_want, relation, nxt=None):
        def f():
            hits, ev = rows(lo, hi), events(lo, hi)
            auth, own = windows(lo, hi)
            fr = hits[0][0] if hits else -1
            ok = (len(hits) == 1 and len(ev) == 1 and relation in ev[0][4] and 'Combat.Event.Hit.Staggered' in ev[0][4] and damage_since(name, nxt) == (damage_want, damage_want)
                  and len(auth) == 1 and auth[0][0] == fr and auth[0][2] - auth[0][1] == 31 and own == [auth[0][:2]])
            return ok, f'hit {hits}, events {[(e[2], sorted(e[4])) for e in ev]}, damage change {damage_since(name, nxt)}, windows {auth} / {own}'
        return f

    def end():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        set_damage(bkey, 0)
        hit_rules_setup()
        return 'restored'

    steps = [
        (0.3, setup),
        (1.8, lambda: ('B beside A', _place_near(a_index, bkey, _side_spot(a_index, bkey)))),
        (3.0, teams(1, 1)),
        (3.1, friendly(False)),
        (3.3, mark('off')),
        (3.3, lambda: ('attack, friendly fire off', combat_tap(a_index, attack))),
        (4.8, friendly(True)),
        (5.0, mark('on')),
        (5.0, lambda: ('attack, friendly fire on', combat_tap(a_index, attack))),
        (6.5, teams(1, 2)),
        (6.6, friendly(False)),
        (6.8, mark('enemy')),
        (6.8, lambda: ('attack, different teams', combat_tap(a_index, attack))),
        (8.3, run.at('the kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (8.3, run.at('friendly fire off: the ally is not hit and does not react', ally_off)),
        (8.3, run.at('friendly fire on: an Ally hit with the Ally variant (damage 3)', ally_on)),
        (8.3, run.at('different teams: an Enemy hit with the move\'s damage (6)', enemy)),
        (8.3, run.at('friendly fire on: the ally loses 3, reacts from the frame of the hit, events say Ally', lambda: victim('on', st['F']['on'], st['F']['enemy'], 3.0, 'Combat.Relationship.Ally', 'enemy')())),
        (8.3, run.at('different teams: the enemy loses 6, reacts from the frame of the hit, events say Enemy', lambda: victim('enemy', st['F']['enemy'], 10 ** 9, 6.0, 'Combat.Relationship.Enemy')())),
        (8.4, teams(-1, -1)),
        (8.4, friendly(False)),
        (8.5, end),
        (8.6, lambda: ('logging off', _hit_logging(False))),
        (8.7, run.finish()),
    ]
    run.run(steps)


def scenario_hit_host(client=0):
    """M4 H8, the host as the attacker (listen server; combat-m4-hits 9b): the host attacks a client with the Root box. Judged on the host's own
    pawn (it is its own authority: the hit resolves the frame it happens, no correction), and on the clients' copies of it:
      - the frame of the first touch computed from the traced centres equals the hit frame, the hurtbox is the one picked
      - the hit reaches the victim client's pawn inbox on the authority the frame after
      - the host's hitstop window (6 frames, starting on the hit) is the one the clients see on their copy of the host pawn
      - the clients' copies of the host pawn made no hit of their own (they interpolate it: nothing is predicted there)
    The victim half (M4 H9c): the victim client reacts as in hit_basic (window from the frame of the hit, damage), and then the host is the victim: client 1
    hits the host, whose window opens on the frame of the hit, equal on both clients' copies of the host pawn, the damage on all three machines, one
    HitReceived, and the host rolls back zero times (it is its own authority).
    Listen server only: a dedicated server has no pawn to play (the suite records a skip)."""
    if detected_mode() != 'listen':
        _last_run.update(name='hit_host', results=[(None, 'skipped: the host plays only in a listen server', '')], verdict='PASS 0 checks (skipped)')
        return
    run = _Run('hit_host', client, judge_reconciles=False)
    hkey, vkey = run.key, key_of_player(1)
    attack = button_mask('Attack')
    st = {'F': {}}

    ckey = key_of_player(1)
    rec, h = [], {'h': None}

    def sampler(_dt):
        try:
            rec.append((run.now(), victim_info(0, ckey), victim_info(1, ckey), victim_info(0, hkey), victim_info(1, hkey), victim_info(2, hkey)))
        except Exception:
            pass

    def setup():
        hit_rules_setup()
        _hit_logging(True)
        st['kit'] = _apply_loadout(hkey, HIT_KIT)
        st['kit1'] = _apply_loadout(ckey, HIT_KIT)
        h['h'] = unreal.register_slate_post_tick_callback(sampler)
        return 'kits', (st['kit'], st['kit1'])

    def place():
        _place_near(0, vkey, (205.0, 0.0))
        return 'placed'

    def check_hit():
        hits = [r for r in _hit_log(0, hkey) if r[0] >= st['F']['p']]
        trace = [r for r in _hit_trace(0, hkey) if r[0] >= st['F']['p']]
        blocks = _trace_blocks(trace)
        if len(hits) != 1 or not blocks:
            return False, f'hits {hits}, {len(blocks)} blocks'
        expect = _first_touch(blocks[0], HIT_BLOCKS[0][1], True, st['vloc'])
        ok = expect is not None and hits[0][0] == expect[0] and hits[0][4] == expect[1] and hits[0][2] == 0
        inbox = [e for e in _inbox_log(0, vkey) if e[0] == hits[0][0] + 1]
        return ok and len(inbox) == 1 and inbox[0][3] == hits[0][4], f'hit {hits[0]}, expected {expect}, inbox {inbox}'

    def windows():
        hits = [r for r in _hit_log(0, hkey) if r[0] >= st['F']['p']]
        if len(hits) != 1:
            return False, f'hits {hits}'
        host = _hit_window(0, hkey)
        seen = [_hit_window(w, hkey) for w in range(1, len(worlds()))]
        ok = host[0] == hits[0][0] and host[1] - host[0] == HIT_ATTACKER_HITSTOP and all(s == host for s in seen)
        return ok, f'hit frame {hits[0][0]}, host window {host}, clients {seen}'

    def proxies_silent():
        logs = [(w, _hit_log(w, hkey)) for w in range(1, len(worlds()))]
        return all(not log for _, log in logs), f'clients\' hit logs of the host pawn: {logs}'

    def victim_client():
        hits = [r for r in _hit_log(0, hkey) if r[0] >= st['F']['p']]
        if len(hits) != 1:
            return False, f'hits {hits}'
        fr = hits[0][0]
        auth, own = _victim_windows(rec, 1, fr)
        ok = len(auth) == 1 and auth[0][:2] == (fr, fr + 8) and auth[0][2] - auth[0][1] == 31 and own[:1] == [auth[0][:2]]
        return ok and damage_of(0, ckey) - st['d1'][0] == 8 and damage_of(1, ckey) - st['d1'][1] == 8, f'hit {fr}, windows {auth} / {own}, damage change {damage_of(0, ckey) - st["d1"][0]} / {damage_of(1, ckey) - st["d1"][1]}'

    def host_victim():
        hits = [r for r in _hit_log(0, ckey) if r[0] >= st['F']['v']]
        if len(hits) != 1:
            return False, f'client hits on the host {hits}'
        fr = hits[0][0]
        auth, _ = _victim_windows(rec, 3, fr)
        own = [(w[:2]) for w in windows_seen([(r[0], r[4], r[5]) for r in rec], 1) if w[0] >= fr]
        c1 = [w[:2] for w in windows_seen([(r[0], r[4], r[4]) for r in rec], 2) if w[0] >= fr]
        c2 = [w[:2] for w in windows_seen([(r[0], r[5], r[5]) for r in rec], 2) if w[0] >= fr]
        win = (fr, fr + 8)
        ok = len(auth) == 1 and auth[0][:2] == win and auth[0][2] - auth[0][1] == 31 and win in c1 and win in c2
        return ok, f'hit {fr}, host window {auth}, seen by client 1 {c1}, by client 2 {c2}'

    def host_damage():
        d = [damage_of(w, hkey) - st['d0'][w] for w in range(len(worlds()))]
        return d == [8, 8, 8], f'damage change of the host pawn on host / client 1 / client 2: {d}'

    def host_events():
        ev = [e for e in event_log(hkey) if e[0] == 'T' and e[1] == 'HitReceived' and e[2] >= st['F']['v']]
        late = [(w, [e for e in event_log(hkey, w) if e[2] >= st['F']['v']]) for w in range(1, len(worlds()))]
        return len(ev) == 1 and all(not rows for _, rows in late), f'host events {[(e[2], sorted(e[4])) for e in ev]}, clients\' {late}'

    def end():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        set_damage(hkey, 0)
        set_damage(ckey, 0)
        hit_rules_setup()
        return 'restored'

    steps = [
        (0.3, setup),
        (1.5, place),
        (2.8, lambda: ('located', st.__setitem__('vloc', tuple(snap(worlds()[0])[vkey]['location'])))),
        (2.85, lambda: ('damage baseline', st.__setitem__('d1', (damage_of(0, ckey), damage_of(1, ckey))))),
        (2.9, lambda: ('mark', st['F'].__setitem__('p', _latest_frame(0, hkey)))),
        (2.9, lambda: ('attack (Root)', combat_tap(0, attack))),
        (4.8, run.at('the kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (4.8, run.at('first touch frame, hurtbox and delivery to the victim inbox', check_hit)),
        (4.8, run.at('the host hitstop window is the one the clients see', windows)),
        (4.8, run.at('the clients made no hit on their copy of the host pawn', proxies_silent)),
        (4.8, run.at('the victim client reacts: window from the frame of the hit, hitstun 31 frames after it, 8 damage on both copies', victim_client)),
        (5.0, lambda: ('mark the damage and the rollbacks of the host', st.update(d0=[damage_of(w, hkey) for w in range(len(worlds()))], rb0=rollbacks(0)))),
        (5.1, lambda: ('the host in front of client 1', _place_near(1, hkey, (205.0, 0.0)))),
        (6.6, lambda: ('mark', st['F'].__setitem__('v', _latest_frame(0, ckey)))),
        (6.65, lambda: ('client 1 attacks the host', combat_tap(1, attack))),
        (9.2, run.at('the host as the victim: window from the frame of the hit, hitstun 31 frames after it, equal on both clients\' copies', host_victim)),
        (9.2, run.at('the host as the victim: 8 damage on the host and on both clients', host_damage)),
        (9.2, run.at('the host as the victim: one HitReceived, published by the host only', host_events)),
        (9.2, run.at('the host rolled back zero times', lambda: (rollbacks(0) == st['rb0'], f'host rollbacks {st["rb0"]} -> {rollbacks(0)}'))),
        (9.2, run.at('the client kit was accepted', lambda: (st.get('kit1') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit1")}'))),
        (9.3, end),
        (9.4, lambda: ('logging off', _hit_logging(False))),
        (9.5, run.finish()),
    ]
    run.run(steps)


# ---- M4 H9c: the victim scenarios. One hit rig: the attacker (client `client`) lands the Root box of the test move on the other client, one "shot" per
# hit, the knobs (rules, thresholds, hurtbox set, damage percent) set between shots. Judged on the sim ticks of the victim's authority copy and its
# own client's copy (the _Run extra samples), the victim_info windows, the published events and the rollbacks of the victim's client.
class _HitRig:
    def __init__(self, name, client=1, judge_reconciles=False):
        self.client = client
        self.vi = 2 if client == 1 else 1
        self.akey, self.vkey = key_of_player(client), key_of_player(self.vi)
        self.run = _Run(name, client, judge_reconciles=judge_reconciles, extra=[('v_srv', 0, self.vkey), ('v_cli', self.vi, self.vkey)])
        self.shots, self.vinfo, self.cursor, self.kit = {}, [], 0.3, None
        self.handle = unreal.register_slate_post_tick_callback(self._tick)

    def _tick(self, _dt):
        try:
            self.vinfo.append((self.run.now(), victim_info(0, self.vkey), victim_info(self.vi, self.vkey)))
        except Exception:
            pass

    def setup_step(self):
        def f():
            hit_rules_setup()
            _hit_logging(True)
            self.kit = _apply_loadout(self.akey, HIT_KIT)
            return 'kit', self.kit
        return (self.cursor, f)

    def end_steps(self, extra=None):
        """Checks the kit, restores the rules and the damage, stops sampling. `extra` = more restore callables"""
        def restore():
            if self.handle is not None:
                unreal.unregister_slate_post_tick_callback(self.handle)
                self.handle = None
            hit_rules_setup()
            set_damage(self.vkey, 0)
            for fn in extra or ():
                fn()
            _hit_logging(False)
            return 'restored'
        t = self.cursor + 0.2
        return [(t, self.run.at('the kit was accepted', lambda: (self.kit == 'OK', f'ServerApplyTestLoadout -> {self.kit}'))), (t, restore), (t + 0.2, self.run.finish())]

    def shot(self, name, prep=None, spot=(205.0, 0.0), wait=3.6, hold=None, release=None):
        """Timeline steps of one hit; advances the cursor. prep() runs 1.7 s before the attack (rules / damage), the victim is placed 1.6 s before it.
        hold() runs at the mark (the victim's stick), release() at the end"""
        st = {'name': name}
        self.shots[name] = st
        t = self.cursor

        def do_prep():
            if prep:
                prep()
            return 'prep', name

        def place():
            _place_near(self.client, self.vkey, spot)
            return 'placed', name

        def mark():
            st.update(F0=_latest_frame(0, self.akey), t0=self.run.now(), rb0=rollbacks(self.vi))
            st['dmg0'] = (damage_of(0, self.vkey), damage_of(self.vi, self.vkey))
            if hold:
                hold()
            return 'mark', st['F0']

        def attack():
            combat_tap(self.client, button_mask('Attack'))
            return 'attack', name

        def collect():
            F0, t0, t1 = st['F0'], st['t0'], self.run.now()
            st['hits'] = [r for r in _hit_log(0, self.akey) if r[0] >= F0]
            ev = [e for e in event_log(self.vkey) if e[2] >= F0]
            st['recv'] = [e for e in ev if e[0] == 'T' and e[1] == 'HitReceived']
            st['kos'] = [e for e in ev if e[1] == 'KO']
            st['srv'], st['cli'] = self.run._win('v_srv', t0, t1, sim=True), self.run._win('v_cli', t0, t1, sim=True)
            st['vinfo'] = [r for r in self.vinfo if r[0] >= t0]
            st['rb'] = rollbacks(self.vi) - st['rb0']
            st['dmg'] = (damage_of(0, self.vkey) - st['dmg0'][0], damage_of(self.vi, self.vkey) - st['dmg0'][1])
            if release:
                release()
            return 'collected', name, len(st['srv'])

        self.cursor = t + 1.75 + wait + 0.1
        return [(t, do_prep), (t + 0.1, place), (t + 1.7, mark), (t + 1.75, attack), (t + 1.75 + wait, collect)]

    # -- readers of a collected shot
    @staticmethod
    def tags(samples):
        return set().union(*[s['tags'] for s in samples] or [set()])

    def hit_tags(self, name):
        r = self.shots[name]['recv']
        return r[0][4] if r else set()

    def displacement(self, name, side='srv'):
        s = self.shots[name][side]
        if not s:
            return 1e9
        p0 = s[0]['loc']
        return max(math.dist(p0[:2], x['loc'][:2]) for x in s)

    def modes(self, name, side='srv'):
        return {s['mode'] for s in self.shots[name][side]}

    def one_hit(self, name):
        s = self.shots[name]
        ok = len(s['hits']) == 1 and len(s['recv']) == 1
        return ok, f"hits {s['hits']}, HitReceived {[(e[2], sorted(e[4])) for e in s['recv']]}"

    def window(self, name, hitstop=8):
        """(ok, detail): one victim window that starts on the hit frame and is `hitstop` frames long, equal on the owner"""
        s = self.shots[name]
        if len(s['hits']) != 1:
            return False, f"hits {s['hits']}"
        fr = s['hits'][0][0]
        auth = [w for w in windows_seen(s['vinfo'], 1) if w[0] >= fr]
        own = [w[:2] for w in windows_seen(s['vinfo'], 2) if w[0] >= fr]
        pairs = []
        for w in own:
            if not pairs or pairs[-1] != w:
                pairs.append(w)
        return [w[:2] for w in auth] == [(fr, fr + hitstop)] and pairs == [(fr, fr + hitstop)], f'hit frame {fr}, authority {auth}, owner {own}'

    def at_most_one_rollback(self, names):
        """The typical hit costs the victim's client at most one rollback: the median over the shots. A single shot may count more under packet loss
        (an idle pawn rolls back about once per 30 s at 60/15/2 %, so the 3.6 s window of a shot catches one now and then); hit_basic judges every
        hit strictly, and a systematic double rollback would show here as a median of 2"""
        counts = sorted(self.shots[n]['rb'] for n in names)
        return counts[len(counts) // 2] <= 1, f"rollbacks of the victim's client per hit {[(n, self.shots[n]['rb']) for n in names]} (median {counts[len(counts) // 2]})"


def scenario_stagger_threshold(client=1):
    """M4 H9c, the stagger test (force against the hurtbox set's threshold; the standard stagger force is the damage x 1, so 8 for a Root hit).
    A threshold of 9: a flinch only (hitstop window, damage, no hitstun, no displacement), the move would resume. Exactly 8: staggered (>=): hitstun
    31 frames after the window. Infinite (super armor): a flinch. Judged on the authority and the owner, one rollback per hit at most."""
    rig = _HitRig('stagger_threshold', client)
    run, vkey = rig.run, rig.vkey
    steps = [rig.setup_step()]
    steps += rig.shot('below', prep=lambda: set_hurtbox(stagger=9.0, infinite=False))
    steps += rig.shot('equal', prep=lambda: set_hurtbox(stagger=8.0, infinite=False))
    steps += rig.shot('infinite', prep=lambda: set_hurtbox(stagger=8.0, infinite=True))
    S = rig.shots

    def flinch(name):
        def f():
            ok1, d1 = rig.one_hit(name)
            tags = {side: rig.tags(S[name][side]) for side in ('srv', 'cli')}
            actions = {r[1][1] for r in S[name]['vinfo']} | {r[2][1] for r in S[name]['vinfo']}
            ok = (ok1 and 'Combat.Event.Hit.Staggered' not in rig.hit_tags(name) and all('Combat.State.Hitstop' in t and 'Combat.State.Hitstun' not in t and 'Combat.Lock.Reaction' not in t for t in tags.values())
                  and not actions & {2, 3, 4} and S[name]['dmg'] == (8.0, 8.0) and rig.displacement(name) < 5.0)
            return ok, f"{d1}; tags srv {sorted(t for t in tags['srv'] if 'Combat' in t)}, owner {sorted(t for t in tags['cli'] if 'Combat' in t)}, actions {sorted(actions)}, damage {S[name]['dmg']}, moved {rig.displacement(name):.1f} cm"
        return f

    def staggered():
        ok1, d1 = rig.one_hit('equal')
        hits = S['equal']['hits']
        fr = hits[0][0] if hits else -1
        wins = {side: [w for w in windows_seen(S['equal']['vinfo'], side) if w[0] >= fr] for side in (1, 2)}
        tags = {side: rig.tags(S['equal'][side]) for side in ('srv', 'cli')}
        ok = (ok1 and 'Combat.Event.Hit.Staggered' in rig.hit_tags('equal') and wins[1] == [(fr, fr + 8, fr + 8 + 31)] and [w[:2] for w in wins[2]][0] == (fr, fr + 8)
              and all('Combat.State.Hitstun' in t for t in tags.values()) and S['equal']['dmg'] == (8.0, 8.0))
        return ok, f'{d1}; windows authority {wins[1]}, owner {wins[2]}'

    t = rig.cursor + 0.1
    steps += [
        (t, run.at('threshold 9 (force 8): a flinch only: window, damage, no hitstun, no displacement', flinch('below'))),
        (t, run.at('threshold 8 (force 8): staggered, hitstun 31 frames after the window, the same on the owner', staggered)),
        (t, run.at('infinite threshold: a flinch only', flinch('infinite'))),
        (t, run.at('the flinch window is 8 frames from the frame of the hit, equal on the owner', lambda: (rig.window('below')[0] and rig.window('infinite')[0], f"{rig.window('below')[1]} / {rig.window('infinite')[1]}"))),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(('below', 'equal', 'infinite')))),
    ]
    rig.cursor = t
    steps += rig.end_steps()
    run.run(steps)


def _presentation_comp(wi, key):
    return _hit_comp(wi, key).get_owner().get_component_by_class(unreal.GSTN_HitPresentationComponent)


def _presentation_info(wi, key):
    """(started, cancelled, cues played, cues suppressed, active effects, last hitstop ms) of the attacker's presentation component on world wi"""
    return tuple(float(x) for x in _presentation_comp(wi, key).get_presentation_info().split(','))


def _lagcomp_info(wi, key):
    """(rewinds, requested ms, applied ms, current ms) of the last rewound attacker step of a pawn on world wi's authority"""
    return tuple(float(x) for x in _hit_comp(wi, key).get_lag_comp_info().split(','))


def scenario_hit_lagcomp(client=1):
    """M4 H10, lag compensation and presentation of the attacker side. The remote client attacks the other client (three hits); the authority resolves
    the attacker against the victims as it saw them (rewound view, capped at the rules' MaxRewindMs). Judged on:
      - the authority built rewound views for the attacker (its counter moved, the applied time is inside the cap) and never for the host pawn
      - the attacker's client showed each hit through its presentation component (started == hits, the meshes it paused were released)
      - the attacker's predicted hitstop window equals the authority's (the clean case; a mismatch is logged, a hit that was corrected is not a failure)
      - the victim's client rolled back at most once per hit (median); the number of rollbacks per hit is the count of hits on the local player that
        arrived as corrections (input to the CM 12.7 decision)
    Numbers are logged in the check details."""
    rig = _HitRig('hit_lagcomp', client)
    run, akey, vkey = rig.run, rig.akey, rig.vkey
    names = ('a', 'b', 'c')
    st = {}
    steps = [rig.setup_step()]
    steps += [(rig.cursor, lambda: ('baseline', st.update(lc0=_lagcomp_info(0, akey), pr0=_presentation_info(client, akey))))]
    for n in names:
        steps += rig.shot(n)
    S = rig.shots

    def rewound():
        lc = _lagcomp_info(0, akey)
        cap = float(hit_rules().get_editor_property('max_rewind_ms'))
        ok = lc[0] - st['lc0'][0] >= len(names) and 0 < lc[2] and lc[3] - lc[2] <= cap + 1e-3
        return ok, f'rewound attacker steps {st["lc0"][0]:.0f} -> {lc[0]:.0f}, requested {lc[1]:.1f} ms, applied {lc[2]:.1f} ms, current {lc[3]:.1f} ms (depth {lc[3] - lc[2]:.1f} ms, cap {cap:.0f}; the view the attacker showed was {lc[3] - lc[1]:.1f} ms behind)'

    def host_never():
        info = _lagcomp_info(0, key_of_player(0)) if detected_mode() == 'listen' else (0.0,)
        return info[0] == 0.0, f'the host pawn rewound steps: {info[0]:.0f}'

    def shown():
        pr = _presentation_info(client, akey)
        started, cancelled = pr[0] - st['pr0'][0], pr[1] - st['pr0'][1]
        paused = _presentation_comp(client, akey).get_num_paused_meshes()
        return started >= len(names) and paused == 0, f'presentation on the attacker\'s client: started {started:.0f}, cancelled {cancelled:.0f}, cues played {pr[2]:.0f}, active {pr[4]:.0f}, paused meshes {paused}, last hitstop {pr[5]:.0f} ms'

    def hitstop_equal():
        seen = []
        for n in names:
            hits = S[n]['hits']
            seen.append((n, hits[0][6] if hits else None))
        # The windows of the pawn on the authority and on the attacker's client after the last shot
        a, o = _hit_window(0, akey), _hit_window(client, akey)
        pred = [h for _, h in seen if h]
        return len(pred) == len(names) and a == o, f'attacker hitstop frames per hit (predicted, from its log) {seen}; last window authority {a}, attacker {o}'

    def victim_hit_only():
        # The test move can also touch the host pawn standing beside the lane (an unrelated hit, the same without lag compensation): the victim's hit is
        # the one its HitReceived follows one frame later
        for n in names:
            frames = {e[2] for e in S[n]['recv']}
            S[n]['hits'] = [h for h in S[n]['hits'] if h[0] + 1 in frames]

    t = rig.cursor + 0.1
    steps += [
        (t, lambda: ('victim hit of each shot', victim_hit_only())),
        (t, run.at('one hit per shot', lambda: (all(rig.one_hit(n)[0] for n in names), '; '.join(rig.one_hit(n)[1] for n in names)))),
        (t, run.at('the authority resolved the attacker against a rewound view inside the cap', rewound)),
        (t, run.at('the host pawn never rewinds', host_never)),
        (t, run.at('the attacker\'s client showed every hit and released its paused meshes', shown)),
        (t, run.at('the attacker\'s predicted hitstop equals the authority\'s', hitstop_equal)),
        (t, run.at('at most one rollback per hit on the victim\'s client (median); the counts are the corrections of hits on the local player', lambda: rig.at_most_one_rollback(names))),
        (t, run.at('windows of the three hits equal on the authority and the victim\'s client', lambda: (all(rig.window(n)[0] for n in names), '; '.join(rig.window(n)[1] for n in names)))),
    ]
    rig.cursor = t
    steps += rig.end_steps()
    run.run(steps)

def scenario_reaction_tiers(client=1):
    """M4 H9c, the reaction tiers. The tier is the knockback K against the thresholds, capped by the hurtbox set's MaxReaction; the harness moves the
    thresholds (K itself is a few hundred cm/s for a Root hit on a fresh victim) so each tier is hit with the same blow:
      - StaggerInPlace: no displacement, stays walking
      - PushBack: slides on the ground, never leaves Walking, no tumble
      - Launch: leaves the ground (Falling), no tumble below the tumble threshold
      - Tumble: Launch above the tumble threshold, Combat.State.Tumble projected, the event says Tumble
      - MaxReaction caps a launch to a push back and a push back to a stagger
    One rollback per hit at most on the victim's client."""
    rig = _HitRig('reaction_tiers', client)
    run = rig.run
    far = 1.0e6
    R = unreal.GSTN_HitReaction

    def tiers(push, launch, tumble=far, cap=None):
        def f():
            set_tuning(push_back_threshold=push, launch_threshold=launch, tumble_threshold=tumble)
            set_hurtbox(max_reaction=cap or R.LAUNCH)
        return f
    steps = [rig.setup_step()]
    steps += rig.shot('stagger', prep=tiers(far, far))
    steps += rig.shot('pushback', prep=tiers(0.0, far))
    steps += rig.shot('launch', prep=tiers(0.0, 0.0))
    steps += rig.shot('tumble', prep=tiers(0.0, 0.0, 0.0))
    steps += rig.shot('cap_push', prep=tiers(0.0, 0.0, 0.0, R.PUSH_BACK))
    steps += rig.shot('cap_stagger', prep=tiers(0.0, 0.0, 0.0, R.STAGGER_IN_PLACE))
    S = rig.shots
    TAG = 'Combat.Event.Hit.Reaction.'

    def tier(name, tag, walking_only, moves, tumble=False, state_tumble=False):
        def f():
            ok1, d1 = rig.one_hit(name)
            tg = rig.hit_tags(name)
            others = {TAG + 'StaggerInPlace', TAG + 'PushBack', TAG + 'Launch'} - {TAG + tag}
            seen = {side: rig.tags(S[name][side]) for side in ('srv', 'cli')}
            moved = {side: rig.displacement(name, side) for side in ('srv', 'cli')}
            modes = {side: rig.modes(name, side) for side in ('srv', 'cli')}
            ok = (ok1 and TAG + tag in tg and not tg & others and ('Combat.Event.Hit.Tumble' in tg) == tumble
                  and all(('Combat.State.Tumble' in t) == state_tumble for t in seen.values())
                  and all((m > 25.0) == moves for m in moved.values())
                  and all((modes[s] <= {'Walking'}) == walking_only for s in modes))
            return ok, f'{d1}; moved {moved}, modes {modes}, tumble state {[("Combat.State.Tumble" in t) for t in seen.values()]}'
        return f
    t = rig.cursor + 0.1
    steps += [
        (t, run.at('StaggerInPlace: no displacement, still walking', tier('stagger', 'StaggerInPlace', True, False))),
        (t, run.at('PushBack: slides on the ground (> 25 cm), never airborne', tier('pushback', 'PushBack', True, True))),
        (t, run.at('Launch: leaves the ground, no tumble', tier('launch', 'Launch', False, True))),
        (t, run.at('Tumble: Launch above the tumble threshold, state and event', tier('tumble', 'Launch', False, True, tumble=True, state_tumble=True))),
        (t, run.at('MaxReaction PushBack caps a launch: slides, stays on the ground', tier('cap_push', 'PushBack', True, True, tumble=False))),
        (t, run.at('MaxReaction StaggerInPlace caps a push back: no displacement', tier('cap_stagger', 'StaggerInPlace', True, False))),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(tuple(S)))),
    ]
    rig.cursor = t
    steps += rig.end_steps()
    run.run(steps)


def _yaw_elev(v):
    """(yaw degrees of the horizontal part, elevation degrees) of a velocity"""
    h = math.hypot(v[0], v[1])
    return math.degrees(math.atan2(v[1], v[0])), math.degrees(math.atan2(v[2], h))


def _launch_velocity(samples):
    """Velocity of the first sim tick of a launch (the first Falling tick of the victim with a real horizontal speed)"""
    for s in samples:
        if s['mode'] == 'Falling' and math.hypot(s['vel'][0], s['vel'][1]) > 100.0:
            return s['vel']
    return None


def scenario_di(client=1):
    """M4 H9c, directional influence: the stick the victim holds when the hitstop ends bends the launch (at most 15 degrees of yaw across it; along it
    the elevation moves, in and out opposite ways). Same blow every time (Launch forced through the thresholds), the victim locked in place (it cannot walk, the stick feeds DI only), the stick held from the attack on:
    neutral, left, right (across the launch), toward the attacker, away. Judged on the victim's authority copy at the first airborne tick; the owner's
    copy must show the same launch (within a degree). Tilted gravity (Up not Z) is not driven by this scenario: the Core tests cover ApplyDI with a
    rotated frame (GSTNCombat.Core.Impact.*), the harness has no per-victim gravity yet."""
    rig = _HitRig('di', client)
    run, S = rig.run, rig.shots
    far = 1.0e6

    def prep():
        # The victim cannot walk (movement lock effect, the stats modifier zeroes the speed) but its stick reaches the combat tick: it stays in the blow
        set_tuning(push_back_threshold=0.0, launch_threshold=0.0, tumble_threshold=far)
        comps(worlds()[0])[rig.vkey].server_set_movement_lock(True)

    def unlock():
        comps(worlds()[0])[rig.vkey].server_set_movement_lock(False)

    def stick(kind):
        def f():
            v = snap(worlds()[0])
            a, p = v[rig.akey]['location'], v[rig.vkey]['location']
            n = max(math.hypot(p[0] - a[0], p[1] - a[1]), 1e-3)
            d = ((p[0] - a[0]) / n, (p[1] - a[1]) / n)
            left = (-d[1], d[0])
            tgt = {'left': (p[0] + 300 * left[0], p[1] + 300 * left[1]), 'right': (p[0] - 300 * left[0], p[1] - 300 * left[1]),
                   'in': (p[0] - 300 * d[0], p[1] - 300 * d[1]), 'out': (p[0] + 300 * d[0], p[1] + 300 * d[1])}[kind]
            hold(rig.vi, 'move', _steer_value(rig.vi, tgt))
        return f

    def let_go():
        release(rig.vi, 'move')

    steps = [rig.setup_step()]
    steps += rig.shot('neutral', prep=prep)
    for kind in ('left', 'right', 'in', 'out'):
        steps += rig.shot(kind, prep=prep, hold=stick(kind), release=let_go)

    def launch(name, side='srv'):
        v = _launch_velocity(S[name][side])
        return _yaw_elev(v) if v else None

    def across():
        n, l, r = launch('neutral'), launch('left'), launch('right')
        if not (n and l and r) or any(len(S[k]['hits']) != 1 for k in ('neutral', 'left', 'right')):
            return False, f'launches neutral {n}, left {l}, right {r}, hits {[len(S[k]["hits"]) for k in ("neutral", "left", "right")]}'
        dl, dr = (l[0] - n[0] + 180) % 360 - 180, (r[0] - n[0] + 180) % 360 - 180
        return dl * dr < 0 and 10.0 <= abs(dl) <= 25.0 and 10.0 <= abs(dr) <= 25.0, f'yaw change left {dl:.1f}, right {dr:.1f} degrees (neutral {n[0]:.1f}, elevations {n[1]:.1f} / {l[1]:.1f} / {r[1]:.1f})'

    def along():
        n, i, o = launch('neutral'), launch('in'), launch('out')
        if not (n and i and o) or any(len(S[k]['hits']) != 1 for k in ('in', 'out')):
            return False, f'launches neutral {n}, in {i}, out {o}'
        di, do = i[1] - n[1], o[1] - n[1]
        return di * do < 0 and abs(di) > 2.0 and abs(do) > 2.0 and abs((i[0] - n[0] + 180) % 360 - 180) < 6.0, f'elevation change in {di:.1f}, out {do:.1f} degrees (neutral {n[1]:.1f}), yaw change in {i[0] - n[0]:.1f}'

    def owner_launched():
        out = [(n, launch(n, 'cli') is not None) for n in S]
        return all(ok for _, ok in out), f'the owner copy was airborne after each blow: {out} (its first airborne tick is predicted and re-simulated, so the launch is judged on the authority)'

    t = rig.cursor + 0.1
    steps += [
        (t, run.at('each blow launched the victim (Falling, one hit)', lambda: (all(launch(n) is not None and len(S[n]['hits']) == 1 for n in S), f'{[(n, launch(n) is not None, len(S[n]["hits"])) for n in S]}'))),
        (t, run.at('stick across the launch: yaw bent to opposite sides by 10 to 25 degrees (the rules limit is 15)', across)),
        (t, run.at('stick along the launch: the elevation moves, in and out opposite ways, yaw kept', along)),
        (t, run.at('the owner copy was airborne after each blow', owner_launched)),
        (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(tuple(S)))),
    ]
    rig.cursor = t
    steps += rig.end_steps(extra=[let_go, unlock])
    run.run(steps)


def scenario_ko_zone(client=1):
    """M4 H9c, knock-outs. A control hit that stays under the damage cap: no KO. A KO zone spawned on every machine (SpawnZone) with the victim
    teleported into it: one Combat.Event.KO with KOReason.Zone published by the host (none on the clients), the victim in the KO state on the authority
    and on its own client. After the respawn (reset) the state is Actionable again. The health cap: a hit that takes the damage past the rules' max health
    knocks out with KOReason.Health, the attacker being the instigator. Zones are removed at the end."""
    rig = _HitRig('ko_zone', client)
    run, S = rig.run, rig.shots
    KO = 5
    st = {}

    def lane_pos():
        lane = lane_keys().index(rig.vkey)
        c = comps(worlds()[0])[rig.vkey]
        sp, off = c.get_spawn_location(), lane_offset(lane)
        return off, (sp.x + off.x, sp.y + off.y, sp.z + off.z)

    def spawn_zone():
        off, p = lane_pos()
        centre = unreal.Vector(p[0] + 600.0, p[1], p[2])
        for w in worlds():
            unreal.GSTN_KOZone.spawn_zone(w, centre, unreal.Vector(150.0, 150.0, 400.0))
        st['zone_f'] = _latest_frame(0, rig.akey)
        comps(worlds()[0])[rig.vkey].server_reset_to_spawn(unreal.Vector(off.x + 600.0, off.y, off.z))
        return 'zone at', (centre.x, centre.y)

    def clear_zones():
        for w in worlds():
            unreal.GSTN_KOZone.destroy_all_zones(w)
        return 'zones removed'

    def respawn():
        off, _ = lane_pos()
        comps(worlds()[0])[rig.vkey].server_reset_test_state(off)
        set_damage(rig.vkey, 0)
        return 'respawned'

    def kos(wi, key, since):
        return [e for e in event_log(key, wi) if e[1] == 'KO' and e[2] >= since]

    def zone_ko():
        ev = kos(0, rig.vkey, st['zone_f'])
        clients = [(w, kos(w, rig.vkey, st['zone_f'])) for w in range(1, len(worlds()))]
        acts = (victim_info(0, rig.vkey)[1], victim_info(rig.vi, rig.vkey)[1])
        ok = len(ev) == 1 and 'Combat.KOReason.Zone' in ev[0][4] and all(not c for _, c in clients) and acts == (KO, KO)
        return ok, f'host events {ev}, clients {clients}, action state (authority, owner) {acts}'

    def alive_again():
        acts = (victim_info(0, rig.vkey)[1], victim_info(rig.vi, rig.vkey)[1])
        return acts == (0, 0), f'action state (authority, owner) {acts}'

    def no_ko_control():
        s = S['control']
        return len(s['hits']) == 1 and not s['kos'] and victim_info(0, rig.vkey)[1] != KO, f"hits {s['hits']}, KO events {s['kos']}, damage {s['dmg']}"

    def health_ko():
        s = S['cap']
        att = [e for e in event_log(rig.akey) if e[1] == 'KO' and e[2] >= s['F0']]
        ok = (len(s['hits']) == 1 and len(s['kos']) == 1 and 'Combat.KOReason.Health' in s['kos'][0][4] and len(att) == 1 and att[0][0] == 'I'
              and victim_info(0, rig.vkey)[1] == KO and victim_info(rig.vi, rig.vkey)[1] == KO and not kos(1, rig.vkey, s['F0']) and not kos(2, rig.vkey, s['F0']))
        return ok, f"hits {s['hits']}, victim's KO events {s['kos']}, attacker's {att}, states ({victim_info(0, rig.vkey)[1]}, {victim_info(rig.vi, rig.vkey)[1]}), damage {s['dmg']}"

    steps = [rig.setup_step()]
    steps += rig.shot('control', prep=lambda: (hit_rules().set_editor_property('damage_mode', unreal.GSTN_DamageMode.PERCENT_CAPPED), set_damage(rig.vkey, 60)))
    t = rig.cursor
    steps += [(t, run.at('a hit that stays under the cap does not knock out', no_ko_control)), (t, respawn)]
    t += 1.0
    steps += [(t, spawn_zone), (t + 2.0, run.at('zone: one KO with KOReason.Zone on the host, none on the clients, KO state on both copies', zone_ko)),
              (t + 2.1, clear_zones), (t + 2.2, respawn), (t + 4.5, run.at('after the respawn the victim is Actionable again on both copies', alive_again))]
    rig.cursor = t + 4.6
    steps += rig.shot('cap', prep=lambda: (hit_rules().set_editor_property('damage_mode', unreal.GSTN_DamageMode.PERCENT_CAPPED), set_damage(rig.vkey, 95)))
    t = rig.cursor
    steps += [(t, run.at('health cap: one KO with KOReason.Health, the attacker is its instigator, host only, KO state on both copies', health_ko)),
              (t, run.at('at most one rollback per hit on the victim\'s client', lambda: rig.at_most_one_rollback(('control', 'cap'))))]
    rig.cursor = t
    steps += rig.end_steps(extra=[clear_zones, respawn])
    run.run(steps)


# ---- M4 H11b: the trampoline (GSTNLevelObjects). A trampoline is static level data, so the scenario spawns the same one in every world of the process
# (UGSTN_TrampolineTestLibrary) with the same arguments, as a level would carry them; it is not replicated.
TRAMPOLINE_KINDS = (
    # name, lean of the launch face towards the pawn's facing (degrees from world up), size (depth, width, height) in cm, launch speed cm/s,
    # height of the capsule centre above the face the pawn is dropped from. The wall one is a face 70 degrees from the horizontal: running into a vertical
    # face needs a stick direction relative to a camera the harness cannot rely on, so the pawn is dropped on a steep face instead
    ('flat', 0.0, (400.0, 400.0, 60.0), 1100.0, 220.0),
    ('tilted', 20.0, (300.0, 300.0, 80.0), 1000.0, 240.0),
    ('wall', 70.0, (500.0, 260.0, 60.0), 900.0, 260.0),
)


def _surface_launch(wi, key):
    """(count, frame, vx, vy, vz, surface type index, incoming speed) of the launches the surface probe of world wi's copy queued; the count includes
    re-simulated frames on a client"""
    p = _hit_comp(wi, key).get_surface_launch_info().split(',')
    return int(p[0]), int(p[1]), float(p[2]), float(p[3]), float(p[4]), int(p[5]), float(p[6])


def _trampolines_everywhere(center, up, size, speed):
    return [unreal.GSTN_TrampolineTestLibrary.spawn_test_trampoline(w, unreal.Vector(*center), unreal.Vector(*up), unreal.Vector(*size), speed,
                                                                    unreal.Vector(0.0, 0.0, 1.0), False, True, False) for w in worlds()]


def _clear_trampolines():
    return [unreal.GSTN_TrampolineTestLibrary.clear_test_trampolines(w) for w in worlds()]


def _lane_state(key):
    """((x, y) of the pawn's lane, floor z under it, yaw) from the pawn at rest on its lane"""
    c = comps(worlds()[0])[key]
    off, sp = lane_offset(lane_keys().index(key)), c.get_spawn_location()
    a = snap(worlds()[0])[key]
    hh = c.get_owner().get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    return (sp.x + off.x, sp.y + off.y), a['location'][2] - hh, a['yaw']


def scenario_trampoline(client=1, shot_time=5.0):
    """M4 H11b, the trampoline (GSTNLevelObjects) and the launch of a combatant that is not tumbling. Two pawns (the client's, and the host's in a listen
    session or the second client's on a dedicated server) are dropped onto a flat, a tilted and a steep trampoline of different sizes and speeds, each
    pawn on its own lane. Wanted per pawn and trampoline: the surface probe queued one launch on the authority, with the velocity along the trampoline's up
    at its speed and the Surface.Type.Trampoline index; a predicting client queued the same velocity; the client's trajectory equals the authority's
    (max distance at equal simulation times) from the launch on, and where the pawn comes to rest the two agree; a host pawn rolled back 0 times. Last
    shot: a TUMBLING victim thrown into a trampoline wall by a real hit bounces once (one Combat.Event.Impact.Bounce on the host, one bounce counted by
    its step) and the surface probe does not launch it."""
    rig = _HitRig('trampoline', client)
    run, vi = rig.run, rig.vi
    listen = detected_mode() == 'listen'
    pawns = [('A', rig.akey, client, 'srv', 'cli')]
    if listen:
        hk = key_of_player(0)
        run.extra += [('h_srv', 0, hk), ('h_prox', client, hk)]
        pawns.append(('H', hk, 0, 'h_srv', 'h_prox'))
    else:
        pawns.append(('V', rig.vkey, vi, 'v_srv', 'v_cli'))
    trampoline_type = None
    try:
        trampoline_type = unreal.GSTN_SurfaceUtils.get_surface_type_index(unreal.GameplayTagLibrary.make_literal_gameplay_tag(unreal.GameplayTag('Surface.Type.Trampoline')))
    except Exception:
        pass
    S = {}   # kind -> {'up': {label: up}, 'speed', 'launch': {label: (auth, own)}, 'win': {label: {side: [...]}}, 'rb': {label: n}}

    def shot_start(kind):
        name, lean, size, speed, drop = kind
        s = S[name] = {'up': {}, 'speed': speed, 'launch': {}, 'win': {}, 'rb': {}, 'n0': {}, 'drop': drop, 'size': size}

        def f():
            _clear_trampolines()
            s['t0'] = run.now()
            for label, key, oi, _, _ in pawns:
                (x, y), floor, yaw = _lane_state(key)
                a = math.radians(lean)
                up = (math.cos(math.radians(yaw)) * math.sin(a), math.sin(math.radians(yaw)) * math.sin(a), math.cos(a))
                centre = (x, y, floor + size[2] / 2.0) if lean == 0.0 else (x, y, floor + 300.0)
                face = tuple(centre[i] + up[i] * size[2] / 2.0 for i in range(3))
                _trampolines_everywhere(centre, up, size, speed)
                s['up'][label] = up
                s['n0'][label] = _surface_launch(0, key)[0]
                s['rb'][label] = rollbacks(oi) if oi else rollbacks(0)
                comps(worlds()[0])[key].server_teleport_exact(unreal.Vector(face[0], face[1], face[2] + drop))
            return 'dropped', name
        return f

    def shot_first_launch(name):
        def f():
            s = S[name]
            for label, key, oi, _, _ in pawns:
                s['launch'][label] = (_surface_launch(0, key), _surface_launch(oi, key) if oi else None)
            return 'first launch read', name
        return f

    def shot_collect(name):
        def f():
            s = S[name]
            t0, t1 = s['t0'] + 1.0, run.now()
            for label, key, oi, a, b in pawns:
                s['win'][label] = {a: run._win(a, t0, t1, sim=True), b: run._win(b, t0, t1, sim=(b != 'h_prox'))}  # a proxy runs no simulation: editor-tick samples
                s['rb'][label] = (s['rb'][label], rollbacks(oi) if oi else rollbacks(0))
            return 'collected', name
        return f

    def traj(entries):
        out = {}
        for e in entries:
            out[e['ms']] = e   # the last entry of a simulation time is the one that stood (a resimulated tick comes later)
        return out

    def check_launch(name, label):
        def f():
            s = S[name]
            (auth, own) = s['launch'][label]
            n = auth[0] - s['n0'][label]
            want = tuple(u * s['speed'] for u in s['up'][label])
            dv = math.dist(auth[2:5], want)
            ok = n == 1 and dv < 1.0 and (trampoline_type is None or auth[5] == trampoline_type)
            detail = f'launches queued on the authority {n}, velocity {tuple(round(v, 1) for v in auth[2:5])} wanted {tuple(round(v, 1) for v in want)} (off by {dv:.2f}), type index {auth[5]} (Trampoline {trampoline_type}), arrival speed {auth[6]:.0f}'
            if own is not None:
                dv_own = math.dist(own[2:5], auth[2:5])
                ok = ok and own[0] >= 1 and own[1] == auth[1] and dv_own < 1.0
                detail += f'; owner: count {own[0]} (re-simulated frames count), frame {own[1]} vs {auth[1]}, velocity off by {dv_own:.2f}'
            return ok, detail
        return f

    def check_traj(name, label, a, b):
        def f():
            w = S[name]['win'][label]
            ta, tb = traj(w[a]), traj(w[b])
            common = sorted(set(ta) & set(tb))
            if len(common) < 60:
                return False, f'only {len(common)} simulation times in common'
            worst = max(math.dist(ta[m]['loc'], tb[m]['loc']) for m in common)
            peak = max(math.dist(e['vel'], (0, 0, 0)) for e in ta.values())
            # 3 cm of position noise plus 0.15 of a simulation frame of travel at the peak speed (peak / 60 * 0.15): after a lost packet the owner is
            # corrected and re-simulates with a sub-frame offset of its own state, and the offset grows with the speed. Measured at 60/15/2 %: the flat and
            # the steep pad stay under 3 cm, the tilted one (peak 1550 cm/s) shows 4.7 to 5.3 cm, which this allows (3 + 3.9); a wrong prediction is tens of cm
            tolerance = 3.0 + peak / 400.0
            return worst < tolerance and peak > 0.4 * S[name]["speed"], f'{len(common)} equal-time ticks, worst distance authority vs owner {worst:.2f} cm (want < {tolerance:.2f}), peak speed {peak:.0f} (launch {S[name]["speed"]:.0f})'
        return f

    def check_rest(name, label, a, b):
        def f():
            w = S[name]['win'][label]
            ta, tb = traj(w[a]), traj(w[b])
            last = max(set(ta) & set(tb), default=None)
            if last is None:
                return False, 'no common tick'
            d = math.dist(ta[last]['loc'], tb[last]['loc'])
            moved = math.dist(ta[min(ta)]['loc'], ta[last]['loc'])
            return d < 3.0, f'last common tick {last}: distance {d:.2f} cm between {a} and {b}, authority moved {moved:.0f} cm from the first sample, mode {ta[last]["mode"]}'
        return f

    def check_proxy(name, label, a, b):
        # the host's pawn as a simulated proxy on the client: interpolated and delayed, so only where it comes to rest is compared (xy)
        def f():
            w = S[name]['win'][label]
            ea, eb = w[a][-1] if w[a] else None, w[b][-1] if w[b] else None
            if not ea or not eb:
                return False, 'no samples'
            d = math.dist(ea['loc'][:2], eb['loc'][:2])
            return d < 40.0, f'host pawn xy: authority {tuple(round(v) for v in ea["loc"][:2])}, proxy on the client {tuple(round(v) for v in eb["loc"][:2])}, {d:.0f} cm apart'
        return f

    def check_host_rollbacks(name, label):
        def f():
            r0, r1 = S[name]['rb'][label]
            return r1 - r0 == 0, f'host pawn rollbacks during the shot {r1 - r0}'
        return f

    def check_client_rollbacks(name, label):
        def f():
            r0, r1 = S[name]['rb'][label]
            return r1 - r0 <= 3, f'rollbacks of the predicting client during the shot {r1 - r0} (the teleport that drops the pawn is the server\'s: one correction, a few more at 2 % loss)'
        return f

    # ---- the tumbling victim thrown into a trampoline wall
    B = {}

    def bounce_prep():
        _clear_trampolines()
        combat_clear(client)
        set_damage(rig.vkey, IMPACT_DAMAGE)
        a = snap(worlds()[0])[rig.akey]
        fwd = (math.cos(math.radians(a['yaw'])), math.sin(math.radians(a['yaw'])))
        centre = IMPACT_FACE + 50.0
        c = (a['location'][0] + fwd[0] * centre, a['location'][1] + fwd[1] * centre, a['location'][2] + 200.0)
        up = (-fwd[0], -fwd[1], 0.0)   # the launch face looks at the attacker
        B['names'] = _trampolines_everywhere(c, up, (1000.0, 1000.0, 100.0), 1100.0)
        B['counts0'] = (_impact_counts(0, rig.vkey), _impact_counts(vi, rig.vkey))
        B['launch0'] = _surface_launch(0, rig.vkey)[0]
        B['fwd'] = fwd
        for other in [k for k in comps(worlds()[0]) if k not in (rig.akey, rig.vkey)]:
            _place_near(client, other, (-350.0, 350.0))
        return 'trampoline wall', B['names']

    def bounce_setup():
        hit_rules_setup()
        set_tuning(push_back_threshold=50.0, launch_threshold=100.0, tumble_threshold=200.0)
        B['rows'] = unreal.GSTN_ImpactTestLibrary.configure_impact_table(IMPACT_ROWS)
        unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(0, 0, False, 1.0)
        return 'impact table on', B['rows']

    def bounce_collect():
        B['counts1'] = (_impact_counts(0, rig.vkey), _impact_counts(vi, rig.vkey))
        B['launch1'] = _surface_launch(0, rig.vkey)[0]
        B['events'] = [e for e in event_log(rig.vkey, 0) if e[1] == 'ImpactBounce' and e[2] >= rig.shots['bounce']['F0']]
        return 'bounce collected'

    def check_bounce():
        s = rig.shots['bounce']
        d = tuple(b - a for a, b in zip(B['counts0'][0], B['counts1'][0]))
        yaw_f = B['fwd']
        vx = [e['vel'][0] * yaw_f[0] + e['vel'][1] * yaw_f[1] for e in s['srv']]
        out, back = max(vx, default=0.0), min(vx, default=0.0)
        ok = d == (0, 1, 0) and len(B['events']) == 1 and out > 150.0 and back < -150.0
        return ok, f'impacts (splat, bounce, break) on the authority {d}, Combat.Event.Impact.Bounce events on the host {len(B["events"])}, speed along the attacker facing: out up to {out:.0f}, back down to {back:.0f}'

    def check_no_launch_while_tumbling():
        return B['launch1'] == B['launch0'], f'surface launches queued for the tumbling victim: {B["launch1"] - B["launch0"]} (the row bounces it, the probe leaves it alone)'

    def end():
        _clear_trampolines()
        unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(0, 0, False, 1.0)
        unreal.GSTN_ImpactTestLibrary.configure_impact_table('')
        set_damage(rig.vkey, 0)
        hit_rules_setup()
        return 'trampolines removed'

    steps = [rig.setup_step(), (0.5, bounce_setup)]
    t = 2.0
    for kind in TRAMPOLINE_KINDS:
        name = kind[0]
        steps += [(t, shot_start(kind)), (t + 1.4, shot_first_launch(name)), (t + shot_time, shot_collect(name))]
        t += shot_time + 0.6
    steps += [(t, lambda: (_clear_trampolines(), 'cleared')[1]), (t + 0.2, lambda: ('lanes', len(reset_lanes())))]
    rig.cursor = t + 3.5
    steps += rig.shot('bounce', prep=bounce_prep, wait=3.6)
    t = rig.cursor
    steps += [(t, bounce_collect)]
    for kind in TRAMPOLINE_KINDS:
        name = kind[0]
        for label, key, oi, a, b in pawns:
            who = f'pawn {label}'
            steps += [(t, run.at(f'{name}: {who} launched once along the trampoline up at its speed, the owner queued the same', check_launch(name, label)))]
            if oi:
                steps += [(t, run.at(f'{name}: {who} trajectory equals the authority\'s on its predicting client', check_traj(name, label, a, b))),
                          (t, run.at(f'{name}: {who} lands where the authority does', check_rest(name, label, a, b))),
                          (t, run.at(f'{name}: {who} client rollbacks', check_client_rollbacks(name, label)))]
            else:
                steps += [(t, run.at(f'{name}: {who} (host) comes to rest where its proxy on the client does', check_proxy(name, label, a, b))),
                          (t, run.at(f'{name}: {who} (host) rolled back 0 times', check_host_rollbacks(name, label)))]
    steps += [(t, run.at('a tumbling victim bounces once off a trampoline wall (one Combat.Event.Impact.Bounce)', check_bounce)),
              (t, run.at('the surface probe does not launch a tumbling victim', check_no_launch_while_tumbling))]
    rig.cursor = t
    steps += rig.end_steps(extra=[end])
    run.run(steps)


# ---- Suite runner (C15): runs scenarios that report through _Run one after the other inside a single editor tick callback, so one python call
# starts the whole batch and a later one reads suite_report(). reset_all() and a settle pause come before each; a scenario that does not
# report within its timeout is recorded as TIMEOUT and the suite moves on. loadout_change goes next to last (its loadout outlives the scenario), then hit_on_wall and hitstop_montage: after hit_on_wall and after the 12 s montage of hitstop_montage the next scenario failed (command_set landing z, jump_squat rising tick), so nothing runs after them.
# The scenarios that walk (weapon_switch, combat_context) come first: the later ones dash and drift the pawn into the corner of the map, and a
# pawn against the wall cannot walk. The suite also walks the pawn back to where it started before each scenario.
# One lane per pawn, 15 m apart: a dodge or a knockback moves a pawn a few metres, never far enough to reach a neighbour (the lanes were 5 m apart
# along -X and a slide of C14 ended in the next pawn's capsule). Probed on the harness level: the floor is clear for 20+ m to -X and -Y of the spawn
# (the wall of scenario_jump_wall is on the +X side, x about 1000); +Y of it there are props, so the third lane goes to -Y, not +Y.
LANE_SPACING = 1500.0
LANE_POSE_TOL = 30.0       # cm, horizontal: the pose the sim confirms before a scenario starts (a teleport next to a prop is nudged a few cm)
LANE_Z_TOL = 120.0   # a dedicated session recorded the spawn at z 0.3 (the pawn stands at 87): the pose, not the height, is what the reset check is for


def lane_offset(lane):
    """Offset from the spawn of lane `lane` (the index of the pawn in the sorted keys): the spawn, one spacing to -X, one spacing to -Y, then both."""
    x, y = ((0, 0), (-1, 0), (0, -1), (-1, -1))[lane % 4]
    return unreal.Vector(LANE_SPACING * x, LANE_SPACING * y, 0.0)


def lane_keys():
    return sorted(comps(worlds()[0]), key=str)


def reset_lanes():
    """Authoritative reset of every pawn to its lane (pose, velocity zero, mode Walking, leftover test state) and of the stats probe flag on every
    machine's copy. Returns the target pose per key so lanes_settled can check it on every world."""
    targets = {}
    for lane, key in enumerate(lane_keys()):
        c = comps(worlds()[0])[key]
        off = lane_offset(lane)
        c.server_reset_test_state(off)
        sp = c.get_spawn_location()
        targets[key] = (sp.x + off.x, sp.y + off.y, sp.z + off.z)
    for w in worlds():
        for c in comps(w).values():
            c.set_stats_probe_enabled(False)
    return targets


def lanes_settled(targets):
    """(ok, detail): on EVERY world each pawn stands on its lane origin, at rest, in Walking (the pose the sim confirmed, not just the server's)."""
    bad = []
    for i, w in enumerate(worlds()):
        for key, v in snap(w).items():
            t = targets.get(key)
            if t is None:
                continue
            loc, vel = v['location'], v['velocity']
            d = ((loc[0] - t[0]) ** 2 + (loc[1] - t[1]) ** 2) ** 0.5
            speed = (vel[0] ** 2 + vel[1] ** 2 + vel[2] ** 2) ** 0.5
            if d > LANE_POSE_TOL or abs(loc[2] - t[2]) > LANE_Z_TOL or speed > 10.0 or v.get('mode') != 'Walking':
                bad.append(f"w{i}/{key}: {d:.0f} cm off, speed {speed:.0f}, {v.get('mode')}")
    return not bad, '; '.join(bad)


# ---- M4 H11: surface impacts of a tumbling victim (combat-m4-hits 7b). The attacker (client) hits the other client with the Root box of the test move;
# the thresholds are lowered so the hit is a tumble, and the victim flies along the attacker's facing into a box the scenario spawns at run time
# (AGSTN_TestSurface, a GSTNSurface provider with a chosen Surface.Type, replicated: no map edit). The impact table is built from rows here
# (UGSTN_ImpactTestLibrary.configure_impact_table) and handed to every combat component of every world.
IMPACT_ROWS = ('Wall,1,100,0.5,20,1,0,7,0.7;'          # splat (20 frames), bounce and break allowed
               'Glass,3,100,0.6,30,1,0,4,0.7;'         # breaks, allows only that
               'Trampoline,2,0,1.2,30,1,1,7,0.7')      # forced bounce
IMPACT_DISTANCE = 205.0   # the victim stands here ahead of the attacker: only the Root box reaches it
IMPACT_FACE = 455.0       # the face of the surface ahead of the attacker. The victim carries IMPACT_DAMAGE percent, which lightens it (the standard weight
                          # curve): the hit launches it at about 740 cm/s at 30 degrees, so it reaches the face (250 cm past it) in the air, 70 cm above
                          # where it stood. A launch that stays within 40 cm of the floor lands at once instead (Mover's Falling mode grounds it)
IMPACT_DAMAGE = 80.0


def _impact_counts(wi, key):
    return tuple(int(x) for x in _hit_comp(wi, key).get_impact_counts().split(','))


def _impact_info(wi, key):
    """(frame, outcome, surface type index, judged speed, damage) of the latest impact the victim step of world wi's copy resolved"""
    return tuple(int(x) for x in _hit_comp(wi, key).get_impact_info().split(','))


def _impact_response(**allowed):
    """Sets the default allowed flags of the rules' victim impact response (splat=False...); returns the previous values to restore"""
    r = hit_rules()
    resp = r.get_editor_property('impact_response')
    d = resp.get_editor_property('default')
    a = d.get_editor_property('allowed')
    old = {k: a.get_editor_property(k) for k in allowed}
    for k, v in allowed.items():
        a.set_editor_property(k, v)
    d.set_editor_property('allowed', a)
    resp.set_editor_property('default', d)
    r.set_editor_property('impact_response', resp)
    return old


def _impact_clear_spot(key, run=900.0):
    """An offset from the pawn's spawn where it stands on the floor with `run` cm of free capsule travel ahead (along its yaw, pawns and the test
    surfaces ignored), a floor under the far end of a flight, and room behind it for the other pawns; None if the lane's surroundings have none"""
    w = worlds()[0]
    ctx = comps(w)[key]
    a = snap(w)[key]
    sp = ctx.get_spawn_location()
    yaw = math.radians(a['yaw'])
    fwd = unreal.Vector(math.cos(yaw), math.sin(yaw), 0.0)
    lib = unreal.GSTN_ImpactTestLibrary
    for r in (0.0, 250.0, 500.0, 750.0, 1000.0, 1250.0, 1500.0):
        for deg in (range(0, 360, 30) if r else (0,)):
            dx, dy = r * math.cos(math.radians(deg)), r * math.sin(math.radians(deg))
            start = unreal.Vector(sp.x + dx, sp.y + dy, a['location'][2] + 10.0)
            if not lib.has_floor_below(ctx, start, 80.0):
                continue
            if lib.sweep_clear_distance(ctx, start, fwd, run) < run - 1.0 or lib.sweep_clear_distance(ctx, start, -fwd, 450.0) < 449.0:
                continue
            if not lib.has_floor_below(ctx, unreal.Vector(start.x + fwd.x * 700.0, start.y + fwd.y * 700.0, start.z), 400.0):
                continue
            if not lib.has_floor_below(ctx, unreal.Vector(start.x - fwd.x * 400.0, start.y - fwd.y * 400.0, start.z), 80.0):
                continue
            return (dx, dy)
    return None


_impact_dbg = {'rows': [], 'handle': None}


def impact_trace(client=1, leaf='Glass', fire_at=1.5):
    """Debug helper (not a scenario): one shot like the scenarios' (surface of type `leaf` ahead of the attacker, hit at `fire_at` s) with the victim's
    combat state and position sampled on every editor tick into _impact_dbg['rows'] [(t, victim_info on the authority, x, y, z)]; read it with
    impact_trace_rows() a few seconds later. Needs the impact table and the kit set (the first part of scenario_impact_wall's setup)"""
    vi = 2 if client == 1 else 1
    vkey, key = key_of_player(vi), key_of_player(client)
    ctx = comps(worlds()[0])[key]
    combat_clear(client)
    set_damage(vkey, 0)
    a = snap(worlds()[0])[key]
    fwd = (math.cos(math.radians(a['yaw'])), math.sin(math.radians(a['yaw'])))
    c = (a['location'][0] + fwd[0] * (IMPACT_FACE + 50.0), a['location'][1] + fwd[1] * (IMPACT_FACE + 50.0), a['location'][2] + 200.0)
    unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(ctx)
    unreal.GSTN_ImpactTestLibrary.spawn_test_surface(ctx, unreal.Vector(*c), unreal.Vector(50.0, 500.0, 500.0), a['yaw'], leaf)
    _place_near(client, vkey, (IMPACT_DISTANCE, 0.0))
    if _impact_dbg['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_impact_dbg['handle'])
    t0, fired, rows = time.time(), {'f': False}, []
    _impact_dbg['rows'] = rows

    def tick(_dt):
        try:
            t = time.time() - t0
            if t > fire_at and not fired['f']:
                fired['f'] = True
                combat_tap(client, button_mask('Attack'))
            loc = snap(worlds()[0])[vkey]['location']
            rows.append((round(t, 2), victim_info(0, vkey), round(loc[0]), round(loc[1]), round(loc[2])))
        except Exception as e:
            rows.append(str(e))
    _impact_dbg['handle'] = unreal.register_slate_post_tick_callback(tick)
    return 'tracing'


def impact_trace_rows():
    if _impact_dbg['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(_impact_dbg['handle'])
        _impact_dbg['handle'] = None
    out, last = [], None
    for r in _impact_dbg['rows']:
        if isinstance(r, str):
            out.append(r)
            continue
        k = (r[1][1:], r[2:])
        if k != last:
            out.append(r)
        last = k
    return out


def scenario_impact_wall(client=1, shot_time=5.5):
    """Surface impacts of a launched victim (M4 H11, combat-m4-hits 7b), the client `client` attacks the other. Six shots, each a real hit that tumbles
    the victim into a box of a given surface type:
      - Wall: splat. Wanted on the authority and the victim's own client: one Splat impact (the state Splat lasts the row's 20 frames and ends Actionable),
        the victim pinned (it does not move through the splat), the impact damage (the rules' 7 times the row's scale) added to the hit's 8 on both copies;
      - Trampoline: bounce, forced by the row: velocity reflected (the victim comes back), still Tumble right after the impact;
      - Glass: break: the surface is broken once, on the authority (BreakSurface called outside the simulation), the victim goes through it;
      - Wall with the hit data forcing a bounce, with the hit data forbidding a splat (nothing happens, the victim slides), and with the victim's response
        forbidding a splat (the same): the precedence of the three layers.
    Judged on the impact counters of the victim step of both copies (re-simulated frames count on the owner, so the owner's may be higher), the last
    impact (outcome and surface index equal on both) and the victim's samples in the attacker's frame."""
    vi = 2 if client == 1 else 1
    vkey = key_of_player(vi)
    run = _Run('impact_wall', client, judge_reconciles=False, extra=[('v_srv', 0, vkey), ('v_cli', vi, vkey)])
    key = run.key
    attack = button_mask('Attack')
    st, h = {'kit': None, 'shots': {}}, {'h': None}
    rec = []  # (t, victim info authority, victim info owner)

    def sampler(_dt):
        try:
            rec.append((run.now(), victim_info(0, vkey), victim_info(vi, vkey)))
        except Exception:
            pass

    def setup():
        hit_rules_setup()
        # a tumble from a hit of the test move (500 base knockback + damage scaling): thresholds low enough for it, and an impact damage to see
        set_tuning(push_back_threshold=50.0, launch_threshold=100.0, tumble_threshold=200.0)
        hit_rules().set_editor_property('base_impact_damage', 7.0)
        st['rows'] = unreal.GSTN_ImpactTestLibrary.configure_impact_table(IMPACT_ROWS)
        unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(0, 0, False, 1.0)
        _hit_logging(True)
        st['kit'] = _apply_loadout(key, HIT_KIT)
        h['h'] = unreal.register_slate_post_tick_callback(sampler)
        st['spot'] = _impact_clear_spot(key)
        if st['spot'] is not None:
            comps(worlds()[0])[key].server_reset_to_spawn(unreal.Vector(st['spot'][0], st['spot'][1], 0.0))
        return 'impact table on', st['rows'], 'components; attacker spot', st['spot']

    def prepare(name, leaf, profile=None, victim_forbids=False):
        def f():
            ctx = comps(worlds()[0])[key]
            unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(ctx)
            combat_clear(client)
            set_damage(vkey, IMPACT_DAMAGE)
            a = snap(worlds()[0])[key]
            yaw = a['yaw']
            fwd = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
            half = (50.0, 500.0, 500.0)
            centre = IMPACT_FACE + half[0]
            c = (a['location'][0] + fwd[0] * centre, a['location'][1] + fwd[1] * centre, a['location'][2] + 200.0)
            st['shots'][name] = {'surface': unreal.GSTN_ImpactTestLibrary.spawn_test_surface(ctx, unreal.Vector(*c), unreal.Vector(*half), yaw, leaf)}
            unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(*(profile or (0, 0, False, 1.0)))
            st['old_allowed'] = _impact_response(splat=False) if victim_forbids else None
            # the third pawn (the host in a listen suite) is moved behind the attacker: it stood in the path of the flight on some lane assignments
            for other in [k for k in comps(worlds()[0]) if k not in (key, vkey)]:
                _place_near(client, other, (-350.0, 350.0))
            _place_near(client, vkey, (IMPACT_DISTANCE, 0.0))
            return 'prepared', name, st['shots'][name]['surface']
        return f

    def fire(name):
        def f():
            s = st['shots'][name]
            s['t_fire'] = run.now()
            s['counts0'] = (_impact_counts(0, vkey), _impact_counts(vi, vkey))
            s['dmg0'] = (damage_of(0, vkey), damage_of(vi, vkey))
            s['F'] = _latest_frame(0, key)
            combat_tap(client, attack)
            return 'attack', name
        return f

    def collect(name):
        def f():
            s = st['shots'][name]
            t0, t1 = s['t_fire'], run.now()
            s['t_end'] = t1
            s['counts1'] = (_impact_counts(0, vkey), _impact_counts(vi, vkey))
            s['info'] = (_impact_info(0, vkey), _impact_info(vi, vkey))
            s['dmg1'] = (damage_of(0, vkey), damage_of(vi, vkey))
            s['surfaces'] = unreal.GSTN_ImpactTestLibrary.get_test_surfaces(comps(worlds()[0])[key])
            # the victim's path on the authority, in the attacker's frame (x ahead, y left, z)
            a = snap(worlds()[0])[key]
            yaw = math.radians(a['yaw'])
            path = []
            for smp in run._win('v_srv', t0, t1, sim=True):
                dx, dy = smp['loc'][0] - a['location'][0], smp['loc'][1] - a['location'][1]
                vx = smp['vel'][0] * math.cos(yaw) + smp['vel'][1] * math.sin(yaw)
                path.append((smp['ms'], dx * math.cos(yaw) + dy * math.sin(yaw), smp['loc'][2], vx, smp['mode'], tuple(sorted(t for t in smp['tags'] if t.startswith('Combat.State')))))
            s['path'] = path
            frames = {}
            for t, ia, io in rec:
                if t0 <= t <= t1:
                    frames[ia[0]] = ia[1]
            s['actions'] = [frames[k] for k in sorted(frames)]
            splat = [k for k in frames if frames[k] == 4]
            s['splat_frames'] = (max(splat) - min(splat) + 1) if splat else 0
            s['hits'] = [r for r in _hit_log(0, key) if r[0] >= s['F']]
            return 'collected', name
        return f

    def delta(s, side):
        return tuple(b - a for a, b in zip(s['counts0'][side], s['counts1'][side]))

    def counts_check(name, want):
        def f():
            s = st['shots'][name]
            auth, own = delta(s, 0), delta(s, 1)
            ok = auth == want and all(o >= a for o, a in zip(own, auth)) and sum(own) >= sum(want)
            outcome_eq = want == (0, 0, 0) or (s['info'][0][1:3] == s['info'][1][1:3])
            return ok and outcome_eq, f"impacts (splat, bounce, break) authority {auth}, owner {own} (re-simulated frames count there), wanted {want}; last impact authority {s['info'][0]}, owner {s['info'][1]}, hits {s.get('hits')}"
        return f

    def splat_check():
        s = st['shots']['splat']
        n = s['splat_frames']
        end = s['actions'][-1] if s['actions'] else None
        face_gap = IMPACT_FACE - max((p[1] for p in s['path']), default=0.0)
        # the state is sampled once per editor tick, which is slower than the 60 Hz simulation: the first and last frames can fall between two ticks
        ok = 17 <= n <= 21 and end == 0 and face_gap < 80.0
        return ok, f"frames in Splat {n} (want 20, sampled per editor tick so up to 2 short), final action state {end}, closest approach to the face {face_gap:.0f} cm (the capsule radius is about 35)"

    def pinned_check():
        s = st['shots']['splat']
        t_splat = [t for t, ia, io in rec if s['t_fire'] <= t <= s['t_end'] and ia[1] == 4]
        if not t_splat:
            return False, 'no splat window sampled'
        w = [p for p in run._win('v_srv', t_splat[0] + 0.05, t_splat[-1] - 0.05, sim=True)]
        spread = max((math.dist(p['loc'], q['loc']) for p in w for q in w), default=1e9)
        spd = max((math.dist(p['vel'], (0, 0, 0)) for p in w), default=1e9)
        return spread < 6.0 and spd < 15.0, f'{len(w)} ticks inside the splat, position spread {spread:.1f} cm, peak speed {spd:.0f} cm/s'

    def damage_check(name, want):
        def f():
            s = st['shots'][name]
            d = (s['dmg1'][0] - s['dmg0'][0], s['dmg1'][1] - s['dmg0'][1])
            return abs(d[0] - want) < 0.6 and abs(d[1] - want) < 0.6, f'damage taken authority {d[0]:.1f}, owner {d[1]:.1f}, wanted {want} (8 from the hit, 7 per unit of the row scale for an impact)'
        return f

    def bounce_check():
        s = st['shots']['bounce']
        vx = [p[3] for p in s['path']]
        out, back = max(vx, default=0.0), min(vx, default=0.0)
        tumbling = any(a == 3 for a in s['actions'])
        return out > 150.0 and back < -150.0 and tumbling, f'speed along the attacker facing: out up to {out:.0f}, back down to {back:.0f}; Tumble seen {tumbling}; action states {sorted(set(s["actions"]))}'

    def break_check():
        s = st['shots']['break']
        rows = [r.split(',') for r in s['surfaces'].split(';') if r]
        broken = bool(rows) and rows[0][1] == '1' and rows[0][2] == '1'
        beyond = max((p[1] for p in s['path']), default=0.0)
        return broken and beyond > IMPACT_FACE + 10.0, f"surfaces (name, broken, BreakSurface calls) on the authority {s['surfaces']}, farthest point of the victim {beyond:.0f} cm ahead (the face is at {IMPACT_FACE:.0f}: a capsule centre past it is inside where the glass was)"

    def slid_check(name):
        def f():
            s = st['shots'][name]
            reach = max((p[1] for p in s['path']), default=0.0)
            return all(a != 4 for a in s['actions']) and reach < IMPACT_FACE, f"action states {sorted(set(s['actions']))} (no Splat), farthest point {reach:.0f} cm (the face is at {IMPACT_FACE:.0f})"
        return f

    def restore():
        if st.get('old_allowed'):
            _impact_response(**st['old_allowed'])
            st['old_allowed'] = None
        return 'response restored'

    def end():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(comps(worlds()[0])[key])
        unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(0, 0, False, 1.0)
        unreal.GSTN_ImpactTestLibrary.configure_impact_table('')
        _impact_response(splat=True)
        hit_rules().set_editor_property('base_impact_damage', 0.0)
        set_damage(vkey, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'restored'

    shots = [
        ('splat', 'Wall', None, False),
        ('bounce', 'Trampoline', None, False),
        ('break', 'Glass', None, False),
        ('force_bounce', 'Wall', (2, 0, False, 1.0), False),     # the hit forces a bounce
        ('forbid_splat', 'Wall', (0, 1, False, 1.0), False),     # the hit forbids a splat
        ('victim_forbids', 'Wall', None, True),                  # the victim's response forbids a splat
    ]
    steps = [(0.3, lambda: ('clear', combat_clear(client))), (0.5, setup)]
    t = 2.5
    for name, leaf, profile, vf in shots:
        steps += [(t, prepare(name, leaf, profile, vf)), (t + 1.3, fire(name)), (t + shot_time, collect(name)), (t + shot_time + 0.1, restore)]
        t += shot_time + 1.0
    t_end = t
    steps += [
        (t_end, run.at('the attacker kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (t_end, run.at('Wall: one splat on the authority and the owner, the same outcome and surface', counts_check('splat', (1, 0, 0)))),
        (t_end, run.at('Wall: Splat lasts the row frames, ends Actionable, the victim stopped at the face', splat_check)),
        (t_end, run.at('Wall: the victim is pinned through the splat (no movement)', pinned_check)),
        (t_end, run.at('Wall: impact damage added to the hit damage on both copies', damage_check('splat', 15.0))),
        (t_end, run.at('Trampoline: one bounce, forced by the row', counts_check('bounce', (0, 1, 0)))),
        (t_end, run.at('Trampoline: the velocity is reflected and the victim stays Tumble', bounce_check)),
        (t_end, run.at('Glass: one break, on the authority, outside the simulation', counts_check('break', (0, 0, 1)))),
        (t_end, run.at('Glass: broken once (BreakSurface called once) and the victim went through', break_check)),
        (t_end, run.at('Glass: no impact damage for a break (the hit only)', damage_check('break', 8.0))),
        (t_end, run.at('Wall with a hit that forces a bounce: bounce, not splat', counts_check('force_bounce', (0, 1, 0)))),
        (t_end, run.at('Wall with a hit that forbids a splat: no impact', counts_check('forbid_splat', (0, 0, 0)))),
        (t_end, run.at('Wall with a hit that forbids a splat: never in the Splat state', slid_check('forbid_splat'))),
        (t_end, run.at('Wall with a victim that forbids a splat: no impact', counts_check('victim_forbids', (0, 0, 0)))),
        (t_end, run.at('Wall with a victim that forbids a splat: never in the Splat state', slid_check('victim_forbids'))),
        (t_end + 0.2, end),
        (t_end + 0.4, run.finish()),
    ]
    run.run(steps)


def scenario_impact_wall_mode(client=1, tap_delay=2.0):
    """Second half of the wall scenario of the hits milestone, for the surface impacts (M4 H11, combat-m4-hits 7b): a victim wall sliding on a wall is
    launched INTO it. `client` is the climber, the other client the attacker. A wall (an AGSTN_TestSurface box of the Wall type, which the wall mode slides on
    like any collider) is spawned ahead of the attacker, whose test move launches along its facing; the climber walks to the wall, jumps, rises along it
    to about 330 cm and starts to slide down; the attack is tapped on the way down, and the spin of the test move (20 frames later, 140 cm around the attacker)
    hits it while it still slides. Wanted: the hit lands once while the climber is WallSliding; the climber is launched
    (Tumble) and its knockback move's probe finds the wall at once: one Splat impact on the authority and the owner, the state Splat for the row's 20 frames,
    the impact damage added to the hit's; afterwards Actionable. What the wall mode does after is reported."""
    att = 2 if client == 1 else 1
    run = _Run('impact_wall_mode', client, judge_reconciles=False)
    wm = wall_mode()
    key, akey = run.key, key_of_player(att)
    ia_move, attack = unreal.load_asset(IA['move']), button_mask('Attack')
    st, h = {'phase': 'idle', 'tw': None, 'reached': False}, {'h': None, 'v': None}
    rec = []
    dist = 135.0   # the face of the wall ahead of the attacker: the climber slides at about 100, inside the 140 cm radius of the spin

    def vsampler(_dt):
        try:
            rec.append((run.now(), victim_info(0, key), victim_info(client, key)))
        except Exception:
            pass

    def frame_x(loc):
        a = snap(worlds()[0])[akey]
        yaw = math.radians(a['yaw'])
        return (loc[0] - a['location'][0]) * math.cos(yaw) + (loc[1] - a['location'][1]) * math.sin(yaw)

    def setup():
        hit_rules_setup()
        set_tuning(push_back_threshold=50.0, launch_threshold=100.0, tumble_threshold=200.0)
        hit_rules().set_editor_property('base_impact_damage', 7.0)
        st['rows'] = unreal.GSTN_ImpactTestLibrary.configure_impact_table(IMPACT_ROWS)
        unreal.GSTN_ImpactTestLibrary.set_hit_impact_profile(0, 0, False, 1.0)
        _hit_logging(True)
        st['kit'] = _apply_loadout(akey, HIT_KIT)
        h['v'] = unreal.register_slate_post_tick_callback(vsampler)
        st['spot'] = _impact_clear_spot(akey)
        if st['spot'] is not None:
            comps(worlds()[0])[akey].server_reset_to_spawn(unreal.Vector(st['spot'][0], st['spot'][1], 0.0))
        return 'attacker spot', st['spot']

    def build():
        ctx = comps(worlds()[0])[akey]
        unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(ctx)
        a = snap(worlds()[0])[akey]
        fwd = (math.cos(math.radians(a['yaw'])), math.sin(math.radians(a['yaw'])))
        half = (50.0, 500.0, 500.0)
        c = (a['location'][0] + fwd[0] * (dist + half[0]), a['location'][1] + fwd[1] * (dist + half[0]), a['location'][2] + 200.0)
        st['wall'] = unreal.GSTN_ImpactTestLibrary.spawn_test_surface(ctx, unreal.Vector(*c), unreal.Vector(*half), a['yaw'], 'Wall')
        for other in [k for k in comps(worlds()[0]) if k not in (key, akey)]:
            _place_near(att, other, (-350.0, 350.0))
        _place_near(att, key, (70.0, 0.0))
        set_damage(key, IMPACT_DAMAGE)
        return 'wall', st['wall']

    def finish_driver(comp):
        comp.stop_holding_input(ia_move)
        st['phase'] = 'done'
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None

    def drive(_dt):
        try:
            now = run.now()
            _, pawn = _local_pawn(client)
            comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
            mode = str(pawn.get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name())
            loc = pawn.get_actor_location()
            a = snap(worlds()[0])[akey]
            fwd = (math.cos(math.radians(a['yaw'])), math.sin(math.radians(a['yaw'])))
            ahead = frame_x((loc.x, loc.y))
            into = (loc.x + fwd[0] * 400.0, loc.y + fwd[1] * 400.0)
            ph = st['phase']
            if ph == 'walk':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                if dist - ahead < 130.0 and mode == 'Walking':
                    press(client, 'jump')
                    st.update(phase='air', t1=now)
                elif now - st['t1'] > 6.0:
                    finish_driver(comp)
            elif ph == 'air':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                if mode == wm:
                    st.update(phase='hit', tw=now, reached=True)
                elif now - st['t1'] > 2.5:
                    finish_driver(comp)
            elif ph == 'hit':
                comp.start_holding_input(ia_move, unreal.Vector(*_steer_value(client, into)))
                # The climber rises along the wall to about 330 cm, then slides down faster and faster (400 cm/s at the floor). The spin of the test move opens 20
                # frames (0.35 s) after the tap and reaches 140 cm around the attacker (height 88), the feet of the climber are 62 cm under its centre: tap just
                # after the top of the rise, so the spin finds the climber about 230 cm up, still on the wall
                if now - st['tw'] > 0.2 and mode == wm and snap(worlds()[0])[key]['velocity'][2] < -20.0 and loc.z < 265.0 and 'F' not in st:
                    st['F'] = _latest_frame(0, akey)
                    st['t_tap'] = now
                    st['d0'] = (damage_of(0, key), damage_of(client, key))
                    st['c0'] = (_impact_counts(0, key), _impact_counts(client, key))
                    st['p_tap'] = (round(frame_x((loc.x, loc.y)), 1), round(loc.z, 1), mode)
                    combat_tap(att, attack)
                if hits() and 'backed' not in st:
                    # the first hit landed: the attacker steps back so the rest of its move (the spin, 20 frames in) does not hit the splatted climber again
                    st['backed'] = now
                    sp = comps(worlds()[0])[akey].get_spawn_location()
                    comps(worlds()[0])[akey].server_reset_to_spawn(unreal.Vector(a['location'][0] - fwd[0] * 130.0 - sp.x, a['location'][1] - fwd[1] * 130.0 - sp.y, 0.0))
                if now - st['tw'] > tap_delay + 1.5:
                    finish_driver(comp)
        except Exception as e:
            unreal.log_error(f'GASNPP impact_wall_mode driver: {e}')
            st['phase'] = 'done'

    def start_driver():
        st.update(phase='walk', t1=run.now())
        h['h'] = unreal.register_slate_post_tick_callback(drive)
        return 'driving'

    def hits():
        return [r for r in _hit_log(0, akey) if r[0] >= st.get('F', 10 ** 9)]

    def hit_on_wall():
        hs = hits()
        # the mode the climber is in when the hit freezes it: the first simulation tick with the hitstop tag
        stop = next((s for s in run._win('srv', st['t_tap'], st['t_tap'] + 1.5, sim=True) if 'Combat.State.Hitstop' in s['tags']), None) if st.get('t_tap') is not None else None
        return st['reached'] and len(hs) == 1 and stop is not None and stop['mode'] == wm, f"reached the wall {st['reached']}, hits {hs} (group 0 is the Root box, 3 the spin), climber at the tap (x ahead of the attacker, z, mode) {st.get('p_tap')}, mode when the hitstop opened {stop['mode'] if stop else None}"

    def impact_check():
        c1 = (_impact_counts(0, key), _impact_counts(client, key))
        auth = tuple(b - a for a, b in zip(st['c0'][0], c1[0]))
        own = tuple(b - a for a, b in zip(st['c0'][1], c1[1]))
        info = (_impact_info(0, key), _impact_info(client, key))
        return auth == (1, 0, 0) and own[0] >= 1 and own[1:] == (0, 0), f'impacts (splat, bounce, break) authority {auth}, owner {own}; last impact {info}'

    def sequence_check():
        frames = {}
        for t, ia, io in rec:
            if t >= st['tw']:
                frames[ia[0]] = ia[1]
        seq = []
        for k in sorted(frames):
            if not seq or seq[-1] != frames[k]:
                seq.append(frames[k])
        n = sum(1 for k in frames if frames[k] == 4)
        return 3 in seq and 4 in seq and seq.index(3) < seq.index(4) and seq[-1] == 0, f'combat action states in order {seq} (0 Actionable, 3 Tumble, 4 Splat), frames sampled in Splat {n}'

    def damage_check():
        d = (damage_of(0, key) - st['d0'][0], damage_of(client, key) - st['d0'][1])
        hit = [e for e in _inbox_log(0, key) if e[0] >= st.get('F', 10 ** 9)]
        want = (hit[0][7] if hit else 0) + 7.0
        return abs(d[0] - want) < 0.6 and abs(d[1] - want) < 0.6, f'damage taken authority {d[0]:.1f}, owner {d[1]:.1f}, wanted {want:.0f} ({want - 7:.0f} from the hit by the inbox, and 7 from the splat)'

    def after():
        seq = []
        for s in run._win('srv', st['tw'], st['tw'] + tap_delay + 3.5, sim=True):
            if not seq or seq[-1] != s['mode']:
                seq.append(s['mode'])
        return True, f'mode sequence of the climber on the authority from the start of the slide: {seq}'

    def end():
        for hd in ('v',):
            if h[hd] is not None:
                unreal.unregister_slate_post_tick_callback(h[hd])
                h[hd] = None
        unreal.GSTN_ImpactTestLibrary.clear_test_surfaces(comps(worlds()[0])[akey])
        unreal.GSTN_ImpactTestLibrary.configure_impact_table('')
        hit_rules().set_editor_property('base_impact_damage', 0.0)
        set_damage(key, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'restored'

    t_end = 2.5 + 8.5 + tap_delay + 4.0
    steps = [
        (0.3, lambda: ('clear', combat_clear(client))),
        (0.5, setup),
        (2.0, build),
        (4.0, start_driver),
        (t_end, run.at('the attacker kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (t_end, run.at('the climber reached the wall and the attack hit it once while it was wall sliding', hit_on_wall)),
        (t_end, run.at('launched into the wall: one splat on the authority and the owner', impact_check)),
        (t_end, run.at('Tumble, then Splat, then Actionable', sequence_check)),
        (t_end, run.at('impact damage added to the hit damage on both copies', damage_check)),
        (t_end, run.at('what the wall mode does after the splat (reported)', after)),
        (t_end + 0.2, end),
        (t_end + 0.4, run.finish()),
    ]
    run.run(steps)


# ---- M4 H12: base dodges, ricochet response, dodge gating. The dodge window is kit data (a hurtbox modifier track of the ability class, Intangible, AffectedTags
# Combat.Hurtbox.Feet, take-off to 40 % of the airborne frames); the ricochet is forced by the fake attacker outcome of the test component (one switch per process).

def _recoil_info(wi, key):
    """(frame, action state, active timeline, timeline start, recoil start) of world wi's copy of a pawn; -1 = none"""
    return tuple(int(x) for x in _hit_comp(wi, key).get_recoil_info().split(','))


def _dodge_window(air):
    return tuple(int(x) for x in unreal.GSTN_GASNPPTestComponent.get_dodge_window(air).split(','))


def scenario_dodge_feet(client=1):
    """M4 H12 / S 12 4b. `client` attacks the other client, who dodges with the planted ground dodge (the real timeline and window, no take-off, so a hitbox fixed
    in the world meets the same feet at every frame). A: the low attack (feet only) is open before the window ends: the feet are missed for the rest of the
    window and hit on the first frame after it (the window is the ability's data: read from the class). B: the same dodge against the plain attack (torso):
    hit inside the window. Same hits on the authority and on the attacker's client."""
    dodger = 2 if client == 1 else 1
    run = _Run('dodge_feet', dodger, judge_reconciles=False)
    dkey, akey, st = run.key, key_of_player(client), {}
    dodge, attack, crouch = button_mask('Dodge'), button_mask('Attack'), button_mask('Crouch')
    TA, TB = 2.0, 6.0
    win = _dodge_window(False)

    def prepare():
        _kit(dkey, False, True, False)
        _srv(dkey).server_set_planted(True)
        hit_rules_setup()
        set_damage(dkey, 0)
        st['kit'] = _apply_loadout(akey, HIT_KIT)
        _hit_logging(True)
        seen['h'] = unreal.register_slate_post_tick_callback(sampler)
        return 'planted dodge kit, rules, attacker kit', st['kit']

    seen = {'h': None, 'starts': []}

    def sampler(_dt):
        try:
            d0 = _recoil_info(0, dkey)[3]
            if d0 >= 0 and d0 not in seen['starts']:
                seen['starts'].append(d0)
        except Exception:
            pass

    def record(tag):
        # The dodge timeline is cancelled by the hit it lets through, so its start is taken from the samples of the run, not read afterwards
        def f():
            later = [d for d in seen['starts'] if d >= st['tap'] - 40 and (tag == 'A' or d > st['A'][0])]
            st[tag] = (later[0] if later else -1, st['tap'])
            return 'dodge timeline start, attack tap frame', st[tag]
        return f

    def hits_since(wi, since):
        return [r for r in _hit_log(wi, akey) if r[0] >= since]

    def feet_check():
        d0, a0 = st['A']
        hits, owner = hits_since(0, a0), hits_since(client, a0)
        if d0 < 0 or a0 < 0 or len(hits) != 1:
            return False, f'dodge start {d0}, attack start {a0}, hits {hits}'
        frame, _, group, hitbox, hurtbox, ricochet, _ = hits[0]
        early = a0 + 3 <= d0 + win[1] - 3
        # The attacker's client sees the dodger through its rewound view of a remote pawn, which lags: it may predict the hit on the feet earlier than the
        # authority decides (the presentation cancels it). The authority's hit is the one judged; the owner's log is reported, not judged (a prediction the authority did not confirm is dropped by the presentation)
        ok = hurtbox == 2 and ricochet == 0 and d0 + win[1] <= frame <= d0 + win[1] + 3 and early
        return ok, f'window {win}, dodge start {d0}, attack tap {a0} (hitbox open from about {a0 + 2}), authority hit {hits[0]}, owner (predicted from its rewound view of the dodger, within 12 frames before the authority hit) {owner}'

    def torso_check():
        d0, a0 = st['B']
        hits, owner = hits_since(0, a0), hits_since(client, a0)
        if d0 < 0 or a0 < 0 or not hits:
            return False, f'dodge start {d0}, attack start {a0}, hits {hits}'
        frame, hurtbox = hits[0][0], hits[0][4]
        ok = hurtbox == 0 and d0 <= frame < d0 + win[1]
        return ok, f'window {win}, dodge start {d0}, hit {hits[0]} (torso is hurtbox 0), owner {owner}'

    def cleanup():
        combat_clear(dodger)
        _srv(dkey).server_set_planted(False)
        _kit(dkey, False, False, False)
        set_damage(dkey, 0)
        hit_rules_setup()
        _hit_logging(False)
        if seen['h'] is not None:
            unreal.unregister_slate_post_tick_callback(seen['h'])
            seen['h'] = None
        return 'cleanup'

    steps = [
        (0.3, prepare),
        (0.6, lambda: ('dodger in front of the attacker', _place_near(client, dkey, (205.0, 0.0)))),
        (TA, lambda: ('A: dodge', combat_tap(dodger, dodge))),
        (TA - 0.3, lambda: ('A: crouch held', combat_hold(client, crouch))),
        (TA + 0.1, lambda: ('A: low attack', (st.__setitem__('tap', _latest_frame(0, akey)), combat_tap(client, attack)))),
        (TA + 0.2, lambda: ('release', combat_clear(client))),
        (TA + 0.5, record('A')),
        (TA + 1.9, run.at('A: the feet were missed during the window and hit right after it', feet_check)),
        (TB - 1.5, lambda: ('dodger back in front of the attacker', (set_damage(dkey, 0), _place_near(client, dkey, (205.0, 0.0))))),
        (TB - 0.1, run.at('B: the dodger is actionable again', lambda: (victim_info(0, dkey)[1] == 0, f'victim info {victim_info(0, dkey)}'))),
        (TB, lambda: ('B: dodge', combat_tap(dodger, dodge))),
        (TB + 0.1, lambda: ('B: plain attack', (st.__setitem__('tap', _latest_frame(0, akey)), combat_tap(client, attack)))),
        (TB + 0.9, record('B')),
        (TB + 1.5, run.at('B: the torso is hit inside the window', torso_check)),
        (TB + 1.6, cleanup),
        (TB + 1.7, run.at('the attacker kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (TB + 2.6, run.finish()),
    ]
    run.run(steps)


def scenario_air_dodge_feet(client=1):
    """M4 H12: the window of the air dodge, read from the snapshot entry of the dodger (the hurt state of every hurtbox) together with the start of its
    timeline, on the authority and on the owner. The editor tick samples the newest simulated frame only, so every sample is judged alone: the feet are
    intangible exactly while (frame - timeline start) lies in the data window, the torso and the head never are, and the two copies agree on the frames
    they share. Both the inside and the outside of the window must have been sampled."""
    run = _Run('air_dodge_feet', client)
    key = run.key
    dodge = button_mask('Dodge')
    T = 2.0
    win = _dodge_window(True)
    rec = {'srv': {}, 'cli': {}}
    h = {'h': None}

    def sampler(_dt):
        try:
            for side, wi in (('srv', 0), ('cli', client)):
                j = json.loads(comps(worlds()[wi])[key].read_snapshot_entry(-1))
                info = _recoil_info(wi, key)
                if info[1] == 1 and info[3] >= 0 and j['frame'] == info[0]:
                    rec[side][j['frame']] = (info[3], tuple(j['hurtStates']))
        except Exception:
            pass

    def judge():
        out, ok = [], True
        for side in ('srv', 'cli'):
            inside = outside = wrong = 0
            for f, (d0, hs) in rec[side].items():
                want = win[0] <= f - d0 < win[1]
                good = len(hs) == 3 and hs[0] == 0 and hs[1] == 0 and (hs[2] == 1) == want
                wrong += 0 if good else 1
                inside += 1 if want else 0
                outside += 0 if want else 1
            ok &= wrong == 0 and inside >= 2 and outside >= 2
            out.append(f'{side}: {inside} samples inside the window {win}, {outside} outside (timeline frames), {wrong} wrong')
        common = set(rec['srv']) & set(rec['cli'])
        diff = [f for f in common if rec['srv'][f] != rec['cli'][f]]
        ok &= not diff
        out.append(f'{len(common)} frames in common, {len(diff)} differ {diff[:5]}')
        return ok, '; '.join(out)

    def start():
        _kit(key)
        _srv(key).server_set_jump_kit(True)
        h['h'] = unreal.register_slate_post_tick_callback(sampler)
        return 'movement and jump kits, sampler'

    def stop():
        if h['h'] is not None:
            unreal.unregister_slate_post_tick_callback(h['h'])
            h['h'] = None
        combat_clear(client)
        _kit(key, False, False, False)
        _srv(key).server_set_jump_kit(False)
        return 'cleanup'

    steps = [
        (0.3, start),
        (T, lambda: ('jump', _jump_tap(client, 0.3))),
        (T + 0.3, lambda: ('release', combat_clear(client))),
        (T + 0.55, lambda: ('air dodge', combat_tap(client, dodge))),
        (T + 4.0, run.at('the feet are intangible for the data window, nothing else', judge)),
        (T + 4.1, stop),
        (T + 5.1, run.finish()),
    ]
    run.run(steps)


def scenario_ricochet(client=1):
    """M4 H12 / S 3b. `client` attacks the other client. Control: a normal hit (attacker hitstop 6, damage, no recoil). Then the fake attacker outcome of the test
    component forces Ricochet: the move ends into the 40 frame recoil of the attack's response (it outlives the 32 frame move), the attacker gets no hitstop and
    no hit-confirm, the victim gets the flagged inbox entry and nothing else (no damage, no hitstop, no hitstun), host and client agree."""
    victim_index = 2 if client == 1 else 1
    vkey = key_of_player(victim_index)
    run = _Run('ricochet', client, judge_reconciles=False)
    akey, st = run.key, {}
    attack = button_mask('Attack')
    T1, T2 = 2.0, 5.0

    def prepare():
        hit_rules_setup()
        set_damage(vkey, 0)
        st['kit'] = _apply_loadout(akey, HIT_KIT)
        _hit_logging(True)
        return 'rules, attacker kit', st['kit']

    def mark(tag):
        def f():
            st[tag] = _latest_frame(0, akey)
            return tag, st[tag]
        return f

    def rows(wi, tag):
        return [r for r in _hit_log(wi, akey) if r[0] >= st[tag]]

    def control():
        hits, owner = rows(0, 'F1'), rows(client, 'F1')
        info = (_recoil_info(0, akey)[4], _recoil_info(client, akey)[4])
        ok = len(hits) >= 1 and hits[0][5] == 0 and hits[0][6] == HIT_ATTACKER_HITSTOP and owner == hits and info == (-1, -1)
        st['w1'] = _hit_window(0, akey)
        st['vw1'] = _hit_window(0, vkey)
        return ok, f'hits {hits}, owner {owner}, recoil start {info}, damage {damage_of(0, vkey)}, attacker window {st["w1"]}'

    def bounced():
        hits, owner = rows(0, 'F2'), rows(client, 'F2')
        ra, rc = _recoil_info(0, akey), _recoil_info(client, akey)
        inbox = [e for e in _inbox_log(0, vkey) if e[0] > st['F2']]
        ok = (len(hits) == 1 and hits[0][5] == 1 and hits[0][6] == 0 and owner == hits and ra[4] == hits[0][0] + 1 and rc[4] == ra[4]
              and len(inbox) == 1 and inbox[0][5] == 1)
        where = (tuple(round(x) for x in snap(worlds()[0])[vkey]['location']), tuple(round(x) for x in snap(worlds()[0])[akey]['location']), victim_info(0, vkey))
        return ok, f'hits {hits}, owner {owner}, recoil start authority {ra}, owner {rc}, victim inbox {inbox}, victim/attacker/victim info {where}'

    def nothing_else():
        va = victim_info(0, vkey)
        ok = (_hit_window(0, akey) == st['w1'] and _hit_window(client, akey) == st['w1'] and _hit_window(0, vkey) == st['vw1'] and va[1] == 0
              and damage_of(0, vkey) == 0)
        return ok, f'attacker window {_hit_window(0, akey)} (was {st["w1"]}), owner {_hit_window(client, akey)}, victim window {_hit_window(0, vkey)} (was {st["vw1"]}), victim info {va}, damage {damage_of(0, vkey)}'

    def cleanup():
        unreal.GSTN_GASNPPTestComponent.set_force_ricochet(False)
        combat_clear(client)
        set_damage(vkey, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'cleanup'

    steps = [
        (0.3, prepare),
        (0.6, lambda: ('victim in front of the attacker', _place_near(client, vkey, (205.0, 0.0)))),
        (T1 - 0.05, mark('F1')),
        (T1, lambda: ('control attack', combat_tap(client, attack))),
        (T1 + 1.5, run.at('control: a normal hit, no recoil', control)),
        (T2 - 1.0, lambda: ('victim back, damage cleared', (set_damage(vkey, 0), _place_near(client, vkey, (205.0, 0.0))))),
        (T2 - 0.3, lambda: ('force the ricochet', unreal.GSTN_GASNPPTestComponent.set_force_ricochet(True))),
        (T2 - 0.05, mark('F2')),
        (T2, lambda: ('attack', combat_tap(client, attack))),
        (T2 + 0.6, run.at('the hit bounced: flagged, no attacker hitstop, recoil starts the next frame on both', bounced)),
        (T2 + 0.62, run.at('no hitstop on either side, the victim untouched', nothing_else)),
        (T2 + 0.74, _in_timeline_seen(run, 'the recoil outlives the 32 frame move', T2 + 0.64, T2 + 0.72)),
        (T2 + 1.5, _in_timeline_seen(run, 'and it ends: the attacker is actionable again', T2 + 1.2, T2 + 1.5, want=False)),
        (T2 + 1.55, cleanup),
        (T2 + 1.6, run.at('the attacker kit was accepted', lambda: (st.get('kit') == 'OK', f'ServerApplyTestLoadout -> {st.get("kit")}'))),
        (T2 + 2.5, run.finish()),
    ]
    run.run(steps)


def _victim_action_is(run, key, label, want, t_max=None):
    """Check: the combat action state (EGSTN_ActionState value) of the victim on the authority and on its owner's copy equals `want`."""
    def f():
        def side(sd):
            wi = 0 if sd == 'srv' else run.client
            got = victim_info(wi, key)[1]
            return got == want, f'action state {got} (want {want})'
        return _both(run, side)
    return run.at(label, f)


def _tag_on_both(run, tag, t0, t1, want=True):
    def f():
        def side(sd):
            seen = run.saw_tag(tag, t0, t1, sd)
            return seen == want, f'{tag} seen {seen} (want {want})'
        return _both(run, side)
    return f


def scenario_kit_escape_in_hitstun(client=1):
    """M4 H12 / H13b, S 6 and S 12 acceptance 1: a kit whose dodge carries Combat.Ability.IgnoreHitstunOnActivation dodges in a REAL hitstun (a real hit
    of the attacker, not the harness hook: ProjectStateTags wipes the hook's tag every frame), on the authority and on the owner alike. The control
    comes first in the same run: the same hit and the same press with the base kit (KIT_Test_Hits) is refused by the combat gate, nothing moves and no
    timeline starts. `client` is the attacker; the dodger is the other client."""
    dodger = 2 if client == 1 else 1
    run = _Run('kit_escape_in_hitstun', dodger, judge_reconciles=False)
    key, akey, st = run.key, key_of_player(client), {}
    dodge, attack = button_mask('Dodge'), button_mask('Attack')
    TH1, TA = 2.0, 2.9           # the control: base kit, hit, dodge in the hitstun
    TL = 5.4                     # the hitstun (150 frames) is over; swap the dodger to the escape kit
    TH2, TB = 6.4, 7.3           # the escape kit: hit, dodge in the hitstun

    def prepare():
        hit_rules_setup()
        set_tuning(min_hitstun_frames=150)
        st['akit'] = _apply_loadout(akey, HIT_KIT)
        st['base'] = _apply_loadout(key, HIT_KIT)
        _hit_logging(True)
        return 'rules, long hitstun, attacker kit, base kit on the dodger', (st['akit'], st['base'])

    def mark(name):
        def f():
            st[name] = _latest_frame(0, akey)
            return 'mark', st[name]
        return f

    def hit_landed(name, t):
        def f():
            hits = [r for r in _hit_log(0, akey) if r[0] >= st[name]]
            seen = {sd: run.saw_tag('Combat.State.Hitstun', t - 0.3, t + 0.1, sd) for sd in ('srv', 'cli')}
            return len(hits) >= 1 and all(seen.values()), f'hits {hits}, Combat.State.Hitstun on the dodger {seen}'
        return f

    def swap():
        st['esc'] = _apply_loadout(key, 'KIT_Test_HitsEscape')
        set_damage(key, 0)
        return 'escape kit on the dodger', st['esc']

    def remark():
        run.log_mark = mark_log()
        return 'judge window starts after the swap', True

    def cleanup():
        combat_clear(dodger)
        set_damage(key, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'cleanup'

    steps = [
        (0.3, prepare),
        (0.5, lambda: ('dodger in front of the attacker', _place_near(client, key, (205.0, 0.0)))),
        (TH1 - 0.05, mark('F1')),
        (TH1, lambda: ('hit', combat_tap(client, attack))),
        (TA - 0.05, run.at('control: the hit landed, the dodger is in a real hitstun', hit_landed('F1', TA))),
        (TA - 0.04, _victim_action_is(run, key, 'control: the combat state is Hitstun (2) on both', 2)),
        (TA, lambda: ('control: dodge in hitstun, base kit', combat_tap(dodger, dodge))),
        (TA + 1.0, _no_move(run, 'control: refused by the combat gate, nothing moved', TA - 0.05, TA + 1.0)),
        (TA + 1.0, _in_timeline_seen(run, 'control: no timeline started', TA, TA + 1.0, want=False)),
        (TL - 0.3, _victim_action_is(run, key, 'the hitstun is over before the swap', 0)),
        (TL, swap),
        (TL + 0.5, lambda: ('dodger in front of the attacker again', _place_near(client, key, (205.0, 0.0)))),
        (TL + 0.6, remark),
        (TH2 - 0.05, mark('F2')),
        (TH2, lambda: ('hit', combat_tap(client, attack))),
        (TB - 0.05, run.at('escape: the hit landed, the dodger is in a real hitstun', hit_landed('F2', TB))),
        (TB - 0.04, _victim_action_is(run, key, 'escape: the combat state is Hitstun (2) on both', 2)),
        (TB, lambda: ('escape: dodge in hitstun, escape kit', combat_tap(dodger, dodge))),
        (TB + 3.0, _landing_pair(run, st, 'escape dodge in a real hitstun', TB - 0.05, TB + 3.0, floor=200.0, tol=2.0)),
        (TB + 3.0, _in_timeline_seen(run, 'escape: the dodge timeline started on both', TB, TB + 1.0, want=True)),
        (TB + 3.1, cleanup),
        (TB + 3.2, run.at('both kits were accepted', lambda: (st.get('akit') == 'OK' and st.get('base') == 'OK' and st.get('esc') == 'OK',
                                                                f'attacker {st.get("akit")}, base {st.get("base")}, escape {st.get("esc")}'))),
        (TB + 4.1, run.finish()),
    ]
    run.run(steps)


def scenario_burst_ends_reaction(client=1):
    """M4 H13b: bEndReactionOnActivation. KIT_Test_HitsBurst on the dodger: Special starts a plain ability that is let into a hit reaction by
    Combat.Ability.IgnoreHitstunOnActivation and leaves the reaction alone (the control: it starts and the hitstun goes on), the dodge button starts
    the one that also ends the reaction. The burst must end the hitstun in its own frame on the authority and on the owner (the hitstun tag is
    gone in the first sample that owns Test.State.Burst), leave the combatant Actionable, and cost no more rollbacks than the control second."""
    dodger = 2 if client == 1 else 1
    run = _Run('burst_ends_reaction', dodger, judge_reconciles=False)
    key, akey, st = run.key, key_of_player(client), {}
    special, dodge, attack = button_mask('Special'), button_mask('Dodge'), button_mask('Attack')
    TH, TP, TB = 2.0, 3.0, 3.9

    def prepare():
        hit_rules_setup()
        set_tuning(min_hitstun_frames=200)
        st['akit'] = _apply_loadout(akey, HIT_KIT)
        st['burst'] = _apply_loadout(key, 'KIT_Test_HitsBurst')
        _hit_logging(True)
        return 'rules, long hitstun, attacker kit, burst kit', (st['akit'], st['burst'])

    def landed():
        hits = [r for r in _hit_log(0, akey) if r[0] >= st['F']]
        seen = {sd: run.saw_tag('Combat.State.Hitstun', TP - 0.4, TP - 0.05, sd) for sd in ('srv', 'cli')}
        return len(hits) >= 1 and all(seen.values()), f'hits {hits}, Combat.State.Hitstun on the dodger {seen}'

    def rb_mark(name):
        def f():
            st[name] = rollbacks(dodger)
            return 'rollbacks', st[name]
        return f

    def same_frame():
        def side(sd):
            w = run._win(sd, TB - 0.1, TB + 0.8, sim=True)
            first = next((i for i, s in enumerate(w) if 'Test.State.Burst' in s['tags']), None)
            if first is None:
                return False, 'the burst ability never owned Test.State.Burst'
            at = w[first]['tags']
            before = w[first - 1]['tags'] if first > 0 else set()
            ended = 'Combat.State.Hitstun' not in at and 'Combat.Lock.Reaction' not in at
            was = 'Combat.State.Hitstun' in before
            return ended and was, f'in the first burst sample: hitstun {"Combat.State.Hitstun" in at}, reaction lock {"Combat.Lock.Reaction" in at}; the sample before had hitstun {was}'
        return _both(run, side)

    def rollbacks_ok():
        control = st['rb_p1'] - st['rb_p0']
        burst = rollbacks(dodger) - st['rb_b0']
        return burst <= max(control, 0) + 1, f'rollbacks of the dodger: control second {control}, burst second {burst} (want at most the control + 1; the link drops packets)'

    def cleanup():
        combat_clear(dodger)
        set_damage(key, 0)
        hit_rules_setup()
        _hit_logging(False)
        return 'cleanup'

    steps = [
        (0.3, prepare),
        (0.5, lambda: ('dodger in front of the attacker', _place_near(client, key, (205.0, 0.0)))),
        (TH - 0.05, lambda: ('mark', st.__setitem__('F', _latest_frame(0, akey)))),
        (TH, lambda: ('hit', combat_tap(client, attack))),
        (TP - 0.05, run.at('the hit landed, the dodger is in a real hitstun', landed)),
        (TP - 0.05, rb_mark('rb_p0')),
        (TP, lambda: ('control: Special, the ability that stays out of the reaction', combat_tap(dodger, special))),
        (TP + 0.6, run.at('control: it started in the hitstun (Test.State.Peek on both)', _tag_on_both(run, 'Test.State.Peek', TP, TP + 0.6))),
        (TP + 0.6, run.at('control: and the hitstun goes on', _tag_on_both(run, 'Combat.State.Hitstun', TP + 0.3, TP + 0.6))),
        (TP + 0.6, _victim_action_is(run, key, 'control: the combat state is still Hitstun (2)', 2)),
        (TP + 0.65, rb_mark('rb_p1')),
        (TB - 0.05, rb_mark('rb_b0')),
        (TB - 0.04, run.at('the burst finds the dodger still in hitstun', _tag_on_both(run, 'Combat.State.Hitstun', TB - 0.3, TB - 0.05))),
        (TB, lambda: ('burst: the dodge button', combat_tap(dodger, dodge))),
        (TB + 0.9, run.at('burst: the ability started in the reaction (Test.State.Burst on both)', _tag_on_both(run, 'Test.State.Burst', TB, TB + 0.9))),
        (TB + 0.9, run.at('burst: the reaction ended in the frame of the activation, on both', same_frame)),
        (TB + 0.9, run.at('burst: and no hitstun tag in the rest of the second', _tag_on_both(run, 'Combat.State.Hitstun', TB + 0.3, TB + 0.9, want=False))),
        (TB + 0.9, _victim_action_is(run, key, 'burst: the combat state is Actionable (0) on both', 0)),
        (TB + 0.95, run.at('burst: no extra rollbacks', rollbacks_ok)),
        (TB + 1.0, cleanup),
        (TB + 1.1, run.at('both kits were accepted', lambda: (st.get('akit') == 'OK' and st.get('burst') == 'OK', f'attacker {st.get("akit")}, burst {st.get("burst")}'))),
        (TB + 2.0, run.finish()),
    ]
    run.run(steps)


SUITE_DEFAULT = ['weapon_switch','combat_context', 'combat_main_inputs', 'combat_spike', 'combat_lock', 'camera_aim', 'timeline_cancel',
                 'triggered_timeline', 'combat_activation', 'variable_jump', 'air_jump_fastfall', 'tilted_gravity_jump', 'external_launch_jump',
                 'dash', 'dash_tilted_gravity', 'ground_dodge', 'air_dodge', 'kit_without_dodge', 'interact_tap_hold',
                 'hitstop', 'jump_squat', 'command_set', 'ex_meter', 'super_code', 'wait_frames', 'spike_stats', 'spike_grants', 'slide_jump',
                 # the hit scenarios: the attacker carries KIT_Test_Hits from here on; the victim (client 2) stays kit-free until hit_trade, so the real dodge in
                 # hitstun (a pawn with no loadout) runs before it
                 'hit_basic', 'stagger_threshold', 'reaction_tiers', 'di', 'ko_zone', 'dodge_blocked_in_hitstun', 'hit_trade', 'friendly_fire', 'hit_host', 'hit_lagcomp',
                 'loadout_change', 'hit_on_wall', 'hit_on_wall_hit', 'impact_wall', 'impact_wall_mode', 'trampoline', 'hitstop_montage',
                 # M4 H12: the dodge window against real hits, the ricochet, the kit that dodges in hitstun (the last one swaps the dodger's loadout)
                 'dodge_feet', 'air_dodge_feet', 'ricochet', 'kit_escape_in_hitstun',
                 # M4 H13b: the burst flag (swaps the dodger's loadout)
                 'burst_ends_reaction']
_suite = {'handle': None, 'results': [], 'queue': [], 'current': None, 'started': 0.0, 'prev': None, 'settle_until': 0.0, 'timeout': 120.0}


def run_suite(names=None, timeout=240.0, settle=2.0, client=1, reset_wait=2.5, allow_kit=False):
    """Starts the suite (poll suite_report()). A loadout cannot be taken off a pawn (the combat API has no empty loadout): once a kit scenario ran, the
    pawn keeps that kit's binding table until PIE restarts, and the scenarios that expect the default table (dodge, jump...) fail without saying why.
    So a suite refuses to start on pawns that carry a loadout, unless allow_kit."""
    st = _suite
    if not allow_kit:
        dirty = [f"{k} ({v['loadout'].get('kit')})" for k, v in snap(worlds()[0]).items() if (v.get('loadout') or {}).get('revision')]
        if dirty:
            return f'refused: restart PIE first, these pawns still carry a loadout: {dirty}'
    if st['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(st['handle'])
    st.update(results=[], queue=list(names or SUITE_DEFAULT), current=None, timeout=timeout, settle=settle, settle_until=0.0, resetting=None, reset_done=False, reset_wait=reset_wait)
    st['client'] = client

    def tick(_dt):
        try:
            now = time.time()
            if st['current'] is not None:
                done = _last_run.get('results') is not st['prev'] and _last_run.get('results') is not None
                if done:
                    res = _last_run
                    bad = [f'{l} -> {d[:200]}' for ok, l, d in res['results'] if ok is False]
                    st['results'].append((st['current'], res['verdict'], bad))
                    st['current'] = None
                    st['settle_until'] = now + st['settle']
                elif now - st['started'] > st['timeout']:
                    st['results'].append((st['current'], 'TIMEOUT', []))
                    st['current'] = None
                    st['settle_until'] = now + st['settle']
                return
            if now < st['settle_until']:
                return
            if st['queue'] and not st.get('reset_done'):
                # Every scenario starts from its lane, standing still, in Walking, with no test state left over, confirmed on every machine
                if st.get('resetting') is None:
                    st['targets'] = reset_lanes()
                    st['resetting'] = now
                    return
                if now - st['resetting'] < st['reset_wait']:
                    return
                ok, detail = lanes_settled(st['targets'])
                if not ok and not st.get('retried') and now - st['resetting'] >= st['reset_wait'] + 3.0:
                    # A pawn teleported next to another that had not left yet is nudged aside: once the others are on their lanes, again
                    st['targets'] = reset_lanes()
                    st['retried'] = True
                    st['resetting'] = now
                    return
                if not ok and now - st['resetting'] < st['reset_wait'] + 8.0:
                    return
                if not ok:
                    st['results'].append((st['queue'][0], f'RESET FAILED {detail}', []))
                    st['queue'].pop(0)
                    st['resetting'] = None
                    st['retried'] = False
                    return
                st['resetting'] = None
                st['retried'] = False
                st['reset_done'] = True
            if not st['queue']:
                unreal.unregister_slate_post_tick_callback(st['handle'])
                st['handle'] = None
                return
            name = st['queue'].pop(0)
            st['reset_done'] = False
            st['prev'] = _last_run.get('results')
            reset_all()
            st['current'], st['started'] = name, now
            globals()['scenario_' + name]()
        except Exception as e:
            # A failure before the scenario started (the reset) drops that scenario, or the same exception would repeat every tick
            name = st.get('current') or (st['queue'].pop(0) if st['queue'] and not st.get('reset_done') else None)
            st['results'].append((name, f'EXCEPTION {e!r}', []))
            st['resetting'] = None
            st['current'] = None
            st['settle_until'] = time.time() + st['settle']

    st['handle'] = unreal.register_slate_post_tick_callback(tick)
    return f"suite of {len(st['queue'])} started"


def suite_report():
    st = _suite
    lines = [f"{n}: {v}" + (f" FAILED {b}" if b else '') for n, v, b in st['results']]
    return ('RUNNING ' if st['handle'] is not None else 'DONE ') + f"{len(st['results'])} done, current {st['current']}, queued {st['queue']}\n" + '\n'.join(lines)
