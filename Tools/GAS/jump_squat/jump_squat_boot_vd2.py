# GUI-editor boot (Run-Timed -Preset Capture -Script <copy of this in %TEMP%>): editor to virtual desktop 2 (owner rule), build the
# Jump Squat state (jump_squat_states.py, which also runs route_states.py), then quit. Headless UnrealEditor-Cmd is not used.
import os, runpy, unreal
_proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
try:
    runpy.run_path(_proj + "Tools/GASNPPTest/vd2_mover.py", run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
os.environ["JUMPSQUAT_QUIT"] = "1"
runpy.run_path(_proj + "Tools/GAS/jump_squat/jump_squat_states.py", run_name="__main__")
