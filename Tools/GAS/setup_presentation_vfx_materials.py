"""Polish M2: atlas texture, master material and effect instances of the VFX kit v1 (GSTNPresentation plugin content).

Style sheet handoffs/stile-sheet-vfx-review.md (A): one master, Unlit, Masked, packed RGB atlas (R shape, G fake light, B erosion),
two flat tones, no outline, death by shrinking or erosion (never a transparent fade), only the flash above 1 in emissive.
- T_GSTN_VFX_Atlas: Tools/GAS/vfx/T_GSTN_VFX_Atlas.png (make_vfx_atlas.py), linear, no sRGB.
- M_GSTN_VFX_Master: the particle's life is in the material (Particle Relative Time = Niagara's normalized age), so the systems need
  no curves: the shape is born at StartScale, eases out to full size by GrowEnd, shrinks to EndScale from ShrinkStart, erodes from
  its edge from ErodeStart; colour ColorStart -> ColorMid (MidTime) -> ColorEnd, times Particle Color (the cue's User.Tint).
  Shade tone = colour * ShadowMul where the atlas G is 0 (FlatLight 1 = one tone).
- MI_GSTN_VFX_<Effect>: one instance per effect look (INSTANCES below).
Idempotent: the master is rebuilt in place, instances are updated. Run inside the editor (Tools/GAS/run_in_editor.py).
"""
import os

import unreal

ROOT = "/GSTNPresentation/VFX"
MATS = f"{ROOT}/Materials"
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ATL = unreal.AssetToolsHelpers.get_asset_tools()
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ATLAS_PNG = os.path.join(PROJECT, "Tools", "GAS", "vfx", "T_GSTN_VFX_Atlas.png")

CELLS = {"Ball": (0, 0), "Ring": (1, 0), "Star4": (2, 0), "Star5": (3, 0), "Drop": (0, 1), "Streak": (1, 1), "ThinRing": (2, 1),
         "FlatBall": (3, 1)}


