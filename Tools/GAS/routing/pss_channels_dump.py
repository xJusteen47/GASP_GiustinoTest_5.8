"""Dump Pose Search schema channels (type, weight, curve name, sample offsets) for the loop schemas and the start schema
(runstart-v6, coordinator check on the phase curve). Out Saved/Routing/pss_channels.txt. Headless."""
import unreal
OUT = unreal.Paths.project_saved_dir() + "Routing/pss_channels.txt"
SCH = ["/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Schemas/PSS_Relaxed_Loops",
       "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Schemas/PSS_Default_Mover",
       "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Schemas/PSS_Relaxed_Starts"]
log = []
def props(o, depth):
    pad = "  " * depth
    names = [p for p in ("weight", "curve_name", "sample_time_offset", "sample_role", "input_query_pose", "component_stripping",
                         "bone", "use_feature", "bones", "sampled_bones", "sample_times", "channels", "sub_channels") ]
    log.append(pad + o.get_class().get_name())
    for n in names:
        try:
            v = o.get_editor_property(n)
        except Exception:
            continue
        if n in ("channels", "sub_channels"):
            for c in v:
                props(c, depth + 1)
        else:
            log.append(pad + "  " + n + " = " + str(v)[:200])
for p in SCH:
    s = unreal.load_asset(p)
    log.append("== " + p.rsplit("/", 1)[-1] + (" missing" if not s else ""))
    if not s:
        continue
    for c in s.get_editor_property("channels"):
        props(c, 1)
open(OUT, "w").write("\n".join(log))
