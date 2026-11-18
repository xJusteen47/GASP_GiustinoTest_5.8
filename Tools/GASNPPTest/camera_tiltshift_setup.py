# Tilt-shift screen band material (owner 2026-11-18: the toy look of Astro Bot / Mario comes from tilt shift). Creates
# /Game/Blueprints/Camera/M_GSTN_TiltShift, the cheap mode of the tilt shift (FGSTN_CameraTiltShiftSettings, Mode = ScreenBand): sharp band
# around the character's height on screen, a 12 tap golden-angle disc blur growing toward the top and bottom of the frame. Optional: without
# it the tilt shift uses Cinematic DOF. Idempotent: an existing material is left as it is.
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GASNPPTest/camera_tiltshift_setup.py" -unattended
# Parameters set every frame by UGSTN_PlaytestCameraModifier: BandCenter (viewport UV y of the character), BandHalfHeight, BandFeather, Radius (px at 1080p).
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
