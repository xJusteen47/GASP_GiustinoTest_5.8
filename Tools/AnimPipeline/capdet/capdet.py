"""Deterministic capture, one command (owner's "7 deterministic additions" 1-4, 2026-11-23): gates -> capture -> ASSERT -> split-screen video.

    python Tools/AnimPipeline/capdet/capdet.py <spec.json> [--stage gates|capture|post|all] [--out <dir>] [--dry]
    python Tools/AnimPipeline/capdet/capdet.py --verify <cap_a> <cap_b> <sequence>      same frames in two runs?

Order, and why: (1) GATES: the clip is judged on its own (rule 15b: ankle gap, toe angle, bone lengths, elbow/knee/wrist jitter,
interpenetration; runstart_pivot_gates.py) and against the BASE clip (basecmp.py: amplitude, speed, root motion, 15 % limit). A fail stops
here: NO editor starts, no capture, no review. (2) CAPTURE: one Play session through pie_capture_timed.ps1 (Run-Timed, 15 min, kills its
process tree, writes ESITO: TIMEOUT), fixed seed, VFX off, scripted inputs, 1/60 s per rendered frame, fixed cameras and fill lights, four
views at once (behind, side, 3/4, close-up). (3) POST: ASSERT (clip weight, count/duration, no black, no repeated frame) and the 2x2
split-screen video at 60 fps, fingerprint for the determinism check.

Spec (json), paths relative to the repo root:
  name, kit, sequence             capture name; the kit and the sequence of pie_capture_move.py
  seed                            integer (default 1); logged, set for python's RNG in the editor, and VFX are off ("vfx": true to keep them)
  cvars                           "a 1;b 2" console variables set before Play
  expect                          {"clip", "window": [first, last], "additive": [..], "min_weight": 0.8, "coverage": 0.9}  (capture_assert.py)
  frames                          rendered frames the sequence has (duration = frames / 60 s); hold_ok: true when the move ends on a hold
  gates                           {"joints": <clip joints json>, "base_joints": <base clip joints json>, "win": 18}; both missing = no gates
                                  and the run says so: "GATES: MANCANTE" is a stop, never a pass (--no-gates only for a base capture)
  clients                         PIE_CAPTURE_CLIENTS (default 0: one world, ~3x faster)
  max_minutes                     default 15 (project limit for captures)
  monitor                         screen number the editor window goes to (default 2 if it exists, else the primary)
Prints at most 5 lines to the console; everything else is in <out>/capdet.json and the stage reports. Exit: 0 OK, 1 a gate or the ASSERT
failed, 2 usage, 3 capture did not finish (ESITO not OK), 124 timeout.
"""
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
sys.path.insert(0, HERE)
import basecmp  # noqa: E402
import post     # noqa: E402

GATES_PY = os.path.join(ROOT, 'Tools', 'AnimPipeline', 'cards', 'runstart', 'runstart_pivot_gates.py')
CAPTURE_PS1 = os.path.join(ROOT, 'Tools', 'GASNPPTest', 'pie_capture_timed.ps1')