def _lin(hex_srgb):
    """sRGB hex -> linear colour (the material works in linear)"""
    rgb = [int(hex_srgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]
    return unreal.LinearColor(lin[0], lin[1], lin[2], 1.0)


DUST = _lin("FFF6E8")
DUST_SHADE = unreal.LinearColor(0.69, 0.62, 0.53, 1.0)  # #D9C7B0 / #FFF6E8 in linear
WHITE = unreal.LinearColor(1, 1, 1, 1)
HIT_MID = _lin("FFD23C")
HIT_END = _lin("FF8A1E")
HIT_SHADE = unreal.LinearColor(1.0, 0.72, 0.55, 1.0)
COOL_SHADE = unreal.LinearColor(0.78, 0.82, 0.95, 1.0)


def _mul(colour, k):
    return unreal.LinearColor(colour.r * k, colour.g * k, colour.b * k, 1.0)


# Screen-calibrated values (round 3). The emissive value is not what shows: eye adaptation inverse, the tone curve and the capture's post
# desaturate light colours and push saturated ones (review r2 read the #FFF6E8 dust as grey #DEDCD7). Measured on a swatch capture
# (NS_GSTN_Swatch, Tools/GASNPPTest/vfx_swatch.py, docs/notes/reviews/polish-m2/r3/swatch.json): grey 1 -> #BAB9BA, 2 -> #DCDCDC,
# 4 -> #F0F0F0, 8 -> #FBFBFB; #FF8A1E x1.5 -> #F57E01, x2.2 -> #FFA102, x4.5 -> #FFD11C. These are used with DisplayGain 1:
# - dust lit, for #F2E9DA on screen (per-channel inverse of the grey ramp), and its shade for ~#D4C2AC (style sheet: #FFF6E8 / #D9C7B0,
#   review r2 minimum #F0E8DA / #D0BEA8)
DUST_SCREEN = unreal.LinearColor(4.5, 3.25, 1.92, 1.0)
DUST_SCREEN_SHADE = unreal.LinearColor(0.38, 0.36, 0.44, 1.0)  # (1.70, 1.18, 0.84) / DUST_SCREEN
# - hit: white core, then #FF8A1E x4.5 (reads ~#FFD11C, the style sheet's #FFD23C), ending at x1.7 (reads ~#FF8A1E). A uniform shade keeps
#   the dark tone orange (review r2: the old warm shade pushed the end colour toward red)
HIT_WHITE = unreal.LinearColor(4.0, 4.0, 4.0, 1.0)
HIT_MID_SCREEN = _mul(HIT_END, 4.5)
HIT_END_SCREEN = _mul(HIT_END, 1.7)
HIT_SCREEN_SHADE = unreal.LinearColor(0.75, 0.75, 0.75, 1.0)

# Effect looks. Times are normalized age (0..1 of the particle's life)
INSTANCES = {
    # Dust balls (run start, skid, jump, landing, impacts): born at 60%, full by frame ~3, shrink to 30% and die there (a ball never
    # shrinks to a point: review r1). DisplayGain: the tone curve greys a plain #FFF6E8 out (r1 read #C9C5BF on screen)
    "DustBall": dict(cell="Ball", StartScale=0.6, GrowEnd=0.12, ShrinkStart=0.4, EndScale=0.3, ErodeStart=1.0, DisplayGain=1.0,
                     colors=(DUST_SCREEN, DUST_SCREEN, DUST_SCREEN), ShadowMul=DUST_SCREEN_SHADE),
    # Landing / knockdown ring: thin, 43% -> 100% in the first 6 frames (ease-out), then thinned by erosion (6 -> 2 cm) until it dies
    "LandRing": dict(cell="ThinRing", FlatLight=1.0, StartScale=0.43, GrowEnd=0.33, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.3,
                     DisplayGain=1.0, colors=(DUST_SCREEN, DUST_SCREEN, DUST_SCREEN)),
    # Landing / knockdown ring on the floor: the thick ring, thinned by erosion from 40% of its life. A flat thin ring is seen nearly edge-on
    # from a low camera (12 degrees: a 6 cm band is ~1 px) and vanished after ~5 frames in review r2; the splat's ring faces the camera
    # Round 4 (owner): thinner, through the smaller size (182 -> 95 cm halves the band) and an earlier erosion (0.4 -> 0.3)
    "GroundRing": dict(cell="Ring", FlatLight=1.0, StartScale=0.43, GrowEnd=0.33, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.3,
                       DisplayGain=1.0, colors=(DUST_SCREEN, DUST_SCREEN, DUST_SCREEN)),
    # Jump: thin ring, smaller and quicker than the landing
    "JumpRing": dict(cell="ThinRing", FlatLight=1.0, StartScale=0.4, GrowEnd=1.0, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.5,
                     DisplayGain=1.0, colors=(DUST_SCREEN, DUST_SCREEN, DUST_SCREEN)),
    # Sprint lines: tapered white streaks, short life
    "SpeedStreak": dict(cell="Streak", FlatLight=1.0, StartScale=0.6, GrowEnd=0.2, ShrinkStart=0.5, EndScale=0.3, ErodeStart=1.0,
                        DisplayGain=2.0, colors=(WHITE, WHITE, WHITE)),
    # Hit: 4-point star shards, white -> yellow -> orange, shrink out; pushed behind the flash for their first frame
    "HitStar": dict(cell="Star4", StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.3, EndScale=0.3, ErodeStart=1.0, DisplayGain=1.0,
                    colors=(HIT_WHITE, HIT_MID_SCREEN, HIT_END_SCREEN), MidTime=0.12, ShadowMul=HIT_SCREEN_SHADE, DepthPush=60.0, DepthPushEnd=0.08, AppearAt=0.05),
    # Strong hit accent: 5-point stars in the row's second colour (User.AccentTint through the particle colour)
    "HitAccent": dict(cell="Star5", StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.3, EndScale=0.3, ErodeStart=1.0, DisplayGain=3.0,
                      colors=(WHITE, WHITE, WHITE), ShadowMul=HIT_SCREEN_SHADE, DepthPush=60.0, DepthPushEnd=0.08, AppearAt=0.05),
    # Hit flash: the only thing above 1 (bloom), one or two frames
    "HitFlash": dict(cell="FlatBall", FlatLight=1.0, EmissiveBoost=4.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=0.0, EndScale=0.3,
                     ErodeStart=1.0, colors=(WHITE, WHITE, HIT_MID), MidTime=0.5),
    # Hit ring: yellow from frame 2 (white only on frame 1: r1 read a dirty beige while it blended), widens and thins
    "HitRing": dict(cell="Ring", FlatLight=1.0, StartScale=0.35, GrowEnd=1.0, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.3,
                    DisplayGain=1.0, colors=(HIT_WHITE, HIT_MID_SCREEN, HIT_END_SCREEN), MidTime=0.08),
    # Tumble trail: balls dropped along the path (tinted by the cue, white by default), shrink out
    "TrailBall": dict(cell="Ball", StartScale=0.8, GrowEnd=0.1, ShrinkStart=0.3, EndScale=0.3, ErodeStart=1.0, DisplayGain=1.0,
                      colors=(DUST_SCREEN, DUST_SCREEN, DUST_SCREEN), ShadowMul=DUST_SCREEN_SHADE),  # same dust as the puffs (review r2)
    # Colour calibration only (NS_GSTN_Swatch, capture with PIE_SWATCH=1): flat, full size all its life, the particle colour as is
    "Swatch": dict(cell="FlatBall", FlatLight=1.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=1.0, EndScale=1.0, ErodeStart=1.0,
                   DisplayGain=1.0, colors=(WHITE, WHITE, WHITE)),
    # Tumble ribbon: one tone; older segments erode first, so the tail tapers
    "TrailRibbon": dict(cell="FlatBall", FlatLight=1.0, StartScale=1.0, GrowEnd=0.01, ShrinkStart=1.0, EndScale=1.0, ErodeStart=0.0,
                        DisplayGain=2.0, colors=(WHITE, WHITE, WHITE)),
}


def import_atlas():
    task = unreal.AssetImportTask()
    task.filename = ATLAS_PNG
    task.destination_path = ROOT
    task.destination_name = "T_GSTN_VFX_Atlas"
    task.replace_existing = True
    task.automated = True
    task.save = False
    ATL.import_asset_tasks([task])
    tex = unreal.load_asset(f"{ROOT}/T_GSTN_VFX_Atlas")
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    # No mips: a small sprite would read a blurred mip where the next cell bleeds in (review r1, the cross at the end of every ball)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    EAL.save_loaded_asset(tex)
    return tex


def _node(mat, cls, x, y, **props):
    node = MEL.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def _link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):  # a wrong pin name fails silently otherwise
        raise RuntimeError(f"cannot connect {a.get_name()}.{a_out} -> {b.get_name()}.{b_in}")


