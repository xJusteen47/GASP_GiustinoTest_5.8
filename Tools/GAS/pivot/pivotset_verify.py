"""Read-only check of the PivotSet nested chooser (skid-walkturn-3): Locomotion Transitions row 0, PivotSet pose search cells
(which clip and which internal database they point at), nested objects of the root. Writes Saved/Pivot/pivotset_verify.txt."""
import os
import unreal

Lib = unreal.ChooserTableEditorLibrary
root = unreal.load_asset("/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch")
n = {c.get_name(): c for c in Lib.get_nested_choosers(root)}
lt, ps = n["Locomotion Transitions"], n.get("PivotSet")
log = ["LT row0 type=%s disabled=%s result=%s cells=%s" % (Lib.get_row_result_type(lt, 0), Lib.is_row_disabled(lt, 0),
       Lib.get_row_result_property(lt, 0, "Chooser"), [Lib.get_cell_value(lt, c, 0) for c in range(Lib.get_num_columns(lt))])]
for r in range(Lib.get_num_rows(ps)):
    log.append("PivotSet row %d: %s" % (r, Lib.get_cell_value(ps, Lib.get_num_columns(ps) - 1, r)))
for c in range(Lib.get_num_columns(ps)):
    log.append("PivotSet col %d %s %s" % (c, Lib.get_column_type(ps, c).split(".")[-1], Lib.get_column_property(ps, c, "InputValue")[:200]))
log.append("nested objects: %s" % [o.get_path_name().split(":")[-1] + " (" + o.get_class().get_name() + ")" for o in Lib.get_nested_objects(root)])
out = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "pivotset_verify.txt")
open(out, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("pivotset_verify: %d lines" % len(log))
