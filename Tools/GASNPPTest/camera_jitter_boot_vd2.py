# Boot for camera_jitter_run.py under Run-Timed -Preset Capture: editor to virtual desktop 2 (owner rule), then the traced Play run.
import os, runpy
_here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(_here, "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
exec(open(os.path.join(_here, "camera_jitter_run.py")).read())
