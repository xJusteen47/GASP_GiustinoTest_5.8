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
