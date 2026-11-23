"""Dump the Steering, Orientation Warping and Offset Root Bone nodes of the GASP ABPs (turnarounds-3, review v27 r1 item 1, A).
Headless: Tools/CI/Save-Assets.ps1 -Script <this> (saves nothing). Env RSD_ABPS = comma-separated ABP paths (default the stock base and
the main locomotion ABP). Writes Saved/AnimPipeline/runstart/steering_dump.txt: per node its class, graph path, the node struct as
text, and every pin with its links (to see what drives Alpha / targets); prints <= 3 lines."""
import os

import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "steering_dump.txt")
ABPS = os.environ.get("RSD_ABPS", "/Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover").split(",")
KEYS = ("Steering", "OffsetRoot", "OrientationWarping", "BlendStack", "MotionMatching")
lines = []
for p in ABPS:
    abp = unreal.load_asset(p)
    lines.append("== %s %s" % (p, abp))
    for o in unreal.ObjectIterator(unreal.EdGraphNode):
        path = o.get_path_name()
        if not path.startswith(p + ".") and not path.startswith(p + ":"):
            continue
        cls = o.get_class().get_name()
        if not any(k in cls for k in KEYS):
            continue
        lines.append("-- %s  %s" % (cls, path))
        try:
            node = o.get_editor_property("node")
            lines.append("   node: " + node.export_text())
        except Exception as e:  # noqa: BLE001
            lines.append("   node: n/a %r" % e)
        try:
            for pin in o.get_editor_property("pins") if hasattr(o, "pins") else []:
                lines.append("   pin %s" % pin)
        except Exception:
            pass
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("RSD %d lines -> %s" % (len(lines), OUT))
