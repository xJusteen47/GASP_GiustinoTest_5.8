"""Builds the prototype toy look of the depth marker: three materials and two style assets in /Game/Blueprints/DepthMarker.

Idempotent: existing assets are rebuilt in place (materials: expressions deleted and recreated; styles: presenters repopulated).
Headless:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<abs path of this file> -unattended -nullrhi -NoP4
Only run it while the owner's editor is closed. Content of /Game/Blueprints is versioned through Git LFS.

Custom primitive data layout (must match FGSTN_DepthToy in GSTN_DepthMarkerToy.h and the setting ToyCustomPrimitiveDataBase = 0):
  0..3 colour (float4)   4 opacity   5..8 beam start (world, float4)   9..12 beam end (world, float4)
The decal cannot read custom primitive data (a decal component is not a primitive): it keeps plain Color / Opacity parameters.
"""
import unreal

FOLDER = "/Game/Blueprints/DepthMarker"
lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
problems = []


def log(msg):
    unreal.log("GSTN_TOY " + msg)


def get_or_create(name, asset_class, factory):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return tools.create_asset(name, FOLDER, asset_class, factory)


def new_material(name, domain, blend, shading, two_sided):
    mat = get_or_create(name, unreal.Material, unreal.MaterialFactoryNew())
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


def vector_param(mat, x, y, name, default, cpd_index=None):
    props = dict(parameter_name=name, default_value=default)
    if cpd_index is not None:
        props.update(use_custom_primitive_data=True, primitive_data_index=cpd_index)
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, **props)


def scalar_param(mat, x, y, name, default, cpd_index=None):
    props = dict(parameter_name=name, default_value=default)
    if cpd_index is not None:
        props.update(use_custom_primitive_data=True, primitive_data_index=cpd_index)
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, **props)


def custom(mat, x, y, code, input_names=()):
    inputs = []
    for input_name in input_names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", input_name)
        inputs.append(item)
    return node(mat, unreal.MaterialExpressionCustom, x, y, code=code, inputs=inputs,
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="DM")


def link(src, dst, input_index, output=""):
    names = lib.get_material_expression_input_names(dst)
    if input_index >= len(names) or not lib.connect_material_expressions(src, output, dst, names[input_index]):
        problems.append("link failed into %s input %d (inputs: %s)" % (dst.get_class().get_name(), input_index, list(names)))


def mul(mat, x, y, a, b):
    m = node(mat, unreal.MaterialExpressionMultiply, x, y)
    link(a, m, 0)
    link(b, m, 1)
    return m


def prop(src, material_property):
    if not lib.connect_material_property(src, "", material_property):
        problems.append("property connect failed: %s" % material_property)


def finish(mat):
    errors = lib.recompile_material(mat)
    for err in (errors or []):
        problems.append("%s: %s" % (mat.get_name(), err))
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log("built " + mat.get_path_name())


# ---------------------------------------------------------------------------------------------------------------------------------
# M_DM_StainedGlass_Decal: dark soft silhouette with a coloured stained-glass core (voronoi cells with dark leading)
# ---------------------------------------------------------------------------------------------------------------------------------
decal = new_material("M_DM_StainedGlass_Decal", unreal.MaterialDomain.MD_DEFERRED_DECAL, unreal.BlendMode.BLEND_TRANSLUCENT,
                     unreal.MaterialShadingModel.MSM_DEFAULT_LIT, False)
