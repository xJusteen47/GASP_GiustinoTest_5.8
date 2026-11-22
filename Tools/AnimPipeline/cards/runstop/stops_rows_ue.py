"""Pose-matched rows for restyled locomotion clips in the clean GASP base chooser (headless UE, run via Tools/CI/Save-Assets.ps1).

GASP stock way (docs/knowledge/gasp/gasp-structure-5.9.md section 10.1, Unreal Assistant): a variant of a locomotion event is a
ROW of the matching leaf table (Stand Stops for stops), copying the neighbour's cells, so it keeps the pose-matched start
(PoseSearchColumn) and the stock exit rules; the mocap row it replaces is disabled, never deleted.

For every row of CHT_GSTN_StockMover_PoseMatch (root + nested) whose result is a SOURCE clip of RS_PAIRS, this script keeps one
sibling row right after it with the same cells and the restyled clip (added once, found again by asset on later runs).
  RS_ROWS=dump   report only (default)
  RS_ROWS=new    restyled rows enabled, source rows disabled
  RS_ROWS=stock  source rows enabled, restyled rows disabled (A/B and fallback C)
RS_PAIRS: "src_path=dst_path;src_path=dst_path" (object paths without the .Name suffix). Saves only with GSTN_SAVE=1.
Report: Saved/AnimPipeline/rows/<RS_TAG or 'rows'>.txt ; prints max 5 lines.
"""
import os
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
MODE = os.environ.get("RS_ROWS", "dump")
TAG = os.environ.get("RS_TAG", "rows")
PAIRS = {}
for part in os.environ.get("RS_PAIRS", "").split(";"):
    if "=" in part:
        a, b = part.split("=", 1)
        PAIRS[a.strip()] = b.strip()
SAVE = os.environ.get("GSTN_SAVE") == "1" and MODE in ("new", "stock")
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", TAG + ".txt")
log = []


def call(name, *args):
    return TS.call_method(name, tuple(args))


def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in call("GetChooserTable", c).nested_choosers:
            tables(n, seen)
    return seen


def pkg(path):
    return path.split(".")[0]


def row_asset(row):
    """Package path of the asset a row returns, or ''."""
    a = row.asset
    return pkg(a.get_path_name()) if a else ""


root = unreal.load_asset(CHT)
changed = False
for t in tables(root, []):
    info = call("GetChooserTable", t)
    rows = list(info.rows)
    srcs = [(i, row_asset(r)) for i, r in enumerate(rows) if row_asset(r) in PAIRS]
    if not srcs:
        continue
    log.append("table %s rows %d columns %d" % (t.get_name(), len(rows), len(info.columns)))
    for i, src in reversed(srcs):
        dst = PAIRS[src]
        nxt = rows[i + 1] if i + 1 < len(rows) else None
        has = nxt is not None and row_asset(nxt) == dst
        log.append("  row %d %s disabled=%s -> sibling %s" % (i, src.split("/")[-1], rows[i].get_editor_property("bDisabled"),
                                                              "present" if has else "missing"))
        if MODE == "dump":
            for c, cell in enumerate(rows[i].cells):
                log.append("    cell %d: %s" % (c, str(cell)[:160]))
            log.append("    result: %s" % str(rows[i].result)[:200])
            continue
        if not has:
            dst_obj = unreal.load_asset(dst)
            n = call("AddChooserRow", t, rows[i].result_type, i + 1, dst_obj)
            for c, cell in enumerate(rows[i].cells):
                val = str(cell).replace(src + "." + src.split("/")[-1], dst + "." + dst.split("/")[-1])
                try:
                    call("SetChooserCell", t, c, n, val)
                except Exception as e:  # output-less or read-only cells: keep the default, report
                    log.append("    cell %d not copied: %s" % (c, str(e)[:120]))
            log.append("    added row %d (%s)" % (n, dst.split("/")[-1]))
        call("SetChooserRowDisabled", t, i, MODE == "new")
        call("SetChooserRowDisabled", t, i + 1, MODE == "stock")
        changed = True
    if MODE != "dump":
        rows2 = list(call("GetChooserTable", t).rows)
        for i, r in enumerate(rows2):
            a = row_asset(r)
            if a in PAIRS.values() and i > 0:
                s = rows2[i - 1]
                diff = [c for c, (x, y) in enumerate(zip(r.cells, s.cells)) if str(x).replace(a, "#") != str(y).replace(row_asset(s), "#")]
                log.append("  check row %d %s disabled=%s, source row disabled=%s, cells differing=%s" % (
                    i, a.split("/")[-1], r.get_editor_property("bDisabled"), s.get_editor_property("bDisabled"), diff))

if SAVE and changed:
    unreal.EditorAssetLibrary.save_loaded_asset(root, False)
    log.append("saved " + CHT)
log.append("ESITO: %s mode=%s pairs=%d" % ("OK" if (MODE == "dump" or changed) else "NOTHING", MODE, len(PAIRS)))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:]))
