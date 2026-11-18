"""VFX v2 (owner decision 1A, 2026-11-17): lit glossy materials for the kit's meshes. Brief docs/notes/stile/vfx-stile-v2-astrobot.md.

- M_GSTN_VFX_Toy: Default Lit, Masked (never Translucent), used with Niagara mesh particles. Real light and sky reflections anchor the effect
  in the scene (r4 read as flat stickers). Life in the material, as the v1 master: Particle Relative Time = Niagara's normalized age.
  Size over life by World Position Offset around the particle (born at StartScale, eases out to 1 by GrowEnd, then "pop out": shrinks to
  EndScale with an ease-in from ShrinkStart). Base colour ColorStart -> ColorMid -> ColorEnd times Particle Color (the cue's tint),
  saturated to 1. Roughness / Specular per look (glossy plastic 0.2-0.35, "cotton" dust 0.6). Emissive = flat energy (EmissiveAmount x
  colour, no inner gradient) + a Fresnel rim (RimColor x RimStrength, power RimPower), both through Eye Adaptation Inverse (the hub is
  exposed for the sun). Hidden before AppearAt (the hit's shards never show in the flash frame).
- M_GSTN_VFX_ToyShadow: the same particles flattened on the floor (dynamic parameter 0.x = User.GroundHeight, world Z) by World Position
  Offset: a dark blue disc (never black, never a decal) straight under each piece, along -Z, slightly wider than the piece. Unlit, Masked.
- MI_GSTN_VFX_Toy<Look>: one instance per look (LOOKS below).
The sprite flash of the hit keeps the v1 master (MI_GSTN_VFX_HitFlash). Run after setup_presentation_vfx_meshes, before
setup_presentation_vfx:  python -I Tools/GAS/run_in_editor.py setup_presentation_vfx_toy
"""
import unreal

import setup_presentation_vfx_materials as V1

MATS = V1.MATS
MEL, EAL, ATL = V1.MEL, V1.EAL, V1.ATL
_node, _link, _scalar, _vector, _op, _unary, _lerp, _ramp, _lin = (V1._node, V1._link, V1._scalar, V1._vector, V1._op, V1._unary, V1._lerp,
                                                                  V1._ramp, V1._lin)


def _material(name, shading):
    path = f"{MATS}/{name}"
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ATL.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_niagara_mesh_particles", True)
    return mat


def _size_offset(mat, t):
    """World Position Offset that scales the mesh around its particle by the size over life; returns (offset node, one, zero)"""
    one_c = _node(mat, unreal.MaterialExpressionConstant, -2600, 400, r=1.0)
    zero_c = _node(mat, unreal.MaterialExpressionConstant, -2600, 1250, r=0.0)
    start_scale = _scalar(mat, "StartScale", 1.0, -2600, 200)
    grow_end = _scalar(mat, "GrowEnd", 0.05, -2600, 300)
    grow_x = _unary(mat, unreal.MaterialExpressionSaturate, _op(mat, unreal.MaterialExpressionDivide, t, grow_end, -2450, 250), -2330, 250)
    grow_inv = _unary(mat, unreal.MaterialExpressionOneMinus, grow_x, -2210, 250)
    grow_e = _unary(mat, unreal.MaterialExpressionOneMinus, _op(mat, unreal.MaterialExpressionMultiply, grow_inv, grow_inv, -2090, 250),
                    -1970, 250)
    grow = _lerp(mat, start_scale, one_c, grow_e, -1800, 250)
    # Pop out: ease-in (x^2) from ShrinkStart to EndScale at the end of life
    shrink_x = _ramp(mat, t, _scalar(mat, "ShrinkStart", 0.6, -2600, 500), one_c, -2400, 500)
    shrink_e = _op(mat, unreal.MaterialExpressionMultiply, shrink_x, shrink_x, -1950, 500)
    shrink = _lerp(mat, one_c, _scalar(mat, "EndScale", 0.0, -2600, 600), shrink_e, -1800, 550)
    size = _op(mat, unreal.MaterialExpressionMultiply, grow, shrink, -1600, 400)
    shrink_by = _unary(mat, unreal.MaterialExpressionOneMinus, size, -1450, 400)
    centre = _node(mat, unreal.MaterialExpressionParticlePositionWS, -1600, 150)
    world = _node(mat, unreal.MaterialExpressionWorldPosition, -1600, 50)
    to_centre = _op(mat, unreal.MaterialExpressionSubtract, centre, world, -1450, 100)
    offset = _op(mat, unreal.MaterialExpressionMultiply, to_centre, shrink_by, -1300, 200)
    return offset, centre, world, one_c, zero_c


