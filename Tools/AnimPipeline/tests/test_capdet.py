"""capdet (basecmp, post, orchestrator order) on synthetic data, offline, no editor. Run: python tests/test_capdet.py"""
import contextlib, io, json, os, shutil, sys, tempfile, unittest

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'capdet'))
import basecmp, post, capdet  # noqa: E402

N = 40
VIEWS = ['behind', 'side', 'q34', 'close']


def joints(swing=1.0, speed=1.0, travel=1.0, n=60):
    t = np.linspace(0, 2 * np.pi, n)
    J = {'root': np.stack([travel * 100 * t / (2 * np.pi), np.zeros(n), np.zeros(n)], 1),
         'thigh_MainPoint_l': np.tile([0, 90, 10], (n, 1)).astype(float), 'thigh_MainPoint_r': np.tile([0, 90, -10], (n, 1)).astype(float)}
    for b in ('hand', 'foot', 'ball'):
        for s, ph in (('l', 0.0), ('r', np.pi)):
            J[f'{b}_MainPoint_{s}'] = np.stack([swing * 40 * np.sin(speed * t + ph), 50 + 0 * t, 12 * (1 if s == 'l' else -1) + 0 * t], 1)
    return {'fps': 30, 'joints': {k: v.tolist() for k, v in J.items()}}


def wjson(d, name, data):
    p = os.path.join(d, name)
    with open(p, 'w') as fh:
        json.dump(data, fh)
    return p


