"""capture_assert.py (project rule 10: a review starts only on ASSERT: OK) on synthetic frames.jsonl rows, plus one real capture.
Offline. Run: python tests/test_capture_assert.py   (or pytest tests/test_capture_assert.py)"""
import json, os, shutil, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import capture_assert as A  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
REAL = os.path.join(ROOT, 'docs', 'notes', 'reviews', 'jab-fk-3', 'variant', 'jab1_frames.jsonl')


def row(f, anim=None, montage=None):
    r = {'f': f, 'combat': {'history': ''}, 'montage': montage}
    if anim is not None:
        r['combat']['anim'] = anim
    return r


def write(rows, d, seq='seq'):
    os.makedirs(os.path.join(d, seq), exist_ok=True)
    p = os.path.join(d, seq, 'frames.jsonl')
    with open(p, 'w') as f:
        for r in rows:
            f.write(json.dumps(r) + '\n')
    return p


def run_stop_rows():
    """f0-19 run loop, f20-29 blend run -> stop, f30-79 stop owns, f80-99 idle."""
    rows = []
    for f in range(100):
        if f < 20:
            a = ['AS_Run_Loop 1.00 0.5']
        elif f < 30:
            w = (f - 19) / 10.0
            a = [f'AS_GSTN_RunStop_L {w:.2f} 0.1', f'AS_Run_Loop {1 - w:.2f} 0.6']
        elif f < 80:
            a = ['AS_GSTN_RunStop_L 1.00 0.9']
        else:
            a = ['AS_Idle 1.00 0.1']
        rows.append(row(f, a))
    return {r['f']: r for r in rows}


def test_ok_and_blend_in_left_out():
    rows = run_stop_rows()
    r = A.check_rows(rows, 'AS_GSTN_RunStop_L', (30, 79))
    assert r['ok'] and r['coverage'] == 1.0, r
    r = A.check_rows(rows, 'AS_GSTN_RunStop_L', (25, 79))   # the blend-in's tail: 2/55 frames below 0.8 = 96 %
    assert r['ok'], r
    r = A.check_rows(rows, 'AS_GSTN_RunStop_L', (20, 79))   # the whole blend-in: 7/60 below = 88 % < 90 %: leave it out
    assert not r['ok'], r
    r = A.check_rows(rows, 'AS_GSTN_RunStop_L', (15, 79))   # starts in the run loop: 12/65 below -> 81 % < 90 %
    assert not r['ok'] and r['worst_owner'][0] == 'AS_Run_Loop', r


def test_wrong_clip_fails_with_owner():
    rows = run_stop_rows()
    r = A.check_rows(rows, 'AS_GSTN_RunStop_R', (30, 79))
    assert not r['ok'] and r['worst_owner'] == ['AS_GSTN_RunStop_L', 1.0], r
    assert A.line(r).startswith('ASSERT: FAIL') and 'owned by AS_GSTN_RunStop_L' in A.line(r)


def test_patterns_and_sum():
    rows = run_stop_rows()
    assert A.check_rows(rows, 'AS_GSTN_RunStop_R|AS_GSTN_RunStop_L', (30, 79))['ok']
    assert A.check_rows(rows, 'as_gstn_runstop_*', (30, 79))['ok']
    # the same clip twice in a blend stack (blending into itself): weights add up
    split = {f: row(f, ['AS_Stop 0.45 0.1', 'AS_Stop 0.45 0.3', 'AS_Idle 0.10 0']) for f in range(10)}
    assert A.check_rows(split, 'AS_Stop', (0, 9))['ok']


def test_one_frame_dip_tolerated_two_not_for_short_windows():
    rows = {f: row(f, ['AM_Jab 1.00 0.1']) for f in range(20)}
    rows[5] = row(5, ['AM_Jab 0.50 0.1', 'AS_Idle 0.50 0'])
    assert A.check_rows(rows, 'AM_Jab', (0, 19))['ok']           # 19/20 = 95 %
    rows[6] = row(6, ['AS_Idle 1.00 0'])
    rows[7] = row(7, ['AS_Idle 1.00 0'])
    assert not A.check_rows(rows, 'AM_Jab', (0, 19))['ok']       # 17/20 = 85 %
    assert A.check_rows(rows, 'AM_Jab', (0, 19), coverage=0.85)['ok']


def test_missing_frames_and_no_data():
    rows = {f: row(f, ['AM_Jab 1.00 0']) for f in range(10)}
    r = A.check_rows(rows, 'AM_Jab', (0, 14))
    assert not r['ok'] and 'missing' in r['reason'], r
    old = {f: row(f) for f in range(10)}
    r = A.check_rows(old, 'AM_Jab', (0, 9))
    assert not r['ok'] and r.get('chain') and 'catena di cattura' in A.line(r), r
    r = A.check_rows(rows, 'AM_Jab', (5, 2))
    assert not r['ok'] and 'empty' in r['reason']


