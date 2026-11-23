"""Read-only dump of the pivot rows of the clean GASP base chooser (skid-walkturn-2).

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -nullrhi. Writes Saved/Pivot/pivot_rows_dump.txt:
columns of every table whose name matches PIVOT_TABLES (default Run F / Sprint Loco Transitions + their Mode Transitions),
and every row with its cells, result and disabled flag. Prints max 5 lines.
"""
import os
import unreal

CHT = os.environ.get("PIVOT_CHT", "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch")
NAMES = [n.strip() for n in os.environ.get("PIVOT_TABLES", "Run F Loco Transitions,Sprint Loco Transitions,Locomotion Transitions").split(",")]
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "pivot_rows_dump.txt")


def call(name, *args):
    return TS.call_method(name, tuple(args))


def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in call("GetChooserTable", c).nested_choosers:
            tables(n, seen)
    return seen


log = []
root = unreal.load_asset(CHT)
log.append("root %s parameters=%s" % (CHT, [str(p)[:200] for p in call("GetChooserTable", root).parameters]))
for t in tables(root, []):
    name = t.get_name()
    if not any(name.replace("_", " ").startswith(n) or name == n for n in NAMES):
        continue
    info = call("GetChooserTable", t)
    log.append("== table %s (%s) rows=%d fallback=%s" % (name, t.get_path_name(), len(info.rows), info.fallback[:200]))
    for c, col in enumerate(info.columns):
        log.append("  col %d %s input=%s param=%d chain=%s disabled=%s" % (c, col.type.split(".")[-1], col.input_type.split(".")[-1],
                                                                           col.parameter_index, list(col.property_chain), col.disabled))
    for i, r in enumerate(info.rows):
        a = r.asset.get_name() if r.asset else "-"
        log.append("  row %d %s %s disabled=%s" % (i, r.result_type.split(".")[-1], a, r.get_editor_property("bDisabled")))
        for c, cell in enumerate(r.cells):
            log.append("     c%d: %s" % (c, str(cell)[:300]))
        if not r.asset:
            log.append("     result: %s" % str(r.result)[:300])
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("pivot_rows_dump: %d lines -> %s" % (len(log), OUT))

# Clip settings (skid-walkturn-3): root motion flags, curve names and Pose Search notify states of every asset in the dumped
# tables plus our candidate clips, so a new pivot clip can copy the stock set (gasp-structure-5.9.md sections 7-8).
EXTRA = [p for p in os.environ.get("PIVOT_CLIPS", "/Game/GSTN/Animation/Mixamo/AS_GSTN_RunPivot_WalkTurn180").split(",") if p]
clog = []
seen_assets = []
for t in tables(root, []):
    name = t.get_name()
    if not any(name.replace("_", " ").startswith(n) or name == n for n in NAMES):
        continue
    for r in call("GetChooserTable", t).rows:
        if r.asset and r.asset not in seen_assets:
            seen_assets.append(r.asset)
for p in EXTRA:
    a = unreal.load_asset(p)
    if a and a not in seen_assets:
        seen_assets.append(a)
for a in seen_assets:
    if not isinstance(a, unreal.AnimSequence):
        clog.append("asset %s (%s) not a sequence" % (a.get_name(), a.get_class().get_name()))
        continue
    curves = sorted(str(c) for c in unreal.AnimationLibrary.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT))
    try:
        evs = unreal.AnimationLibrary.get_animation_notify_events(a)
        ns = sorted(set((e.notify_state_class.get_class().get_name() if e.notify_state_class else
                         e.notify.get_class().get_name() if e.notify else "?") for e in evs))
    except Exception as ex:  # notify read is a courtesy
        ns = ["? " + str(ex)[:80]]
    clog.append("clip %s len=%.3f rm=%s lock=%s lockmode=%s curves=%s notifies=%s" % (
        a.get_name(), a.get_play_length(), a.get_editor_property("enable_root_motion"), a.get_editor_property("force_root_lock"),
        a.get_editor_property("root_motion_root_lock"), curves, ns))
COUT = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "pivot_clips_dump.txt")
open(COUT, "w", encoding="utf-8").write("\n".join(clog) + "\n")
print("pivot_clips_dump: %d clips -> %s" % (len(clog), COUT))
