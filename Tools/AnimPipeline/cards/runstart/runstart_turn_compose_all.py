# Turnarounds-2 (2026-11-21): composes the 4 turnaround starts (180/90 L/R) with the w2t recipe (runstart-turnaround-desktop) plus
# the new knobs, in one call. usage: python runstart_turn_compose_all.py <suffix> [KEY=VAL ...]
#   suffix: output TAG = w2t<deg><side><suffix> (e.g. 'f' -> w2t180Lf); extra KEY=VAL override the recipe (FOOT_RIGID=0 ANKLE_MIN=0
#   with suffix 'chk' must rebuild the old w2t* bit-exact: checked here, max point diff printed).
# Prints 1 line per clip (+ the composer's ankle line). Composer output: Saved/AnimPipeline/runstart/generic_<TAG>_{pose.npz,curves.json}
import os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE.split(os.sep + 'Tools' + os.sep)[0]
RS = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart')
W1 = dict(R0='2.3', N_REV='19', HOLD='12', RATE_SAG='0.15', LEAN_OUT='20', ENTRY='4', ENTRY_LEAD='1', BOUNCE='0.3', LEAN_DEG='24',
          LEAN_IN='4', ARM='1.7', ARM_BACK='10', DROP_CM='12', DROP_IN='4', DROP_HOLD='12', DROP_OUT='19')
W2T = dict(W1, LEAN_OUT='22', DROP_HOLD='13', DROP_OUT='22', PIN_U='19', TURN_LEAN='chest',
           TMAP=os.path.join(ROOT, 'docs', 'notes', 'reviews', 'runstart-t180a_h1', 't180_game_tmap.json'))
CLIPS = {'180L': '180_L_Lfoot', '180R': '180_R_Rfoot', '090L': '090_L_Lfoot', '090R': '090_R_Rfoot'}
suffix = sys.argv[1]
over = dict(a.split('=', 1) for a in sys.argv[2:])
for key, src in CLIPS.items():
    tag = 'w2t%s%s' % (key, suffix)
    env = dict(os.environ, **W2T, **over, TAG=tag, SRC=os.path.join(RS, 'turn', 'M_Relaxed_Run_Reface_Start_%s.meta.json' % src))
    r = subprocess.run([sys.executable, os.path.join(HERE, 'runstart_generic_compose.py')], env=env, capture_output=True, text=True)
    lines = (r.stdout + r.stderr).strip().splitlines()
    ank = [l for l in lines if l.startswith('ankle')]
    msg = '%s rc=%d %s | %s' % (tag, r.returncode, lines[0][:70] if lines else '', ank[0] if ank else (lines[-1][:120] if lines else ''))
    if suffix == 'chk' and r.returncode == 0:
        a, b = np.load(os.path.join(RS, 'generic_%s_pose.npz' % tag)), np.load(os.path.join(RS, 'generic_w2t%s_pose.npz' % key))
        msg += ' | max diff vs w2t%s %.4f' % (key, max(float(np.abs(a[f] - b[f]).max()) for f in b.files))
    print(msg)
