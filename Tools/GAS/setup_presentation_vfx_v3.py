"""VFX v3 "Astro Bot semi-realistic" (docs/notes/stile/vfx-stile-v3-design-sheet.md, owner refs docs/notes/reviews/ref-astrobot-vfx/):
textures and materials. Energy = light, air = real smoke, solids = toys (the v2 Toy materials stay, for debris only).

- T_GSTN_VFX_Star / Streak / Smoke: procedural PNGs (Tools/GAS/vfx/make_vfx_v3_textures.py, run it first with a plain python).
- M_GSTN_VFX_Flash: Unlit, Translucent (never additive only: an additive white vanishes on snow), for sprites. White core (tex R) + a
  saturated fringe (tex G, FringeColor) over the shape (tex A), times the particle colour; size over life inside the quad (StartScale ->
  1 by GrowEnd, then to EndScale from ShrinkStart), fades out from FadeStart. Exposure-proof emissive (Eye Adaptation Inverse x
  Brightness): above 1 = bloom. Hit star, streaks, sparks, lasers, snow grains.
- M_GSTN_VFX_Smoke: Default Lit, Translucent, lit per vertex by the translucency volume (the cheapest lit translucency), Depth Fade (no
  hard cut on floors and walls). A random puff of the 2x2 atlas per particle (Particle Random), grows from StartScale while it erodes
  from ErodeStart (tex B noise), self-shading from tex G. Base colour = the particle colour (User.SurfaceColor: the floor's colour).
- M_GSTN_VFX_ContactShadow: Unlit translucent disc for UGSTN_PresentationComponent's contact shadow, opacity from custom primitive data 0.
Run in the open editor:  python -I Tools/GAS/run_in_editor.py setup_presentation_vfx_v3    (idempotent)
"""
import os

import unreal

import setup_presentation_vfx_materials as V1

ROOT, MATS = V1.ROOT, V1.MATS
MEL, EAL, ATL = V1.MEL, V1.EAL, V1.ATL
_node, _link, _scalar, _vector, _op, _unary, _lerp, _ramp, _lin = (V1._node, V1._link, V1._scalar, V1._vector, V1._op, V1._unary, V1._lerp,
                                                                  V1._ramp, V1._lin)
TEX_DIR = os.path.join(V1.PROJECT, "Tools", "GAS", "vfx")
TEXTURES = ["T_GSTN_VFX_Star", "T_GSTN_VFX_Streak", "T_GSTN_VFX_Smoke"]


