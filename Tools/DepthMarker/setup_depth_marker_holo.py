"""Builds the holographic look of the classic depth marker (owner 2026-11-17: "more holographic, less solid"; style ref Astro Bot / Nintendo).

Two materials in /Game/Blueprints/DepthMarker, both driven by the presenters' MID parameters Color (player colour) and Opacity:
  M_DM_Holo        ring and dash segments (ISM cubes). Additive, unlit, no lighting cost: soft scanlines that drift along the marker's own
                   column, a brighter edge, a short rare flicker and a gentle breathe. ~25 instructions: toaster-safe.
  M_DM_Holo_Decal  a blob shadow for a Decal presenter: a dim translucent disc with a crisp ring and the same scanlines.
Config/DefaultGSTNDepthMarker.ini points DefaultSegmentMaterial / DefaultDecalMaterial at them. Every look number is a material parameter
(make a material instance to tune it; Fable reviews the look in game before the owner sees it, rule 10).

Idempotent (expressions deleted and recreated). Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<abs path of this file> -unattended -nullrhi -NoP4
Only run it while the owner's editor is closed. Prints "GSTN_HOLO FINISHED OK" or the problems.
"""
import unreal

FOLDER = "/Game/Blueprints/DepthMarker"
lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
problems = []


def log(msg):
    unreal.log("GSTN_HOLO " + msg)


def get_or_create(name, asset_class, factory):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return tools.create_asset(name, FOLDER, asset_class, factory)


def new_material(name, domain, blend, shading, two_sided):
    mat = get_or_create(name, unreal.Material, unreal.MaterialFactoryNew())
    for _ in range(30):
        if lib.get_num_material_expressions(mat) == 0:
            break
        lib.delete_all_material_expressions(mat)
    mat.set_editor_property("material_domain", domain)
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("shading_model", shading)
    mat.set_editor_property("two_sided", two_sided)
    return mat


def node(mat, cls, x, y, **props):
    expr = lib.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def vector_param(mat, x, y, name, default):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=default)


def scalar_param(mat, x, y, name, default):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)


def custom(mat, x, y, code, input_names, output=unreal.CustomMaterialOutputType.CMOT_FLOAT1):
    inputs = []
    for input_name in input_names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", input_name)
        inputs.append(item)
    return node(mat, unreal.MaterialExpressionCustom, x, y, code=code, inputs=inputs, output_type=output, description="DMHolo")


def link(src, dst, input_index):
    names = lib.get_material_expression_input_names(dst)
    if input_index >= len(names) or not lib.connect_material_expressions(src, "", dst, names[input_index]):
        problems.append("link failed into %s input %d" % (dst.get_class().get_name(), input_index))


def prop(src, material_property):
    if not lib.connect_material_property(src, "", material_property):
        problems.append("property connect failed: %s" % material_property)


def finish(mat):
    for err in (lib.recompile_material(mat) or []):
        problems.append("%s: %s" % (mat.get_name(), err))
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log("built " + mat.get_path_name())


def wire(mat, custom_node, inputs):
    for index, src in enumerate(inputs):
        link(src, custom_node, index)


# Shared HLSL: scanlines along world Z by default (ScanAxis), drifting; a rare 1-frame-ish dim flicker; a slow breathe.
HOLO_COMMON = """
float3 P = LWCToFloat(Parameters.AbsoluteWorldPosition);
float3 ax = normalize(ScanAxis.xyz + float3(0.0, 0.0, 1e-4));
float s = frac(dot(P, ax) / max(ScanSpacing, 0.5) - Time * ScanSpeed);
float scan = lerp(1.0, 0.55 + 0.45 * smoothstep(0.0, 0.35, s) * (1.0 - smoothstep(0.55, 1.0, s)), ScanStrength);
float tick = floor(Time * 20.0);
float rnd = frac(sin(tick * 12.9898) * 43758.5453);
float flick = rnd > (1.0 - FlickerChance) ? (1.0 - FlickerDepth) : 1.0;
float breathe = 0.92 + 0.08 * sin(Time * 2.2);
"""

