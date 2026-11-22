"""Run start curve variant (runstart-v6-desktop, owner 2026-11-20): a COPY of a run start clip with the stock GASP start curve set
(curves_author kind start_stock: movedata_speed from the clip's root speed, enable_warping 0 -> 1 over the first 10 %, phase in the
stock triangle convention +1 L contact / -1 R contact). The source clip is never touched (v4r5 is locked, rule 13b).
Then the chooser rows: every row playing RSC_SRC gets a sibling row with RSC_DST (stops_rows_ue.py, RS_ROWS, default new).
Headless via Tools/CI/Save-Assets.ps1. Env: RSC_SRC, RSC_DST (object paths without .Name), RSC_ROWS (new|stock|skip).
Report Saved/Routing/curves_author.txt and Saved/AnimPipeline/rows/<RS_TAG>.txt; prints nothing."""
import os
import runpy

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.environ.get("RSC_SRC", "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r5")
DST = os.environ.get("RSC_DST", "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r5c")
ROWS = os.environ.get("RSC_ROWS", "new")
EAL = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/GSTN"], True)
if not EAL.does_asset_exist(DST):
    assert EAL.duplicate_asset(SRC, DST), "duplicate failed"
    EAL.save_asset(DST, only_if_is_dirty=False)
os.environ["CURVES_CLIPS"] = DST + ":start_stock"
runpy.run_path(os.path.join(ROOT, "Tools", "GAS", "routing", "curves_author.py"), run_name="__main__")
if ROWS in ("new", "stock"):
    os.environ["RS_ROWS"] = ROWS
    os.environ["RS_PAIRS"] = SRC + "=" + DST
    os.environ.setdefault("RS_TAG", "runstart_" + DST.rsplit("_", 1)[-1] + "_" + ROWS)
    runpy.run_path(os.path.join(ROOT, "Tools", "AnimPipeline", "cards", "runstop", "stops_rows_ue.py"), run_name="__main__")
