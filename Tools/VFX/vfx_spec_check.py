"""VFX spec check: a blocking gate BEFORE a VFX capture goes to review (audit_setup_agentico.md §2.4, 2026-11-20).

The dust series lost rounds to defects a script can see without a capture: particles that shrink, a layer that never fades to 0,
a self-lit material where the owner asked for scene light, one lonely layer. This checks an effect's SPEC (its layers and the
material presets they use) against the owner's VFX rules, offline, in seconds. It unifies Tools/GASNPPTest/vfx_size_monotonic.py
(same size model, now one of the checks).

    python Tools/VFX/vfx_spec_check.py <spec.json> [<spec.json> ...] [--out report.txt]
    python Tools/VFX/vfx_spec_check.py --all          (every Tools/VFX/specs/*.json)

Spec (one per effect, Tools/VFX/specs/<effect>.json; paths relative to the repo root):
  {"effect": "landing_dust", "kind": "dust", "min_layers": 2,
   "setup": "Tools/GAS/setup_presentation_vfx_puff.py",        where the material presets live (LOOKS = self-lit, LIT_LOOKS = scene-lit)
   "textures": "Tools/GAS/vfx",                                  flipbook PNGs (<tex>.png; alpha = coverage)
   "layers": [
     {"name": "skirt", "preset": "DustLit"},                     size, fade and material from the preset (the puff material model)
     {"name": "grit",  "preset": "GritLit", "kind": "solid"},    a layer can have its own kind
     {"name": "flash", "kind": "flash", "material": {"shading": "unlit"},
      "size": [[0, 0.2], [0.3, 1.0], [1, 1.4]], "alpha": [[0, 1], [0.5, 0.8], [1, 0]]}     explicit curves over life t = 0..1
   ]}

Rules by kind (owner rules, each with its scope; a layer's "rules" list overrides its kind's):
  dust   grow_only   the drawn size never decreases over life; it vanishes only through opacity (owner 2026-11-19, realistic dust)
         lit         scene-lit material: takes the direction and colour of the real light (A2 r6, DK clip study cause 1)
         end_alpha   alpha reaches 0 at the end of life (DK: dust only grows while it fades to 0)
  solid  lit, end_alpha                 (grit, debris: solids may shrink, owner A2 r6)
  flash  end_alpha                      (sparks, flashes, streaks: stylised, self-lit allowed)
Effect: at least min_layers layers (default 2: one central shockwave plus its supporting layer, never a lone sprite).

Size of a preset layer (as vfx_size_monotonic.py): the flipbook frame at life t (t * FlipRate over the frames, two blended) times
the quad scale lerp(StartScale, 1, easeOut(t / GrowEnd)) * lerp(1, EndScale, ramp(t, ShrinkStart, 1)); measured as the radius of
the silhouette (alpha > 0.06), per variant row: a drop > 0.5 % between two of 61 life steps = FAIL. End alpha: the brightest
pixel at t = 1 of flipbook alpha * fade(1) * OpacityMax, fade = 1 - ramp(t, FadeStart)^FadeCurve: > 0.01 = FAIL.
The visible extent (alpha * fade > 0.03) is reported up to 95 % of life, as information.
Niagara side (sprites sized only at spawn, no update-stage size module) is not visible here: the setup log checks it.

Prints at most 5 lines: VFX_SPEC: PASS / FAIL, then the first failures. Exit 0 PASS, 1 FAIL, 2 usage / missing input.
"""
import ast
import glob
import json
import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
KIND_RULES = {
    'dust': ('grow_only', 'lit', 'end_alpha'),
    'solid': ('lit', 'end_alpha'),
    'flash': ('end_alpha',),
}
LIT_SHADING = ('lit', 'default_lit', 'defaultlit', 'msm_default_lit', 'subsurface', 'two_sided_foliage', 'thin_translucent')
SIZE_TOL = 0.005        # 0.5 % per life step: pixel counting noise between two blended frames
END_ALPHA_MAX = 0.01
STEPS = 60
PRESET_DEFAULTS = dict(StartScale=1.0, GrowEnd=0.05, ShrinkStart=0.0, EndScale=0.0, FadeStart=1.0, FadeCurve=1.0, OpacityMax=1.0,
                       FlipRate=1.0, tex='T_GSTN_VFX_PuffFB')   # the puff material's own parameter defaults
