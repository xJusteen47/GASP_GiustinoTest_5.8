# Land v2 GUI boot (Run-Timed -Preset Capture -Script <copy of this in %TEMP%>): editor to virtual desktop 2 (owner rule), then
# LANDV2_BUILD=1 builds the retimed clip and the chooser row (land_retime_v2_ue.py, saves), then the Play capture (pie_capture_move.py,
# env PIE_CAPTURE_*), which quits the editor. Headless UnrealEditor-Cmd is not used.
import os, runpy, traceback, unreal
_proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
try:
    runpy.run_path(_proj + "Tools/GASNPPTest/vd2_mover.py", run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
if os.environ.get("LANDV2_BUILD") == "1":
    try:
        runpy.run_path(_proj + "Tools/AnimPipeline/cards/jumpland/land_retime_v2_ue.py", run_name="__main__")
    except Exception:
        unreal.log_error("LANDV2 BUILD FAILED: " + traceback.format_exc()[-1500:])
        if os.environ.get("LANDV2_STOP_ON_FAIL", "1") == "1":
            unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
            raise
__file__ = _proj + "Tools/GASNPPTest/pie_capture_move.py"
exec(open(__file__).read())