def test_montages():
    # new C++ rows: the montage is an entry of combat.anim with its blend weight
    rows = {f: row(f, ['AS_Idle 0.25 0', f'AM_Jab {0.75 if f < 3 else 1.0:.2f} 0.1'], ['AM_Jab', f * 1.0]) for f in range(20)}
    assert A.check_rows(rows, 'AM_Jab', (3, 19))['ok']
    # old rows: montage name only; refused unless --montage-implicit (then weight 1.0)
    old = {f: row(f, None, ['AM_Jab', f * 1.0]) for f in range(20)}
    assert not A.check_rows(old, 'AM_Jab', (0, 19))['ok']
    assert A.check_rows(old, 'AM_Jab', (0, 19), montage_implicit=True)['ok']
    # a montage row with a weight (third element) counts without the flag
    w = {f: row(f, None, ['AM_Jab', f * 1.0, 0.9]) for f in range(20)}
    assert A.check_rows(w, 'AM_Jab', (0, 19))['ok']


def test_spec_and_cli():
    d = tempfile.mkdtemp()
    try:
        write(list(run_stop_rows().values()), d, 'mv_run_stop')
        spec = {'_doc': 'ignored', 'mv_run_stop': [{'clip': 'AS_GSTN_RunStop_L', 'window': [30, 79]}, {'clip': 'AS_Idle', 'window': [85, 99]}]}
        ok, res = A.check_spec(spec, d)
        assert ok and len(res) == 2
        spec['mv_skid'] = {'clip': 'AS_Skid', 'window': [0, 10]}   # sequence never captured
        ok, res = A.check_spec(spec, d)
        assert not ok and A.summary_lines(ok, res)[0] == 'ASSERT: FAIL 1/3'
        sp = os.path.join(d, 'asserts.json'); json.dump(spec, open(sp, 'w'))
        assert A.main(['--spec', sp, d]) == 1
        assert open(os.path.join(d, 'assert.txt')).readline().startswith('ASSERT: FAIL')
        fr = os.path.join(d, 'mv_run_stop', 'frames.jsonl')
        assert A.main([fr, 'AS_GSTN_RunStop_L', '30:79']) == 0
        assert json.load(open(os.path.join(d, 'mv_run_stop', 'assert.json')))['ok']
        assert A.main([fr, 'AS_GSTN_RunStop_L']) == 2
        assert A.check_spec({}, d)[0] is False                    # no expectation = no proof
    finally:
        shutil.rmtree(d)


def test_additive_layers_do_not_compete():
    # cached-pose logging: base clip max(1-a, a) while the additive lean alpha a ramps; idle blending out still competes
    rows = {f: row(f, [f'AS_V3 {max(1 - a, a):.2f} 0.1', f'BS_Relaxed_Walk_Leans {a:.2f} 0.1'])
            for f, a in zip(range(0, 11), [i / 10 for i in range(11)])}
    rows[11] = row(11, ['AS_V3 0.60 0.1', 'M_Idle 0.40 0.1', 'BS_Relaxed_Walk_Leans 0.30 0.1'])
    assert not A.check_rows(rows, 'AS_V3', (0, 10))['ok']
    r = A.check_rows(rows, 'AS_V3', (0, 10), additive=['BS_Relaxed_*Lean*'])
    assert r['ok'] and r['min_in_window'] == 1.0, r
    r = A.check_rows(rows, 'AS_V3', (11, 11), additive=['BS_Relaxed_*Lean*'])
    assert not r['ok'] and abs(r['frames'][11] - 0.6) < 1e-3, r


def test_real_capture():
    """jab-fk-3 variant capture (before montage weights were logged): the jab montage owns f30-f40 only with --montage-implicit."""
    if not os.path.exists(REAL):
        print('  skip: no real capture'); return
    rows = A.load_rows(REAL)
    assert not A.check_rows(rows, 'AM_OverlordJab1*', (30, 40))['ok']
    assert A.check_rows(rows, 'AM_OverlordJab1*', (30, 40), montage_implicit=True)['ok']
    assert not A.check_rows(rows, 'AM_OverlordDropkick*', (30, 40), montage_implicit=True)['ok']


if __name__ == '__main__':
    fails = 0
    for name, fn in sorted((k, v) for k, v in dict(globals()).items() if k.startswith('test_')):
        try:
            fn(); print(f'PASS {name}')
        except AssertionError as e:
            fails += 1; print(f'FAIL {name}: {e}')
    print(f'ESITO: {"OK" if not fails else "FAIL"} ({fails} failed)')
    sys.exit(1 if fails else 0)