FLIPBOOK = dict(frames=16, grid=8, cell=128, variants=4)


def _path(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


def load_presets(setup_path):
    """{name: (params, shading)} from the setup module's LOOKS (self-lit puff, 'unlit') and LIT_LOOKS (scene-lit, 'lit').
    The module imports unreal, so it is parsed, not imported; only its two dict literals are evaluated (dict(...) calls)."""
    tree = ast.parse(open(setup_path, encoding='utf-8').read())
    env = {'dict': dict}
    out = {}
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) \
                and node.targets[0].id in ('LOOKS', 'LIT_LOOKS'):
            val = eval(compile(ast.Expression(node.value), setup_path, 'eval'), {'__builtins__': {}}, env)  # noqa: S307 our source
            env[node.targets[0].id] = val
            shading = 'lit' if node.targets[0].id == 'LIT_LOOKS' else 'unlit'
            for k, v in val.items():
                out[k] = (dict(v), shading)
    return out


def _ramp(t, a):
    return 0.0 if a >= 1 else min(max((t - a) / (1 - a), 0.0), 1.0)


def _frames(tex_path, fb):
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(tex_path).convert('RGBA'), dtype=np.float32)[..., 3] / 255.0
    g, c, n = fb['grid'], fb['cell'], fb['frames']
    rows = []
    for v in range(fb['variants']):
        cells = []
        for f in range(n):
            k = v * n + f
            cells.append(a[(k // g) * c:(k // g + 1) * c, (k % g) * c:(k % g + 1) * c])
        if all(x.size for x in cells):
            rows.append(cells)
    return rows


def preset_curves(p, tex_path, fb):
    """Per variant: [(t, geo_radius, vis_radius, max_alpha)] over life, the puff material model."""
    import numpy as np
    out = []
    n, c = fb['frames'], fb['cell']
    for frames in _frames(tex_path, fb):
        rows = []
        for s in range(STEPS + 1):
            t = s / float(STEPS)
            fpos = min(t * p['FlipRate'], 1.0) * (n - 1)
            f0, w = int(fpos), fpos - int(fpos)
            alpha = frames[f0] * (1 - w) + frames[min(f0 + 1, n - 1)] * w
            g = min(t / max(p['GrowEnd'], 1e-4), 1.0)
            scale = (p['StartScale'] + (1 - p['StartScale']) * (1 - (1 - g) ** 2)) * (1 + (p['EndScale'] - 1) * _ramp(t, p['ShrinkStart']))
            fade = 1 - _ramp(t, p['FadeStart']) ** p['FadeCurve']
            geo = math.sqrt((alpha > 0.06).sum() / math.pi) / c * scale
            vis = math.sqrt((alpha * fade * p['OpacityMax'] > 0.03).sum() / math.pi) / c * scale
            rows.append((t, geo, vis, float(np.max(alpha)) * fade * p['OpacityMax']))
        out.append(rows)
    return out


def _interp(curve, t):
    pts = sorted((float(a), float(b)) for a, b in curve)
    if t <= pts[0][0]:
        return pts[0][1]
    for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
        if t <= t1:
            return v0 + (v1 - v0) * (t - t0) / max(t1 - t0, 1e-9)
    return pts[-1][1]


def check_layer(layer, spec, presets, kind):
    """Returns (fails, infos) for one layer."""
    name = layer.get('name') or layer.get('preset') or '?'
    lk = layer.get('kind', kind)
    rules = layer.get('rules') or KIND_RULES.get(lk)
    fails, infos = [], []
    if rules is None:
        return [f'{name}: unknown kind "{lk}" (dust, solid, flash) and no "rules"'], infos
    shading, curves = None, None
    if layer.get('preset'):
        if layer['preset'] not in presets:
            return [f'{name}: preset {layer["preset"]} not in {spec.get("setup")}'], infos
        params, shading = presets[layer['preset']]
        p = dict(PRESET_DEFAULTS); p.update(params); p.update(layer.get('override', {}))
        tex = _path(os.path.join(spec.get('textures', 'Tools/GAS/vfx'), p['tex'] + '.png'))
        if not os.path.exists(tex):
            return [f'{name}: flipbook {tex} missing'], infos
        curves = preset_curves(p, tex, dict(FLIPBOOK, **spec.get('flipbook', {})))
    else:
        if 'size' not in layer or 'alpha' not in layer:
            return [f'{name}: no preset and no explicit size/alpha curves'], infos
        size = [_interp(layer['size'], s / float(STEPS)) for s in range(STEPS + 1)]
        alpha = [_interp(layer['alpha'], s / float(STEPS)) for s in range(STEPS + 1)]
        curves = [[(s / float(STEPS), size[s], size[s] if alpha[s] > 0.03 else 0.0, alpha[s]) for s in range(STEPS + 1)]]
    shading = (layer.get('material') or {}).get('shading', shading)
    if 'lit' in rules:
        if shading is None:
            fails.append(f'{name}: material shading unknown (add "material": {{"shading": ...}})')
        elif str(shading).lower() not in LIT_SHADING:
            fails.append(f'{name}: material {shading}, rule "lit" wants a scene-lit material ({lk})')
    worst_geo, worst_vis, worst_at, end_alpha = 0.0, 0.0, None, 0.0
    for rows in curves:
        for (t0, g0, v0, _), (t1, g1, v1, a1) in zip(rows, rows[1:]):
            drop = (g0 - g1) / max(g0, 1e-6)
            if drop > worst_geo:
                worst_geo, worst_at = drop, t1
            if t1 <= 0.95 and v0 > 0:
                worst_vis = max(worst_vis, (v0 - v1) / max(v0, 1e-6))
        end_alpha = max(end_alpha, rows[-1][3])
    if 'grow_only' in rules and worst_geo > SIZE_TOL:
        fails.append(f'{name}: shrinks {worst_geo * 100:.1f}% at life {worst_at:.2f} (rule grow_only: size never decreases)')
    if 'end_alpha' in rules and end_alpha > END_ALPHA_MAX:
        fails.append(f'{name}: alpha {end_alpha:.2f} at end of life (rule end_alpha: fades to 0)')
    infos.append(f'{name}: kind {lk}, shading {shading}, max size drop {worst_geo * 100:.1f}%, visible drop to 95% life '
                 f'{worst_vis * 100:.1f}% (info), end alpha {end_alpha:.3f}')
    return fails, infos


def check_spec(spec_path):
    spec = json.load(open(spec_path, encoding='utf-8'))
    name = spec.get('effect', os.path.splitext(os.path.basename(spec_path))[0])
    kind = spec.get('kind', 'dust')
    layers = spec.get('layers', [])
    fails, infos = [], []
    need = int(spec.get('min_layers', 2))
    if len(layers) < need:
        fails.append(f'{len(layers)} layer(s), at least {need} (one central shockwave + its support, never a lone sprite)')
    presets = {}
    if any(l.get('preset') for l in layers):
        setup = _path(spec.get('setup', 'Tools/GAS/setup_presentation_vfx_puff.py'))
        if not os.path.exists(setup):
            return name, [f'setup {setup} missing (presets cannot be read)'], infos
        presets = load_presets(setup)
    for layer in layers:
        f, i = check_layer(layer, spec, presets, kind)
        fails += f; infos += i
    return name, fails, infos


def main(argv):
    argv = list(argv)
    out = None
    if '--out' in argv:
        i = argv.index('--out'); out = argv[i + 1]; del argv[i:i + 2]
    if argv == ['--all']:
        argv = sorted(glob.glob(os.path.join(ROOT, 'Tools', 'VFX', 'specs', '*.json')))
    if not argv or not all(os.path.exists(a) for a in argv):
        print('usage: vfx_spec_check.py <spec.json> [...] | --all   (missing: %s)' % [a for a in argv if not os.path.exists(a)])
        return 2
    lines, report, ok = [], [], True
    for sp in argv:
        name, fails, infos = check_spec(sp)
        ok &= not fails
        report.append(f'== {name}: {"PASS" if not fails else "FAIL"} ({sp})')
        report += [f'FAIL {x}' for x in fails] + [f'info {x}' for x in infos]
        lines += [f'{name}: {x}' for x in fails]
    out = out or os.path.join(ROOT, 'Saved', 'VfxSpec', 'vfx_spec_check.txt')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w', encoding='utf-8').write('\n'.join(report) + '\n')
    print(f'VFX_SPEC: {"PASS" if ok else "FAIL"} ({len(argv)} effect(s))')
    for x in lines[:3]:
        print(x)
    print(f'report: {out}')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
