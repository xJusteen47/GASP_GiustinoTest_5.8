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
    # One call leaves about half of the nodes behind (it deletes while iterating): repeat until the graph is empty
    for _ in range(30):
        if lib.get_num_material_expressions(mat) == 0:
            break
        lib.delete_all_material_expressions(mat)
    if lib.get_num_material_expressions(mat) != 0:
        problems.append("%s: %d expressions survived delete_all_material_expressions" % (name, lib.get_num_material_expressions(mat)))
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


def custom(mat, x, y, code, input_names=(), output=unreal.CustomMaterialOutputType.CMOT_FLOAT1):
    inputs = []
    for input_name in input_names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", input_name)
        inputs.append(item)
    return node(mat, unreal.MaterialExpressionCustom, x, y, code=code, inputs=inputs,
                output_type=output, description="DM")


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
# M_DM_StainedGlass_Decal: the landing spot. r = 0 at the centre, r = 1 at the landing radius (the decal half size = the beam base radius).
# A soft dark contact shadow, a stained-glass core (voronoi cells in the player colour, dark leading between them) and a crisp thin ring.
# Softness (0 crisp .. 1 soft) comes from the presenter (soft when high); everything fades to nothing before r = 1, so the box edge never shows.
# ---------------------------------------------------------------------------------------------------------------------------------
decal = new_material("M_DM_StainedGlass_Decal", unreal.MaterialDomain.MD_DEFERRED_DECAL, unreal.BlendMode.BLEND_TRANSLUCENT,
                     unreal.MaterialShadingModel.MSM_DEFAULT_LIT, False)