def _scalar(mat, name, default, x, y):
    return _node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)


def _vector(mat, name, default, x, y):
    return _node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=default)


def _op(mat, cls, a, b, x, y, a_out="", b_out=""):
    node = _node(mat, cls, x, y)
    _link(a, a_out, node, "A")
    _link(b, b_out, node, "B")
    return node


def _unary(mat, cls, a, x, y):
    node = _node(mat, cls, x, y)
    _link(a, "", node, "")
    return node


def _lerp(mat, a, b, alpha, x, y):
    node = _node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    _link(a, "", node, "A")
    _link(b, "", node, "B")
    _link(alpha, "", node, "Alpha")
    return node


def _ramp(mat, t, start, end, x, y):
    """saturate((t - start) / max(end - start, 0.001))"""
    num = _op(mat, unreal.MaterialExpressionSubtract, t, start, x, y)
    span = _op(mat, unreal.MaterialExpressionSubtract, end, start, x, y + 80)
    span_safe = _node(mat, unreal.MaterialExpressionMax, x + 120, y + 80, const_b=0.001)
    _link(span, "", span_safe, "A")
    div = _op(mat, unreal.MaterialExpressionDivide, num, span_safe, x + 240, y)
    return _unary(mat, unreal.MaterialExpressionSaturate, div, x + 360, y)