def P(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


def run_gates(spec, out):
    """Returns (ok, [lines]). Missing inputs are a FAIL (a capture without gates is how crossed feet reached the owner)."""
    g = spec.get('gates')
    if not g or not g.get('joints'):
        return False, ['GATES: MANCANTE no "gates.joints" in the spec (give the clip joints json, or --no-gates for a base capture)']
    if not os.path.exists(P(g['joints'])):
        return False, [f'GATES: MANCANTE {g["joints"]} does not exist (export the clip first)']
    r = subprocess.run([sys.executable, '-I', GATES_PY, '--json', P(g['joints']), str(g.get('win', 18))], cwd=ROOT, capture_output=True,
                       text=True, timeout=300)
    last = [l for l in (r.stdout or '').splitlines() if l.strip()]
    tail = last[-1] if last else ''
    ok = r.returncode == 0 and tail.startswith('GATES: OK')
    lines = [tail if tail.startswith('GATES:') else 'GATES: FAIL ' + (r.stderr.strip().splitlines() or ['no output'])[-1][:160]]
    open(os.path.join(out, 'gates.txt'), 'w', encoding='utf-8').write(r.stdout + '\n' + r.stderr)
    if ok and g.get('base_joints'):
        if not os.path.exists(P(g['base_joints'])):
            return False, lines + [f'BASE: MANCANTE {g["base_joints"]} does not exist']
        rep = basecmp.compare(P(g['base_joints']), P(g['joints']))
        json.dump(rep, open(os.path.join(out, 'basecmp.json'), 'w', encoding='utf-8'), indent=1)
        lines += basecmp.lines(rep)[:2]
        ok = rep['ok']
    return ok, lines


def write_assert_spec(spec, out):
    e = dict(spec['expect']); e.setdefault('min_weight', 0.8); e.setdefault('coverage', 0.9)
    p = os.path.join(out, 'asserts.json')
    json.dump({spec['sequence']: [e]}, open(p, 'w', encoding='utf-8'))
    return p


def run_capture(spec, out, dry=False):
    """Run-Timed does the timing: on the limit it kills the editor and its children and writes ESITO: TIMEOUT to <out>/run.log.esito."""
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ)
    env.update(PIE_CAPTURE_VIEWSET='det', PIE_CAPTURE_SEED=str(spec.get('seed', 1)), PIE_CAPTURE_CVARS=spec.get('cvars', ''),
               PIE_CAPTURE_NOVFX='0' if spec.get('vfx') else '1', PIE_CAPTURE_CLIENTS=str(spec.get('clients', 0)),
               PIE_CAPTURE_ASSERT_REPLACE='1', PIE_CAPTURE_SIZE=str(spec.get('size', 480)))
    cmd = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', CAPTURE_PS1, '-Kit', spec['kit'], '-Out', out,
           '-Sequences', spec['sequence'], '-Assert', write_assert_spec(spec, out), '-MaxMinutes', str(spec.get('max_minutes', 15)),
           '-Monitor', str(spec.get('monitor', 2))]
    if dry:
        return None, ['CAPTURE: dry run: ' + ' '.join(cmd[-10:])]
    t0 = time.time()
    r = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True)
    esito_f = os.path.join(out, 'run.log.esito')
    esito = open(esito_f, encoding='utf-8-sig').readline().strip() if os.path.exists(esito_f) else 'ESITO: MANCANTE'
    open(os.path.join(out, 'esito.txt'), 'w', encoding='utf-8').write(esito + f'\nrc={r.returncode} {time.time() - t0:.0f}s\n')
    return r.returncode, [esito + f' (rc {r.returncode}, {time.time() - t0:.0f} s)']


def main(argv):
    argv = list(argv)
    if argv[:1] == ['--verify'] and len(argv) == 4:
        ok, ls = post.verify(argv[1], argv[2], argv[3]); print('\n'.join(ls)); return 0 if ok else 1
    stage, out, dry, nogates = 'all', None, False, False
    for flag in ('--stage', '--out'):
        if flag in argv:
            i = argv.index(flag); v = argv[i + 1]; del argv[i:i + 2]
            if flag == '--stage': stage = v
            else: out = v
    if '--dry' in argv: argv.remove('--dry'); dry = True
    if '--no-gates' in argv: argv.remove('--no-gates'); nogates = True
    if len(argv) != 1 or not os.path.exists(argv[0]) or stage not in ('gates', 'capture', 'post', 'all'):
        print(__doc__.split('\n\n')[0]); return 2
    spec = json.load(open(argv[0], encoding='utf-8'))
    out = P(out or os.path.join('Saved', 'Capdet', spec['name']))
    os.makedirs(out, exist_ok=True)
    rep, lines, rc = {'spec': spec, 'seed': spec.get('seed', 1), 'stages': {}}, [], 0
    if stage in ('gates', 'all') and not nogates:
        ok, ls = run_gates(spec, out); rep['stages']['gates'] = {'ok': ok, 'lines': ls}; lines += ls
        if not ok:
            lines.append('CATTURA: SALTATA (un gate non passa: niente editor, niente review)')
            rc = 1
    if rc == 0 and stage in ('capture', 'all'):
        crc, ls = run_capture(spec, out, dry); rep['stages']['capture'] = {'rc': crc, 'lines': ls}; lines += ls
        if crc not in (None, 0, 3):
            rc = 124 if crc == 124 else 3
    if rc == 0 and stage in ('post', 'all') and not dry:
        ok, ls = post.run(out, spec['sequence'], spec); rep['stages']['post'] = {'ok': ok, 'lines': ls}; lines += ls
        rc = 0 if ok else 1
    json.dump(rep, open(os.path.join(out, 'capdet.json'), 'w', encoding='utf-8'), indent=1)
    print('\n'.join(lines[:5]))
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
