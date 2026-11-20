"""vfx_spec_check.py on synthetic effects (no editor, no real textures): a tiny setup module with LOOKS / LIT_LOOKS and a generated
flipbook whose blob grows with the frame. Needs numpy + Pillow. Run: python Tools/VFX/tests/test_vfx_spec_check.py (or pytest)"""
import json, os, shutil, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import vfx_spec_check as V  # noqa: E402

SETUP = '''
import unreal  # never imported by the check: the file is parsed
LOOKS = {
    "SelfLitDust": dict(tex="T_Grow", StartScale=0.8, GrowEnd=1.0, ShrinkStart=1.0, EndScale=1.0, FadeStart=0.5),
    "Shrinker": dict(tex="T_Grow", StartScale=1.0, GrowEnd=0.1, ShrinkStart=0.5, EndScale=0.2, FadeStart=0.5),
}
LIT_LOOKS = {
    "LitDust": dict(LOOKS["SelfLitDust"], Brightness=0.9),
    "LitNoFade": dict(LOOKS["SelfLitDust"], FadeStart=1.0),
    "LitShrinker": dict(LOOKS["Shrinker"]),
}
'''
FB = {'frames': 4, 'grid': 4, 'cell': 32, 'variants': 1}


def _workspace():
    import numpy as np
    from PIL import Image
    d = tempfile.mkdtemp()
    open(os.path.join(d, 'setup.py'), 'w').write(SETUP)
    img = np.zeros((128, 128, 4), dtype=np.uint8)
    yy, xx = np.mgrid[0:32, 0:32]
    for f in range(4):   # frame f: a disc of radius 6 + 2f px in cell (row 0, column f): the silhouette grows
        disc = ((yy - 16) ** 2 + (xx - 16) ** 2) <= (6 + 2 * f) ** 2
        img[0:32, f * 32:(f + 1) * 32, 3] = disc * 255
    Image.fromarray(img, 'RGBA').save(os.path.join(d, 'T_Grow.png'))
    return d


def _spec(d, layers, **kw):
    s = dict(effect='t', kind='dust', setup=os.path.join(d, 'setup.py'), textures=d, flipbook=FB, layers=layers)
    s.update(kw)
    p = os.path.join(d, 'spec_%d.json' % len(os.listdir(d)))
    json.dump(s, open(p, 'w'))
    return p


def _fails(d, layers, **kw):
    return V.check_spec(_spec(d, layers, **kw))[1]


def test_rules():
    d = _workspace()
    try:
        assert V.load_presets(os.path.join(d, 'setup.py'))['LitDust'][1] == 'lit'
        assert _fails(d, [{'name': 'a', 'preset': 'LitDust'}, {'name': 'b', 'preset': 'LitDust'}]) == []
        f = _fails(d, [{'name': 'a', 'preset': 'LitDust'}])
        assert len(f) == 1 and 'layer' in f[0], f
        f = _fails(d, [{'name': 'a', 'preset': 'SelfLitDust'}, {'name': 'b', 'preset': 'LitDust'}])
        assert len(f) == 1 and 'a: material unlit' in f[0], f
        f = _fails(d, [{'name': 'a', 'preset': 'LitShrinker'}, {'name': 'b', 'preset': 'LitDust'}])
        assert len(f) == 1 and 'shrinks' in f[0], f
        f = _fails(d, [{'name': 'a', 'preset': 'LitNoFade'}, {'name': 'b', 'preset': 'LitDust'}])
        assert len(f) == 1 and 'alpha' in f[0], f
        # solids may shrink (owner A2 r6), still lit and fading
        assert _fails(d, [{'name': 'a', 'preset': 'LitShrinker', 'kind': 'solid'}, {'name': 'b', 'preset': 'LitDust'}]) == []
        # explicit curves: a flash may be self-lit, must fade to 0
        flash = {'name': 'fl', 'kind': 'flash', 'material': {'shading': 'unlit'}, 'size': [[0, 0.2], [1, 1.0]], 'alpha': [[0, 1], [1, 0]]}
        assert _fails(d, [flash, {'name': 'b', 'preset': 'LitDust'}]) == []
        stuck = dict(flash, alpha=[[0, 1], [1, 0.5]])
        assert any('alpha 0.50' in x for x in _fails(d, [stuck, {'name': 'b', 'preset': 'LitDust'}]))
        shrink_dust = {'name': 'sd', 'material': {'shading': 'lit'}, 'size': [[0, 1], [0.5, 1.2], [1, 0.8]], 'alpha': [[0, 1], [1, 0]]}
        assert any('shrinks' in x for x in _fails(d, [shrink_dust, {'name': 'b', 'preset': 'LitDust'}]))
        assert any('not in' in x for x in _fails(d, [{'name': 'x', 'preset': 'Nope'}, {'name': 'b', 'preset': 'LitDust'}]))
        # CLI: exit code and first line
        good = _spec(d, [{'name': 'a', 'preset': 'LitDust'}, {'name': 'b', 'preset': 'LitDust'}])
        assert V.main([good, '--out', os.path.join(d, 'r.txt')]) == 0
        bad = _spec(d, [{'name': 'a', 'preset': 'Shrinker'}])
        assert V.main([bad, '--out', os.path.join(d, 'r.txt')]) == 1
        assert V.main([os.path.join(d, 'missing.json')]) == 2
    finally:
        shutil.rmtree(d)


def test_repo_specs_parse():
    """Every committed spec is valid JSON with layers (their presets are checked on the desktop, where the setup file lives)."""
    for p in sorted(os.listdir(os.path.join(os.path.dirname(HERE), 'specs'))):
        s = json.load(open(os.path.join(os.path.dirname(HERE), 'specs', p)))
        assert s.get('layers'), p


if __name__ == '__main__':
    fails = 0
    for name, fn in sorted((k, v) for k, v in dict(globals()).items() if k.startswith('test_')):
        try:
            fn(); print(f'PASS {name}')
        except AssertionError as e:
            fails += 1; print(f'FAIL {name}: {e}')
    print(f'ESITO: {"OK" if not fails else "FAIL"} ({fails} failed)')
    sys.exit(1 if fails else 0)
