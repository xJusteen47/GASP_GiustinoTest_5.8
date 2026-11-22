# Boot for a Play capture under Tools/CI/Run-Timed.ps1 -Preset Capture (the editor opens the startup map, L_Test_Hub): moves the editor to virtual
# desktop 2 (owner rule), then runs pie_capture_move.py, which reads PIE_CAPTURE_KIT / PIE_CAPTURE_OUT / PIE_CAPTURE_SEQUENCES / PIE_CAPTURE_BUTTON
# from the environment Run-Timed passes on, and quits the editor when done. Launcher: Tools/GASNPPTest/pie_capture_timed.ps1
import os, runpy
_here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(_here, "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
exec(open(os.path.join(_here, "pie_capture_move.py")).read())
