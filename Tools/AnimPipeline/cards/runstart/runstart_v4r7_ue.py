"""Run start v4r7 into UE (runstart-v7-desktop, 2026-11-20): headless through Tools/CI/Save-Assets.ps1 (GSTN_SAVE=1).
1. runstart_v4_ue.py on RunStart_v4r7.fbx -> AS_GSTN_RunStart_v4r7 (same exit 1.0667 s, block transition, curves as v4r6).
2. Curves: the same curve set the active v4r6 carries (if v4r6 has enable_warping, curves_author kind start; else the import's set).
3. Rows: every chooser row playing v4r6 gets a v4r7 sibling, v4r6 disabled (stops_rows_ue.py RS_ROWS=new; v4r6 stays as a row).
4. Sync markers (read only, owner question): markers of v4r6/v4r7, the stock run loop and the stock run starts.
Report Saved/AnimPipeline/runstart/v4r7_ue.json; prints nothing."""
import json
import os
import runpy

import unreal

AL = unreal.AnimationLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FOLDER = "/Game/GSTN/Animation/RunStart"
V6, V7 = FOLDER + "/AS_GSTN_RunStart_v4r6", FOLDER + "/AS_GSTN_RunStart_v4r7"
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "v4r7_ue.json")
rep = {}


def curve_names(path):
    s = unreal.load_asset(path)
    return sorted(str(x) for x in AL.get_animation_curve_names(s, unreal.RawCurveTrackTypes.RCT_FLOAT)) if s else None


def markers(path):
    s = unreal.load_asset(path)
    if not s:
        return None
    return [(str(m.marker_name), round(float(m.time), 3)) for m in AL.get_animation_sync_markers(s)]


rep["v4r6_curves"] = curve_names(V6)
if os.environ.get("RSV7_SKIP_IMPORT") != "1":
    os.environ.update(RSV4_NAME="AS_GSTN_RunStart_v4r7", RSV4_FBX="RunStart_v4r7.fbx", RSV3_EXIT_AT="1.0667")
    runpy.run_path(PROJ + "Tools/AnimPipeline/cards/runstart/runstart_v4_ue.py", run_name="__main__")
    if rep["v4r6_curves"] and "enable_warping" in rep["v4r6_curves"]:
        os.environ["CURVES_CLIPS"] = V7 + ":start"
        os.environ.pop("CURVES_DRY", None)
        runpy.run_path(PROJ + "Tools/GAS/routing/curves_author.py", run_name="__main__")
    os.environ.update(RS_ROWS="new", RS_TAG="runstart_v4r7_new", RS_PAIRS=V6 + "=" + V7)
    runpy.run_path(PROJ + "Tools/AnimPipeline/cards/runstop/stops_rows_ue.py", run_name="__main__")
rep["v4r7_curves"] = curve_names(V7)

# sync markers: ours and every stock run start / run loop clip found under the GASP mannequin run folder
stock = [a for a in unreal.EditorAssetLibrary.list_assets("/Game/Characters/UEFN_Mannequin/Animations/Run", True, False)
         if ("Run_Start" in a or "Run_Loop" in a)]
rep["markers"] = {p.rsplit("/", 1)[-1].split(".")[0]: markers(p.split(".")[0]) for p in [V6, V7] + stock[:40]}
rep["stock_with_markers"] = sorted(k for k, v in rep["markers"].items() if v)
with open(OUT, "w") as f:
    json.dump(rep, f, indent=1)
