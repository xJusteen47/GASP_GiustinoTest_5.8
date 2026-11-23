# Tilt (focus) materials (owner 2026-11-18: the toy look of Astro Bot / Mario comes from tilt shift; tilt and shift are independent). Creates
# /Game/Blueprints/Camera/M_GSTN_TiltShift, the cheap screen band (FGSTN_CameraTiltShiftSettings, Mode = ScreenBand): sharp band around the
# character's height on screen, a 12 tap golden-angle disc blur growing toward the top and bottom of the frame; and
# /Game/Blueprints/Camera/M_GSTN_TiltPlane, the real tilt (Mode = TiltedPlane): blur from each pixel's distance to a focal plane rotated
# around the camera's right axis (Scheimpflug), same 12 taps. Shift needs no asset (off-axis projection in C++). Optional: without them the
# tilt uses Cinematic DOF. Idempotent: an existing material is left as it is.
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GASNPPTest/camera_tiltshift_setup.py" -unattended
# Parameters set every frame by UGSTN_PlaytestCameraModifier. Band: BandCenter (viewport UV y of the character), BandHalfHeight, BandFeather,
# Radius (px at 1080p). Plane: PlanePoint, PlaneNormal (camera relative), BlurScale, Radius.
import unreal

DIR = '/Game/Blueprints/Camera'
NAME = 'M_GSTN_TiltShift'
PATH = f'{DIR}/{NAME}'

HLSL = r'''
float d = abs(VUV.y - Center) - HalfHeight;
float m = saturate(d / max(Feather, 0.001));
m = m * m * (3.0 - 2.0 * m);
float r = m * Radius * View.ViewSizeAndInvSize.y / 1080.0;
if (r < 0.5) return Base.rgb;
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 texel = View.BufferSizeAndInvSize.zw;
float3 acc = Base.rgb;
[unroll] for (int i = 0; i < 12; i++)
{
    float a = i * 2.39996323;
    float rr = r * sqrt((i + 0.5) / 12.0);
    acc += SceneTextureLookup(uv + float2(cos(a), sin(a)) * rr * texel, 14, true).rgb;
}
return acc / 13.0;
'''

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log(f'[camera_tiltshift_setup] {PATH} exists, left untouched')
else:
    MEL = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    def scalar(name, value, y):
        e = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', value)
        return e

    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', HLSL)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'TiltShiftBand')
    names = ['VUV', 'Center', 'HalfHeight', 'Feather', 'Radius', 'Base']
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)

    screen = MEL.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -700, -200)
    scene = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -700, 400)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    ok = MEL.connect_material_expressions(screen, 'ViewportUV', custom, 'VUV')
    ok &= MEL.connect_material_expressions(scalar('BandCenter', 0.5, 0), '', custom, 'Center')
    ok &= MEL.connect_material_expressions(scalar('BandHalfHeight', 0.25, 100), '', custom, 'HalfHeight')
    ok &= MEL.connect_material_expressions(scalar('BandFeather', 0.35, 200), '', custom, 'Feather')
    ok &= MEL.connect_material_expressions(scalar('Radius', 3.0, 300), '', custom, 'Radius')
    ok &= MEL.connect_material_expressions(scene, 'Color', custom, 'Base')
    ok &= MEL.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    if ok:
        unreal.log(f'[camera_tiltshift_setup] created {PATH}')
    else:
        unreal.log_error(f'[camera_tiltshift_setup] {PATH} created but a pin did not connect: open it and check the graph')


# ---- Tilted plane (real tilt) ----
PLANE_NAME = 'M_GSTN_TiltPlane'
PLANE_PATH = f'{DIR}/{PLANE_NAME}'
PLANE_HLSL = r'''
float s = max(length(Rel), 1.0);
float d = abs(dot(Rel - PlanePoint.xyz, normalize(PlaneNormal.xyz)));
float r = min(BlurScale * d / s, Radius) * View.ViewSizeAndInvSize.y / 1080.0;
if (r < 0.5) return Base.rgb;
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 texel = View.BufferSizeAndInvSize.zw;
float3 acc = Base.rgb;
[unroll] for (int i = 0; i < 12; i++)
{
    float a = i * 2.39996323;
    float rr = r * sqrt((i + 0.5) / 12.0);
    acc += SceneTextureLookup(uv + float2(cos(a), sin(a)) * rr * texel, 14, true).rgb;
}
return acc / 13.0;
'''

if unreal.EditorAssetLibrary.does_asset_exist(PLANE_PATH):
    unreal.log(f'[camera_tiltshift_setup] {PLANE_PATH} exists, left untouched')
else:
    MEL = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(PLANE_NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    def param(cls, name, value, y):
        e = MEL.create_material_expression(mat, cls, -700, y)
        e.set_editor_property('parameter_name', name)
        e.set_editor_property('default_value', value)
        return e

    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', PLANE_HLSL)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'TiltPlane')
    inputs = []
    for n in ['Rel', 'PlanePoint', 'PlaneNormal', 'BlurScale', 'Radius', 'Base']:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)

    # Camera relative world position of the pixel (reconstructed from depth in a post process material): no large world coordinates in HLSL
    wpos = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -700, -200)
    wpos.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    scene = MEL.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -700, 500)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    ok = MEL.connect_material_expressions(wpos, '', custom, 'Rel')
    ok &= MEL.connect_material_expressions(param(unreal.MaterialExpressionVectorParameter, 'PlanePoint', unreal.LinearColor(400, 0, 0, 0), 0), '', custom, 'PlanePoint')
    ok &= MEL.connect_material_expressions(param(unreal.MaterialExpressionVectorParameter, 'PlaneNormal', unreal.LinearColor(1, 0, 0, 0), 100), '', custom, 'PlaneNormal')
    ok &= MEL.connect_material_expressions(param(unreal.MaterialExpressionScalarParameter, 'BlurScale', 12.0, 200), '', custom, 'BlurScale')
    ok &= MEL.connect_material_expressions(param(unreal.MaterialExpressionScalarParameter, 'Radius', 4.0, 300), '', custom, 'Radius')
    ok &= MEL.connect_material_expressions(scene, 'Color', custom, 'Base')
    ok &= MEL.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    if ok:
        unreal.log(f'[camera_tiltshift_setup] created {PLANE_PATH}')
    else:
        unreal.log_error(f'[camera_tiltshift_setup] {PLANE_PATH} created but a pin did not connect: open it and check the graph')
