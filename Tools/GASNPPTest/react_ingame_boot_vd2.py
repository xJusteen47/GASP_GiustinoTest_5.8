# Boot for the in-game reaction capture under Tools/CI/Run-Timed.ps1 -Preset Capture: moves the editor to virtual desktop 2 (owner rule),
# then runs pie_capture_react_ingame.py (reads PIE_CAPTURE_OUT / PIE_CAPTURE_SEQUENCES / PIE_CAPTURE_CVARS from the environment).
import os, runpy
_here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(_here, "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
exec(open(os.path.join(_here, "pie_capture_react_ingame.py")).read())
