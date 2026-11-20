# Run start v3 GUI boot (Run-Timed -Preset Capture -Script <copy of this in %TEMP%>): editor to virtual desktop 2 (owner rule), then
# RSV3_BUILD=1 imports v3 + checks it (runstart_v3_ue.py) and wires the ABP (run_start_states.py RUNSTART_VARIANT=v3 -> route_states.py,
# saves), then the Play capture (pie_capture_move.py, env PIE_CAPTURE_*), which quits the editor. Headless UnrealEditor-Cmd is not used.
import os, runpy, traceback, unreal
_proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
try:
    runpy.run_path(_proj + "Tools/GASNPPTest/vd2_mover.py", run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
if os.environ.get("RSV3_BUILD") == "1":
    try:
        runpy.run_path(_proj + "Tools/AnimPipeline/cards/runstart/runstart_v3_ue.py", run_name="__main__")
        runpy.run_path(_proj + "Tools/GAS/run_start/run_start_states.py", run_name="__main__")
    except Exception:
        unreal.log_error("RSV3 BUILD FAILED: " + traceback.format_exc()[-1500:])
        unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
        raise
if os.environ.get("RSV3_CAPTURE", "1") == "1":
    __file__ = _proj + "Tools/GASNPPTest/pie_capture_move.py"
    exec(open(__file__).read())
else:
    unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
