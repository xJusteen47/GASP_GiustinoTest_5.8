# Wall jump and wall detach check after rollback plan F1 (the wall mode reads both from the command: FGSTN_MoverButtonInputs).
# Boot driver for Tools/CI/Run-Timed.ps1 -Preset Capture -Script <copy of this file without spaces in the path>.
#
# PIE listen host + 2 clients with the harness GameMode (gasnpp_test.setup_editor), network emulation WALL_LAG ms per direction.
# For the host's own pawn (world 0) and client 1's pawn (world 1), twice each: put in front of the harness wall (x=1010), walk in, jump
# onto it; then either a real jump press through Enhanced Input (wall jump) or gstn.Wall.Detach 1 (the detach button, sent in the
# command like a held crouch). For both the pawn's own copy and the host's authoritative copy it logs the Mover mode before, the modes
# seen in the next 0.8 s and the height/distance change, so a wall jump (rises, pushes off) and a detach (falls) are told apart on both
# machines. Lines "WALLCHK RESULT ..." and a summary; ESITO OK when the driver ran to the end (the verdict is in the RESULT lines).
import os, runpy, sys, time
import unreal

_here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools', 'GASNPPTest')
sys.path.insert(0, _here)
try:
    runpy.run_path(os.path.join(_here, 'vd2_mover.py'), run_name='vd2_mover')
except Exception as e:
    print('vd2_mover failed:', e)
import gasnpp_test as m

LAG = int(os.environ.get('WALL_LAG', '60'))
WALL_X, WALL_Y = 1010.0, -880.0
# jump = a real IA_Jump press (Enhanced Input -> combat command -> FGSTN_MoverButtonInputs), jumpdbg = gstn.Wall.Jump (the same command entry,
# without the button layout), detach = gstn.Wall.Detach 1
# crouch = a real IA_Crouch hold (GASP's crouch action); jumpcfg / crouchcfg = the actions the combat InputConfig binds to Input.Button.Jump /
# Input.Button.Crouch, added only when they differ from IA_Jump / IA_Crouch. Each kind for the host and client 1, WALL_ROUNDS rounds; a case
# whose setup misses the wall is retried once
ROUNDS = int(os.environ.get('WALL_ROUNDS', '3'))
KINDS = ['jump', 'jumpdbg', 'detach', 'crouch']
CASES = []
ACTIONS = {'jump': m.IA['jump'], 'crouch': '/Game/Input/IA_Crouch'}


def log(msg):
    unreal.log('WALLCHK ' + msg)


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
# The harness wall (face x=1010) is in the harness level, not in the startup map L_Test_Hub. Opened, never saved
MAP = os.environ.get('WALL_MAP', '/Game/GSTN/_Test/Maps/DefaultLevel_Harness')
log(f'load {MAP}: {les.load_level(MAP)}')
log('setup ' + str(m.setup_editor(True)))
st = {'t0': time.time(), 'phase': 'boot', 'case': 0, 'results': [], 'esito': 'FAIL'}
ia_move = unreal.load_asset(m.IA['move'])


def enter(ph):
    st['phase'] = ph
    st['tp'] = time.time()


def el():
    return time.time() - st['tp']


def copies(idx):
    """(own copy, authoritative copy on the host) of the pawn world idx's player controls"""
    _, pawn = m._local_pawn(idx)
    key = m.key_of_player(idx)
    auth = m.comps(m.worlds()[0])[key].get_owner()
    return pawn, auth


def mode_of(actor):
    return str(actor.get_component_by_class(unreal.CharacterMoverComponent).get_movement_mode_name())


def finish(esito):
    st['esito'] = esito
    try:
        les.editor_request_end_play()
    except Exception as e:
        log(f'end play failed: {e}')
    enter('quit')


def tick(_dt):
    try:
        _tick()
    except Exception as e:
        log(f'driver error in {st["phase"]}: {e}')
        try:
            m.comps(m.worlds()[CASES[st['case']][0]])
        except Exception:
            pass
        finish('FAIL')


def _path(obj):
    try:
        return obj.get_path_name() if obj else 'None'
    except Exception:
        return str(obj)