def _appear(mat, t, x, y):
    """1 from AppearAt on (normalized age), 0 before"""
    late = _op(mat, unreal.MaterialExpressionSubtract, t, _scalar(mat, "AppearAt", 0.0, x, y), x + 150, y)
    late_k = _node(mat, unreal.MaterialExpressionMultiply, x + 300, y, const_b=10000.0)
    _link(late, "", late_k, "A")
    late_1 = _node(mat, unreal.MaterialExpressionAdd, x + 420, y, const_b=1.0)
    _link(late_k, "", late_1, "A")
    return _unary(mat, unreal.MaterialExpressionSaturate, late_1, x + 540, y)


def _eye_inverse(mat, value, x, y):
    node = _node(mat, unreal.MaterialExpressionEyeAdaptationInverse, x, y)
    if not any(MEL.connect_material_expressions(value, "", node, name) for name in ("Light Value", "LightValue", "LightValueInput")):
        raise RuntimeError("cannot connect the Eye Adaptation Inverse input")
    return node


def build_toy():
    mat = _material("M_GSTN_VFX_Toy", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    t = _node(mat, unreal.MaterialExpressionParticleRelativeTime, -2600, 0)
    offset, _centre, _world, one_c, zero_c = _size_offset(mat, t)
    color = _node(mat, unreal.MaterialExpressionParticleColor, -700, 900)

    mid_t = _scalar(mat, "MidTime", 0.3, -1200, 1150)
    col1 = _lerp(mat, _vector(mat, "ColorStart", unreal.LinearColor(1, 1, 1, 1), -1200, 700),
                 _vector(mat, "ColorMid", unreal.LinearColor(1, 1, 1, 1), -1200, 850), _ramp(mat, t, zero_c, mid_t, -1050, 1150), -550, 750)
    col2 = _lerp(mat, col1, _vector(mat, "ColorEnd", unreal.LinearColor(1, 1, 1, 1), -1200, 1000),
                 _ramp(mat, t, mid_t, one_c, -1050, 1300), -400, 800)
    tinted = _op(mat, unreal.MaterialExpressionMultiply, col2, color, -250, 800)
    base = _unary(mat, unreal.MaterialExpressionSaturate, tinted, -100, 800)

    # Flat energy (no inner gradient) + Fresnel rim, exposure-proof
    energy = _op(mat, unreal.MaterialExpressionMultiply, base, _scalar(mat, "EmissiveAmount", 0.0, -250, 1000), 50, 950)
    fresnel = _node(mat, unreal.MaterialExpressionFresnel, -250, 1150)
    _link(_scalar(mat, "RimPower", 3.0, -400, 1150), "", fresnel, "ExponentIn")
    rim = _op(mat, unreal.MaterialExpressionMultiply, fresnel, _vector(mat, "RimColor", unreal.LinearColor(1, 1, 1, 1), -250, 1300), 50, 1150)
    rim_k = _op(mat, unreal.MaterialExpressionMultiply, rim, _scalar(mat, "RimStrength", 0.5, -250, 1450), 200, 1150)
    glow = _op(mat, unreal.MaterialExpressionAdd, energy, rim_k, 350, 1050)
    emissive = _eye_inverse(mat, glow, 500, 1050)

    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(_scalar(mat, "Roughness", 0.3, 300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(_scalar(mat, "Specular", 0.6, 300, 400), "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(_appear(mat, t, -700, 1600), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def build_shadow():
    """Each vertex goes to the floor (Z = GroundHeight + 1.5) under its particle, spread ShadowSpread around it: the piece's footprint seen
    from straight above, a little wider. The piece's own size over life is applied first, so the shadow shrinks with it"""
    mat = _material("M_GSTN_VFX_ToyShadow", unreal.MaterialShadingModel.MSM_UNLIT)
    t = _node(mat, unreal.MaterialExpressionParticleRelativeTime, -2600, 0)
    offset, centre, world, _one, _zero = _size_offset(mat, t)
    sized = _op(mat, unreal.MaterialExpressionAdd, world, offset, -1100, 200)
    around = _op(mat, unreal.MaterialExpressionSubtract, sized, centre, -950, 200)
    spread = _op(mat, unreal.MaterialExpressionMultiply, around, _scalar(mat, "ShadowSpread", 1.25, -1100, 350), -800, 250)
    flat_xy = _op(mat, unreal.MaterialExpressionAdd, centre, spread, -650, 250)
    dyn = _node(mat, unreal.MaterialExpressionDynamicParameter, -1100, 500)
    ground = _node(mat, unreal.MaterialExpressionAdd, -900, 500, const_b=1.5)
    _link(dyn, "Param1", ground, "A")
    xy = _node(mat, unreal.MaterialExpressionComponentMask, -500, 250, r=True, g=True, b=False, a=False)
    _link(flat_xy, "", xy, "")
    target = _op(mat, unreal.MaterialExpressionAppendVector, xy, ground, -350, 300)
    wpo = _op(mat, unreal.MaterialExpressionSubtract, target, world, -200, 300)
    MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    shade = _eye_inverse(mat, _vector(mat, "ShadowColor", unreal.LinearColor(0.03, 0.08, 0.32, 1), -400, 600), -200, 600)
    MEL.connect_material_property(shade, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(_appear(mat, t, -700, 900), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


# Base colours are albedo now (lit by the scene), not screen-calibrated emissive: <= 1
DUST = _lin("FFF6E8")
WHITE = unreal.LinearColor(1, 1, 1, 1)
HIT_MID = _lin("FFD23C")
HIT_END = _lin("FF8A1E")
NEON = unreal.LinearColor(1.0, 0.62, 0.05, 1)  # saturated amber for the hit ring

LOOKS = {
    # "Cotton" dust (run start, skid, landing and impact balls, trail): warm white, matte-ish, born at 60% -> 100% by frame 3, pop out
    "Dust": dict(Roughness=0.6, Specular=0.5, StartScale=0.6, GrowEnd=0.15, ShrinkStart=0.45, EndScale=0.0, RimStrength=0.6, RimPower=2.0,
                 EmissiveAmount=0.3,  # r1 review: white went grey in the hub light
                 colors=(DUST, DUST, DUST)),
    # Glossy white plastic (landing / jump ring, sprint capsules)
    "Gloss": dict(Roughness=0.25, Specular=0.65, StartScale=0.45, GrowEnd=0.3, ShrinkStart=0.55, EndScale=0.0, RimStrength=0.6, RimPower=2.0,
                  EmissiveAmount=0.3,
                  colors=(DUST, DUST, DUST)),
    "Streak": dict(Roughness=0.25, Specular=0.65, StartScale=0.7, GrowEnd=0.2, ShrinkStart=0.5, EndScale=0.0, RimStrength=0.4,
                   colors=(WHITE, WHITE, WHITE)),
    # Hit shards: glossy gems, white -> yellow -> orange, full size on their first shown frame, pop out; hidden on the flash frame
    "Gem": dict(Roughness=0.22, Specular=0.7, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.4, EndScale=0.0, RimStrength=0.45, MidTime=0.3,
                EmissiveAmount=0.35, AppearAt=0.05, colors=(WHITE, HIT_MID, HIT_END)),
    # Strong hit debris: the kit's accent colour (particle colour = User.AccentTint), bounces on the floor, pops out
    "Debris": dict(Roughness=0.25, Specular=0.7, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.75, EndScale=0.0, RimStrength=0.4,
                   EmissiveAmount=0.15, AppearAt=0.02, colors=(WHITE, WHITE, WHITE)),
    # "Neon glass" ring of the hit: opaque torus, flat saturated emissive + white Fresnel rim (never translucent)
    "Neon": dict(Roughness=0.3, Specular=0.5, StartScale=0.85, GrowEnd=0.15,  # r1 review: biggest on frame 1, not at the hitstop end
                 ShrinkStart=0.6, EndScale=0.0, RimStrength=1.5, RimPower=2.0,
                 EmissiveAmount=2.5, MidTime=0.08, colors=(WHITE, NEON, NEON)),
}


def build_instances(toy):
    made = []
    for name, look in LOOKS.items():
        path = f"{MATS}/MI_GSTN_VFX_Toy{name}"
        mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else ATL.create_asset(
            f"MI_GSTN_VFX_Toy{name}", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", toy)
        scalars = {"EmissiveAmount": 0.0, "RimPower": 3.0, "MidTime": 0.3, "AppearAt": 0.0}
        scalars.update({k: v for k, v in look.items() if isinstance(v, (int, float))})
        for key, value in scalars.items():
            MEL.set_material_instance_scalar_parameter_value(mi, key, float(value))
        for key, value in zip(("ColorStart", "ColorMid", "ColorEnd"), look["colors"]):
            MEL.set_material_instance_vector_parameter_value(mi, key, value)
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        made.append(name)
    return made


def shadow_instance(look, shadow):
    """MI_GSTN_VFX_ToyShadow<Look>: same size over life as the piece it follows"""
    path = f"{MATS}/MI_GSTN_VFX_ToyShadow{look}"
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else ATL.create_asset(
        f"MI_GSTN_VFX_ToyShadow{look}", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", shadow)
    for key in ("StartScale", "GrowEnd", "ShrinkStart", "EndScale", "AppearAt"):
        if key in LOOKS[look]:
            MEL.set_material_instance_scalar_parameter_value(mi, key, float(LOOKS[look][key]))
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)


SHADOWED = ("Dust", "Debris", "Gem")


def run():
    toy = build_toy()
    shadow = build_shadow()
    made = build_instances(toy)
    for look in SHADOWED:
        shadow_instance(look, shadow)
    return f"toy + shadow + {len(made)} looks + {len(SHADOWED)} shadow looks"
