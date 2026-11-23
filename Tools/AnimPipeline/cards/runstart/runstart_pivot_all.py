# Turnarounds 3 = variant 27 (2026-11-21): composes the two w2 revs (C0 = L stance / R stance, no idle entry; same recipe as the
# w2t clips: runstart_turn_compose_all.py W2T without SRC) and the pivot clips (runstart_pivot_compose.py), then the gates.
# usage: python runstart_pivot_all.py [180L,180R,090L,090R] [KEY=VAL ...]   (KEY=VAL go to the pivot composer)
# Output TAG w2t<turn>p (UE: AS_GSTN_RunStart_Generic_w2t<turn>p, runstart_turn_ue.py RST_SUFFIX=p RST_K=27). Prints <= 2 lines per clip.
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE.split(os.sep + 'Tools' + os.sep)[0]
W2 = dict(R0='2.3', N_REV='19', HOLD='12', RATE_SAG='0.15', LEAN_OUT='22', ENTRY='0', ENTRY_LEAD='1', BOUNCE='0.3', LEAN_DEG='24',
          LEAN_IN='4', ARM='1.7', ARM_BACK='10', DROP_CM='12', DROP_IN='4', DROP_HOLD='13', DROP_OUT='22', PIN_U='19',
          TMAP=os.path.join(ROOT, 'docs', 'notes', 'reviews', 'runstart-t180a_h1', 't180_game_tmap.json'),
          FOOT_RIGID='1', ANKLE_MIN='11', ARM_IK='1', ARM_SOFT='0.85')
turns = (sys.argv[1] if len(sys.argv) > 1 else '180L').split(',')
over = dict(a.split('=', 1) for a in sys.argv[2:])
py = sys.executable
for side, c0 in (('L', '1.25'), ('R', '12.25')):   # stance of the step foot; pivot foot lands on rev f3 (review r1 item 3)
    if any(t.endswith(side) for t in turns):
        r = subprocess.run([py, os.path.join(HERE, 'runstart_generic_compose.py')], env=dict(os.environ, **W2, C0=c0, TAG='p27rev' + side),
                           capture_output=True, text=True)
        if r.returncode:
            print('rev %s FAIL %s' % (side, (r.stdout + r.stderr).strip().splitlines()[-1])); sys.exit(1)
for t in turns:
    r = subprocess.run([py, os.path.join(HERE, 'runstart_pivot_compose.py'), t, 'w2t%sp' % t], env=dict(os.environ, **over), capture_output=True, text=True)
    lines = (r.stdout + r.stderr).strip().splitlines()
    print('%s rc=%d %s' % (t, r.returncode, ' | '.join(lines[:3]) if r.returncode == 0 else lines[-1]))
    if r.returncode == 0:
        g = subprocess.run([py, os.path.join(HERE, 'runstart_pivot_gates.py'), 'w2t%sp' % t, '12'], capture_output=True, text=True)
        print('  ' + ' | '.join((g.stdout + g.stderr).strip().splitlines()[-2:]))
