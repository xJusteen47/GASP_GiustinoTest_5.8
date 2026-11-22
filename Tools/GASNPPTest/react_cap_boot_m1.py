# Wrapper: move the editor to virtual desktop 2 (owner rule), then run animpipe's standalone_capture.py with the job in env ANIMPIPE_JOB.
import os, runpy
try:
    runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
exec(open(os.environ["ANIMPIPE_BOOT"]).read())
