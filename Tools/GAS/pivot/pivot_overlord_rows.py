"""Point the PivotSet run rows at the Overlord pivot (wc8ksn): R row = the retimed clip, L row = its baked mirror.

PivotSet (pivotset_build.py) has, per gait, one row copied from the stock Turn_L_180 row and one from Turn_R_180. The left clip is
a baked mirror (pivot_mirror_bake.py, MDT_UEFN_Mannequin), both rows UnmirroredOnly like the stock L/R pair: runtime mirroring
(MirroredOnly) would also need the Blend Stack mirrored pin and a schema mirror table (Assistant 2026-11-21). This script also
reports the transitions schema, for the record.
Env PIVOT_CLIP, PIVOT_CLIP_L (assets), PIVOT_ROWS = dump (default) | set. PIVOT_LT = on | off | keep (Locomotion Transitions -> PivotSet row).
Saves only with GSTN_SAVE=1 and PIVOT_ROWS=set. Report Saved/Pivot/overlord_rows_<mode>.txt, prints <= 3 lines.
"""
import os
import re

import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
CLIP = os.environ.get("PIVOT_CLIP", "/Game/GSTN/Animation/Pivot/AS_GSTN_RunPivot_Overlord_R")
CLIP_L = os.environ.get("PIVOT_CLIP_L", "/Game/GSTN/Animation/Pivot/AS_GSTN_RunPivot_Overlord_L")
MODE = os.environ.get("PIVOT_ROWS", "dump")
BLEND = os.environ.get("PIVOT_BLEND", "0.1")  # seconds, empty = keep the copied stock value
LT_MODE = os.environ.get("PIVOT_LT", "keep")
SAVE = os.environ.get("GSTN_SAVE") == "1" and (MODE == "set" or LT_MODE in ("on", "off"))
Lib = unreal.ChooserTableEditorLibrary
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "overlord_rows_%s.txt" % MODE)
log = []


def nested(root, name):
    return next((c for c in Lib.get_nested_choosers(root) if c.get_name() == name), None)


root = unreal.load_asset(CHT)
lt = nested(root, "Locomotion Transitions")
ps = nested(root, "PivotSet")
# mirror table of the transitions database schema
db =unreal.load_object(None, CHT + "." + CHT.split("/")[-1] + ":PSD Locomotion Transitions")
if db:
    sch = db.get_editor_property("schema")
    mdt = None
    for prop in ("mirror_data_table",):
        try:
            mdt = sch.get_editor_property(prop)
        except Exception as e:
            log.append("schema prop %s: %s" % (prop, str(e)[:100]))
    log.append("PSD Locomotion Transitions schema %s mirror table %s" % (sch.get_path_name() if sch else None, mdt.get_path_name() if mdt else None))
else:
    log.append("PSD Locomotion Transitions not loaded")

rows = Lib.get_num_rows(ps)
log.append("PivotSet rows %d" % rows)
if MODE == "set":
    obj = unreal.load_asset(CLIP).get_path_name()
    # rows come in (L, R) pairs per gait; only the Run pair (gait cell NewEnumerator1)
    for r in range(rows):
        gait = Lib.get_cell_value(ps, 0, r)
        if "NewEnumerator1" not in gait:
            continue
        # the FutureFacingDelta cell tells the side: Max=-135 (turning left, stock Turn_L row) or Min=135 (right)
        side = "L" if "Max=-135" in Lib.get_cell_value(ps, 2, r) else "R"
        clip = unreal.load_asset(CLIP if side == "R" else CLIP_L).get_path_name()
        ok = Lib.set_row_result_property(ps, r, "Asset", clip)
        log.append("row %d %s -> %s set=%s" % (r, side, clip.split(".")[-1], ok))
        if BLEND:
            # stock row blends in over 0.3 s: as long as the whole retimed core, so the clip would only reach full weight as the
            # turn ends. Our row only (stock rows keep 0.3).
            cell = Lib.get_cell_value(ps, 5, r)
            new = re.sub(r"(BlendTime_[0-9A-Za-z_]+=)[0-9.]+", lambda m: m.group(1) + "%.6f" % float(BLEND), cell)
            log.append("   blend set=%s %s" % (Lib.set_cell_value(ps, 5, r, new), re.search(r"BlendTime_[^,]+", Lib.get_cell_value(ps, 5, r)).group(0)[-9:]))
if LT_MODE in ("on", "off"):
    for r in range(Lib.get_num_rows(lt)):
        if ".PivotSet" in Lib.get_row_result_property(lt, r, "Chooser"):
            Lib.set_row_disabled(lt, r, LT_MODE == "off")
            log.append("LT row %d disabled=%s" % (r, Lib.is_row_disabled(lt, r)))
for r in range(rows):
    log.append("row %d: %s | %s" % (r, Lib.get_cell_value(ps, 0, r), Lib.get_row_result_property(ps, r, "Data")[:400]
                                     if Lib.get_row_result_property(ps, r, "Data") else ""))
    for c in range(1, Lib.get_num_columns(ps)):
        log.append("   col %d %s" % (c, Lib.get_cell_value(ps, c, r)[:160]))
if SAVE:
    log.append("saved %s" % unreal.EditorAssetLibrary.save_loaded_asset(root, False))
log.append("ESITO: OK mode=%s save=%s" % (MODE, SAVE))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:])[:400])
