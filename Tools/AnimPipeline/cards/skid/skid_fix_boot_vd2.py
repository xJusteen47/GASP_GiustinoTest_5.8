# Skid fix GUI boot (Run-Timed -Preset Capture -Script <copy of this in scratch>): editor to virtual desktop 2 (owner rule), then
# SKIDFIX_BAKE=1 bakes the skid clip's root track into the locomotion DA (skid_turn_curve_bake.py) and
# rewrites the Skid exits (skid_fix_rules.py), both saved, then the Play capture
# (pie_capture_move.py, env PIE_CAPTURE_*), which quits the editor. Headless UnrealEditor-Cmd is not used.
import os, runpy, traceback, unreal
_proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
try:
    runpy.run_path(_proj + "Tools/GASNPPTest/vd2_mover.py", run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
if os.environ.get("SKIDFIX_BAKE") == "1":
    try:
        runpy.run_path(_proj + "Tools/GAS/skid_turn_curve_bake.py", run_name="__main__")
        runpy.run_path(_proj + "Tools/GAS/skid_fix_rules.py", run_name="__main__")
    except Exception:
        unreal.log_error("SKIDFIX BAKE FAILED: " + traceback.format_exc()[-1500:])
        unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
        raise
__file__ = _proj + "Tools/GASNPPTest/pie_capture_move.py"
exec(open(__file__).read())
