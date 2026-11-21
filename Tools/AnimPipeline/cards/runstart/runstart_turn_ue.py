"""Turnarounds 2 into UE in ONE headless run (turnarounds-2, 2026-11-21): Tools/CI/Save-Assets.ps1 -Script <this>.
1. Clips: for each of w2t{180,090}{L,R}<RST_SUFFIX> (default 'f'), runstart_generic_g_ue.py with RSGG_TAG and RSGG_SRC (the stock
   reface meta), as the first turnaround round did one Save-Assets run per clip.
2. Forward start rows at RST_K / RST_K + 10 (default 26 / 36): runstart_generic_rows_ue.py, RSG_ASSET = RST_FWD (default w2fs,
   the clip of variant 24), so variant 26 is variant 24 plus the new turnarounds.
3. Turnaround rows in leaf "Run F Loco Transitions": the stock reface copies [21.5, 30.5] and [31.5, ..) are split around K and
   K + 10 (-> [21.5, K-0.5] + [K+0.5, 30.5], [31.5, K+9.5] + [K+10.5, ..)) and each of our variant-21 / 31 turnaround rows gets a
   sibling at K / K + 10 with the new clip (same cells). At K only the new clip is a candidate; 21-25 unchanged.
Idempotent. Dry run unless GSTN_SAVE=1. Report Saved/AnimPipeline/rows/runstart_turn2.txt; prints <= 4 lines."""
import os
import re
import runpy

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
RS = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")
FOLDER = "/Game/GSTN/Animation/RunStart/"
SUF = os.environ.get("RST_SUFFIX", "f")
K = int(os.environ.get("RST_K", "26"))
FWD = os.environ.get("RST_FWD", "AS_GSTN_RunStart_Generic_w2fs")
SAVE = os.environ.get("GSTN_SAVE") == "1"
CLIPS = {"180L": "180_L_Lfoot", "180R": "180_R_Rfoot", "090L": "090_L_Lfoot", "090R": "090_R_Rfoot"}
# turnarounds-3 (variant 27): RST_KEYS = the directions built this run (default all); the others get rows with the RST_FALLBACK
# suffix clip (already imported), so every direction keeps exactly one candidate at K. RST_LO1 / RST_LO2 = lower bound of the stock
# copies to split (21.5 / 31.5 the first time; 26.5 / 36.5 once variant 26 split them).
KEYS = os.environ.get("RST_KEYS", ",".join(CLIPS)).split(",")
FALLBACK = os.environ.get("RST_FALLBACK", SUF)
SUFK = {k: (SUF if k in KEYS else FALLBACK) for k in CLIPS}
LO1, LO2 = float(os.environ.get("RST_LO1", "21.5")), float(os.environ.get("RST_LO2", "31.5"))
log = []

# 1. clips
for key, src in [(k, s) for k, s in CLIPS.items() if k in KEYS]:
    os.environ["RSGG_TAG"] = "w2t%s%s" % (key, SUF)
    os.environ["RSGG_SRC"] = os.path.join(RS, "turn", "M_Relaxed_Run_Reface_Start_%s.meta.json" % src)
    try:
        runpy.run_path(os.path.join(HERE, "runstart_generic_g_ue.py"), run_name="__main__")
        log.append("clip w2t%s%s: %s" % (key, SUF, unreal.EditorAssetLibrary.does_asset_exist(FOLDER + "AS_GSTN_RunStart_Generic_w2t%s%s" % (key, SUF))))
    except SystemExit:
        pass
    except Exception as e:  # noqa: BLE001
        log.append("clip w2t%s%s FAILED %r" % (key, SUF, e))
for k in ("RSGG_TAG", "RSGG_SRC"):
    os.environ.pop(k, None)

# 2. forward start rows (stock F rows are disabled since v3: plain siblings)
os.environ.update(RSG_ASSET=FOLDER + FWD, RSG_K=str(K), RSG_REPORT="runstart_turn2_fwd")
try:
    runpy.run_path(os.path.join(HERE, "runstart_generic_rows_ue.py"), run_name="__main__")
except SystemExit:
    pass