def log_button_map(pawn):
    """Logs the harness pawn's combat InputConfig: the asset, every button (tag -> input action) and the mapping contexts; adds the
    jumpcfg / crouchcfg cases when the config's Jump / Crouch actions are not GASP's IA_Jump / IA_Crouch"""
    cls = getattr(unreal, 'GSTN_CombatSimComponent', None)
    comp = pawn.get_component_by_class(cls) if cls else None
    log(f'BUTTONMAP pawn {pawn.get_class().get_name()} combat component {_path(comp)}')
    if comp is None:
        return
    cfg = None
    for prop in ('active_input_config', 'input_config'):
        try:
            v = comp.get_editor_property(prop)
            if hasattr(v, 'load_synchronous'):
                v = v.load_synchronous()
            elif hasattr(v, 'get_asset'):
                v = v.get_asset()
            log(f'BUTTONMAP {prop} = {_path(v)}')
            cfg = cfg or v
        except Exception as e:
            log(f'BUTTONMAP {prop} not readable: {e}')
    if cfg is None:
        return
    found = {}
    try:
        for b in cfg.get_editor_property('buttons'):
            tag = str(b.get_editor_property('tag').get_editor_property('tag_name'))
            act = b.get_editor_property('action')
            act = act.load_synchronous() if hasattr(act, 'load_synchronous') else act
            log(f'BUTTONMAP button {tag} -> {_path(act)}')
            found[tag] = act
    except Exception as e:
        log(f'BUTTONMAP buttons not readable: {e}')
    try:
        for c in cfg.get_editor_property('mapping_contexts'):
            ctx = c.get_editor_property('context')
            log(f'BUTTONMAP context {_path(ctx.load_synchronous() if hasattr(ctx, "load_synchronous") else ctx)}')
    except Exception as e:
        log(f'BUTTONMAP contexts not readable: {e}')
    for tag, kind, gasp in (('Input.Button.Jump', 'jumpcfg', ACTIONS['jump']), ('Input.Button.Crouch', 'crouchcfg', ACTIONS['crouch'])):
        act = found.get(tag)
        log(f'BUTTONMAP {tag}: {"MISSING" if act is None else _path(act)} (GASP action {gasp})')
        if act is not None and not _path(act).startswith(gasp):
            ACTIONS[kind] = _path(act)
            KINDS.append(kind)


def _tick():
    ph = st['phase']
    if ph == 'boot' and time.time() - st['t0'] > 8:
        les.editor_request_begin_play()
        log('PIE requested')
        enter('wait_pie')
    elif ph == 'wait_pie':
        ws = m.worlds()
        if len(ws) >= 3 and el() > 8:
            m.net(LAG, 0, 0)
            log(f'worlds {len(ws)}, topology {m.check_topology()}, wall mode {m.wall_mode()}, net {LAG} ms')
            p, _ = copies(1)
            log_button_map(p)
            for r in range(ROUNDS):
                for kind in KINDS:
                    for idx in (0, 1):
                        CASES.append((idx, kind))
            log(f'cases {len(CASES)}: {KINDS} x host, client1 x {ROUNDS} rounds')
            enter('warm')
        elif el() > 60:
            log(f'PIE not ready: {len(ws)} worlds')
            finish('FAIL')
    elif ph == 'warm' and el() > 6:
        start_case()
    elif ph == 'place' and el() > 1.0:
        enter('walk')
    elif ph == 'walk':
        idx, kind = CASES[st['case']]
        pawn, _ = copies(idx)
        comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
        comp.start_holding_input(ia_move, unreal.Vector(*m._steer_value(idx, (WALL_X + 200.0, WALL_Y))))
        if WALL_X - pawn.get_actor_location().x < 130.0 and mode_of(pawn) == 'Walking':
            m.press(idx, 'jump')
            enter('air')
        elif el() > 6:
            record(idx, kind, 'never reached jump distance', None)
    elif ph == 'air':
        idx, kind = CASES[st['case']]
        pawn, auth = copies(idx)
        comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
        comp.start_holding_input(ia_move, unreal.Vector(*m._steer_value(idx, (WALL_X + 200.0, WALL_Y))))
        st.setdefault('air_modes', set()).add(mode_of(pawn))
        if mode_of(pawn) == st['wm']:
            st['air_modes'] = set()
            enter('wall')
        elif el() > 2.5:
            loc = pawn.get_actor_location()
            record(idx, kind, f'never on the wall (mode {mode_of(pawn)}, modes seen after the jump press {sorted(st["air_modes"])}, at x {loc.x:.0f} y {loc.y:.0f} z {loc.z:.0f})', None)
            st['air_modes'] = set()
    elif ph == 'wall' and el() > 0.6:
        idx, kind = CASES[st['case']]
        pawn, auth = copies(idx)
        comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
        comp.stop_holding_input(ia_move)
        st['before'] = {'own': (mode_of(pawn), pawn.get_actor_location()), 'auth': (mode_of(auth), auth.get_actor_location())}
        st['seen'] = {'own': [mode_of(pawn)], 'auth': [mode_of(auth)]}
        st['x0'] = {'own': pawn.get_actor_location().x, 'auth': auth.get_actor_location().x}
        st['maxdz'] = {'own': -1e9, 'auth': -1e9}
        if kind in ('jump', 'jumpcfg'):
            comp.press_input(unreal.load_asset(ACTIONS[kind]))
        elif kind in ('crouch', 'crouchcfg'):
            comp.start_holding_input(unreal.load_asset(ACTIONS[kind]), unreal.Vector(1.0, 0.0, 0.0))
        elif kind == 'jumpdbg':
            unreal.SystemLibrary.execute_console_command(None, 'gstn.Wall.Jump')
        else:
            unreal.SystemLibrary.execute_console_command(None, 'gstn.Wall.Detach 1')
        enter('watch')
    elif ph == 'watch':
        idx, kind = CASES[st['case']]
        pawn, auth = copies(idx)
        for k, a in (('own', pawn), ('auth', auth)):
            if st['seen'][k][-1] != mode_of(a):
                st['seen'][k].append(mode_of(a))
            st['maxdz'][k] = max(st['maxdz'][k], a.get_actor_location().z - st['before'][k][1].z)
        if el() > 0.8:
            if kind == 'detach':
                unreal.SystemLibrary.execute_console_command(None, 'gstn.Wall.Detach 0')
            if kind in ('crouch', 'crouchcfg'):
                pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent).stop_holding_input(unreal.load_asset(ACTIONS[kind]))
            out = {}
            for k, a in (('own', pawn), ('auth', auth)):
                m0, l0 = st['before'][k]
                l1 = a.get_actor_location()
                out[k] = f'{k}: {" > ".join(st["seen"][k])} maxdz {st["maxdz"][k]:+.0f} dz {l1.z - l0.z:+.0f} dx {l1.x - l0.x:+.0f}'
            record(idx, kind, None, out)


