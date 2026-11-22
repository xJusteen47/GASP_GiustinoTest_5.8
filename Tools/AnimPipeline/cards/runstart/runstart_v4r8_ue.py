"""Run start v4r8 (Luigi timing) into UE, same steps as runstart_v4r7_ue.py: headless through Tools/CI/Save-Assets.ps1 (GSTN_SAVE=1).
Env: RSV8_NAME (default AS_GSTN_RunStart_v4r8; v4r8b for the 2x-Luigi variant), RSV8_EXIT_AT (= EXIT_AT printed by
runstart_retime_casc_v4r8.py), RSV8_ROWS=new (rows v4r7 off, v4r8 on) or none (asset only, rows untouched; for v4r8b).
v4r7 stays a row, disabled (approved/runstart-v4r7 is locked, rule 13b). Back to v4r7: stops_rows_ue.py RS_ROWS=new RS_PAIRS v4r8=v4r7.
1. runstart_v4_ue.py on <name>.fbx (block transition, curves as v4r7).
2. Curves: the curve set v4r7 carries (enable_warping -> curves_author kind start).
3. Rows (RSV8_ROWS=new): every chooser row playing v4r7 gets a v4r8 sibling, v4r7 disabled.
4. Sync markers (read only), as v4r7.
Report Saved/AnimPipeline/runstart/<v4r8|v4r8b>_ue.json; prints nothing."""
import json
import os
import runpy

import unreal

AL = unreal.AnimationLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FOLDER = "/Game/GSTN/Animation/RunStart"
NAME = os.environ.get("RSV8_NAME", "AS_GSTN_RunStart_v4r8")
V6, V7 = FOLDER + "/AS_GSTN_RunStart_v4r7", FOLDER + "/" + NAME
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", NAME.split("_")[-1] + "_ue.json")
rep = {}


def curve_names(path):
    s = unreal.load_asset(path)
    return sorted(str(x) for x in AL.get_animation_curve_names(s, unreal.RawCurveTrackTypes.RCT_FLOAT)) if s else None


def markers(path):
    s = unreal.load_asset(path)
    if not isinstance(s, unreal.AnimSequence):
        return None
    return [(str(m.marker_name), round(float(m.time), 3)) for m in AL.get_animation_sync_markers(s)]


rep["src_curves"] = curve_names(V6)
if os.environ.get("RSV7_SKIP_IMPORT") != "1":
    os.environ.update(RSV4_NAME=NAME, RSV4_FBX=NAME.replace("AS_GSTN_", "") + ".fbx", RSV3_EXIT_AT=os.environ["RSV8_EXIT_AT"])
    runpy.run_path(PROJ + "Tools/AnimPipeline/cards/runstart/runstart_v4_ue.py", run_name="__main__")
    if rep["src_curves"] and "enable_warping" in rep["src_curves"]:
        os.environ["CURVES_CLIPS"] = V7 + ":start"
        os.environ.pop("CURVES_DRY", None)
        runpy.run_path(PROJ + "Tools/GAS/routing/curves_author.py", run_name="__main__")
    if os.environ.get("RSV8_ROWS", "new") == "new":
        os.environ.update(RS_ROWS="new", RS_TAG="runstart_" + NAME.split("_")[-1] + "_new", RS_PAIRS=V6 + "=" + V7)
        runpy.run_path(PROJ + "Tools/AnimPipeline/cards/runstop/stops_rows_ue.py", run_name="__main__")
rep["dst_curves"] = curve_names(V7)

# sync markers: ours and every stock run start / run loop clip found under the GASP mannequin run folder
stock = [a for a in unreal.EditorAssetLibrary.list_assets("/Game/Characters/UEFN_Mannequin/Animations/Run", True, False)
         if ("Run_Start" in a or "Run_Loop" in a)]
rep["markers"] = {p.rsplit("/", 1)[-1].split(".")[0]: markers(p.split(".")[0]) for p in [V6, V7] + stock[:40]}
rep["stock_with_markers"] = sorted(k for k, v in rep["markers"].items() if v)
with open(OUT, "w") as f:
    json.dump(rep, f, indent=1)