fwd_rep = open(os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_turn2_fwd.txt"), encoding="utf-8").read().splitlines()
log.append("fwd rows: " + (fwd_rep[-1] if fwd_rep else "no report"))
for k in ("RSG_ASSET", "RSG_K", "RSG_REPORT"):
    os.environ.pop(k, None)

# 3. turnaround rows
CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
LEAF = "Run F Loco Transitions"
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
call = lambda name, *a: TS.call_method(name, tuple(a))


def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in call("GetChooserTable", c).nested_choosers:
            tables(n, seen)
    return seen


leaf = [t for t in tables(unreal.load_asset(CHT), []) if t.get_name() == LEAF][0]
info = call("GetChooserTable", leaf)
c_var = next(i for i, c in enumerate(info.columns) if "GSTN_RunStartVariant" in [str(x) for x in c.property_chain])
asset_of = lambda r: r.asset.get_path_name().split(".")[0] if r.asset else ""


def rng2(lo, hi):
    f = lambda v: "%.6f" % (v if v is not None else 0.0)
    return "(Min=%s,Max=%s,bNoMin=%s,bNoMax=%s)" % (f(lo), f(hi), lo is None, hi is None)


rows = lambda: list(call("GetChooserTable", leaf).rows)
STOCK = ["/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Reface_Start_" + s for s in CLIPS.values()]
for lo, hi, lo2, hi2 in ((LO1, 30.5, K + 0.5, 30.5), (LO2, None, K + 10.5, None)):
    for st in STOCK:
        rr = rows()
        i = next((n for n, r in enumerate(rr) if asset_of(r) == st and str(r.cells[c_var]) == rng2(lo, hi)), None)
        if i is None:
            log.append("stock copy %s %s: not found (already split?)" % (st.split("_Start_")[-1], rng2(lo, hi)))
            continue
        if not SAVE:
            log.append("would split row %d %s" % (i, st.split("_Start_")[-1]))
            continue
        j = call("AddChooserRow", leaf, rr[i].result_type, i + 1, unreal.load_asset(st))
        for c, cell in enumerate(rr[i].cells):
            call("SetChooserCell", leaf, c, j, rng2(lo2, hi2) if c == c_var else str(cell))
        call("SetChooserRowDisabled", leaf, j, False)
        call("SetChooserCell", leaf, c_var, i, rng2(lo, K - 0.5 if hi == 30.5 else K + 9.5))
        log.append("split %s: row %d -> %s, new row %d %s" % (st.split("_Start_")[-1], i, rng2(lo, K - 0.5 if hi == 30.5 else K + 9.5), j, rng2(lo2, hi2)))
for key in CLIPS:
    old, new = FOLDER + "AS_GSTN_RunStart_Generic_w2t" + key, FOLDER + "AS_GSTN_RunStart_Generic_w2t%s%s" % (key, SUFK[key])
    for v_old, v_new in ((21, K), (31, K + 10)):
        rr = rows()
        srcs = [n for n, r in enumerate(rr) if asset_of(r) == old and str(r.cells[c_var]) == rng2(v_old - 0.5, v_old + 0.5)]
        have = [n for n, r in enumerate(rr) if asset_of(r) == new and str(r.cells[c_var]) == rng2(v_new - 0.5, v_new + 0.5)]
        for i in srcs[len(have):]:
            if not SAVE:
                log.append("would add sibling %s at %d" % (key, v_new))
                continue
            j = call("AddChooserRow", leaf, rr[i].result_type, i + 1, unreal.load_asset(new))
            for c, cell in enumerate(rr[i].cells):
                call("SetChooserCell", leaf, c, j, rng2(v_new - 0.5, v_new + 0.5) if c == c_var else str(cell))
            call("SetChooserRowDisabled", leaf, j, False)
            log.append("sibling %s at %d: row %d" % (key, v_new, j))
# check: at K and K+10 each reface direction has exactly our clip as an enabled candidate
rr = rows()
bad = []
for v in (K, K + 10):
    for key, st in zip(CLIPS, STOCK):
        new = FOLDER + "AS_GSTN_RunStart_Generic_w2t%s%s" % (key, SUFK[key])
        cand = []
        for r in rr:
            if r.get_editor_property("bDisabled") or asset_of(r) not in (st, new):
                continue
            m = re.search(r"Min=([-0-9.]+),Max=([-0-9.]+),bNoMin=(\w+),bNoMax=(\w+)", str(r.cells[c_var]))
            if m and (m.group(3) == "True" or float(m.group(1)) <= v) and (m.group(4) == "True" or v <= float(m.group(2))):
                cand.append(asset_of(r).split("/")[-1])
        if not cand or any(c != new.split("/")[-1] for c in cand):
            bad.append((v, key, cand))
if SAVE:
    log.append("saved chooser %s" % unreal.EditorAssetLibrary.save_asset(CHT, False))
log.append("ESITO: %s turn2 rows (bad %s)" % ("OK" if not bad else "FAIL", bad[:4]))
out = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_turn2.txt")
os.makedirs(os.path.dirname(out), exist_ok=True)
open(out, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-4:]))
