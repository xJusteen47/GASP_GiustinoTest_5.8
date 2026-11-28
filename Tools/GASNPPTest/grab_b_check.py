# grab-B check (owner playtest 2026-11-27: "with the Overlord kit, B does not grab the dummies"). A PIE driver on L_Test_Hub, the owner's map and
# dummies (the hub's automation spawns them with KIT_Test_DefenceLanding). For each kit in GRABB_KITS (default KIT_Test_Overlord,KIT_Test_Playtest), in
# one Play session: the host gets the kit, a dummy is put 90 cm in front of it, then
#   1. the dummy is offered as a grab target (Interact.Target.Grab on the host),
#   2. a tap of IA_GASNPP_Interact (pad B in IMC_GASNPP_Test), injected through Enhanced Input, grabs it: Body link roles grabber 1 / grabbed 2,
#   3. one stick turn (IA_Move, counter-clockwise) spins it: a Command.Link.Spin.* hit on the dummy,
#   4. a neutral Attack throws it (Notation N.): a Command.Link.Throw.* hit on the dummy and the link gone.
# Nothing is saved. Report: Saved/Logs/grab-ganon/grab_b_check.txt (last line OVERALL: PASS / FAIL), then QUIT_EDITOR.
#   UnrealEditor.exe <uproject> /Game/GSTN/Maps/L_Test_Hub -ExecCmds="py <boot.py>" -unattended -nosplash
#   boot.py:  exec(open(r"<project>/Tools/GASNPPTest/grab_b_check.py").read())
import math
import os
import sys
import time
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(_PROJ, 'Tools', 'GASNPPTest'))
import gasnpp_test as g    # noqa: E402
import link_scenarios as ls  # noqa: E402

g.IA['interact'] = '/Game/GSTN/Input/IA_GASNPP_Interact'

KITS = [k for k in os.environ.get('GRABB_KITS', 'KIT_Test_Overlord,KIT_Test_Playtest').split(',') if k]
OUT = os.path.join(_PROJ, 'Saved', 'Logs', 'grab-ganon', 'grab_b_check.txt')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
HUB = '/Game/GSTN/Maps/L_Test_Hub'
lines, results = [], []
st = {'t': time.time(), 'phase': 'start', 'steps': [], 'kit': 0, 'h': None}


