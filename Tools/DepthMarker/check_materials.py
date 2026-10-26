"""Compile-checks the depth marker toy materials with a real RHI (the commandlet needs -AllowCommandletRendering).

Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<abs path of this file> -unattended -nosplash -nosound -NoP4 -RenderOffscreen -AllowCommandletRendering
recompile_material blocks until the shader map is done; a successful compile reports non-zero pixel shader instructions, a failed one
leaves 0 and logs 'Failed to compile Material' (also scan the log for it). Prints GSTN_MATCHECK lines, ends with RESULT: PASS / FAIL.
"""
import unreal

FOLDER = "/Game/Blueprints/DepthMarker/"
NAMES = ["M_DM_StainedGlass_Decal", "M_DM_Beam", "M_DM_FogPuff", "M_DM_CrystalCharacter", "M_DM_CrackShell"]
lib = unreal.MaterialEditingLibrary
failed = []

for name in NAMES:
    mat = unreal.EditorAssetLibrary.load_asset(FOLDER + name)
    if not mat:
        unreal.log_error("GSTN_MATCHECK %s: not found" % name)
        failed.append(name)
        continue
    lib.recompile_material(mat)
    st = lib.get_statistics(mat)
    ps, vs = st.num_pixel_shader_instructions, st.num_vertex_shader_instructions
    ok = ps > 0 and vs > 0
    unreal.log("GSTN_MATCHECK %s: %s (PS %d, VS %d instructions)" % (name, "OK" if ok else "NOT COMPILED / ERRORS", ps, vs))
    if not ok:
        failed.append(name)

unreal.log("GSTN_MATCHECK RESULT: %s" % ("FAIL " + ",".join(failed) if failed else "PASS"))