alpha_code = """
float2 p = Parameters.TexCoords[0].xy * 2.0 - 1.0;
float r = length(p);
float shadow = 1.0 - smoothstep(0.55, 1.0, r);
float core = 1.0 - smoothstep(0.40, 0.48, r);
return max(shadow * 0.55, core * 0.92);
"""
glass_code = """
float2 p = Parameters.TexCoords[0].xy * 2.0 - 1.0;
float r = length(p);
float core = 1.0 - smoothstep(0.34, 0.44, r);
float2 g = p * 3.2;
float2 cell = floor(g);
float2 f = frac(g);
float d1 = 8.0;
float d2 = 8.0;
float shade = 0.5;
[unroll] for (int j = -1; j <= 1; j++)
{
    [unroll] for (int i = -1; i <= 1; i++)
    {
        float2 o = float2(i, j);
        float2 c = cell + o;
        float2 h = frac(sin(float2(dot(c, float2(127.1, 311.7)), dot(c, float2(269.5, 183.3)))) * 43758.5453);
        float2 q = o + h - f;
        float d = dot(q, q);
        if (d < d1) { d2 = d1; d1 = d; shade = frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453); }
        else if (d < d2) { d2 = d; }
    }
}
float lead = smoothstep(0.0, 0.14, sqrt(d2) - sqrt(d1));
return core * lead * (0.5 + 0.5 * shade);
"""
n_alpha = custom(decal, -700, -100, alpha_code)
n_glass = custom(decal, -700, 100, glass_code)
n_color = vector_param(decal, -700, 300, "Color", unreal.LinearColor(1.0, 0.8, 0.2, 1.0))
n_opacity = scalar_param(decal, -700, 450, "Opacity", 1.0)
n_gain = node(decal, unreal.MaterialExpressionConstant, -500, 520, r=3.0)
prop(mul(decal, -300, -50, n_alpha, n_opacity), unreal.MaterialProperty.MP_OPACITY)
prop(mul(decal, -100, 150, mul(decal, -300, 200, n_color, n_glass), n_gain), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(decal)

# ---------------------------------------------------------------------------------------------------------------------------------
# M_DM_Beam: additive unlit light shaft; soft silhouette (fresnel), soft at the character and a little at the landing, depth fade
# ---------------------------------------------------------------------------------------------------------------------------------
beam = new_material("M_DM_Beam", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_ADDITIVE,
                    unreal.MaterialShadingModel.MSM_UNLIT, True)
axis_code = """
float3 P = LWCToFloat(Parameters.AbsoluteWorldPosition);
float3 S = Start.xyz;
float3 E = End.xyz;
float3 d = E - S;
float len2 = max(dot(d, d), 1.0);
float t = saturate(dot(P - S, d) / len2);
float tip = smoothstep(0.0, 0.3, t);
float landing = 1.0 - 0.5 * smoothstep(0.85, 1.0, t);
float shimmer = 0.85 + 0.15 * sin(Time * 3.0 + t * 14.0);
return tip * landing * shimmer;
"""
b_color = vector_param(beam, -900, -300, "Color", unreal.LinearColor(1.0, 0.9, 0.5, 1.0), 0)
b_opacity = scalar_param(beam, -900, -150, "Opacity", 1.0, 4)
b_start = vector_param(beam, -900, 0, "BeamStart", unreal.LinearColor(0, 0, 0, 0), 5)
b_end = vector_param(beam, -900, 150, "BeamEnd", unreal.LinearColor(0, 0, 100, 0), 9)
b_time = node(beam, unreal.MaterialExpressionTime, -900, 300)
b_axis = custom(beam, -600, 100, axis_code, ("Start", "End", "Time"))
link(b_start, b_axis, 0)
link(b_end, b_axis, 1)
link(b_time, b_axis, 2)
b_fresnel = node(beam, unreal.MaterialExpressionFresnel, -600, 300, exponent=2.2)
b_centre = node(beam, unreal.MaterialExpressionOneMinus, -450, 300)
link(b_fresnel, b_centre, 0)
b_strength = node(beam, unreal.MaterialExpressionConstant, -450, 420, r=0.7)
b_shape = mul(beam, -300, 150, mul(beam, -450, 150, b_axis, b_centre), b_opacity)
b_shaped = mul(beam, -150, 150, b_shape, b_strength)
b_depth = node(beam, unreal.MaterialExpressionDepthFade, 0, 150, fade_distance_default=35.0)
link(b_shaped, b_depth, 0)
b_gain = node(beam, unreal.MaterialExpressionConstant, -300, -250, r=2.5)
prop(mul(beam, -100, -250, b_color, b_gain), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(b_depth, unreal.MaterialProperty.MP_OPACITY)
finish(beam)

# ---------------------------------------------------------------------------------------------------------------------------------
# M_DM_FogPuff: unlit translucent quad; drifting fbm noise inside a radial falloff, depth fade against the scene
# ---------------------------------------------------------------------------------------------------------------------------------
puff = new_material("M_DM_FogPuff", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_TRANSLUCENT,
                    unreal.MaterialShadingModel.MSM_UNLIT, True)
puff_code = """
float2 uv = Parameters.TexCoords[0].xy * 2.0 - 1.0;
float r = length(uv);
float t = Time * 0.12;
float2 q = uv * 1.8 + float2(t, -0.7 * t);
float n = 0.0;
float a = 0.5;
[unroll] for (int o = 0; o < 3; o++)
{
    float2 i = floor(q);
    float2 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float h00 = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float h10 = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float h01 = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float h11 = frac(sin(dot(i + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    n += a * lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
    q = q * 2.0 + 3.7;
    a *= 0.5;
}
float radial = 1.0 - smoothstep(0.1, 1.0, r);
return saturate(radial * smoothstep(0.2, 0.8, n + radial * 0.4));
"""
p_color = vector_param(puff, -900, -300, "Color", unreal.LinearColor(1.0, 0.9, 0.5, 1.0), 0)
p_opacity = scalar_param(puff, -900, -150, "Opacity", 1.0, 4)
p_time = node(puff, unreal.MaterialExpressionTime, -900, 0)
p_mask = custom(puff, -600, 0, puff_code, ("Time",))
link(p_time, p_mask, 0)
p_strength = node(puff, unreal.MaterialExpressionConstant, -450, 150, r=0.75)
p_alpha = mul(puff, -300, 0, mul(puff, -450, 0, p_mask, p_opacity), p_strength)
p_depth = node(puff, unreal.MaterialExpressionDepthFade, -100, 0, fade_distance_default=45.0)
link(p_alpha, p_depth, 0)
p_gain = node(puff, unreal.MaterialExpressionConstant, -300, -250, r=1.8)
prop(mul(puff, -100, -250, p_color, p_gain), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(p_depth, unreal.MaterialProperty.MP_OPACITY)
finish(puff)

# ---------------------------------------------------------------------------------------------------------------------------------
# Styles: DMS_Toy (below / above layers) and DMS_Toy_Approach (approach marker), named in the project ini
# ---------------------------------------------------------------------------------------------------------------------------------
decal_mat = unreal.EditorAssetLibrary.load_asset(FOLDER + "/M_DM_StainedGlass_Decal")
beam_mat = unreal.EditorAssetLibrary.load_asset(FOLDER + "/M_DM_Beam")
puff_mat = unreal.EditorAssetLibrary.load_asset(FOLDER + "/M_DM_FogPuff")


def assign_materials(style):
    for presenter in style.get_editor_property("presenters"):
        if isinstance(presenter, unreal.GSTN_DepthPresenter_Decal):
            presenter.set_editor_property("material_override", decal_mat)
        elif isinstance(presenter, unreal.GSTN_DepthPresenter_Cone):
            presenter.set_editor_property("material_override", beam_mat)
        elif isinstance(presenter, unreal.GSTN_DepthPresenter_FogPuff):
            presenter.set_editor_property("material_override", puff_mat)


approach = get_or_create("DMS_Toy_Approach", unreal.GSTN_DepthMarkerStyle, unreal.DataAssetFactory())
approach.populate_toy_approach_presenters()
assign_materials(approach)
unreal.EditorAssetLibrary.save_loaded_asset(approach)

toy = get_or_create("DMS_Toy", unreal.GSTN_DepthMarkerStyle, unreal.DataAssetFactory())
toy.populate_toy_presenters()
assign_materials(toy)
toy.set_editor_property("approach_style", approach)
unreal.EditorAssetLibrary.save_loaded_asset(toy)

cone = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cone")
if cone:
    bounds = cone.get_bounds()
    log("engine cone bounds origin=%s extent=%s" % (bounds.origin, bounds.box_extent))

if problems:
    for entry in problems:
        unreal.log_error("GSTN_TOY PROBLEM " + entry)
    log("FINISHED WITH %d PROBLEMS" % len(problems))
else:
    log("FINISHED OK")
