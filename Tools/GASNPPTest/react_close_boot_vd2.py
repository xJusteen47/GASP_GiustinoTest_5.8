# hit-arms-1: close-up copy of the in-game reaction capture (camera 190 cm, 640 px) for hand review. Boot like react_ingame_boot_vd2.py.
import os, runpy
_proj = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
_tools = os.path.join(_proj, 'Tools', 'GASNPPTest')
try:
    runpy.run_path(os.path.join(_tools, "vd2_mover.py"), run_name="vd2_mover")
except Exception as e:
    print("vd2_mover failed:", e)
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "pie_capture_react_close.py")).read())