def log(msg):
    lines.append(str(msg))
    unreal.log(f'[grab_b] {msg}')
    with open(OUT, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def check(name, ok, detail):
    results.append(bool(ok))
    log(f'{"PASS" if ok else "FAIL"} | {name} | {detail}')


def host():
    return g.key_of_player(0)


def dummy():
    """The dummy nearest the host (dummies have no player state: their key is the actor name)"""
    v = g.snap(g.worlds()[0])
    me = v[host()]['location']
    cands = [(math.dist(me[:2], s['location'][:2]), k) for k, s in v.items() if not k.startswith('P')]
    return min(cands)[1]


def events(key, prefix):
    return [e for e in g.event_log(key) if any(t.startswith(prefix) for t in e[4])]


def kit_steps(kit):
    """(time, fn) steps of one kit, times from the step start"""
    ctx = {}
    s = []
    s.append((0.0, lambda: log(f'--- {kit}: loadout {g._apply_loadout(host(), kit)}')))

    def place():
        ctx['d'] = dummy()
        ctx['spin0'] = len(events(ctx['d'], 'Command.Link.Spin'))
        ctx['throw0'] = len(events(ctx['d'], 'Command.Link.Throw'))
        ctx['grab0'] = len(events(ctx['d'], 'Command.Link.Grab'))
        g._place_near(0, ctx['d'], (90.0, 0.0))
        log(f'dummy {ctx["d"]} placed 90 cm ahead')
    s.append((1.5, place))
    s.append((2.3, lambda: check(f'{kit} 1 dummy offered as grab target', ls.has_tag(0, host(), 'Interact.Target.Grab'),
                                 f'Interact.Target.Grab on host={ls.has_tag(0, host(), "Interact.Target.Grab")}')))
    # The press goes through Enhanced Input (IA_GASNPP_Interact, the action pad B maps to in IMC_GASNPP_Test), like the owner's pad: 0.15 s, so it
    # spans two editor frames and stays a tap (the hold is the kit swap)
    s.append((2.4, lambda: g.hold(0, 'interact', (1.0, 0.0, 0.0))))
    s.append((2.55, lambda: g.release(0, 'interact')))

    def grabbed():
        h, d = ls.body(0, host()), ls.body(0, ctx['d'])
        ctx['grabbed'] = bool(h and d and h[0] == 1 and d[0] == 2)
        check(f'{kit} 2 B (Interact tap) grabs', ctx['grabbed'],
              f'host body {h}, dummy body {d}, grab hits {len(events(ctx["d"], "Command.Link.Grab")) - ctx["grab0"]}')
    s.append((3.1, grabbed))
    # Two stick turns counter-clockwise (IA_Move x right, y forward): the grab spin needs 360 degrees of stick
    for k in range(17):
        a = math.pi / 2 + k * math.pi / 4
        s.append((3.3 + k * 0.035, (lambda v=(math.cos(a), math.sin(a), 0.0): g.hold(0, 'move', v))))
    s.append((3.3 + 17 * 0.035, lambda: g.release(0, 'move')))
    s.append((4.4, lambda: check(f'{kit} 3 stick turn spins the held dummy', len(events(ctx['d'], 'Command.Link.Spin')) > ctx['spin0'],
                                 f'spin hits on dummy {len(events(ctx["d"], "Command.Link.Spin")) - ctx["spin0"]}, body {ls.body(0, ctx["d"])}')))
    s.append((4.6, lambda: g.combat_tap(0, g.button_mask('Attack'))))

    def thrown():
        n = len(events(ctx['d'], 'Command.Link.Throw')) - ctx['throw0']
        d = ls.body(0, ctx['d'])
        check(f'{kit} 4 neutral Attack throws', n > 0 and not (d and d[0] == 2), f'throw hits on dummy {n}, dummy body {d}, '
              f'damage events {[(e[1], e[3]) for e in events(ctx["d"], "Command.Link.Throw")][-2:]}')
    s.append((6.0, thrown))
    return s, 6.5


def tick(_dt):
    try:
        el = time.time() - st['t']
        if st['phase'] == 'start' and el > 5:
            if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0] != HUB:
                les.load_level(HUB)
            les.editor_request_begin_play()
            st.update(phase='ready', t=time.time())
        elif st['phase'] == 'ready':
            ws = g.worlds()
            ok = ws and len([k for k in g.comps(ws[0]) if not k.startswith('P')]) >= 2
            if ok and el > 12:
                log(f'play ready: worlds {len(ws)}, pawns {sorted(g.comps(ws[0]))}')
                st.update(phase='kit', t=time.time())
            elif el > 120:
                log('pawns not ready')
                st['phase'] = 'quit'
        elif st['phase'] == 'kit':
            if st['kit'] >= len(KITS):
                log(f'OVERALL: {"PASS" if results and all(results) else "FAIL"} ({sum(results)}/{len(results)})')
                st['phase'] = 'quit'
                return
            st['steps'], st['end'] = kit_steps(KITS[st['kit']])
            st.update(phase='run', t=time.time())
        elif st['phase'] == 'run':
            while st['steps'] and el >= st['steps'][0][0]:
                st['steps'].pop(0)[1]()
            if el >= st['end']:
                st['kit'] += 1
                st.update(phase='kit', t=time.time())
        elif st['phase'] == 'quit':
            unreal.unregister_slate_post_tick_callback(st['h'])
            st['phase'] = 'dead'
            les.editor_request_end_play()
            unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
    except Exception:
        log('EXCEPTION ' + traceback.format_exc()[-1500:])
        if st['phase'] == 'run':
            st['steps'] = []
        else:
            st['phase'] = 'quit'


log(f'grab_b_check start, kits {KITS}')
st['h'] = unreal.register_slate_post_tick_callback(tick)