def record(idx, kind, error, out):
    who = 'host' if idx == 0 else f'client{idx}'
    if error and not st.get('retried'):
        log(f'retry {kind} {who}: {error}')
        st['retried'] = True
        start_case(advance=False)
        return
    st['retried'] = False
    if error:
        line = f'{kind} {who}: SETUP FAIL {error}'
    else:
        wm = st['wm']
        ok_both = True
        for k in ('own', 'auth'):
            m0, l0 = st['before'][k]
        if kind in ('jump', 'jumpdbg', 'jumpcfg'):
            # a wall jump: the pawn leaves the wall into the air (Falling) and rises above the press height, on both copies
            ok_both = all('Falling' in st['seen'][k] and st['maxdz'][k] > 20 for k in ('own', 'auth'))
        else:
            # a detach: the pawn leaves the wall into the air (Falling) without rising, on both copies (a slide to the floor never shows Falling)
            ok_both = all('Falling' in st['seen'][k] and st['maxdz'][k] < 20 for k in ('own', 'auth'))
        line = f'{kind} {who}: {"PASS" if ok_both else "FAIL"} | ' + ' | '.join(out.values())
    st['results'].append(line)
    log('RESULT ' + line)
    try:
        pawn, _ = copies(idx)
        pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent).stop_holding_input(ia_move)
    except Exception:
        pass
    st['case'] += 1
    if st['case'] < len(CASES):
        start_case()
    else:
        log('SUMMARY ' + ' || '.join(st['results']))
        finish('OK')


def start_case(advance=True):
    idx, kind = CASES[st['case']]
    st['wm'] = m.wall_mode()
    key = m.key_of_player(idx)
    c = m.comps(m.worlds()[0])[key]
    sp = c.get_spawn_location()
    c.server_reset_to_spawn(unreal.Vector(680.0 - sp.x, WALL_Y - sp.y, 0.0))
    unreal.SystemLibrary.execute_console_command(None, 'gstn.Wall.ResetMemory')
    log(f'case {st["case"]}: {kind} for world {idx} ({key}), wall mode {st["wm"]}')
    enter('place')


def _quit():
    pass


def tick_wrapper(dt):
    if st['phase'] == 'quit':
        if el() > 4:
            try:
                log('restore ' + str(m.setup_editor(False)))
            except Exception as e:
                log(f'restore failed: {e}')
            unreal.log('ESITO: ' + st['esito'])
            unreal.unregister_slate_post_tick_callback(_h)
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
        return
    tick(dt)


_h = unreal.register_slate_post_tick_callback(tick_wrapper)
log(f'driver loaded, lag {LAG}, cases {CASES}')
