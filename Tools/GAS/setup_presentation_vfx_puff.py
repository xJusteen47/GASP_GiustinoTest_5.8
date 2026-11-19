"""Smoke redo A1 (docs/notes/reviews/vfx-v3/hd1/REVIEW-SMOKE-LOOK.md, option A, material candidate (b) "self-lit"): clustered stylised puffs.

- T_GSTN_VFX_PuffFB: 8x8 flipbook, 2 variants x 32 frames (Tools/GAS/vfx/make_vfx_puff_flipbook.py, run it first with a plain python).
  RG = normal in the frame (y up), B = crease occlusion, A = crisp lumpy density.
- M_GSTN_VFX_Puff: Unlit, Translucent, Depth Fade. Frame from the particle's relative time (x FlipRate), two samples blended; variant
  from Particle Random. Form shading: the texture normal against LightDir (frame space: x right, y up, z toward the camera; the sprites
  never rotate, so the light stays on top), wrapped, through a soft two-tone ramp: lit side = surface colour x LitColor, shaded side =
  surface colour x ShadeColor (cool), times the crease occlusion. No EmissiveLift, no RimShade: the brightness is the eye-adapted
  emissive itself, the edge is the texture's crisp silhouette. Surface colour = particle colour (User.SurfaceColor), leaning to SnowColor
  on dark floors (DarkSnow), as in M_GSTN_VFX_Smoke. Size over life inside the quad: pop from StartScale by GrowEnd, shrink to EndScale
  from ShrinkStart (the flipbook's lobes die one by one on top of that).
- M_GSTN_VFX_ContactDisc: Unlit translucent soft disc for a ground-facing sprite: the floor darkens under the dust for a moment.
Run in the open editor:  python -I Tools/GAS/run_in_editor.py setup_presentation_vfx_puff    (idempotent)
"""
import unreal

import setup_presentation_vfx_v3 as V3

ROOT, MATS, MEL, EAL = V3.ROOT, V3.MATS, V3.MEL, V3.EAL
_node, _link, _scalar, _vector, _op, _unary, _lerp, _ramp = V3._node, V3._link, V3._scalar, V3._vector, V3._op, V3._unary, V3._lerp, V3._ramp
U = unreal
FRAMES = 16
VARIANTS = 4


def _mask(mat, node, x, y, out="", **ch):
    m = _node(mat, U.MaterialExpressionComponentMask, x, y, **{k: ch.get(k, False) for k in ("r", "g", "b", "a")})
    _link(node, out, m, "")
    return m


def _const(mat, v, x, y):
    return _node(mat, U.MaterialExpressionConstant, x, y, r=float(v))


def _cell_uv(mat, uvq, index, row_off, x, y):
    """(uvq + (index mod 8, floor(index / 8) + row_off)) / 8"""
    col = _op(mat, U.MaterialExpressionFmod, index, _const(mat, 8, x, y + 60), x + 120, y)
    row = _op(mat, U.MaterialExpressionAdd,
              _unary(mat, U.MaterialExpressionFloor, _op(mat, U.MaterialExpressionDivide, index, _const(mat, 8, x, y + 160), x + 120, y + 120),
                     x + 240, y + 120), row_off, x + 360, y + 120)
    cell = _op(mat, U.MaterialExpressionAppendVector, col, row, x + 480, y)
    return _op(mat, U.MaterialExpressionDivide, _op(mat, U.MaterialExpressionAdd, uvq, cell, x + 600, y), _const(mat, 8, x + 600, y + 80),
               x + 720, y)


def _pivot_uv(mat, size, x, y):
    """The quad's uv scaled by 1/size around Pivot (0.5, 0.85 = the puff's bottom): a puff shrinks onto the floor, not into the air"""
    uv = _node(mat, U.MaterialExpressionTextureCoordinate, x, y)
    pivot = _mask(mat, _vector(mat, "Pivot", U.LinearColor(0.5, 0.85, 0, 0), x, y + 100), x + 150, y + 100, r=True, g=True)
    scaled = _op(mat, U.MaterialExpressionDivide, _op(mat, U.MaterialExpressionSubtract, uv, pivot, x + 300, y), size, x + 450, y)
    clamped = _node(mat, U.MaterialExpressionClamp, x + 750, y, min_default=0.01, max_default=0.99)
    _link(_op(mat, U.MaterialExpressionAdd, scaled, pivot, x + 600, y), "", clamped, "Input")
    return clamped