def build_master(atlas):
    path = f"{MATS}/M_GSTN_VFX_Master"
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ATL.create_asset("M_GSTN_VFX_Master", MATS, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    mat.set_editor_property("two_sided", True)
    for flag in ("used_with_niagara_sprites", "used_with_niagara_ribbons", "used_with_niagara_mesh_particles"):
        mat.set_editor_property(flag, True)

    t = _node(mat, unreal.MaterialExpressionParticleRelativeTime, -2600, 0)
    color = _node(mat, unreal.MaterialExpressionParticleColor, -700, 900)
    one_c = _node(mat, unreal.MaterialExpressionConstant, -2600, 400, r=1.0)
    zero_c = _node(mat, unreal.MaterialExpressionConstant, -2600, 1250, r=0.0)

    # Size of the shape inside the quad: lerp(StartScale, 1, easeOut(t / GrowEnd)) * lerp(1, EndScale, ramp(t, ShrinkStart, 1))
    start_scale = _scalar(mat, "StartScale", 0.6, -2600, 200)
    grow_end = _scalar(mat, "GrowEnd", 0.12, -2600, 300)
    grow_x = _unary(mat, unreal.MaterialExpressionSaturate, _op(mat, unreal.MaterialExpressionDivide, t, grow_end, -2450, 250), -2330, 250)
    grow_inv = _unary(mat, unreal.MaterialExpressionOneMinus, grow_x, -2210, 250)
    grow_e = _unary(mat, unreal.MaterialExpressionOneMinus, _op(mat, unreal.MaterialExpressionMultiply, grow_inv, grow_inv, -2090, 250),
                    -1970, 250)
    grow = _lerp(mat, start_scale, one_c, grow_e, -1800, 250)
    shrink_start = _scalar(mat, "ShrinkStart", 0.55, -2600, 500)
    end_scale = _scalar(mat, "EndScale", 0.0, -2600, 600)
    shrink = _lerp(mat, one_c, end_scale, _ramp(mat, t, shrink_start, one_c, -2400, 500), -1900, 550)
    size = _op(mat, unreal.MaterialExpressionMultiply, grow, shrink, -1600, 400)
    size_safe = _node(mat, unreal.MaterialExpressionMax, -1450, 400, const_b=0.001)
    _link(size, "", size_safe, "A")

    # uv in the cell, scaled around its centre and clamped (cell borders are empty in every channel), then into the 4x2 atlas
    uv = _node(mat, unreal.MaterialExpressionTextureCoordinate, -1600, -200)
    centred = _node(mat, unreal.MaterialExpressionSubtract, -1450, -200, const_b=0.5)
    _link(uv, "", centred, "A")
    scaled = _op(mat, unreal.MaterialExpressionDivide, centred, size_safe, -1300, -150)
    back = _node(mat, unreal.MaterialExpressionAdd, -1150, -150, const_b=0.5)
    _link(scaled, "", back, "A")
    # Clamped inside the cell's empty border (>= 25 px of 256), never on its edge: a clamped sample on the edge is filtered with the next
    # cell, and a shrinking shape clamps most of its quad (review r1: a stripe, then a cross, on every ball in its last frames)
    clamped = _node(mat, unreal.MaterialExpressionClamp, -1000, -150, min_default=0.05, max_default=0.95)
    _link(back, "", clamped, "Input")
    cell = _op(mat, unreal.MaterialExpressionAppendVector, _scalar(mat, "CellX", 0.0, -1150, 0), _scalar(mat, "CellY", 0.0, -1150, 100),
               -1000, 50)
    shifted = _op(mat, unreal.MaterialExpressionAdd, clamped, cell, -850, -100)
    grid = _node(mat, unreal.MaterialExpressionConstant2Vector, -850, 50, r=4.0, g=2.0)
    atlas_uv = _op(mat, unreal.MaterialExpressionDivide, shifted, grid, -700, -50)
    tex = _node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, -50, parameter_name="Atlas", texture=atlas,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    _link(atlas_uv, "", tex, "UVs")

    # Colour over life, times the particle colour (tint)
    mid_t = _scalar(mat, "MidTime", 0.3, -1200, 1150)
    col1 = _lerp(mat, _vector(mat, "ColorStart", unreal.LinearColor(1, 1, 1, 1), -1200, 700),
                 _vector(mat, "ColorMid", unreal.LinearColor(1, 1, 1, 1), -1200, 850), _ramp(mat, t, zero_c, mid_t, -1050, 1150), -550, 750)
    col2 = _lerp(mat, col1, _vector(mat, "ColorEnd", unreal.LinearColor(1, 1, 1, 1), -1200, 1000),
                 _ramp(mat, t, mid_t, one_c, -1050, 1300), -400, 800)
    life_col = _op(mat, unreal.MaterialExpressionMultiply, col2, color, -250, 800)

    # Two flat tones: shade where G = 0 (unless FlatLight)
    light = _op(mat, unreal.MaterialExpressionMax, tex, _scalar(mat, "FlatLight", 0.0, -500, 250), -300, 200, a_out="G")
    one3 = _node(mat, unreal.MaterialExpressionConstant3Vector, -300, 480, constant=unreal.LinearColor(1, 1, 1, 1))
    tone = _lerp(mat, _vector(mat, "ShadowMul", COOL_SHADE, -300, 350), one3, light, -100, 350)
    lit = _op(mat, unreal.MaterialExpressionMultiply, life_col, tone, 50, 500)
    boosted = _op(mat, unreal.MaterialExpressionMultiply, lit, _scalar(mat, "EmissiveBoost", 1.0, 50, 650), 200, 550)
    # Exposure-proof: the hub is exposed for a sun, so an unlit emissive of 1 would render near black. Eye Adaptation Inverse turns the
    # value into "this bright on screen" whatever the exposure; DisplayGain lifts it past the tone curve's shoulder (white stays white)
    gained = _op(mat, unreal.MaterialExpressionMultiply, boosted, _scalar(mat, "DisplayGain", 1.3, 200, 700), 350, 550)
    emissive = _node(mat, unreal.MaterialExpressionEyeAdaptationInverse, 500, 550)
    # The input's name differs between versions; a wrong name fails silently and the node then outputs its default (every effect grey)
    if not any(MEL.connect_material_expressions(gained, "", emissive, name) for name in ("Light Value", "LightValue", "LightValueInput")):
        raise RuntimeError("cannot connect the Eye Adaptation Inverse input")

    # Erosion: keep = min(particle alpha, 1 - ramp(t, ErodeStart, 1)); visible where B >= 1 - keep (clip 0.5)
    keep_life = _unary(mat, unreal.MaterialExpressionOneMinus, _ramp(mat, t, _scalar(mat, "ErodeStart", 0.6, -700, 1400), one_c, -550, 1400),
                       -50, 1400)
    # Hidden before AppearAt (normalized age): the hit's shards must not show in the flash's frame (review r2: a cream star on frame 1).
    # appear = saturate((t - AppearAt) * 10000 + 1): 1 at t = 0 when AppearAt is 0 (the default: every other effect shows on frame 1)
    late = _op(mat, unreal.MaterialExpressionSubtract, t, _scalar(mat, "AppearAt", 0.0, -700, 1550), -550, 1550)
    late_k = _node(mat, unreal.MaterialExpressionMultiply, -400, 1550, const_b=10000.0)
    _link(late, "", late_k, "A")
    late_1 = _node(mat, unreal.MaterialExpressionAdd, -280, 1550, const_b=1.0)
    _link(late_k, "", late_1, "A")
    appear = _unary(mat, unreal.MaterialExpressionSaturate, late_1, -160, 1550)
    keep_alive = _op(mat, unreal.MaterialExpressionMin, keep_life, appear, -40, 1500)
    keep = _op(mat, unreal.MaterialExpressionMin, color, keep_alive, 100, 1300, a_out="A")
    erode = _op(mat, unreal.MaterialExpressionAdd, tex, keep, 250, 1250, a_out="B")
    erode_bias = _node(mat, unreal.MaterialExpressionAdd, 400, 1250, const_b=-0.5)
    _link(erode, "", erode_bias, "A")
    mask = _op(mat, unreal.MaterialExpressionMin, tex, erode_bias, 550, 1200, a_out="R")

    # Pushed back from the camera in the first part of life (DepthPush cm until DepthPushEnd): the hit's shards are born inside the flash and
    # must not draw over it (review r1: a dark star in the frame-1 flash). 0 = off
    push_t = _unary(mat, unreal.MaterialExpressionOneMinus, _ramp(mat, t, zero_c, _scalar(mat, "DepthPushEnd", 0.1, 400, 1600), 450, 1600),
                    850, 1600)
    depth = _op(mat, unreal.MaterialExpressionMultiply, push_t, _scalar(mat, "DepthPush", 0.0, 850, 1700), 1000, 1650)
    MEL.connect_material_property(depth, "", unreal.MaterialProperty.MP_PIXEL_DEPTH_OFFSET)
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    return mat


def build_instances(master):
    made = []
    for name, look in INSTANCES.items():
        path = f"{MATS}/MI_GSTN_VFX_{name}"
        mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else ATL.create_asset(
            f"MI_GSTN_VFX_{name}", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", master)
        col, row = CELLS[look["cell"]]
        scalars = {"CellX": col, "CellY": row, "FlatLight": 0.0, "EmissiveBoost": 1.0, "MidTime": 0.3, "DisplayGain": 1.3,
                   "DepthPush": 0.0, "DepthPushEnd": 0.1, "AppearAt": 0.0}
        scalars.update({k: v for k, v in look.items() if isinstance(v, (int, float))})
        for key, value in scalars.items():
            MEL.set_material_instance_scalar_parameter_value(mi, key, float(value))
        for key, value in zip(("ColorStart", "ColorMid", "ColorEnd"), look["colors"]):
            MEL.set_material_instance_vector_parameter_value(mi, key, value)
        MEL.set_material_instance_vector_parameter_value(mi, "ShadowMul", look.get("ShadowMul", COOL_SHADE))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        made.append(name)
    return made


def run():
    atlas = import_atlas()
    master = build_master(atlas)
    made = build_instances(master)
    unreal.log(f"[presentation_vfx] atlas + master + {len(made)} instances")
    return made


if __name__ == "__main__":
    run()
