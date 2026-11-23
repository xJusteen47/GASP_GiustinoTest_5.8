"""Owner 2026-11-20: do our replacement clips carry the same curves as the GASP clips they replace? Read only.
For each pair (ours -> stock source row it replaces in CHT_GSTN_StockMover_PoseMatch): float curve names on each side, and per shared
curve min/max/mean and 11 samples on normalized time (0..1). Report Saved/Routing/curves_compare_pairs.json + .txt. Runs in an open
editor (MCP execute_python) or headless."""
import json, os
import unreal

AL = unreal.AnimationLibrary
ANIM = "/Game/Characters/UEFN_Mannequin/Animations/"
PAIRS = [
    ("/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r5", ANIM + "Run/M_Relaxed_Run_Start_F_Lfoot"),
    ("/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r5", ANIM + "Run/M_Relaxed_Run_Start_F_Rfoot"),
]
OUT = unreal.Paths.project_saved_dir() + "Routing/curves_compare_pairs"


def find(name):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(package_paths=["/Game"], recursive_paths=True,
                        class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence")])
    return [a.get_asset().get_path_name().split(".")[0] for a in reg.get_assets(f) if str(a.asset_name) == name]


# the other replacements, found by name (folders moved over time)
for ours, src in (("AS_GSTN_SprintStop_Ganon_L", "M_Relaxed_Sprint_Stop_F_Lfoot"), ("AS_GSTN_SprintStop_Ganon_R", "M_Relaxed_Sprint_Stop_F_Rfoot"),
                  ("AS_GSTN_LandStand_Retime_v2", "M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot"),
                  ("SS_180_Turn_Root180", "M_Neutral_Run_Pivot_F_Lfoot")):
    a, b = find(ours), find(src)
    if a and b:
        PAIRS.append((a[0], b[0]))


def curves(path):
    s = unreal.load_asset(path)
    n = AL.get_animation_curve_names(s, unreal.RawCurveTrackTypes.RCT_FLOAT)
    L = s.get_play_length()
    d = {}
    for c in n:
        v = [AL.get_float_value_at_time(s, c, L * i / 10.0) for i in range(11)]
        d[str(c)] = {"min": round(min(v), 3), "max": round(max(v), 3), "s": [round(x, 2) for x in v]}
    return {"length": round(L, 3), "curves": d}


rep, lines = [], []
for ours, src in PAIRS:
    a, b = curves(ours), curves(src)
    ka, kb = set(a["curves"]), set(b["curves"])
    miss, extra = sorted(kb - ka), sorted(ka - kb)
    diff = {}
    for c in sorted(ka & kb):
        x, y = a["curves"][c], b["curves"][c]
        if abs(x["min"] - y["min"]) > 0.25 or abs(x["max"] - y["max"]) > 0.25:
            diff[c] = {"ours": [x["min"], x["max"]], "gasp": [y["min"], y["max"]]}
    rep.append({"ours": ours, "gasp": src, "len": [a["length"], b["length"]], "missing": miss, "extra": extra, "range_diff": diff,
                "ours_curves": a["curves"], "gasp_curves": b["curves"]})
    lines.append("%s vs %s: missing %s | extra %s | range diff %s" % (ours.split("/")[-1], src.split("/")[-1], miss, extra, diff))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(rep, open(OUT + ".json", "w"), indent=1)
open(OUT + ".txt", "w").write("\n".join(lines) + "\n")