def build_puff(tex):
    mat = V3._material("M_GSTN_VFX_Puff", U.MaterialShadingModel.MSM_UNLIT, U.BlendMode.BLEND_TRANSLUCENT)
    t = _node(mat, U.MaterialExpressionParticleRelativeTime, -3200, 0)
    one_c = _const(mat, 1, -3200, 400)
    size = V3._life_size(mat, t, one_c, -3000, 100)
    uvq = _pivot_uv(mat, size, -1800, -300)
    # Frame index: saturate(t * FlipRate) * 31 -> i0, i1 = min(i0 + 1, 31), blend by frac
    ft = _op(mat, U.MaterialExpressionMultiply,
             _unary(mat, U.MaterialExpressionSaturate, _op(mat, U.MaterialExpressionMultiply, t, _scalar(mat, "FlipRate", 1.0, -2400, -600),
                                                         -2250, -650), -2120, -650), _const(mat, FRAMES - 1, -2120, -560), -2000, -650)
    i0 = _unary(mat, U.MaterialExpressionFloor, ft, -1880, -700)
    fr = _unary(mat, U.MaterialExpressionFrac, ft, -1880, -600)
    i1 = _op(mat, U.MaterialExpressionMin, _op(mat, U.MaterialExpressionAdd, i0, one_c, -1760, -700), _const(mat, FRAMES - 1, -1760, -620),
             -1640, -700)
    # Variant: rows 0-3 or 4-7
    rnd = _node(mat, U.MaterialExpressionParticleRandom, -2000, -850)
    row_off = _op(mat, U.MaterialExpressionMultiply,
                  _unary(mat, U.MaterialExpressionFloor, _op(mat, U.MaterialExpressionMultiply, rnd, _const(mat, VARIANTS - 0.001, -1880, -800), -1760, -850),
                         -1640, -850), _const(mat, 2, -1640, -780), -1520, -850)
    uv0 = _cell_uv(mat, uvq, i0, row_off, -1400, -700)
    uv1 = _cell_uv(mat, uvq, i1, row_off, -1400, -450)
    s0 = _node(mat, U.MaterialExpressionTextureSampleParameter2D, -500, -700, parameter_name="Tex", texture=tex,
               sampler_type=U.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    s1 = _node(mat, U.MaterialExpressionTextureSampleParameter2D, -500, -450, parameter_name="Tex", texture=tex,
               sampler_type=U.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    _link(uv0, "", s0, "UVs")
    _link(uv1, "", s1, "UVs")
    # A texture sample's default output is RGB only: the alpha is blended on its own
    tx = _lerp(mat, s0, s1, fr, -300, -600)
    tx_a = _node(mat, U.MaterialExpressionLinearInterpolate, -300, -350)
    _link(s0, "A", tx_a, "A")
    _link(s1, "A", tx_a, "B")
    _link(fr, "", tx_a, "Alpha")
    # Normal: xy = RG * 2 - 1, z = sqrt(1 - |xy|^2)
    nxy = _node(mat, U.MaterialExpressionSubtract, -50, -800, const_b=1.0)
    _link(_op(mat, U.MaterialExpressionMultiply, _mask(mat, tx, -180, -850, r=True, g=True), _const(mat, 2, -180, -760), -120, -820), "", nxy, "A")
    nz = _unary(mat, U.MaterialExpressionSquareRoot,
                _unary(mat, U.MaterialExpressionSaturate,
                       _op(mat, U.MaterialExpressionSubtract, one_c, _op(mat, U.MaterialExpressionDotProduct, nxy, nxy, 80, -700), 200, -700),
                       320, -700), 440, -700)
    n = _op(mat, U.MaterialExpressionAppendVector, nxy, nz, 560, -800)
    light = _unary(mat, U.MaterialExpressionNormalize,
                   _mask(mat, _vector(mat, "LightDir", U.LinearColor(-0.45, 0.7, 0.55, 0), 400, -950), 560, -950, r=True, g=True, b=True),
                   700, -950)
    ndl = _op(mat, U.MaterialExpressionDotProduct, n, light, 820, -850)
    wrap = _scalar(mat, "Wrap", 0.3, 820, -750)
    wrapped = _op(mat, U.MaterialExpressionDivide, _op(mat, U.MaterialExpressionAdd, ndl, wrap, 960, -850),
                  _op(mat, U.MaterialExpressionAdd, one_c, wrap, 960, -750), 1100, -850)
    ramp = _ramp(mat, wrapped, _scalar(mat, "RampLo", 0.3, 1100, -700), _scalar(mat, "RampHi", 0.72, 1100, -640), 1250, -850)
    smooth = _op(mat, U.MaterialExpressionMultiply, _op(mat, U.MaterialExpressionMultiply, ramp, ramp, 1650, -900),
                 _op(mat, U.MaterialExpressionSubtract, _const(mat, 3, 1650, -760),
                     _op(mat, U.MaterialExpressionMultiply, ramp, _const(mat, 2, 1650, -700), 1720, -720), 1800, -760), 1950, -850)
    # Surface colour, snow lean on dark floors (bright = saturate((G - 0.4) * 3.333), as M_GSTN_VFX_Smoke)
    pcol = _node(mat, U.MaterialExpressionParticleColor, 400, 300)
    over = _node(mat, U.MaterialExpressionSubtract, 600, 500, const_b=0.4)
    _link(_mask(mat, pcol, 480, 500, g=True), "", over, "A")
    bright = _unary(mat, U.MaterialExpressionSaturate, _op(mat, U.MaterialExpressionMultiply, over, _const(mat, 3.333, 700, 560), 760, 500),
                    880, 500)
    snow_k = _op(mat, U.MaterialExpressionMultiply, _scalar(mat, "DarkSnow", 0.5, 880, 600),
                 _unary(mat, U.MaterialExpressionOneMinus, bright, 1000, 560), 1120, 560)
    surf = _lerp(mat, _mask(mat, pcol, 1000, 300, r=True, g=True, b=True),
                 _mask(mat, _vector(mat, "SnowColor", U.LinearColor(0.85, 0.88, 0.95, 1), 1000, 400), 1120, 400, r=True, g=True, b=True),
                 snow_k, 1250, 350)
    lit = _op(mat, U.MaterialExpressionMultiply, surf,
              _mask(mat, _vector(mat, "LitColor", U.LinearColor(1, 1, 1, 1), 1250, 150), 1400, 150, r=True, g=True, b=True), 1550, 200)
    shade = _op(mat, U.MaterialExpressionMultiply, surf,
                _mask(mat, _vector(mat, "ShadeColor", U.LinearColor(0.58, 0.66, 0.86, 1), 1250, 450), 1400, 450, r=True, g=True, b=True),
                1550, 400)
    occ = _lerp(mat, one_c, _mask(mat, tx, 1550, 600, b=True), _scalar(mat, "OccStrength", 0.7, 1550, 680), 1700, 600)
    col = _op(mat, U.MaterialExpressionMultiply, _lerp(mat, shade, lit, smooth, 2100, 300), occ, 2250, 400)
    # A1 review: on the dark floor the lit side came out darker than the floor (auto exposure lifts it): a brighter emissive there
    gain = _lerp(mat, _scalar(mat, "DarkBrightness", 1.6, 2100, 600), _scalar(mat, "Brightness", 1.0, 2100, 550), bright, 2250, 550)
    emissive = V3._eye_inverse(mat, _op(mat, U.MaterialExpressionMultiply, col, gain, 2400, 450),
                               2550, 450)
    alpha = _op(mat, U.MaterialExpressionMultiply, tx_a, pcol, 1850, 820, b_out="A")
    # Owner 2026-11-19: real dust thins out as it spreads, so the puff fades toward alpha 0 while it grows (FadeStart < 1; 1 = off)
    fade = V3._fade(mat, t, one_c, "FadeStart", 1.0, 1700, 1100)
    alpha_f = _op(mat, U.MaterialExpressionMultiply, alpha, fade, 1850, 1050)
    alpha_m = _op(mat, U.MaterialExpressionMultiply, alpha_f, _scalar(mat, "OpacityMax", 1.0, 1850, 950), 2000, 850)
    depth = _node(mat, U.MaterialExpressionDepthFade, 2150, 850)
    if not any(MEL.connect_material_expressions(alpha_m, "", depth, nm) for nm in ("Opacity", "InOpacity")):
        raise RuntimeError("cannot connect the Depth Fade opacity")
    if not any(MEL.connect_material_expressions(_scalar(mat, "FadeDistance", 12.0, 2000, 1000), "", depth, nm)
               for nm in ("FadeDistance", "Fade Distance")):
        raise RuntimeError("cannot connect the Depth Fade distance")
    MEL.connect_material_property(emissive, "", U.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(depth, "", U.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def build_disc():
    mat = V3._material("M_GSTN_VFX_ContactDisc", U.MaterialShadingModel.MSM_UNLIT, U.BlendMode.BLEND_TRANSLUCENT)
    t = _node(mat, U.MaterialExpressionParticleRelativeTime, -1400, 300)
    one_c = _const(mat, 1, -1400, 500)
    size = V3._life_size(mat, t, one_c, -1300, 600)
    uvq = V3._scaled_uv(mat, size, -1200, 0)
    centred = _node(mat, U.MaterialExpressionSubtract, -500, 0, const_b=0.5)
    _link(uvq, "", centred, "A")
    r = _node(mat, U.MaterialExpressionMultiply, -350, 0, const_b=2.0)
    _link(_unary(mat, U.MaterialExpressionLength, centred, -420, 0), "", r, "A")
    edge = _ramp(mat, r, _scalar(mat, "Inner", 0.25, -350, 100), one_c, -200, 50)
    inside = _unary(mat, U.MaterialExpressionOneMinus, _op(mat, U.MaterialExpressionMultiply, edge, edge, 0, 50), 120, 50)
    fade = V3._fade(mat, t, one_c, "FadeStart", 0.35, -400, 400)
    pcol = _node(mat, U.MaterialExpressionParticleColor, -200, -300)
    alpha = _op(mat, U.MaterialExpressionMultiply, _op(mat, U.MaterialExpressionMultiply, inside, fade, 250, 100),
                _scalar(mat, "MaxOpacity", 0.22, 250, 200), 400, 150)
    alpha_p = _op(mat, U.MaterialExpressionMultiply, alpha, pcol, 550, 150, b_out="A")
    colour = _op(mat, U.MaterialExpressionMultiply, _mask(mat, pcol, 0, -300, r=True, g=True, b=True),
                 _mask(mat, _vector(mat, "DiscTint", U.LinearColor(0.32, 0.36, 0.5, 1), 0, -200), 150, -200, r=True, g=True, b=True), 300, -250)
    MEL.connect_material_property(V3._eye_inverse(mat, colour, 450, -250), "", U.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(alpha_p, "", U.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


LOOKS = {
    # Landing skirt / wisps: pop to full in 15 % of life, hold, shrink to 35 % from 55 % (the flipbook's lobes die on top of that)
    "PuffDust": dict(DarkSnow=0.85, Wrap=0.15, RampLo=0.18, RampHi=0.5, OccStrength=0.5, ShadeColor=(0.5, 0.58, 0.8), FlipRate=1.0, StartScale=0.55, GrowEnd=0.15, ShrinkStart=0.30, EndScale=0.15),
    # Snow clumps: little lumpy solids, the flipbook barely moves (they stay round), shrink at the end
    # A1b (owner on the A1 video: "too stylised, DK is realistic"): the same material on the soft billowy smoke flipbook
    # (make_vfx_smoke_flipbook.py): softer light wrap and ramp, grows and thins out instead of shrinking
    "SmokeReal": dict(tex="T_GSTN_VFX_SmokeFB", DarkSnow=0.85, Wrap=0.4, RampLo=0.1, RampHi=0.8, ShadeColor=(0.55, 0.62, 0.8),
                      OccStrength=0.6, FlipRate=1.0, StartScale=0.45, GrowEnd=1.0, FadeStart=0.15, ShrinkStart=1.0, EndScale=1.0, FadeDistance=20.0),
    "PuffClump": dict(DarkSnow=0.85, FlipRate=0.3, StartScale=0.8, GrowEnd=0.08, ShrinkStart=0.6, EndScale=0.0, OccStrength=0.5),
}


def run():
    tex = V3.import_texture("T_GSTN_VFX_PuffFB")
    tex.set_editor_property("compression_settings", U.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)  # RG are normals: no BC3 blocks
    EAL.save_loaded_asset(tex)
    textures = {"T_GSTN_VFX_PuffFB": tex}
    for name in ("T_GSTN_VFX_SmokeFB",):
        textures[name] = V3.import_texture(name)
        textures[name].set_editor_property("compression_settings", U.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
        EAL.save_loaded_asset(textures[name])
    puff = build_puff(tex)
    disc = build_disc()
    made = []
    for name, look in LOOKS.items():
        mi = V3._instance(name, puff)
        for k, v in look.items():
            if k == "tex":
                MEL.set_material_instance_texture_parameter_value(mi, "Tex", textures[v])
            elif isinstance(v, tuple):
                MEL.set_material_instance_vector_parameter_value(mi, k, U.LinearColor(*v, 1.0))
            else:
                MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        made.append(name)
    mi = V3._instance("ContactDisc", disc)
    for k, v in dict(StartScale=0.6, GrowEnd=0.3, ShrinkStart=1.0, EndScale=1.0, MaxOpacity=0.40).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    made.append("ContactDisc")
    return f"PuffFB, 2 materials, instances: {', '.join(made)}"