class BaseCmp(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def cmp(self, **kw):
        return basecmp.compare(wjson(self.d, 'b.json', joints()), wjson(self.d, 'v.json', joints(**kw)))

    def test_same_is_ok(self):
        r = self.cmp(); self.assertTrue(r['ok'], r['fails'])

    def test_smaller_swing_fails_over_15(self):
        r = self.cmp(swing=0.8); self.assertFalse(r['ok']); self.assertTrue(r['fails'][0][0].startswith('amp.'))
        self.assertTrue(basecmp.lines(r)[0].startswith('BASE: FAIL amp'))

    def test_14_percent_passes(self):
        self.assertTrue(self.cmp(swing=0.86)['ok'])

    def test_slower_fails(self):
        r = self.cmp(speed=0.7); self.assertFalse(r['ok'])  # same extent, 30 % slower along the track

    def test_less_root_travel_fails(self):
        r = self.cmp(travel=0.7); self.assertEqual(r['fails'][0][0], 'root.travel')

    def test_gain_is_not_a_fail(self):
        self.assertTrue(self.cmp(swing=1.4, travel=1.5)['ok'])

    def test_no_common_joints(self):
        a = wjson(self.d, 'a.json', {'fps': 30, 'joints': {'x': [[0, 0, 0]] * 5}})
        r = basecmp.compare(a, a); self.assertFalse(r['ok']); self.assertIn('no common joints', r['reason'])


def rows(n, clip='AS_RunStart', w=1.0):
    return [{'f': f, 'combat': {'anim': [f'{clip} {w} 0.1']}} for f in range(n)]


def make_capture(d, seq='s', n=N, dark=None, dup=None, clip='AS_RunStart', w=1.0, nrows=None):
    rng = np.random.RandomState(7)
    for v in VIEWS:
        os.makedirs(os.path.join(d, seq, v), exist_ok=True)
        for f in range(n):
            a = (rng.rand(48, 48, 3) * 180 + 40).astype('uint8')
            if v == 'side' and dark == f:
                a[:] = 0
            if v == 'side' and dup and f == dup:
                a = np.array(Image.open(os.path.join(d, seq, v, f'f{f - 1:03d}.png')).convert('RGB'))
            Image.fromarray(a).save(os.path.join(d, seq, v, f'f{f:03d}.png'))
    with open(os.path.join(d, seq, 'frames.jsonl'), 'w') as fh:
        for r in rows(nrows or n, clip, w):
            fh.write(json.dumps(r) + '\n')


SPEC = {'name': 't', 'sequence': 's', 'frames': N, 'expect': {'clip': 'AS_RunStart', 'window': [10, 35]}}


class Post(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def test_ok_and_video(self):
        make_capture(self.d); ok, ls = post.run(self.d, 's', SPEC)
        self.assertTrue(ok, ls); self.assertEqual(ls[0], 'ASSERT: OK')
        self.assertTrue(os.path.exists(os.path.join(self.d, 's', 'split.mp4')) or os.path.exists(os.path.join(self.d, 's', 'split.webp')))
        self.assertTrue(os.path.exists(os.path.join(self.d, 's', 'fingerprint.json')))

    def test_wrong_clip(self):
        make_capture(self.d, clip='Idle'); ok, ls = post.run(self.d, 's', SPEC)
        self.assertFalse(ok); self.assertTrue(ls[0].startswith('ASSERT: FAIL'))

    def test_black_frame(self):
        make_capture(self.d, dark=20); ok, ls = post.run(self.d, 's', SPEC)
        self.assertFalse(ok); self.assertIn('black side', ls[0])

    def test_repeated_frame(self):
        make_capture(self.d, dup=20); ok, ls = post.run(self.d, 's', SPEC)
        self.assertFalse(ok); self.assertIn('duplicate side', ls[0])
        s2 = dict(SPEC, hold_ok=True); self.assertTrue(post.run(self.d, 's', s2)[0])

    def test_wrong_duration(self):
        make_capture(self.d, n=N - 5); ok, ls = post.run(self.d, 's', SPEC)
        self.assertFalse(ok); self.assertTrue('count' in ls[0] or 'too short' in ls[0], ls[0])

    def test_rows_png_mismatch(self):
        make_capture(self.d, nrows=N - 3); ok, ls = post.run(self.d, 's', SPEC)
        self.assertFalse(ok)

    def test_determinism_verify(self):
        a, b = tempfile.mkdtemp(), tempfile.mkdtemp()
        try:
            make_capture(a); make_capture(b); post.run(a, 's', SPEC); post.run(b, 's', SPEC)
            self.assertTrue(post.verify(a, b, 's')[0])
            make_capture(b, dark=5); post.run(b, 's', SPEC)
            ok, ls = post.verify(a, b, 's'); self.assertFalse(ok); self.assertIn('side', ls[0])
        finally:
            shutil.rmtree(a, ignore_errors=True); shutil.rmtree(b, ignore_errors=True)


class Order(unittest.TestCase):
    """a gate that fails or is missing: the editor must never start"""
    def run_main(self, spec, extra=()):
        d = tempfile.mkdtemp(); sp = wjson(d, 'spec.json', spec); called = []
        orig = capdet.run_capture
        capdet.run_capture = lambda *a, **k: called.append(1) or (0, ['CAPTURE: x'])
        buf = io.StringIO()
        try:
            with contextlib.redirect_stdout(buf):
                rc = capdet.main([sp, '--out', os.path.join(d, 'out'), *extra])
        finally:
            capdet.run_capture = orig; shutil.rmtree(d, ignore_errors=True)
        return rc, buf.getvalue(), called

    def test_missing_gates_stop(self):
        rc, out, called = self.run_main(dict(SPEC, kit='K'))
        self.assertEqual(rc, 1); self.assertIn('GATES: MANCANTE', out); self.assertIn('SALTATA', out); self.assertFalse(called)

    def test_failing_gate_stops(self):
        orig = capdet.run_gates
        capdet.run_gates = lambda spec, out: (False, ['GATES: FAIL ankles f3 6.0 cm'])
        try:
            rc, out, called = self.run_main(dict(SPEC, kit='K'))
        finally:
            capdet.run_gates = orig
        self.assertEqual(rc, 1); self.assertFalse(called); self.assertLessEqual(len(out.strip().splitlines()), 5)

    def test_base_gate_failure_stops(self):
        d = tempfile.mkdtemp()
        try:
            b, v = wjson(d, 'b.json', joints()), wjson(d, 'v.json', joints(swing=0.5))
            orig = capdet.run_gates
            # only the BASE half runs for real: the first half (runstart_pivot_gates) needs the editor's loop dump
            capdet.run_gates = lambda spec, out: (lambda r: (r['ok'], basecmp.lines(r)))(basecmp.compare(b, v))
            try:
                rc, out, called = self.run_main(dict(SPEC, kit='K'))
            finally:
                capdet.run_gates = orig
            self.assertEqual(rc, 1); self.assertIn('BASE: FAIL', out); self.assertFalse(called)
        finally:
            shutil.rmtree(d, ignore_errors=True)

    def test_gates_ok_capture_runs(self):
        orig = capdet.run_gates
        capdet.run_gates = lambda spec, out: (True, ['GATES: OK'])
        try:
            rc, out, called = self.run_main(dict(SPEC, kit='K'), ['--stage', 'capture'])
        finally:
            capdet.run_gates = orig
        self.assertEqual(rc, 0); self.assertTrue(called)


if __name__ == '__main__':
    unittest.main(verbosity=1)
