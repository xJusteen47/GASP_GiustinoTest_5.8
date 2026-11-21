"""Stance width layer, setup (runstart-stancewidth, owner 2026-11-21: w2 feet too tight / crossing, widen them WITHOUT redoing the clip).
1. ABP: puts the GSTN Stance Width node (FGSTN_AnimNode_StanceWidth) right in front of the Output Pose of the clean base
   ABP_GSTN_StockMover (gstn.Loco.Base 1), after GASP's leg IK. Idempotent; backup in Saved/Backups/GAS/stancewidth.
2. Clips: for each SW_PAIRS "src=dst" a COPY dst of src (src is never touched: w2 is locked, rule 13b) with the opt-in curves
   GSTN_StanceWidth_L / _R. Each foot ramps in from 0 at f0 to 1 just before its first contact, and out to 0 inside its longest
   swing of the second half of the clip (no planted foot slides; 0 at the hand-off, the stock loop stays as it is).
3. Rows: runstart_generic_rows_ue.py with RSG_ASSET=dst at gstn.RunStart.Variant SW_K (default 22) and SW_K + 10 (Handoff),
   siblings of the stock rows (env RSG_STOCK / RSG_SPLIT passed through for the turnarounds).
Headless via Tools/CI/Save-Assets.ps1 (dry run otherwise). Report Saved/AnimPipeline/stancewidth/setup.txt; prints max 3 lines.
Divergence row: docs/notes/gasp-divergenze.md (21d)."""
import os
import runpy
import shutil
import traceback

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ABP = "/Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover"
RS = "/Game/GSTN/Animation/RunStart/"
PAIRS = [p.split("=") for p in os.environ.get("SW_PAIRS", RS + "AS_GSTN_RunStart_Generic_w2=" + RS + "AS_GSTN_RunStart_Generic_w2s").split(",")]
K = int(os.environ.get("SW_K", "22"))
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
EAL, AL = unreal.EditorAssetLibrary, unreal.AnimationLibrary
FLT = unreal.RawCurveTrackTypes.RCT_FLOAT
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "stancewidth")
os.makedirs(OUT, exist_ok=True)
log = []


def ramps(contact, n):
    """(in_end, out_start, out_end) frames: 0 -> 1 over [0, in_end], 1 -> 0 over [out_start, out_end]"""
    on = [c > 0.5 for c in contact]
    first = next((i for i, c in enumerate(on) if c), n)
    in_end = max(2, first - 1)
    swings, i = [], n // 2
    while i < n:
        if not on[i]:
            j = i
            while j < n and not on[j]:
                j += 1
            swings.append((i, j - 1))
            i = j
        else:
            i += 1
    a, b = max(swings, key=lambda s: s[1] - s[0]) if swings else (n - 6, n - 1)
    out_start, out_end = a + 1, min(b - 1, n - 3)
    if out_end - out_start < 3:
        out_start, out_end = max(in_end + 1, n - 8), n - 3
    return in_end, out_start, out_end


try:
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/GSTN"], True)
    # 1. node in the ABP
    lib = unreal.GSTN_AnimGraphAuthoringLibrary
    abp = unreal.load_asset(ABP)
    node_class = unreal.GSTN_AnimGraphNode_StanceWidth.static_class()
    before = lib.describe_graphs(abp)
    open(os.path.join(OUT, "abp_before.txt"), "w", encoding="utf-8").write(before)
    if SAVE:
        bak = os.path.join(ROOT, "Saved", "Backups", "GAS", "stancewidth")
        os.makedirs(bak, exist_ok=True)
        src_file = os.path.join(ROOT, "Content", "GSTN", "Locomotion", "StockBase", "ABP_GSTN_StockMover.uasset")
        if not os.path.exists(os.path.join(bak, "ABP_GSTN_StockMover.uasset")):
            shutil.copy2(src_file, bak)
        ok_insert = lib.insert_pose_node_before_output(abp, node_class)
        report, ok = lib.compile_with_report(abp)
        open(os.path.join(OUT, "compile.txt"), "w", encoding="utf-8").write(str(report))
        saved = EAL.save_loaded_asset(abp, False) if ok_insert and ok else False
        open(os.path.join(OUT, "abp_after.txt"), "w", encoding="utf-8").write(lib.describe_graphs(abp))
        log.append("abp: insert=%s compile=%s saved=%s" % (ok_insert, ok, saved))
    else:
        log.append("abp: would insert GSTN Stance Width before Output Pose (already there: %s)" % ("Stance Width" in before))

    # 2. clip copies with the opt-in curves
    for src, dst in PAIRS:
        src_seq = unreal.load_asset(src)
        t, cl = AL.get_float_keys(src_seq, "contact_l")
        _, cr = AL.get_float_keys(src_seq, "contact_r")
        n = len(t)
        keys = {}
        for side, contact in (("L", cl), ("R", cr)):
            a, b, c = ramps(list(contact), n)
            frames, vals = [0, a, b, c], [0.0, 1.0, 1.0, 0.0]
            keys[side] = ([t[f] for f in frames], vals)
            log.append("%s %s: ramp in f0-%d, out f%d-%d of %d" % (dst.split("/")[-1], side, a, b, c, n))
        if not SAVE:
            continue
        if not EAL.does_asset_exist(dst):
            assert EAL.duplicate_asset(src, dst), "duplicate failed " + dst
        seq = unreal.load_asset(dst)
        for side in ("L", "R"):
            name = "GSTN_StanceWidth_" + side
            if AL.does_curve_exist(seq, name, FLT):
                AL.remove_curve(seq, name, False)
            AL.add_curve(seq, name, FLT, False)
            AL.add_float_curve_keys(seq, name, keys[side][0], keys[side][1])
        log.append("  saved clip %s" % EAL.save_asset(dst, only_if_is_dirty=False))

    # 3. chooser rows
    for src, dst in PAIRS:
        os.environ["RSG_ASSET"] = dst
        os.environ["RSG_K"] = str(K)
        os.environ["RSG_REPORT"] = "stancewidth_rows_" + dst.split("_")[-1]
        runpy.run_path(os.path.join(ROOT, "Tools", "AnimPipeline", "cards", "runstart", "runstart_generic_rows_ue.py"), run_name="__main__")
        rep = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", os.environ["RSG_REPORT"] + ".txt")
        log.append("rows %s: %s" % (dst.split("/")[-1], open(rep, encoding="utf-8").read().strip().splitlines()[-1]))
    log.append("ESITO: OK" + ("" if SAVE else " (dry run)"))
except Exception:
    log.append("ESITO: FAIL " + traceback.format_exc()[-1200:])
open(os.path.join(OUT, "setup.txt"), "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:]))