DECAL_COMMON = """
float2 p = UV.xy * 2.0 - 1.0;
float r = length(p);
float s = saturate(Softness);
float coreR = 0.64;
float core = 1.0 - smoothstep(coreR - 0.05 - 0.10 * s, coreR, r);
float pulse = 1.0 + 0.012 * sin(Time * 3.0);
float ringC = 0.86 * pulse;
float ringW = lerp(0.03, 0.065, s);
float ring = 1.0 - smoothstep(ringW * 0.5, ringW, abs(r - ringC));
float edge = 1.0 - smoothstep(0.93, 1.0, r);
"""
alpha_code = DECAL_COMMON + """
float shadow = (1.0 - smoothstep(lerp(0.45, 0.12, s), 1.0, r));
shadow = shadow * shadow * (3.0 - 2.0 * shadow) * 0.55;
float ringA = ring * lerp(1.0, 0.6, s) * 0.95;
return max(shadow, max(core * 0.85, ringA)) * edge;
"""
glass_code = DECAL_COMMON + """
float2 g = p / coreR * 2.4;
float2 cell = floor(g);
float2 f = frac(g);
float d1 = 8.0;
float d2 = 8.0;
float shade = 0.5;
float hue = 0.0;
[unroll] for (int j = -1; j <= 1; j++)
{
    [unroll] for (int i = -1; i <= 1; i++)
    {
        float2 o = float2(i, j);
        float2 c = cell + o;
        float2 h = frac(sin(float2(dot(c, float2(127.1, 311.7)), dot(c, float2(269.5, 183.3)))) * 43758.5453);
        float2 q = o + h - f;
        float d = dot(q, q);
        if (d < d1) { d2 = d1; d1 = d; shade = frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453); hue = h.x; }
        else if (d < d2) { d2 = d; }
    }
}
float lead = smoothstep(0.02, 0.16, sqrt(d2) - sqrt(d1));
float3 glassTint = lerp(Color.rgb, float3(1.0, 1.0, 1.0), 0.3 * hue);
float breathe = 0.92 + 0.08 * sin(Time * 2.4 + shade * 6.0);
float3 glass = glassTint * (0.55 + 0.75 * shade) * lead * core * breathe * 1.5;
float3 ringCol = lerp(Color.rgb, float3(1.0, 1.0, 1.0), 0.2) * ring * 1.35;
return glass + ringCol;
"""
n_uv = node(decal, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
n_time = node(decal, unreal.MaterialExpressionTime, -1100, 120)
n_soft = scalar_param(decal, -1100, 240, "Softness", 0.2)
n_color = vector_param(decal, -1100, 360, "Color", unreal.LinearColor(1.0, 0.8, 0.2, 1.0))
n_opacity = scalar_param(decal, -1100, 520, "Opacity", 1.0)
n_alpha = custom(decal, -800, -100, alpha_code, ("UV", "Time", "Softness"))
link(n_uv, n_alpha, 0)
link(n_time, n_alpha, 1)
link(n_soft, n_alpha, 2)
n_glass = custom(decal, -800, 150, glass_code, ("UV", "Time", "Softness", "Color"), unreal.CustomMaterialOutputType.CMOT_FLOAT3)
link(n_uv, n_glass, 0)
link(n_time, n_glass, 1)
link(n_soft, n_glass, 2)
link(n_color, n_glass, 3)
# Only on surfaces that face Up (a floor or a gentle slope): the box is thin, but its edge still touches a wall beside the spot; the scene
# normal under each pixel (the GBuffer is there when a deferred decal draws) keeps the decal off walls. UpDirection is set by the presenter.
n_up = vector_param(decal, -1100, 680, "UpDirection", unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
n_scene_normal = node(decal, unreal.MaterialExpressionSceneTexture, -1100, 800, scene_texture_id=unreal.SceneTextureId.PPI_WORLD_NORMAL)
facing_code = """
float3 n = normalize(N.xyz + float3(0.0, 0.0, 1.0e-4));
return smoothstep(0.5, 0.85, saturate(dot(n, normalize(Up.xyz))));
"""
n_facing = custom(decal, -800, 700, facing_code, ("N", "Up"))
link(n_scene_normal, n_facing, 0)
link(n_up, n_facing, 1)
prop(mul(decal, -250, -50, mul(decal, -400, -50, n_alpha, n_opacity), n_facing), unreal.MaterialProperty.MP_OPACITY)
prop(n_glass, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(decal)

# ---------------------------------------------------------------------------------------------------------------------------------
# M_DM_Beam: a fake god ray. Additive unlit, two sided, on the engine Cone (the presenter extends the cone above the character so the radius
# at the character is the wanted top radius; the material cuts it at the character); alpha = axial gradient (fades near the character and before the ground) x fresnel soft edges x a very
# faint stained-glass cell pattern (one light with the decal) x camera distance fade (never a wall of colour when the camera is inside) x
# DepthFade against floor and walls. Colour is desaturated; the opacity (about 0.3) comes from the presenter, the decal is the marker.
# ---------------------------------------------------------------------------------------------------------------------------------
beam = new_material("M_DM_Beam", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_ADDITIVE,
                    unreal.MaterialShadingModel.MSM_UNLIT, True)
axis_code = """
float3 P = LWCToFloat(Parameters.AbsoluteWorldPosition);
float3 S = Start.xyz;
float3 E = End.xyz;
float3 d = E - S;
float len = max(length(d), 1.0);
float3 dir = d / len;
float ds = dot(P - S, dir);
float fadeTop = smoothstep(0.0, min(80.0, len * 0.4), ds);
float fadeBot = 1.0 - smoothstep(len - min(70.0, len * 0.4), len - 3.0, ds);
float shimmer = 0.9 + 0.1 * sin(Time * 2.0 + ds * 0.03);
float3 refv = abs(dir.z) < 0.9 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
float3 ax = normalize(cross(dir, refv));
float3 ay = cross(dir, ax);
float3 q = P - S - dir * ds;
float ang = atan2(dot(q, ay), dot(q, ax));
float2 g = float2(ang * 1.2732, ds * 0.022 - Time * 0.12);
float2 cell = floor(g);
float2 f = frac(g);
float d1 = 8.0;
float d2 = 8.0;
[unroll] for (int j = -1; j <= 1; j++)
{
    [unroll] for (int i = -1; i <= 1; i++)
    {
        float2 o = float2(i, j);
        float2 c = cell + o;
        c.x = c.x - 8.0 * floor(c.x / 8.0);
        float2 h = frac(sin(float2(dot(c, float2(127.1, 311.7)), dot(c, float2(269.5, 183.3)))) * 43758.5453);
        float2 qq = o + h - f;
        float dd = dot(qq, qq);
        if (dd < d1) { d2 = d1; d1 = dd; } else if (dd < d2) { d2 = dd; }
    }
}
float lead = smoothstep(0.0, 0.2, sqrt(d2) - sqrt(d1));
float pattern = 0.9 + 0.1 * lead;
float cam = smoothstep(40.0, 180.0, CamDepth);
return fadeTop * fadeBot * shimmer * pattern * cam;
"""
b_color = vector_param(beam, -1200, -300, "Color", unreal.LinearColor(1.0, 0.9, 0.5, 1.0), 0)
b_opacity = scalar_param(beam, -1200, -150, "Opacity", 1.0, 4)
b_start = vector_param(beam, -1200, 0, "BeamStart", unreal.LinearColor(0, 0, 0, 0), 5)
b_end = vector_param(beam, -1200, 150, "BeamEnd", unreal.LinearColor(0, 0, 100, 0), 9)
b_time = node(beam, unreal.MaterialExpressionTime, -1200, 450)
b_pdepth = node(beam, unreal.MaterialExpressionPixelDepth, -1200, 570)
b_axis = custom(beam, -800, 100, axis_code, ("Start", "End", "Time", "CamDepth"))
link(b_start, b_axis, 0)
link(b_end, b_axis, 1)
link(b_time, b_axis, 2)
link(b_pdepth, b_axis, 3)
b_fresnel = node(beam, unreal.MaterialExpressionFresnel, -800, 300, exponent=2.5)
b_centre = node(beam, unreal.MaterialExpressionOneMinus, -650, 300)
link(b_fresnel, b_centre, 0)
b_shape = mul(beam, -300, 150, mul(beam, -500, 150, b_axis, b_centre), b_opacity)
b_depth = node(beam, unreal.MaterialExpressionDepthFade, 0, 150, fade_distance_default=40.0)
link(b_shape, b_depth, 0)
b_luma = node(beam, unreal.MaterialExpressionDesaturation, -500, -300, luminance_factors=unreal.LinearColor(0.3, 0.59, 0.11, 0.0))
link(b_color, b_luma, 0)
b_fraction = node(beam, unreal.MaterialExpressionConstant, -500, -200, r=0.55)
link(b_fraction, b_luma, 1)
b_gain = node(beam, unreal.MaterialExpressionConstant, -300, -250, r=1.8)
prop(mul(beam, -100, -250, b_luma, b_gain), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(b_depth, unreal.MaterialProperty.MP_OPACITY)
finish(beam)

# ---------------------------------------------------------------------------------------------------------------------------------
# M_DM_FogPuff: unlit translucent quad; drifting fbm noise inside a radial falloff, depth fade against the scene
# ---------------------------------------------------------------------------------------------------------------------------------
puff = new_material("M_DM_FogPuff", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_TRANSLUCENT,
                    unreal.MaterialShadingModel.MSM_UNLIT, True)
puff_code = """
float2 uv = UV.xy * 2.0 - 1.0;
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
p_uv = node(puff, unreal.MaterialExpressionTextureCoordinate, -900, 120)
p_mask = custom(puff, -600, 0, puff_code, ("Time", "UV"))
link(p_time, p_mask, 0)
link(p_uv, p_mask, 1)
p_strength = node(puff, unreal.MaterialExpressionConstant, -450, 150, r=0.75)
p_alpha = mul(puff, -300, 0, mul(puff, -450, 0, p_mask, p_opacity), p_strength)
p_depth = node(puff, unreal.MaterialExpressionDepthFade, -100, 0, fade_distance_default=45.0)
link(p_alpha, p_depth, 0)
p_gain = node(puff, unreal.MaterialExpressionConstant, -300, -250, r=1.8)
prop(mul(puff, -100, -250, p_color, p_gain), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(p_depth, unreal.MaterialProperty.MP_OPACITY)
finish(puff)

# ---------------------------------------------------------------------------------------------------------------------------------
# Character glow: the crystal colour shining through cracks in the shell, only when the light passes through (airborne).
# The marker writes LightThrough (custom primitive data 16) and the player colour (17..20) into the meshes of the owner. MF_DM_CrackGlow is the
# reusable layer: add its Emissive output to the emissive of any character material (CrackMask darkens the base a little along the cracks).
# The cracks are voronoi cell edges in LOCAL space (they stick to the body parts); some cells stay whole; the interior glow is a fake
# subsurface term (stronger where the surface faces the camera: more glass in front of the core). Grounded: Light = 0, nothing glows.
# ---------------------------------------------------------------------------------------------------------------------------------
CPD_LIGHT = 16
CPD_COLOR = 17

CRACK_CODE = """
float3 p = LocalPos * 0.085;
float3 cell = floor(p);
float3 f = frac(p);
float d1 = 8.0;
float d2 = 8.0;
float shade = 0.0;
[unroll] for (int k = -1; k <= 1; k++)
{
    [unroll] for (int j = -1; j <= 1; j++)
    {
        [unroll] for (int i = -1; i <= 1; i++)
        {
            float3 o = float3(i, j, k);
            float3 c = cell + o;
            float3 h = frac(sin(float3(dot(c, float3(127.1, 311.7, 74.7)), dot(c, float3(269.5, 183.3, 246.1)), dot(c, float3(113.5, 271.9, 124.6)))) * 43758.5453);
            float3 q = o + h - f;
            float d = dot(q, q);
            if (d < d1) { d2 = d1; d1 = d; shade = frac(sin(dot(c, float3(12.9898, 78.233, 37.719))) * 43758.5453); }
            else if (d < d2) { d2 = d; }
        }
    }
}
float edge = sqrt(d2) - sqrt(d1);
float crack = (1.0 - smoothstep(0.0, 0.07, edge)) * step(0.25, shade);
float pulse = 0.85 + 0.15 * sin(Time * 2.2 + shade * 6.0);
float interior = saturate(Facing) * Bias;
float glow = Light * (crack * 3.5 + interior * 0.5 * pulse);
return float4(Color.rgb * glow, crack);
"""


def new_function(name):
    func = get_or_create(name, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    for _ in range(30):
        try:
            if lib.get_num_material_expressions_in_function(func) == 0:
                break
        except Exception:
            pass
        lib.delete_all_material_expressions_in_function(func)
    return func


def fnode(func, cls, x, y, **props):
    expr = lib.create_material_expression_in_function(func, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


glow_fn = new_function("MF_DM_CrackGlow")
f_light = fnode(glow_fn, unreal.MaterialExpressionScalarParameter, -900, 0, parameter_name="LightThrough", default_value=0.0,
                use_custom_primitive_data=True, primitive_data_index=CPD_LIGHT)
f_color = fnode(glow_fn, unreal.MaterialExpressionVectorParameter, -900, 150, parameter_name="GlowColor",
                default_value=unreal.LinearColor(1.0, 0.8, 0.2, 1.0), use_custom_primitive_data=True, primitive_data_index=CPD_COLOR)
f_bias = fnode(glow_fn, unreal.MaterialExpressionScalarParameter, -900, 300, parameter_name="InteriorBias", default_value=0.6)
f_time = fnode(glow_fn, unreal.MaterialExpressionTime, -900, 450)
f_local = fnode(glow_fn, unreal.MaterialExpressionLocalPosition, -900, 600)
f_fresnel = fnode(glow_fn, unreal.MaterialExpressionFresnel, -900, 750, exponent=2.0)
f_facing = fnode(glow_fn, unreal.MaterialExpressionOneMinus, -700, 750)
link(f_fresnel, f_facing, 0)
f_code = fnode(glow_fn, unreal.MaterialExpressionCustom, -500, 300, code=CRACK_CODE, description="DMCrack",
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs = []
for input_name in ("LocalPos", "Light", "Color", "Time", "Facing", "Bias"):
    item = unreal.CustomInput()
    item.set_editor_property("input_name", input_name)
    inputs.append(item)
f_code.set_editor_property("inputs", inputs)
for index, src in enumerate((f_local, f_light, f_color, f_time, f_facing, f_bias)):
    link(src, f_code, index)
f_rgb = fnode(glow_fn, unreal.MaterialExpressionComponentMask, -250, 250, r=True, g=True, b=True, a=False)
f_a = fnode(glow_fn, unreal.MaterialExpressionComponentMask, -250, 400, r=False, g=False, b=False, a=True)
link(f_code, f_rgb, 0)
link(f_code, f_a, 0)
f_rgb_color = fnode(glow_fn, unreal.MaterialExpressionComponentMask, -250, 550, r=True, g=True, b=True, a=False)
link(f_color, f_rgb_color, 0)
for index, (name, src) in enumerate((("Emissive", f_rgb), ("CrackMask", f_a), ("Light", f_light), ("Color", f_rgb_color))):
    out = fnode(glow_fn, unreal.MaterialExpressionFunctionOutput, 0, 150 * index, output_name=name)
    link(src, out, 0)
try:
    lib.update_material_function(glow_fn, None)
except Exception as exc:
    problems.append("update_material_function: %s" % exc)
unreal.EditorAssetLibrary.save_loaded_asset(glow_fn)
log("built " + glow_fn.get_path_name())


def call_glow(mat, x, y):
    call = node(mat, unreal.MaterialExpressionMaterialFunctionCall, x, y)
    try:
        call.set_editor_property("material_function", glow_fn)
    except Exception as exc:
        problems.append("function call: %s" % exc)
    return call


def link_out(src, output, dst, input_index):
    names = lib.get_material_expression_input_names(dst)
    if input_index >= len(names) or not lib.connect_material_expressions(src, output, dst, names[input_index]):
        problems.append("link %s failed into %s input %d" % (output, dst.get_class().get_name(), input_index))


def prop_out(src, output, material_property):
    if not lib.connect_material_property(src, output, material_property):
        problems.append("property connect failed: %s %s" % (output, material_property))


# M_DM_CrystalCharacter: the naked crystal man. Dark glassy body in the player colour, rim light always, cracks only dark lines on the ground;
# in the air the cracks and the interior glow light up.
crystal = new_material("M_DM_CrystalCharacter", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_OPAQUE,
                       unreal.MaterialShadingModel.MSM_DEFAULT_LIT, False)
c_call = call_glow(crystal, -800, 0)
c_fresnel = node(crystal, unreal.MaterialExpressionFresnel, -800, 300, exponent=3.0)
c_rim_gain = node(crystal, unreal.MaterialExpressionConstant, -800, 450, r=0.45)
c_color_scale = node(crystal, unreal.MaterialExpressionConstant, -800, 600, r=0.3)
c_base = node(crystal, unreal.MaterialExpressionMultiply, -300, 600)
link_out(c_call, "Color", c_base, 0)
link(c_color_scale, c_base, 1)
c_dark = node(crystal, unreal.MaterialExpressionConstant, -300, 720, r=0.6)
c_crack_dark = node(crystal, unreal.MaterialExpressionOneMinus, -300, 800)
c_crack_scale = node(crystal, unreal.MaterialExpressionMultiply, -500, 800)
link_out(c_call, "CrackMask", c_crack_scale, 0)
link(c_dark, c_crack_scale, 1)
link(c_crack_scale, c_crack_dark, 0)
prop(mul(crystal, -100, 650, c_base, c_crack_dark), unreal.MaterialProperty.MP_BASE_COLOR)
c_rim_color = node(crystal, unreal.MaterialExpressionMultiply, -500, 450)
link(c_fresnel, c_rim_color, 0)
link_out(c_call, "Color", c_rim_color, 1)
c_rim_scaled = mul(crystal, -300, 450, c_rim_color, c_rim_gain)
c_emissive = node(crystal, unreal.MaterialExpressionAdd, -100, 300)
link_out(c_call, "Emissive", c_emissive, 0)
link(c_rim_scaled, c_emissive, 1)
prop(c_emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(node(crystal, unreal.MaterialExpressionConstant, -100, 900, r=0.08), unreal.MaterialProperty.MP_ROUGHNESS)
prop(node(crystal, unreal.MaterialExpressionConstant, -100, 980, r=0.9), unreal.MaterialProperty.MP_SPECULAR)
finish(crystal)

# M_DM_CrackShell: a plain opaque shell (the mannequin look) with the crack glow layer on top: what a normal character material does with MF_DM_CrackGlow
shell = new_material("M_DM_CrackShell", unreal.MaterialDomain.MD_SURFACE, unreal.BlendMode.BLEND_OPAQUE,
                     unreal.MaterialShadingModel.MSM_DEFAULT_LIT, False)
s_call = call_glow(shell, -800, 0)
s_base = node(shell, unreal.MaterialExpressionConstant3Vector, -800, 300, constant=unreal.LinearColor(0.72, 0.75, 0.82, 1.0))
s_dark = node(shell, unreal.MaterialExpressionConstant, -800, 450, r=0.5)
s_crack = node(shell, unreal.MaterialExpressionMultiply, -600, 450)
link_out(s_call, "CrackMask", s_crack, 0)
link(s_dark, s_crack, 1)
s_inv = node(shell, unreal.MaterialExpressionOneMinus, -450, 450)
link(s_crack, s_inv, 0)
prop(mul(shell, -200, 350, s_base, s_inv), unreal.MaterialProperty.MP_BASE_COLOR)
prop_out(s_call, "Emissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(node(shell, unreal.MaterialExpressionConstant, -200, 600, r=0.55), unreal.MaterialProperty.MP_ROUGHNESS)
finish(shell)

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
