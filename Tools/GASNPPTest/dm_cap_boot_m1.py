# Boot for the depth marker PIE capture: editor to virtual desktop 2 (owner rule), then pie_capture_move.py (env PIE_CAPTURE_*).
import os, runpy
here = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(here, "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
exec(open(os.path.join(here, "pie_capture_move.py")).read())
