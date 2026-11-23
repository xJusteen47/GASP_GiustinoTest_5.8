"""Read-only dump for runstart-phase (2026-11-21): run start rows + run loop rows of CHT_GSTN_StockMover_PoseMatch
(all cells, incl. OUT BlendTime/curve/profile and PoseSearchColumn data), and clip facts (length, sync markers, notifies, curves)
for the run start clips and the loop clips they hand off to. Writes Saved/AnimPipeline/rows/runstart_phase_dump.txt, prints 2 lines."""
import os
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_phase_dump.txt")
L = []


def call(n, *a):
    return TS.call_method(n, tuple(a))


def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in call("GetChooserTable", c).nested_choosers:
            tables(n, seen)
    return seen


clips = set()
for t in tables(unreal.load_asset(CHT), []):
    name = t.get_name()
    if name not in ("Run F Loco Transitions", "Run Loops", "Locomotion Loops") and "Run" not in name:
        continue
    info = call("GetChooserTable", t)
    L.append("== TABLE %s  cols=%d rows=%d" % (name, len(info.columns), len(info.rows)))
    for ci, c in enumerate(info.columns):
        L.append("  col %d %s chain=%s" % (ci, str(c)[:160], [str(x) for x in getattr(c, "property_chain", [])]))
    for ri, r in enumerate(info.rows):
        a = r.asset.get_path_name().split(".")[0] if r.asset else ""
        if name == "Run F Loco Transitions" and "RunStart" not in a and "Run_Start" not in a:
            continue
        clips.add(a)
        L.append("  row %d %s disabled=%s" % (ri, a, r.get_editor_property("bDisabled")))
        for ci, cell in enumerate(r.cells):
            L.append("     c%d %s" % (ci, str(cell)[:600]))

L.append("== CLIPS")
for a in sorted(x for x in clips if x):
    s = unreal.load_asset(a)
    if not isinstance(s, unreal.AnimSequence):
        L.append("%s: %s" % (a, type(s).__name__)); continue
    ln = s.get_play_length()
    names = [str(c.get_editor_property("curve_name")) if hasattr(c, "get_editor_property") else str(c) for c in []]
    try:
        curves = [str(x) for x in unreal.AnimationLibrary.get_animation_curve_names(s, unreal.RawCurveTrackTypes.RCT_FLOAT)]
    except Exception as e:
        curves = ["err %s" % e]
    try:
        markers = [(str(m.marker_name), round(m.time, 3)) for m in s.get_editor_property("authored_sync_markers")]
    except Exception as e:
        markers = ["err %s" % e]
    try:
        nev = [(str(n.notify_name), type(n.notify_state_class).__name__ if n.notify_state_class else (type(n.notify).__name__ if n.notify else ""),
                round(n.get_editor_property("link_value"), 3) if False else None) for n in s.get_editor_property("notifies")]
        nev = [(n.notify_state_class.get_class().get_name() if n.notify_state_class else (n.notify.get_class().get_name() if n.notify else str(n.notify_name)),
                round(n.get_editor_property("trigger_time_offset"), 3)) for n in s.get_editor_property("notifies")]
    except Exception as e:
        nev = ["err %s" % e]
    L.append("%s len=%.3f rootmotion=%s loop=%s" % (a, ln, s.get_editor_property("enable_root_motion"), s.get_editor_property("loop") if hasattr(s, "loop") else "?"))
    L.append("   curves=%s" % curves)
    L.append("   markers=%s" % markers)
    L.append("   notifies=%s" % nev[:20])
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(L) + "\n")
print("ESITO: OK lines %d" % len(L))
print(OUT)
