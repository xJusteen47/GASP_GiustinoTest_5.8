"""capdet additions 5-7 (montage, hash cache, approved-pose regression) on synthetic data, offline, no editor and no real repo.

Run: python Tools/AnimPipeline/tests/test_capdet2.py

The three scripts are the parts that can hide a mistake instead of showing it: a montage that silently shifts a frame, a cache that
serves a stale capture, a regression that passes because it found nothing to check. So every test here is about the FAILURE being
loud: a missing frame, a changed input, a pose that moved, an unverifiable row.

The git tests build a throwaway repository in a temp dir (git init, a tag, a changed file): no network, no project state.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
CAPDET = os.path.join(os.path.dirname(HERE), 'capdet')
sys.path.insert(0, CAPDET)
import cache, montage, regress  # noqa: E402

N = 24
VIEWS = ['behind', 'side', 'q34', 'close']
SPEC = {'name': 't', 'sequence': 's', 'frames': N, 'expect': {'clip': 'AS_RunStart', 'window': [4, 19]},
        'ab': {'hit': 8, 'faf': 16, 'around': 1, 'video': [4, 12]}}


def make_capture(d, seq='s', n=N, seed=7, views=VIEWS, skip=None):
    """A capture laid out the way pie_capture_move.py writes one. `skip` drops that frame from the 'side' view."""
    rng = np.random.RandomState(seed)
    for v in views:
        os.makedirs(os.path.join(d, seq, v), exist_ok=True)
        for f in range(n):
            if v == 'side' and skip == f:
                continue
            a = (rng.rand(32, 32, 3) * 180 + 40).astype('uint8')
            Image.fromarray(a).save(os.path.join(d, seq, v, f'f{f:03d}.png'))
    with open(os.path.join(d, seq, 'frames.jsonl'), 'w') as fh:
        for f in range(n):
            fh.write(json.dumps({'f': f, 'combat': {'anim': ['AS_RunStart 1.0 0.1']}}) + '\n')


# ---------------------------------------------------------------- 5. montage

class FrameList(unittest.TestCase):
    def test_hit_and_faf_windows(self):
        self.assertEqual(montage.frame_list(SPEC), [7, 8, 9, 15, 16, 17])

    def test_overlapping_windows_are_not_duplicated(self):
        s = {'ab': {'hit': 10, 'faf': 11, 'around': 2}}
        self.assertEqual(montage.frame_list(s), [8, 9, 10, 11, 12, 13])

    def test_explicit_frames_win(self):
        self.assertEqual(montage.frame_list(SPEC, explicit=['3', '1', '3']), [1, 3])

    def test_window_fallback_without_hit_or_faf(self):
        self.assertEqual(montage.frame_list({'expect': {'window': [10, 20]}}), [10, 15, 20])

    def test_negative_frames_dropped(self):
        self.assertEqual(montage.frame_list({'ab': {'hit': 1, 'around': 3}}), [0, 1, 2, 3, 4])

    def test_video_range_prefers_spec(self):
        self.assertEqual(montage.video_range(SPEC, {}), (4, 12))
        self.assertEqual(montage.video_range({'expect': {'window': [2, 9]}}, {}), (2, 9))


class Montage(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.a, self.b = os.path.join(self.d, 'base'), os.path.join(self.d, 'var')
        make_capture(self.a, seed=1)
        make_capture(self.b, seed=2)

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def test_one_grid_per_view_same_frames(self):
        ok, lines, rep = montage.run(self.a, self.b, 's', SPEC, cell=32)
        self.assertTrue(ok, lines)
        self.assertEqual(sorted(rep['grids']), sorted(VIEWS))
        self.assertEqual(rep['frames'], [7, 8, 9, 15, 16, 17])
        for v, p in rep['grids'].items():
            self.assertTrue(os.path.exists(p), p)
            with Image.open(p) as im:
                self.assertEqual(im.size, (64, 6 * 32))             # 2 columns (base | variant), one row per frame
        # every view's video covers the SAME range, and that range is the spec's
        self.assertEqual(rep['video_range'], [4, 12])
        self.assertEqual(sorted(rep['videos']), sorted(VIEWS))

    def test_missing_frame_is_a_loud_fail(self):
        shutil.rmtree(self.b)
        make_capture(self.b, seed=2, skip=8)       # the variant lacks the hit frame
        ok, lines, _ = montage.run(self.a, self.b, 's', SPEC, cell=32)
        self.assertFalse(ok)
        self.assertIn('MONTAGE: FAIL', lines[0])
        self.assertIn('f8', lines[0])
        self.assertIn('var/side', lines[0])

    def test_no_shared_view_fails(self):
        shutil.rmtree(self.b)
        make_capture(self.b, seed=2, views=['other'])
        ok, lines, _ = montage.run(self.a, self.b, 's', SPEC, cell=32)
        self.assertFalse(ok)
        self.assertIn('no view in common', lines[0])

    def test_missing_capture_dir_fails(self):
        ok, lines, _ = montage.run(os.path.join(self.d, 'nope'), self.b, 's', SPEC)
        self.assertFalse(ok)
        self.assertIn('does not exist', lines[0])

    def test_no_frames_to_show_fails(self):
        ok, lines, _ = montage.run(self.a, self.b, 's', {}, cell=32)
        self.assertFalse(ok)
        self.assertIn('no frames to show', lines[0])

    def test_views_filter_and_no_video(self):
        ok, lines, rep = montage.run(self.a, self.b, 's', SPEC, views=['side'], cell=32, video=False)
        self.assertTrue(ok, lines)
        self.assertEqual(list(rep['grids']), ['side'])
        self.assertEqual(rep['videos'], {})

    def test_webp_fallback_without_ffmpeg(self):
        real = montage.post.find_ffmpeg
        montage.post.find_ffmpeg = lambda: None
        try:
            ok, lines, rep = montage.run(self.a, self.b, 's', SPEC, cell=32)
        finally:
            montage.post.find_ffmpeg = real
        self.assertTrue(ok, lines)
        self.assertTrue(all(p.endswith('.webp') for p in rep['videos'].values()), rep['videos'])
        self.assertIn('no ffmpeg', lines[1])


# ---------------------------------------------------------------- 6. hash cache

class Cache(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.root = cache.ROOT
        cache.ROOT = self.d
        os.makedirs(os.path.join(self.d, 'poses'))
        self.pose = os.path.join(self.d, 'poses', 'p.casc')
        with open(self.pose, 'w') as fh:
            fh.write('pose A')
        self.spec = {'name': 'x', 'seed': 1, 'expect': {'clip': 'c', 'window': [0, 9]},
                     'cache': {'inputs': ['poses/*.casc'], 'build_inputs': []}}
        self.out = os.path.join(self.d, 'out')
        os.makedirs(os.path.join(self.out, 's', 'side'))
        with open(os.path.join(self.out, 's', 'post.json'), 'w') as fh:
            fh.write('{"ok": true}')
        with open(os.path.join(self.out, 's', 'side', 'f000.png'), 'w') as fh:
            fh.write('png')

    def tearDown(self):
        cache.ROOT = self.root
        shutil.rmtree(self.d, ignore_errors=True)

    def test_miss_then_save_then_hit(self):
        self.assertFalse(cache.check(self.spec)[0])
        cache.save(self.spec, self.out)
        hit, m, _ = cache.check(self.spec)
        self.assertTrue(hit)
        self.assertEqual(m['missing'], [])

    def test_changed_pose_invalidates(self):
        cache.save(self.spec, self.out)
        k1 = cache.manifest(self.spec)['key']
        with open(self.pose, 'w') as fh:
            fh.write('pose B')
        self.assertNotEqual(cache.manifest(self.spec)['key'], k1)
        self.assertFalse(cache.check(self.spec)[0])

    def test_touch_alone_does_not_invalidate(self):
        k1 = cache.manifest(self.spec)['key']
        os.utime(self.pose, (0, 0))      # a checkout rewrites mtimes; the content did not change
        self.assertEqual(cache.manifest(self.spec)['key'], k1)

    def test_spec_change_invalidates_but_volatile_keys_do_not(self):
        k = cache.manifest(self.spec)['key']
        self.assertEqual(cache.manifest(dict(self.spec, monitor=3, max_minutes=30, name='other'))['key'], k)
        self.assertNotEqual(cache.manifest(dict(self.spec, seed=2))['key'], k)
        self.assertNotEqual(cache.manifest(dict(self.spec, cvars='a 1'))['key'], k)
        win = {'clip': 'c', 'window': [0, 10]}
        self.assertNotEqual(cache.manifest(dict(self.spec, expect=win))['key'], k)

    def test_missing_input_is_recorded_and_never_a_silent_hit(self):
        spec = dict(self.spec, cache={'inputs': ['poses/absent.casc'], 'build_inputs': []})
        m = cache.manifest(spec)
        self.assertEqual(m['missing'], ['poses/absent.casc'])
        self.assertFalse(cache.check(spec)[0])

    def test_reuse_copies_outputs_back(self):
        cache.save(self.spec, self.out)
        dst = os.path.join(self.d, 'again')
        ok, _, lines = cache.reuse(self.spec, dst)
        self.assertTrue(ok, lines)
        self.assertIn('CACHE: HIT', lines[0])
        self.assertTrue(os.path.exists(os.path.join(dst, 's', 'post.json')))
        # frame PNGs are not stored by default (they are the bulk and no verdict needs them)
        self.assertFalse(os.path.exists(os.path.join(dst, 's', 'side', 'f000.png')))
        self.assertIn('frame PNGs not stored', lines[0])

    def test_with_frames_stores_the_pngs(self):
        cache.save(self.spec, self.out, with_frames=True)
        dst = os.path.join(self.d, 'again2')
        self.assertTrue(cache.reuse(self.spec, dst)[0])
        self.assertTrue(os.path.exists(os.path.join(dst, 's', 'side', 'f000.png')))

    def test_reuse_without_entry_is_a_miss(self):
        ok, _, lines = cache.reuse(self.spec, os.path.join(self.d, 'x'))
        self.assertFalse(ok)
        self.assertIn('CACHE: MISS', lines[0])

    def test_build_kind_ignores_the_spec(self):
        src = os.path.join(self.d, 'Source')
        os.makedirs(src)
        with open(os.path.join(src, 'a.cpp'), 'w') as fh:
            fh.write('int main(){}')
        spec = dict(self.spec, cache={'inputs': ['poses/*.casc'], 'build_inputs': ['Source/**/*.cpp']})
        k = cache.manifest(spec, 'build')['key']
        self.assertEqual(cache.manifest(dict(spec, seed=99), 'build')['key'], k)   # the spec cannot change a build
        with open(os.path.join(src, 'a.cpp'), 'w') as fh:
            fh.write('int main(){return 1;}')
        self.assertNotEqual(cache.manifest(spec, 'build')['key'], k)

    def test_build_inputs_are_part_of_the_capture_key(self):
        src = os.path.join(self.d, 'Source')
        os.makedirs(src)
        with open(os.path.join(src, 'a.cpp'), 'w') as fh:
            fh.write('v1')
        spec = dict(self.spec, cache={'inputs': ['poses/*.casc'], 'build_inputs': ['Source/**/*.cpp']})
        k = cache.manifest(spec)['key']
        with open(os.path.join(src, 'a.cpp'), 'w') as fh:
            fh.write('v2')
        self.assertNotEqual(cache.manifest(spec)['key'], k)   # new binaries can move a pose


# ---------------------------------------------------------------- 7. approved-pose regression

TABLE = ('| mossa | versione | asset | nota | tag |\n|---|---|---|---|---|\n'
         '| run start | w9 | AS_W9 (Saved/w9.casc), rows Variant 20 / 30 (Handoff) | owner ok | approved/runstart-w9 |\n')


class Rows(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.t = os.path.join(self.d, 't.md')
        with open(self.t, 'w') as fh:
            fh.write(TABLE)

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def test_parses_name_path_and_tag_past_a_second_paren(self):
        r = regress.rows(self.t)
        self.assertEqual(len(r), 1)
        self.assertEqual(r[0]['asset'], 'AS_W9')
        self.assertEqual(r[0]['source'], 'Saved/w9.casc')     # the FIRST group, not "Handoff"
        self.assertEqual(r[0]['tag'], 'approved/runstart-w9')

    def test_header_and_separator_are_skipped(self):
        self.assertEqual([x['version'] for x in regress.rows(self.t)], ['w9'])

    def test_missing_table_is_no_rows(self):
        self.assertEqual(regress.rows(os.path.join(self.d, 'absent.md')), [])


def _git(d, *a):
    subprocess.run(['git'] + list(a), cwd=d, capture_output=True, text=True, check=False)


class Regress(unittest.TestCase):
    """A throwaway repo: an asset committed and tagged, then changed. No network, no project state."""

    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.root = regress.ROOT
        regress.ROOT = self.d
        self.cache_root = cache.ROOT
        cache.ROOT = self.d
        os.makedirs(os.path.join(self.d, 'Content'))
        os.makedirs(os.path.join(self.d, 'docs', 'notes'))
        self.asset = os.path.join(self.d, 'Content', 'AS_W9.uasset')
        with open(self.asset, 'w') as fh:
            fh.write('approved pose')
        self.table = os.path.join(self.d, regress.TABLE)
        with open(self.table, 'w') as fh:
            fh.write(TABLE)
        _git(self.d, 'init', '-q')
        _git(self.d, 'config', 'user.email', 't@t'); _git(self.d, 'config', 'user.name', 't')
        _git(self.d, 'add', '-A'); _git(self.d, 'commit', '-qm', 'approve')
        _git(self.d, 'tag', 'approved/runstart-w9')

    def tearDown(self):
        regress.ROOT = self.root
        cache.ROOT = self.cache_root
        shutil.rmtree(self.d, ignore_errors=True)

    def _loose(self, text='casc A'):
        with open(os.path.join(self.d, 'Saved', 'w9.casc'), 'w') as fh:
            fh.write(text)

    def test_unchanged_asset_plus_lock_is_ok(self):
        os.makedirs(os.path.join(self.d, 'Saved'))
        self._loose()
        self.assertTrue(regress.record('approved/runstart-w9')[0])
        ok, results, extra = regress.check()
        self.assertTrue(ok, [r['why'] for r in results] + [extra])
        self.assertIn('REGRESS: OK', regress.lines(ok, results, extra)[0])

    def test_changed_asset_fails_loudly(self):
        os.makedirs(os.path.join(self.d, 'Saved'))
        self._loose()
        regress.record('approved/runstart-w9')
        with open(self.asset, 'w') as fh:
            fh.write('someone re-imported this')
        ok, results, extra = regress.check()
        self.assertFalse(ok)
        self.assertIn('CHANGED since the tag', results[0]['why'][0])
        self.assertIn('REGRESS: FAIL', regress.lines(ok, results, extra)[0])

    def test_changed_loose_file_fails_against_the_lock(self):
        os.makedirs(os.path.join(self.d, 'Saved'))
        self._loose()
        regress.record('approved/runstart-w9')
        self._loose('casc B')
        ok, results, _ = regress.check()
        self.assertFalse(ok)
        self.assertIn('CHANGED since the lock', ' '.join(results[0]['why']))

    def test_loose_file_without_lock_is_a_fail_that_says_how_to_fix_it(self):
        os.makedirs(os.path.join(self.d, 'Saved'))
        self._loose()
        ok, results, _ = regress.check()
        self.assertFalse(ok)
        self.assertIn('has no lock', ' '.join(results[0]['why']))
        self.assertIn('--record approved/runstart-w9', ' '.join(results[0]['why']))

    def test_deleted_asset_fails(self):
        os.makedirs(os.path.join(self.d, 'Saved'))
        self._loose()
        regress.record('approved/runstart-w9')
        os.remove(self.asset)
        ok, results, _ = regress.check()
        self.assertFalse(ok)
        self.assertIn('gone from the worktree', ' '.join(results[0]['why']))

    def test_missing_tag_fails(self):
        _git(self.d, 'tag', '-d', 'approved/runstart-w9')
        ok, results, _ = regress.check()
        self.assertFalse(ok)
        self.assertIn('tag missing here', ' '.join(results[0]['why']))

    def test_row_without_tag_fails(self):
        with open(self.table, 'w') as fh:
            fh.write('| mossa | versione | asset | nota | tag |\n|---|---|---|---|---|\n| run start | w9 | AS_W9 | ok |  |\n')
        ok, results, _ = regress.check()
        self.assertFalse(ok)
        self.assertIn('names no approved/* tag', results[0]['why'][0])

    def test_tag_without_a_row_is_a_fail(self):
        os.makedirs(os.path.join(self.d, 'Saved'))
        self._loose()
        regress.record('approved/runstart-w9')
        _git(self.d, 'tag', 'approved/runstart-w10')
        ok, results, extra = regress.check()
        self.assertFalse(ok)
        self.assertEqual(extra, ['approved/runstart-w10'])
        self.assertIn('no row in', ' '.join(regress.lines(ok, results, extra)))

    def test_empty_table_is_a_fail_not_a_pass(self):
        with open(self.table, 'w') as fh:
            fh.write('| mossa | versione | asset | nota | tag |\n|---|---|---|---|---|\n')
        ok, results, extra = regress.check()
        self.assertFalse(ok)
        self.assertIn('no approved rows', regress.lines(ok, results, extra)[0])

    def test_record_refuses_a_file_that_is_not_there(self):
        ok, lines = regress.record('approved/runstart-w9')
        self.assertFalse(ok)
        self.assertIn('not on this machine', lines[0])

    def test_record_refuses_an_unknown_tag(self):
        ok, lines = regress.record('approved/nope')
        self.assertFalse(ok)
        self.assertIn('no row with tag', lines[0])


class JointsTolerance(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.d, ignore_errors=True)

    def _w(self, name, shift=0.0, n=5):
        p = os.path.join(self.d, name)
        with open(p, 'w') as fh:
            json.dump({'fps': 30, 'joints': {'hand_l': [[i + shift, 0.0, 0.0] for i in range(n)]}}, fh)
        return p

    def test_identical_is_zero(self):
        worst, _ = regress.joints_delta(self._w('a.json'), self._w('b.json'))
        self.assertEqual(worst, 0.0)

    def test_small_shift_is_under_tolerance(self):
        worst, _ = regress.joints_delta(self._w('a.json'), self._w('b.json', shift=0.05))
        self.assertLess(worst, regress.TOL_CM)

    def test_real_move_is_over_tolerance_and_says_where(self):
        worst, where = regress.joints_delta(self._w('a.json'), self._w('b.json', shift=0.9))
        self.assertGreater(worst, regress.TOL_CM)
        self.assertTrue(where.startswith('hand_l f'))

    def test_different_frame_count_is_not_comparable(self):
        worst, why = regress.joints_delta(self._w('a.json'), self._w('b.json', n=7))
        self.assertIsNone(worst)
        self.assertIn('frames at approval', why)

    def test_no_shared_joint(self):
        p = os.path.join(self.d, 'c.json')
        with open(p, 'w') as fh:
            json.dump({'fps': 30, 'joints': {'foot_r': [[0, 0, 0]]}}, fh)
        worst, why = regress.joints_delta(self._w('a.json'), p)
        self.assertIsNone(worst)
        self.assertIn('share no joint', why)


if __name__ == '__main__':
    unittest.main(verbosity=1)
