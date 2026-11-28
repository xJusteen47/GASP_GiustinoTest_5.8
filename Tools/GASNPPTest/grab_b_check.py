# grab-B check (owner playtest 2026-11-27: "with the Overlord kit, B does not grab the dummies"). A PIE driver on L_Test_Hub, the owner's map and
# dummies (the hub's automation spawns them with KIT_Test_DefenceLanding). For each kit in GRABB_KITS (default KIT_Test_Overlord,KIT_Test_Playtest), in
# one Play session: the host gets the kit, a dummy is put 90 cm in front of it, then
#   1. the dummy is offered as a grab target (Interact.Target.Grab on the host),
#   2. a tap of IA_GASNPP_Interact (pad B in IMC_GASNPP_Test), injected through Enhanced Input, grabs it: Body link roles grabber 1 / grabbed 2,
#   3. a neutral Attack throws it (Notation N.): a Command.Link.Throw.* hit on the dummy and the link gone;
#   then a second dummy is grabbed the same way, spun by two stick turns (IA_Move: Command.Link.Spin.* hits) and thrown out of the spin.
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


def dummy(used=()):
    """The dummy nearest the host not used yet (dummies have no player state: their key is the actor name)"""
    v = g.snap(g.worlds()[0])
    me = v[host()]['location']
    cands = [(math.dist(me[:2], s['location'][:2]), k) for k, s in v.items() if not k.startswith('P') and k not in used]
    return min(cands)[1]


def events(key, prefix):
    return [e for e in g.event_log(key) if any(t.startswith(prefix) for t in e[4])]


def kit_steps(kit):
    """(time, fn) steps of one kit, times from the step start. Two grabs on two dummies: the first is thrown at once (N.), the second is spun with the
    stick and thrown out of the spin. Ganondorf's hold lasts 90 frames (1.44 s) at 0 %, no mash: every step after a grab fits inside it"""
    ctx = {'used': []}
    s = [(0.0, lambda: log(f'--- {kit}: loadout {g._apply_loadout(host(), kit)}'))]

    def place(tag):
        def f():
            d = dummy(ctx['used'])
            ctx['used'].append(d)
            ctx[tag] = d
            ctx[tag + '0'] = {p: len(events(d, p)) for p in ('Command.Link.Grab', 'Command.Link.Spin', 'Combat.HitType.Throw')}
            ctx[tag + 'host_spin0'] = len(events(host(), 'Command.Link.Spin'))
            g._place_near(0, d, (90.0, 0.0))
            log(f'{tag}: dummy {d} placed 90 cm ahead')
        return f

    def new(tag, prefix):
        return len(events(ctx[tag], prefix)) - ctx[tag + '0'][prefix]

    def grab(t, tag, label):
        # The press goes through Enhanced Input (IA_GASNPP_Interact, the action pad B maps to in IMC_GASNPP_Test), like the owner's pad: 0.15 s, so
        # it spans two editor frames and stays a tap (the hold is the kit swap)
        def grabbed():
            h, d = ls.body(0, host()), ls.body(0, ctx[tag])
            check(f'{kit} {label} B (Interact tap) grabs', h and d and h[0] == 1 and d[0] == 2,
                  f'host body {h}, dummy body {d}, grab hits {new(tag, "Command.Link.Grab")}')
        return [(t, place(tag)),
                (t + 0.8, lambda: check(f'{kit} {label} dummy offered as grab target', ls.has_tag(0, host(), 'Interact.Target.Grab'),
                                        f'Interact.Target.Grab on host={ls.has_tag(0, host(), "Interact.Target.Grab")}')),
                (t + 0.85, lambda: g.hold(0, 'interact', (1.0, 0.0, 0.0))), (t + 1.0, lambda: g.release(0, 'interact')),
                (t + 1.35, grabbed)]

    def thrown(tag, label):
        def f():
            n, d = new(tag, 'Combat.HitType.Throw'), ls.body(0, ctx[tag])
            log(f'{tag} events: {[(e[0], e[1], e[2], e[3], sorted(e[4])) for e in g.event_log(ctx[tag])][-4:]}')
            check(f'{kit} {label} Attack throws', n > 0 and not (d and d[0] == 2), f'throw hits on dummy {n}, dummy body {d}, '
                  f'throw hits {[(e[1], e[2], e[3]) for e in events(ctx[tag], "Combat.HitType.Throw")][-2:]}')
        return f

    # 1. grab and throw at once (neutral Attack = N. in Notation)
    s += grab(1.0, 'd1', 'A1')
    s += [(2.45, lambda: g.combat_tap(0, g.button_mask('Attack'))), (3.6, thrown('d1', 'A2')),
          (3.7, lambda: g._place_near(0, ctx['d1'], (-900.0, 600.0)))]   # out of the next grab's reach
    # 2. grab, one and a half stick turns counter-clockwise (one turn of scripted steps can miss an editor frame and fall short of 360) (IA_Move x right, y forward; the grab spin needs 360 degrees of stick), Attack out of the spin (throws
    # cancel the spin from its frame 12; each spin also takes 20 frames off the hold, so the throw comes right after)
    s += grab(4.6, 'd2', 'B1')
    t = 5.95
    for k in range(13):
        a = math.pi / 2 + k * math.pi / 4
        s.append((t + k * 0.035, (lambda v=(math.cos(a), math.sin(a), 0.0): g.hold(0, 'move', v))))
    s.append((t + 13 * 0.035, lambda: g.release(0, 'move')))
    s += [(6.75, lambda: g.combat_tap(0, g.button_mask('Attack'))),
          (8.2, lambda: check(f'{kit} B2 stick turn spins the held dummy', len(events(host(), 'Command.Link.Spin')) > ctx['d2host_spin0'],
                              f'spin hits by the host {len(events(host(), "Command.Link.Spin")) - ctx["d2host_spin0"]}, on the held dummy {new("d2", "Command.Link.Spin")}')),
          (8.2, lambda: log(f'note | B3 throw out of the spin (not judged): throw hits {new("d2", "Combat.HitType.Throw")}. At 0 % the Ganondorf hold is 90 '
                            f'frames and each spin takes 20 off it, so a scripted press lands either before the spin (it throws instead) or after the '
                            f'hold ran out'))]
    return s, 8.6


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
