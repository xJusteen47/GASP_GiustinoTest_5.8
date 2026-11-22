"""Conservation guard (clipcheck/conserve.py, project rule 10): an automatic fix that takes away > 15 % of motion is a FAIL.
Offline, no editor. The FBX case needs `pip install bpy` and is skipped without it.
Run: python tests/test_conserve.py   (or pytest tests/test_conserve.py)"""
import math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'clipcheck'))
import clip_filters  # noqa: E402
import conserve  # noqa: E402

FPS = 60.0
N = 60
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
FBX = os.path.join(ROOT, 'docs', 'notes', 'reviews', 'dash-ganon', 'r3', 'AS_GSTN_Dash_Ganon_r3.fbx')


def q_axis(deg, axis=(0.0, 0.0, 1.0)):
    h = math.radians(deg) / 2.0
    s = math.sin(h)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(h))


def swing(amp_deg, n=N, cycles=1.0):
    return [q_axis(amp_deg * math.sin(2 * math.pi * cycles * f / (n - 1))) for f in range(n)]


def clip(arm_amp=60.0, leg_amp=40.0, root_speed=300.0, pelvis_bob=4.0):
    """A small synthetic run-like clip: arms and legs swing, the pelvis bobs, the root moves forward."""
    tr = {}
    for b in conserve.CHAINS['arm_l'] + conserve.CHAINS['arm_r']:
        tr[b] = {'q': swing(arm_amp), 't': [(0, 0, 0)] * N}
    for b in conserve.CHAINS['leg_l'] + conserve.CHAINS['leg_r']:
        tr[b] = {'q': swing(leg_amp, cycles=2.0), 't': [(0, 0, 0)] * N}
    for b in ('spine_01', 'head'):
        tr[b] = {'q': swing(10.0), 't': [(0, 0, 0)] * N}
    tr['pelvis'] = {'q': [q_axis(0)] * N, 't': [(0, 0, 90 + pelvis_bob * math.sin(4 * math.pi * f / (N - 1))) for f in range(N)]}
    tr['root'] = {'q': [q_axis(0)] * N, 't': [(root_speed * f / FPS, 0, 0) for f in range(N)]}
    return tr


def test_identical_passes():
    r = conserve.compare(clip(), clip(), FPS)
    assert r['ok'], r['fails']


def test_damped_arms_fail():
    before = clip()
    after = conserve.overlay(before, {b: {'q': swing(60.0 * 0.75)} for b in conserve.CHAINS['arm_r']})
    r = conserve.compare(before, after, FPS)
    assert not r['ok']
    assert any(f.startswith('excursion.arm_r:') for f in r['fails']), r['fails']
    assert any(f.startswith('speed.arm_r:') for f in r['fails']), r['fails']
    assert not any('arm_l' in f for f in r['fails'])


def test_small_loss_passes():
    before = clip()
    after = conserve.overlay(before, {b: {'q': swing(60.0 * 0.9)} for b in conserve.CHAINS['arm_r']})
    assert conserve.compare(before, after, FPS)['ok']


def test_gain_passes():
    before = clip()
    after = conserve.overlay(before, {b: {'q': swing(90.0)} for b in conserve.CHAINS['arm_l']})
    assert conserve.compare(before, after, FPS)['ok']


def test_root_and_pelvis_loss_fail():
    r = conserve.compare(clip(), clip(root_speed=240.0), FPS)
    assert not r['ok'] and any(f.startswith('root') for f in r['fails']), r['fails']
    r = conserve.compare(clip(), clip(pelvis_bob=2.5), FPS)
    assert not r['ok'] and any(f.startswith('pelvis') for f in r['fails']), r['fails']


def test_still_chain_not_judged():
    before = clip()
    tiny = {b: {'q': swing(2.0)} for b in ('spine_01', 'head')}
    tinier = {b: {'q': swing(0.5)} for b in ('spine_01', 'head')}
    r = conserve.compare(conserve.overlay(before, tiny), conserve.overlay(before, tinier), FPS)
    assert r['ok'], r['fails']
    assert r['metrics']['excursion.spine:head'][2] < -50   # reported, not judged


def test_deflick_is_not_a_loss():
    """A one-frame 120 deg glitch removed by the real filter (clip_filters.clean_track) must pass the guard."""
    before = clip()
    q = swing(60.0)
    q[30] = q_axis(120.0, (1.0, 0.0, 0.0))
    before['hand_r'] = {'q': q, 't': [(0, 0, 0)] * N}
    new, rep = clip_filters.clean_track(q, FPS)
    assert rep is not None   # the filter did remove it
    r = conserve.compare(before, conserve.overlay(before, {'hand_r': {'q': new}}), FPS)
    assert r['ok'], r['fails']


def test_eased_whip_fails():
    """A fast whip (90 deg in 3 frames) stretched over 15 frames keeps its swing but loses its speed: FAIL (cartoon snaps are life)."""
    def whip(span):
        return [q_axis(90.0 * min(1.0, max(0.0, (f - 20) / float(span)))) for f in range(N)]
    before = clip()
    before['lowerarm_r'] = {'q': whip(3), 't': [(0, 0, 0)] * N}
    after = conserve.overlay(before, {'lowerarm_r': {'q': whip(15)}})
    r = conserve.compare(before, after, FPS)
    assert not r['ok'] and any(f.startswith('speed.arm_r:lowerarm_r') for f in r['fails']), r['fails']
    assert r['metrics']['excursion.arm_r:lowerarm_r'][2] > -1   # the swing itself is kept


def test_held_pose_counts():
    """A held extreme (4 frames) shortened by a fix is a real loss, not a flick."""
    before = clip()
    q = [q_axis(10.0)] * N
    q = q[:28] + [q_axis(80.0)] * 4 + q[32:]
    before['upperarm_l'] = {'q': q, 't': [(0, 0, 0)] * N}
    after = conserve.overlay(before, {'upperarm_l': {'q': [q_axis(10.0)] * 28 + [q_axis(50.0)] * 4 + [q_axis(10.0)] * (N - 32)}})
    r = conserve.compare(before, after, FPS)
    assert not r['ok'] and any(f.startswith('excursion.arm_l:upperarm_l') for f in r['fails']), r['fails']


def _has_bpy():
    try:
        import bpy  # noqa: F401
        return True
    except Exception:
        return False


def test_fbx_cli():
    """clip_check.py --conserve on a real UE export: same file = OK (skipped without pip bpy)."""
    if not _has_bpy() or not os.path.exists(FBX):
        print('  skip: no bpy or no fbx'); return
    out = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'conserve_test.json')
    p = subprocess.run([sys.executable, os.path.join(HERE, '..', 'clipcheck', 'clip_check.py'), '--conserve', FBX, FBX, out],
                       capture_output=True, text=True)
    assert p.returncode == 0 and 'CONSERVE: OK' in p.stdout, p.stdout + p.stderr


if __name__ == '__main__':
    fails = 0
    for name, fn in sorted((k, v) for k, v in dict(globals()).items() if k.startswith('test_')):
        try:
            fn(); print(f'PASS {name}')
        except AssertionError as e:
            fails += 1; print(f'FAIL {name}: {e}')
    print(f'ESITO: {"OK" if not fails else "FAIL"} ({fails} failed)')
    sys.exit(1 if fails else 0)
