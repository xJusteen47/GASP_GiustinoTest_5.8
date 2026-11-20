"""Our pivot clips as a nested chooser PivotSet in the clean GASP base chooser (skid-walkturn-3; plan in skid-walkturn-2 handover).

Shape (Unreal Assistant 2026-11-20, docs/notes/assistant/2026-11-20_pivot*.md; Chooser.cpp fall-through verified):
  Locomotion Transitions (no PoseSearchColumn) row 0 = nested PivotSet (Stance Stand, Gait any, Direction any, MovementMode_Recent
  == None i.e. the Loco case, like the stock '* Loco Transitions' rows). If no PivotSet row passes, evaluation falls through
  to the stock rows below, so every stock pivot is the fallback (order skin -> kit -> stock: future rows go above ours).
  PivotSet columns = copies of Run F Loco Transitions columns (Speed2D, FutureFacingDelta, IsPivoting, MovementDirection_Recent,
  OUT struct, its OWN PoseSearchColumn) + Gait (copied from Locomotion Transitions). Rows: one per (gait, side), cells copied
  from the stock 180 turn row of that side (Run F Loco rows Turn_L/R_180_Lfoot), result replaced by our clip.
Env PIVOTSET = dump (default) | build (recreate PivotSet, row enabled) | on | off (enable/disable the Locomotion Transitions row:
A/B base vs variant). PIVOT_RUN_CLIP, PIVOT_SPRINT_CLIP (empty = no sprint rows). Saves only with GSTN_SAVE=1.
Report Saved/Pivot/pivotset_<mode>.txt, prints max 3 lines.
"""
import os
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
MODE = os.environ.get("PIVOTSET", "dump")
RUN_CLIP = os.environ.get("PIVOT_RUN_CLIP", "/Game/GSTN/Animation/Mixamo/AS_GSTN_RunPivot_WalkTurn180")
SPRINT_CLIP = os.environ.get("PIVOT_SPRINT_CLIP", "")
SAVE = os.environ.get("GSTN_SAVE") == "1" and MODE != "dump"
NAME = "PivotSet"
Lib = unreal.ChooserTableEditorLibrary
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "pivotset_%s.txt" % MODE)
log = []
GAIT = {"Run": "NewEnumerator1", "Sprint": "NewEnumerator2"}


def nested(root, name):
    return next((c for c in Lib.get_nested_choosers(root) if c.get_name() == name), None)


def find_row(table, needle):
    for r in range(Lib.get_num_rows(table)):
        if needle in Lib.get_row_result_property(table, r, "Chooser") or needle in Lib.get_row_result_property(table, r, "Asset"):
            return r
    return -1


def row_by_asset(table, asset_name):
    for r in range(Lib.get_num_rows(table)):
        if asset_name in Lib.get_row_result_property(table, r, "Asset"):
            return r
    return -1


def lt_pivot_row(lt):
    for r in range(Lib.get_num_rows(lt)):
        if (".%s'" % NAME) in Lib.get_row_result_property(lt, r, "Chooser") or ("." + NAME) in Lib.get_row_result_property(lt, r, "Chooser"):
            return r
    return -1


root = unreal.load_asset(CHT)
lt = nested(root, "Locomotion Transitions")
runloco = nested(root, "Run F Loco Transitions")
ps = nested(root, NAME)
log.append("tables: LT rows %d, RunFLoco rows %d, PivotSet %s, LT pivot row %d" % (
    Lib.get_num_rows(lt), Lib.get_num_rows(runloco), "yes" if ps else "no", lt_pivot_row(lt)))

if MODE == "build":
    if ps:
        r = lt_pivot_row(lt)
        if r >= 0:
            Lib.delete_rows(lt, [r])
        Lib.delete_nested_object(root, ps)
        log.append("old PivotSet deleted")
    ps = Lib.create_nested_chooser(lt, -1, NAME)
    r = Lib.get_num_rows(lt) - 1
    Lib.move_row(lt, r, 0)
    for c, v in enumerate(["(Value=NewEnumerator0,Comparison=MatchEqual)", "(Value=NewEnumerator0,Comparison=MatchAny)",
                           "(Value=NewEnumerator0,Comparison=MatchAny)", "(Value=NewEnumerator4,Comparison=MatchEqual)"]):
        if not Lib.set_cell_value(lt, c, 0, v):
            log.append("  LT cell %d not set" % c)
    for c in range(Lib.get_num_columns(runloco)):
        Lib.copy_column(runloco, c, ps, -1)
    gc = Lib.copy_column(lt, 1, ps, 0)  # Gait
    log.append("PivotSet columns: %s" % [Lib.get_column_type(ps, c).split(".")[-1] for c in range(Lib.get_num_columns(ps))])
    plan = [("Run", RUN_CLIP)] + ([("Sprint", SPRINT_CLIP)] if SPRINT_CLIP else [])
    src_rows = {"L": row_by_asset(runloco, "M_Relaxed_Run_Turn_L_180_Lfoot"), "R": row_by_asset(runloco, "M_Relaxed_Run_Turn_R_180_Lfoot")}
    for gait, clip in plan:
        asset = unreal.load_asset(clip)
        obj = asset.get_path_name()
        for side, sr in src_rows.items():
            n = Lib.copy_rows(runloco, [sr], ps, -1, False)
            row = Lib.get_num_rows(ps) - 1
            Lib.set_row_result_property(ps, row, "Asset", obj)
            Lib.set_cell_value(ps, 0, row, "(Value=%s,Comparison=MatchEqual)" % GAIT[gait])
            log.append("  row %d %s %s from RunFLoco row %d" % (row, gait, side, sr))
    Lib.auto_populate_all(ps)
    for row in range(Lib.get_num_rows(ps)):
        cells = [Lib.get_cell_value(ps, c, row)[:140] for c in range(Lib.get_num_columns(ps))]
        log.append("  check row %d %s: %s" % (row, Lib.get_row_result_property(ps, row, "Asset").split(".")[-1], cells))
elif MODE in ("on", "off"):
    r = lt_pivot_row(lt)
    if r < 0:
        log.append("no PivotSet row")
    else:
        Lib.set_row_disabled(lt, r, MODE == "off")
        log.append("LT row %d disabled=%s" % (r, Lib.is_row_disabled(lt, r)))
else:
    for r in range(Lib.get_num_rows(lt))[:3]:
        log.append("  LT row %d %s %s" % (r, Lib.get_row_result_type(lt, r).split(".")[-1], Lib.get_row_result_property(lt, r, "Chooser")[-80:]))
if SAVE:
    log.append("saved %s" % unreal.EditorAssetLibrary.save_loaded_asset(root, False))
log.append("ESITO: OK mode=%s save=%s" % (MODE, SAVE))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:])[:400])