# ---------------------------------------------------------------------------------------------------------------------------------
seg = new_material("M_DM_Holo", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_ADDITIVE, unreal.MaterialShadingModel.MSM_UNLIT, True)
seg_code = HOLO_COMMON + """
float edge = 1.0 + EdgeBoost * Fres;
float3 tint = lerp(Color.rgb, float3(1.0, 1.0, 1.0), Whiten);
return tint * Intensity * saturate(Opacity) * scan * flick * breathe * edge;
"""
s_time = node(seg, unreal.MaterialExpressionTime, -1200, 0)
s_fres = node(seg, unreal.MaterialExpressionFresnel, -1200, 100, exponent=3.0)
s_color = vector_param(seg, -1200, 220, "Color", unreal.LinearColor(0.2, 0.8, 1.0, 1.0))
s_opacity = scalar_param(seg, -1200, 380, "Opacity", 1.0)
s_axis = vector_param(seg, -1200, 480, "ScanAxis", unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
s_spacing = scalar_param(seg, -1200, 640, "ScanSpacing", 6.0)
s_speed = scalar_param(seg, -1200, 740, "ScanSpeed", 0.6)
s_strength = scalar_param(seg, -1200, 840, "ScanStrength", 0.7)
s_fchance = scalar_param(seg, -1200, 940, "FlickerChance", 0.04)
s_fdepth = scalar_param(seg, -1200, 1040, "FlickerDepth", 0.45)
s_edge = scalar_param(seg, -1200, 1140, "EdgeBoost", 1.5)
s_white = scalar_param(seg, -1200, 1240, "Whiten", 0.25)
s_int = scalar_param(seg, -1200, 1340, "Intensity", 2.2)
s_out = custom(seg, -700, 300, seg_code,
               ("Time", "Fres", "Color", "Opacity", "ScanAxis", "ScanSpacing", "ScanSpeed", "ScanStrength", "FlickerChance",
                "FlickerDepth", "EdgeBoost", "Whiten", "Intensity"), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
wire(seg, s_out, (s_time, s_fres, s_color, s_opacity, s_axis, s_spacing, s_speed, s_strength, s_fchance, s_fdepth, s_edge, s_white, s_int))
prop(s_out, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(seg)

# ---------------------------------------------------------------------------------------------------------------------------------
dec = new_material("M_DM_Holo_Decal", unreal.MaterialDomain.MD_DEFERRED_DECAL, unreal.BlendMode.BLEND_TRANSLUCENT,
                   unreal.MaterialShadingModel.MSM_UNLIT, False)
DISC = """
float2 p = UV.xy * 2.0 - 1.0;
float r = length(p);
float ring = 1.0 - smoothstep(RingWidth * 0.5, RingWidth, abs(r - 0.85));
float disc = (1.0 - smoothstep(0.2, 0.85, r)) * DiscAlpha;
float edge = 1.0 - smoothstep(0.93, 1.0, r);
"""
dec_alpha = DISC + HOLO_COMMON + """
return saturate(max(disc, ring) * edge * saturate(Opacity) * scan * flick);
"""
dec_color = DISC + HOLO_COMMON + """
float3 tint = lerp(Color.rgb, float3(1.0, 1.0, 1.0), Whiten);
return tint * Intensity * (ring + 0.35 * disc) * breathe;
"""
d_uv = node(dec, unreal.MaterialExpressionTextureCoordinate, -1200, -100)
d_time = node(dec, unreal.MaterialExpressionTime, -1200, 0)
d_color = vector_param(dec, -1200, 100, "Color", unreal.LinearColor(0.2, 0.8, 1.0, 1.0))
d_opacity = scalar_param(dec, -1200, 260, "Opacity", 1.0)
d_axis = vector_param(dec, -1200, 360, "ScanAxis", unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
d_spacing = scalar_param(dec, -1200, 520, "ScanSpacing", 5.0)
d_speed = scalar_param(dec, -1200, 620, "ScanSpeed", 0.5)
d_strength = scalar_param(dec, -1200, 720, "ScanStrength", 0.6)
d_fchance = scalar_param(dec, -1200, 820, "FlickerChance", 0.04)
d_fdepth = scalar_param(dec, -1200, 920, "FlickerDepth", 0.45)
d_ring = scalar_param(dec, -1200, 1020, "RingWidth", 0.05)
d_disc = scalar_param(dec, -1200, 1120, "DiscAlpha", 0.3)
d_white = scalar_param(dec, -1200, 1220, "Whiten", 0.25)
d_int = scalar_param(dec, -1200, 1320, "Intensity", 2.0)
common_in = ("UV", "Time", "Opacity", "ScanAxis", "ScanSpacing", "ScanSpeed", "ScanStrength", "FlickerChance", "FlickerDepth",
             "RingWidth", "DiscAlpha")
common_src = (d_uv, d_time, d_opacity, d_axis, d_spacing, d_speed, d_strength, d_fchance, d_fdepth, d_ring, d_disc)
d_a = custom(dec, -700, 0, dec_alpha, common_in)
wire(dec, d_a, common_src)
d_c = custom(dec, -700, 400, dec_color, common_in + ("Color", "Whiten", "Intensity"), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
wire(dec, d_c, common_src + (d_color, d_white, d_int))
prop(d_a, unreal.MaterialProperty.MP_OPACITY)
prop(d_c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(dec)

if problems:
    for entry in problems:
        unreal.log_error("GSTN_HOLO PROBLEM " + entry)
    log("FINISHED WITH %d PROBLEMS" % len(problems))
else:
    log("FINISHED OK")
