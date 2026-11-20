"""Read-only: sample float curves and root yaw/translation of pivot clips (skid-walkturn-3).

Headless pythonscript commandlet. Env PIVOT_CLIPS = comma separated asset paths. Writes Saved/Pivot/pivot_curves_sample.txt
(11 samples per curve over the clip, root yaw and xy at the same times). Prints 1 line.
"""
import os
import unreal

CLIPS = [p for p in os.environ.get("PIVOT_CLIPS", "").split(",") if p]
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "pivot_curves_sample.txt")
AL = unreal.AnimationLibrary
log = []
for p in CLIPS:
    a = unreal.load_asset(p)
    L = a.get_play_length()
    ts = [L * i / 10.0 for i in range(11)]
    log.append("== %s len=%.3f" % (a.get_name(), L))
    for c in sorted(str(x) for x in AL.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT)):
        kt, kv = AL.get_float_keys(a, c)
        kt, kv = list(kt), list(kv)

        def at(t):
            if not kt:
                return 0.0
            for i in range(1, len(kt)):
                if t <= kt[i]:
                    f = (t - kt[i - 1]) / max(kt[i] - kt[i - 1], 1e-6)
                    return kv[i - 1] + (kv[i] - kv[i - 1]) * max(0.0, min(1.0, f))
            return kv[-1]
        vals = [at(t) for t in ts]
        log.append("  %-20s %s" % (c, " ".join("%.2f" % v for v in vals)))
    roots = []
    for t in ts:
        tr = AL.get_bone_pose_for_time(a, "root", t, False)
        roots.append("(%.0f,%.0f,%.0f)" % (tr.translation.x, tr.translation.y, tr.rotation.rotator().yaw))
    log.append("  root x,y,yaw       " + " ".join(roots))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("pivot_curves_sample: %d lines -> %s" % (len(log), OUT))