def import_texture(name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(TEX_DIR, name + ".png")
    task.destination_path = ROOT
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    ATL.import_asset_tasks([task])
    tex = unreal.load_asset(f"{ROOT}/{name}")
    tex.set_editor_property("srgb", False)  # channels are masks, not colours
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(tex)
    return tex


def _material(name, shading, blend):
    path = f"{MATS}/{name}"
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ATL.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("two_sided", True)
    for flag in ("used_with_niagara_sprites", "used_with_niagara_ribbons"):
        mat.set_editor_property(flag, True)
    return mat


def _life_size(mat, t, one_c, x, y):
    """lerp(StartScale, 1, easeOut(t / GrowEnd)) * lerp(1, EndScale, ramp(t, ShrinkStart, 1)), >= 0.001"""
    grow_x = _unary(mat, unreal.MaterialExpressionSaturate,
                    _op(mat, unreal.MaterialExpressionDivide, t, _scalar(mat, "GrowEnd", 0.05, x, y + 100), x + 150, y + 50), x + 270, y + 50)
    grow_inv = _unary(mat, unreal.MaterialExpressionOneMinus, grow_x, x + 390, y + 50)
    grow_e = _unary(mat, unreal.MaterialExpressionOneMinus, _op(mat, unreal.MaterialExpressionMultiply, grow_inv, grow_inv, x + 510, y + 50),
                    x + 630, y + 50)
    grow = _lerp(mat, _scalar(mat, "StartScale", 1.0, x, y), one_c, grow_e, x + 800, y)
    shrink = _lerp(mat, one_c, _scalar(mat, "EndScale", 0.0, x, y + 300),
                   _ramp(mat, t, _scalar(mat, "ShrinkStart", 0.0, x, y + 200), one_c, x + 150, y + 200), x + 800, y + 200)
    size = _op(mat, unreal.MaterialExpressionMultiply, grow, shrink, x + 950, y + 100)
    safe = _node(mat, unreal.MaterialExpressionMax, x + 1100, y + 100, const_b=0.001)
    _link(size, "", safe, "A")
    return safe


def _scaled_uv(mat, size, x, y, lo=0.0, hi=1.0):
    """The quad's uv scaled around its centre by 1/size: the shape grows and shrinks inside the quad"""
    uv = _node(mat, unreal.MaterialExpressionTextureCoordinate, x, y)
    centred = _node(mat, unreal.MaterialExpressionSubtract, x + 150, y, const_b=0.5)
    _link(uv, "", centred, "A")
    scaled = _op(mat, unreal.MaterialExpressionDivide, centred, size, x + 300, y)
    back = _node(mat, unreal.MaterialExpressionAdd, x + 450, y, const_b=0.5)
    _link(scaled, "", back, "A")
    clamped = _node(mat, unreal.MaterialExpressionClamp, x + 600, y, min_default=lo, max_default=hi)
    _link(back, "", clamped, "Input")
    return clamped


def _eye_inverse(mat, value, x, y):
    node = _node(mat, unreal.MaterialExpressionEyeAdaptationInverse, x, y)
    if not any(MEL.connect_material_expressions(value, "", node, name) for name in ("Light Value", "LightValue", "LightValueInput")):
        raise RuntimeError("cannot connect the Eye Adaptation Inverse input")
    return node


def _fade(mat, t, one_c, name, default, x, y):
    """1 - ramp(t, <name>, 1)"""
    return _unary(mat, unreal.MaterialExpressionOneMinus, _ramp(mat, t, _scalar(mat, name, default, x, y), one_c, x + 150, y), x + 600, y)


def build_flash(star):
    mat = _material("M_GSTN_VFX_Flash", unreal.MaterialShadingModel.MSM_UNLIT, unreal.BlendMode.BLEND_TRANSLUCENT)
    t = _node(mat, unreal.MaterialExpressionParticleRelativeTime, -2600, 0)
    one_c = _node(mat, unreal.MaterialExpressionConstant, -2600, 400, r=1.0)
    size = _life_size(mat, t, one_c, -2400, 100)
    uv = _scaled_uv(mat, size, -1200, -200)
    tex = _node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -450, -200, parameter_name="Tex", texture=star,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    _link(uv, "", tex, "UVs")
    pcol = _node(mat, unreal.MaterialExpressionParticleColor, -450, 300)
    core = _op(mat, unreal.MaterialExpressionMultiply, _vector(mat, "CoreColor", unreal.LinearColor(1, 1, 1, 1), -300, 50), tex, -100, 0,
               b_out="R")
    fringe = _op(mat, unreal.MaterialExpressionMultiply, _vector(mat, "FringeColor", unreal.LinearColor(1, 0.35, 0.03, 1), -300, 150), tex,
                 -100, 120, b_out="G")
    colour = _op(mat, unreal.MaterialExpressionAdd, core, fringe, 50, 60)
    tinted = _op(mat, unreal.MaterialExpressionMultiply, colour, pcol, 200, 100)
    bright = _op(mat, unreal.MaterialExpressionMultiply, tinted, _scalar(mat, "Brightness", 4.0, 200, 250), 350, 150)
    emissive = _eye_inverse(mat, bright, 500, 150)
    fade = _fade(mat, t, one_c, "FadeStart", 0.6, -1200, 600)
    alpha = _op(mat, unreal.MaterialExpressionMultiply, tex, fade, 100, 400, a_out="A")
    alpha_p = _op(mat, unreal.MaterialExpressionMultiply, alpha, pcol, 250, 450, b_out="A")
    opacity = _op(mat, unreal.MaterialExpressionMultiply, alpha_p, _scalar(mat, "OpacityMax", 1.0, 250, 600), 400, 500)
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def build_smoke_full(smoke):
    mat = _material("M_GSTN_VFX_Smoke", unreal.MaterialShadingModel.MSM_DEFAULT_LIT, unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
    t = _node(mat, unreal.MaterialExpressionParticleRelativeTime, -2600, 0)
    one_c = _node(mat, unreal.MaterialExpressionConstant, -2600, 400, r=1.0)
    size = _life_size(mat, t, one_c, -2400, 100)
    uv = _scaled_uv(mat, size, -1200, -300, 0.02, 0.98)
    rnd = _node(mat, unreal.MaterialExpressionParticleRandom, -1350, -500)
    rnd4 = _node(mat, unreal.MaterialExpressionMultiply, -1200, -500, const_b=3.999)
    _link(rnd, "", rnd4, "A")
    idx = _unary(mat, unreal.MaterialExpressionFloor, rnd4, -1050, -500)
    cx = _node(mat, unreal.MaterialExpressionFmod, -900, -560)
    _link(idx, "", cx, "A")
    _link(_node(mat, unreal.MaterialExpressionConstant, -1050, -420, r=2.0), "", cx, "B")
    half = _node(mat, unreal.MaterialExpressionMultiply, -900, -460, const_b=0.5)
    _link(idx, "", half, "A")
    cy = _unary(mat, unreal.MaterialExpressionFloor, half, -780, -460)
    cell = _op(mat, unreal.MaterialExpressionAppendVector, cx, cy, -650, -500)
    shifted = _op(mat, unreal.MaterialExpressionAdd, uv, cell, -500, -400)
    atlas_uv = _node(mat, unreal.MaterialExpressionMultiply, -350, -400, const_b=0.5)
    _link(shifted, "", atlas_uv, "A")
    tex = _node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -200, -400, parameter_name="Tex", texture=smoke,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    _link(atlas_uv, "", tex, "UVs")
    pcol = _node(mat, unreal.MaterialExpressionParticleColor, -450, 300)

    # r8 Fable: the cloud's shading (ShadeMin 0.62, rim 0.68, lift 0.4) separates the lobes on the dark floor but reads grey, darker than
    # a white floor. Each shading value has a *Light twin used on bright floors, blended by the surface colour (particle colour, G as a
    # cheap luminance) from 0.4 to 0.7. The MIs that need no difference set the twin to the same value
    over = _node(mat, unreal.MaterialExpressionSubtract, -550, 800, const_b=0.4)
    _link(_component(mat, pcol, "G", -700, 800), "", over, "A")
    span = _node(mat, unreal.MaterialExpressionMultiply, -400, 800, const_b=3.333)
    _link(over, "", span, "A")
    bright = _unary(mat, unreal.MaterialExpressionSaturate, span, -250, 800)

    def _by_floor(name, default, x, y):
        return _lerp(mat, _scalar(mat, name, default, x, y), _scalar(mat, name + "Light", default, x, y + 40), bright, x + 150, y)

    # Self-shading: lerp(ShadeMin, 1, G); base colour = surface colour (particle colour) x shading
    shade = _lerp(mat, _by_floor("ShadeMin", 0.55, -300, 50), one_c, _component(mat, tex, "G", -100, 150), 100, 100)
    # r5 Fable: ShadeMin 0.4 read as soot on a white floor. The puff stays the floor's colour (ShadeMin ~0.7); the edge gets a thin, slightly
    # darker soft rim (RimShade at density 0, 1 at RimWidth) and the contrast comes from opacity, not from darkening the whole puff
    rim = _lerp(mat, _by_floor("RimShade", 0.8, -300, 380), one_c,
                _unary(mat, unreal.MaterialExpressionSaturate,
                       _op(mat, unreal.MaterialExpressionDivide, _component(mat, tex, "R", -100, 470), _scalar(mat, "RimWidth", 0.35, -100, 540),
                           50, 480), 180, 480), 300, 400)
    # hd1 DK review: on a near-white floor the puff was white on white (owner: "non si vedono"). The shaded side gets a cool tint, like
    # snow in shadow (a neutral grey read as soot in r5), on bright floors only: tint = lerp(1, ShadeTint, bright), applied to the shade
    # hd1 attempt 1 review: on the dark floor the dust read as dry earth (warm tan): there the tint is at half strength and the colour
    # leans to snow, lerp(surface, SnowColor, DarkSnow), fading out from G 0.4 to 0.7 like the *Light twins
    half_c = _node(mat, unreal.MaterialExpressionConstant, -450, 1050, r=0.5)
    tint_k = _lerp(mat, half_c, one_c, bright, -250, 1050)
    tint = _lerp(mat, one_c, _vector(mat, "ShadeTint", unreal.LinearColor(1, 1, 1, 1), -300, 1000), tint_k, -100, 1000)
    snow_k = _op(mat, unreal.MaterialExpressionMultiply, _scalar(mat, "DarkSnow", 0.0, -450, 1150),
                 _op(mat, unreal.MaterialExpressionSubtract, one_c, bright, -300, 1200), -200, 1150)
    pcol_s = _lerp(mat, pcol, _vector(mat, "SnowColor", unreal.LinearColor(0.85, 0.88, 0.95, 1), -300, 1250), snow_k, -50, 1200)
    shade_t = _lerp(mat, _op(mat, unreal.MaterialExpressionMultiply, shade, tint, 150, 1000), shade,
                    _component(mat, tex, "G", 150, 1100), 250, 1000)
    base = _op(mat, unreal.MaterialExpressionMultiply, _op(mat, unreal.MaterialExpressionMultiply, pcol_s, shade_t, 250, 150), rim, 330, 200)
    lift = _eye_inverse(mat, _op(mat, unreal.MaterialExpressionMultiply, base, _by_floor("EmissiveLift", 0.12, 100, 300), 400, 250),
                        550, 250)

    # Erosion: density - erode(t) * (0.35 + B), sharpened a little (soft but not mush)
    erode = _ramp(mat, t, _scalar(mat, "ErodeStart", 0.25, -1200, 700), one_c, -1050, 700)
    noise = _node(mat, unreal.MaterialExpressionAdd, -100, 600, const_b=0.35)
    _link(tex, "B", noise, "A")
    eaten = _op(mat, unreal.MaterialExpressionMultiply, erode, noise, 50, 650)
    dens = _op(mat, unreal.MaterialExpressionSubtract, tex, eaten, 200, 600, a_out="R")
    sharp = _node(mat, unreal.MaterialExpressionMultiply, 350, 600, const_b=1.4)
    _link(dens, "", sharp, "A")
    dens_s = _unary(mat, unreal.MaterialExpressionSaturate, sharp, 500, 600)
    alpha = _op(mat, unreal.MaterialExpressionMultiply, dens_s, pcol, 650, 650, b_out="A")
    alpha_m = _op(mat, unreal.MaterialExpressionMultiply, alpha, _scalar(mat, "OpacityMax", 0.8, 650, 800), 800, 700)
    depth = _node(mat, unreal.MaterialExpressionDepthFade, 950, 700)
    if not any(MEL.connect_material_expressions(alpha_m, "", depth, name) for name in ("Opacity", "InOpacity")):
        raise RuntimeError("cannot connect the Depth Fade opacity")
    if not any(MEL.connect_material_expressions(_scalar(mat, "FadeDistance", 35.0, 800, 900), "", depth, name)
               for name in ("FadeDistance", "Fade Distance")):
        raise RuntimeError("cannot connect the Depth Fade distance")
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(_scalar(mat, "Roughness", 0.9, 400, 0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(lift, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(depth, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def _component(mat, node, channel, x, y):
    mask = _node(mat, unreal.MaterialExpressionComponentMask, x, y, r=channel == "R", g=channel == "G", b=channel == "B", a=channel == "A")
    _link(node, "", mask, "")
    return mask


def build_contact_shadow():
    path = f"{MATS}/M_GSTN_VFX_ContactShadow"
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ATL.create_asset("M_GSTN_VFX_ContactShadow", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", False)
    uv = _node(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    centred = _node(mat, unreal.MaterialExpressionSubtract, -1050, 0, const_b=0.5)
    _link(uv, "", centred, "A")
    length = _unary(mat, unreal.MaterialExpressionLength, centred, -900, 0)
    r = _node(mat, unreal.MaterialExpressionMultiply, -780, 0, const_b=2.0)
    _link(length, "", r, "A")
    one_c = _node(mat, unreal.MaterialExpressionConstant, -900, 200, r=1.0)
    # Crisp: full inside EdgeStart, 0 at the rim, a short smooth edge (astro_2: a hard-edged dark disc)
    edge = _ramp(mat, r, _scalar(mat, "EdgeStart", 0.82, -900, 100), one_c, -650, 50)
    inside = _unary(mat, unreal.MaterialExpressionOneMinus, _op(mat, unreal.MaterialExpressionMultiply, edge, edge, -300, 50), -180, 50)
    k = _node(mat, unreal.MaterialExpressionScalarParameter, -300, 200, parameter_name="HeightOpacity", default_value=1.0,
              use_custom_primitive_data=True, primitive_data_index=0)
    a1 = _op(mat, unreal.MaterialExpressionMultiply, inside, k, -50, 100)
    opacity = _op(mat, unreal.MaterialExpressionMultiply, a1, _scalar(mat, "MaxOpacity", 0.6, -50, 250), 100, 150)
    colour = _vector(mat, "ShadowColor", unreal.LinearColor(0.012, 0.016, 0.05, 1), -50, -100)
    MEL.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


ORANGE = unreal.LinearColor(1.0, 0.22, 0.0, 1)  # saturated: reads on white snow and on dark rock
CYAN = unreal.LinearColor(0.25, 0.75, 1.0, 1)
BLUE = unreal.LinearColor(0.05, 0.35, 1.0, 1)  # r5 Fable: the sprint lines' cyan fringe washed out to white; a deeper blue survives the bloom
WHITE = unreal.LinearColor(1, 1, 1, 1)

FLASH_LOOKS = {
    # astro_1: the hit's white star-burst, biggest on frame 1, shrinks to nothing in its life (~4 frames)
    "FlashStar": dict(tex="T_GSTN_VFX_Star", Brightness=7.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.0, EndScale=0.0, FadeStart=0.7,
                      fringe=ORANGE),
    # Short coloured streaks flying out of the hit (velocity-aligned)
    # r7 Fable: the strong fan held full length v36-v44 over the victim's legs (the hitstop slows the system to 0.15 after 2 free frames,
    # so ShrinkStart 0.35 of a 0.15 s life began at ~frame 9): 0.12 starts the shrink on frame 2-3, about half length by frame 4
    "FlashStreak": dict(tex="T_GSTN_VFX_Streak", Brightness=5.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.12, EndScale=0.0,
                        FadeStart=0.6, fringe=ORANGE),
    # Tiny sparks
    "FlashSpark": dict(tex="T_GSTN_VFX_Star", Brightness=8.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.3, EndScale=0.0, FadeStart=0.8,
                       fringe=unreal.LinearColor(1.0, 0.75, 0.1, 1)),
    # astro_2: thin very bright lines (sprint lines, jets)
    "FlashLaser": dict(tex="T_GSTN_VFX_Streak", Brightness=6.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.6, EndScale=0.2, FadeStart=0.5,
                       fringe=BLUE),
    # astro_3: fine snow/dust grains, the floor's colour (particle colour), small glints
    "Grain": dict(tex="T_GSTN_VFX_Star", Brightness=1.4, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.5, EndScale=0.0, FadeStart=0.6,
                  fringe=WHITE),
}
SMOKE_LOOKS = {
    # Impact / landing dust and snow: big soft lit puffs that grow and erode
    # r5 Fable: ShadeMin 0.4 / EmissiveLift 0.06 read as soot on the light floor. r6 self-check: 0.7 / 0.14 still mid grey on the 0.95
    # floor (the lit translucent puff gets far less light than the floor): EmissiveLift 0.5 (eye-adapted, so ~half the surface colour on
    # screen whatever the exposure, still < 1: no bloom), ShadeMin 0.8, rim 0.8 keeps a soft edge. On-screen opacity was ~0.3-0.4 on the
    # dark floor -> OpacityMax 1.0 (the particle alpha and the erosion keep it below that)
    # hd1 DK review: white on white on the light floor -> a cool shaded underside and rim there (ShadeTint, *Light values), denser longer
    # hd1 attempt 1 review, dark floor: brighter (lift 0.8), a little shaded (0.65), towards snow (DarkSnow 0.4)
    "SmokeDust": dict(DarkSnow=0.4, OpacityMax=1.0, ShadeMin=0.65, StartScale=0.45, GrowEnd=0.75, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.3,
                      FadeDistance=35.0, EmissiveLift=0.8, Roughness=0.9, RimShade=0.72, RimWidth=0.45,
                       ShadeMinLight=0.62, RimShadeLight=0.60, EmissiveLiftLight=0.5, ShadeTint=(0.78, 0.86, 1.0)),
    # Impact / bounce cloud lobes (r7 Fable: one flat blob): lower ShadeMin and a wider, darker rim give each lobe a lit top and a shaded
    # underside so the overlapping lobes separate; OpacityMax 0.8 (dark v119 read as an opaque sticker). Lift 0.4: on the light floor the
    # extra shading must not read as soot (r5: ShadeMin 0.4 did)
    "SmokeCloud": dict(DarkSnow=0.4, OpacityMax=1.0, ShadeMin=0.62, StartScale=0.5, GrowEnd=0.7, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.25,
                       FadeDistance=35.0, EmissiveLift=0.7, Roughness=0.9, RimShade=0.68, RimWidth=0.5,
                       # r8 Fable: grey on the white floor -> paler on bright floors only
                       ShadeMinLight=0.60, RimShadeLight=0.62, EmissiveLiftLight=0.55, ShadeTint=(0.78, 0.86, 1.0)),
    # Locomotion dust: thinner, quicker
    "SmokeLight": dict(DarkSnow=0.4, OpacityMax=1.0, ShadeMin=0.7, StartScale=0.6, GrowEnd=0.8, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.2,
                       FadeDistance=25.0, EmissiveLift=0.75, Roughness=0.9, RimShade=0.8, RimWidth=0.35,
                       ShadeMinLight=0.66, RimShadeLight=0.64, EmissiveLiftLight=0.5, ShadeTint=(0.78, 0.86, 1.0)),
}


def _instance(name, parent):
    path = f"{MATS}/MI_GSTN_VFX_{name}"
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else ATL.create_asset(
        f"MI_GSTN_VFX_{name}", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    return mi


def build_instances(flash, smoke, shadow):
    made = []
    for name, look in FLASH_LOOKS.items():
        mi = _instance(name, flash)
        for key, value in look.items():
            if isinstance(value, (int, float)):
                MEL.set_material_instance_scalar_parameter_value(mi, key, float(value))
        MEL.set_material_instance_vector_parameter_value(mi, "FringeColor", look["fringe"])
        MEL.set_material_instance_texture_parameter_value(mi, "Tex", unreal.load_asset(f"{ROOT}/{look['tex']}"))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        made.append(name)
    for name, look in SMOKE_LOOKS.items():
        mi = _instance(name, smoke)
        for key, value in look.items():
            if key in ("ShadeTint", "SnowColor"):
                MEL.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(*value, 1.0))
            else:
                MEL.set_material_instance_scalar_parameter_value(mi, key, float(value))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        made.append(name)
    mi = _instance("ContactShadow", shadow)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    made.append("ContactShadow")
    return made


def build_capture_backdrop():
    """Capture only (Tools/GASNPPTest/pie_capture_vfx.py PIE_BACKDROP): plain lit surface, colour from the Color parameter"""
    path = f"{MATS}/M_GSTN_VFX_CaptureBackdrop"
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ATL.create_asset("M_GSTN_VFX_CaptureBackdrop", MATS, unreal.Material, unreal.MaterialFactoryNew())
    MEL.connect_material_property(_vector(mat, "Color", unreal.LinearColor(0.8, 0.8, 0.8, 1), -300, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(_scalar(mat, "Roughness", 0.85, -300, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def run():
    tex ={name: import_texture(name) for name in TEXTURES}
    flash = build_flash(tex["T_GSTN_VFX_Star"])
    smoke = build_smoke_full(tex["T_GSTN_VFX_Smoke"])
    shadow = build_contact_shadow()
    made = build_instances(flash, smoke, shadow)
    build_capture_backdrop()
    return f"3 textures, 3 materials, {len(made)} instances: {', '.join(made)}"
