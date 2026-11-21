"""Generic run start for the plain GASP gait ("John normale", owner 2026-11-21): the STOCK relaxed run starts as a switchable
variant while the Overlord run loop is redone (the Overlord start must end on the new loop, so it waits).
In leaf table "Run F Loco Transitions" of CHT_GSTN_StockMover_PoseMatch the stock rows (M_Relaxed_Run_Start_F_Lfoot / _Rfoot) are
disabled since v3. For each of them this adds two ENABLED siblings, the source rows stay as they are:
  variant RSG_K (default 4)       cells copied 1:1 (stock blend, stock sampling range)          gstn.RunStart.Variant 4
  variant RSG_K + 10 (14)         as runstart_handoff_ue.py: BlendTime RSH_BLEND (0.1) RSH_CURVE (Linear), PoseSearchColumn
                                  SamplingRange 0..RSH_ENTRY_MAX (0.02)                         + gstn.RunStart.Handoff 1
The stock clips already carry `phase` in the stock triangle (validated in runstart_handoff_ue.py), so no clip copy is needed.
Idempotent (sibling found by asset + variant range + same filter cells). Dry run unless GSTN_SAVE=1 / CLIPFIX_WRITE=1.
Report Saved/AnimPipeline/rows/runstart_generic.txt; prints max 3 lines. Divergence row: docs/notes/gasp-divergenze.md (20)."""
import os
import re
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
LEAF = "Run F Loco Transitions"
VAR = "GSTN_RunStartVariant"
STOCK = ["/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Lfoot",
         "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Rfoot"]
# Turnarounds (runstart-turnaround-desktop, 2026-11-21): RSG_STOCK = comma-separated stock clips whose rows get the siblings (the
# REFACE starts 090/180 L/R of the same leaf). Those stock rows are ENABLED (unlike F, disabled since v3), so RSG_SPLIT=1 keeps
# them for every other variant: the source row gets variant range (.., K-0.5] and two stock copies cover [K+0.5, K+9.5] and
# [K+10.5, ..): at K and K+10 only our clip is a candidate (no pose-search coin toss between stock and ours).
if os.environ.get("RSG_STOCK"):
    STOCK = ["/Game/Characters/UEFN_Mannequin/Animations/Run/" + x.strip() for x in os.environ["RSG_STOCK"].split(",")]
SPLIT = os.environ.get("RSG_SPLIT") == "1"
K = int(os.environ.get("RSG_K", "4"))
# RSG_ASSET (runstart-generic-desktop g1): siblings play this clip instead of the stock one; RSG_SRC limits the source rows
ASSET = os.environ.get("RSG_ASSET", "")
SRC = os.environ.get("RSG_SRC", "")
BLEND = float(os.environ.get("RSH_BLEND", "0.1"))
CURVE = os.environ.get("RSH_CURVE", "Linear")
ENTRY_MAX = float(os.environ.get("RSH_ENTRY_MAX", "0.02"))
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
EAL = unreal.EditorAssetLibrary
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", os.environ.get("RSG_REPORT", "runstart_generic") + ".txt")
log = []


def call(name, *args):
    return TS.call_method(name, tuple(args))


def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in call("GetChooserTable", c).nested_choosers:
            tables(n, seen)
    return seen


def asset_of(row):
    a = row.asset
    return a.get_path_name().split(".")[0] if a else ""


def rng(k):
    return "(Min=%.6f,Max=%.6f,bNoMin=False,bNoMax=False)" % (k - 0.5, k + 0.5)


leaf = [t for t in tables(unreal.load_asset(CHT), []) if t.get_name() == LEAF][0]
info = call("GetChooserTable", leaf)
types = [str(c.type) for c in info.columns]
c_var = next(i for i, c in enumerate(info.columns) if VAR in [str(x) for x in c.property_chain])
c_out = next(i for i, t in enumerate(types) if t.endswith("OutputStructColumn"))
c_ps = next(i for i, t in enumerate(types) if t.endswith("PoseSearchColumn"))


def tuned(col, text):
    if col == c_out:
        text = re.sub(r"(BlendTime_[0-9A-F_]+=)[0-9.]+", lambda m: m.group(1) + "%.6f" % BLEND, text)
        return re.sub(r"(BlendCurve_[0-9A-F_]+=)\w+", lambda m: m.group(1) + CURVE, text)
    if col == c_ps:
        return re.sub(r"SamplingRange=\(Min=[0-9.]+,Max=[0-9.]+\)", "SamplingRange=(Min=0.000000,Max=%.6f)" % ENTRY_MAX, text)
    return text


def filters(row):
    return [str(c) for n, c in enumerate(row.cells) if n not in (c_var, c_out, c_ps)]


rows = list(info.rows)
# source = the stock start rows as they were before our variants (disabled, variant column "any")
srcs = [i for i, r in enumerate(rows) if asset_of(r) in STOCK and "bNoMin=True" in str(r.cells[c_var]) and SRC in asset_of(r)]
log.append("stock source rows %s" % srcs)
for i in srcs:
    log.append("  src %d %s disabled=%s var=%s" % (i, asset_of(rows[i]).split("/")[-1], rows[i].get_editor_property("bDisabled"), str(rows[i].cells[c_var])))


def rng2(lo, hi):
    f = lambda v: "%.6f" % (v if v is not None else 0.0)
    return "(Min=%s,Max=%s,bNoMin=%s,bNoMax=%s)" % (f(lo), f(hi), lo is None, hi is None)


if SPLIT:
    for i in reversed(srcs):
        if rows[i].get_editor_property("bDisabled"):
            continue
        for lo, hi in ((K + 10 + 0.5, None), (K + 0.5, K + 9.5)):         # stock copies, inserted right below the source
            rows = list(call("GetChooserTable", leaf).rows)
            st = asset_of(rows[i])
            j = next((n for n, r in enumerate(rows) if asset_of(r) == st and str(r.cells[c_var]) == rng2(lo, hi)
                      and filters(r) == filters(rows[i])), None)
            if not SAVE:
                log.append("row %d %s: would %s stock copy %s" % (i, st.split("/")[-1], "update" if j is not None else "add", rng2(lo, hi)))
                continue
            if j is None:
                j = call("AddChooserRow", leaf, rows[i].result_type, i + 1, unreal.load_asset(st))
            for c, cell in enumerate(rows[i].cells):
                call("SetChooserCell", leaf, c, j, rng2(lo, hi) if c == c_var else str(cell))
            call("SetChooserRowDisabled", leaf, j, False)
            log.append("row %d %s: stock copy %s at row %d" % (i, st.split("/")[-1], rng2(lo, hi), j))
        if SAVE:
            call("SetChooserCell", leaf, c_var, i, rng2(None, K - 0.5))
            log.append("row %d: source variant range -> %s" % (i, rng2(None, K - 0.5)))
    rows = list(call("GetChooserTable", leaf).rows)
    srcs = [i for i, r in enumerate(rows) if asset_of(r) in STOCK and "bNoMin=True" in str(r.cells[c_var]) and SRC in asset_of(r)]
for i in reversed(srcs):
    for k, tune in ((K + 10, True), (K, False)):   # inserted right below i: K ends up first, then K + 10
        rows = list(call("GetChooserTable", leaf).rows)
        dst = ASSET or asset_of(rows[i])
        j = next((n for n, r in enumerate(rows) if asset_of(r) == dst and str(r.cells[c_var]) == rng(k)
                  and filters(r) == filters(rows[i])), None)
        if not SAVE:
            log.append("row %d %s: would %s sibling at %d" % (i, asset_of(rows[i]).split("/")[-1], "update" if j is not None else "add", k))
            continue
        if j is None:
            j = call("AddChooserRow", leaf, rows[i].result_type, i + 1, unreal.load_asset(dst))
            log.append("row %d %s: added sibling at %d (row %d)" % (i, asset_of(rows[i]).split("/")[-1], k, j))
        for c, cell in enumerate(rows[i].cells):
            val = rng(k) if c == c_var else (tuned(c, str(cell)) if tune else str(cell))
            call("SetChooserCell", leaf, c, j, val)
        call("SetChooserRowDisabled", leaf, j, False)

rows = list(call("GetChooserTable", leaf).rows)
n = 0
for i, r in enumerate(rows):
    if asset_of(r) in STOCK + [ASSET]:
        o = str(r.cells[c_out])
        b = re.search(r"BlendTime_[0-9A-F_]+=([0-9.]+)", o)
        sr = re.search(r"SamplingRange=\(([^)]*)\)", str(r.cells[c_ps]))
        n += (not r.get_editor_property("bDisabled")) and str(r.cells[c_var]) in (rng(K), rng(K + 10)) and (asset_of(r) == ASSET or not ASSET)
        log.append("check row %d %s disabled=%s var=%s blend=%s sampling=%s" % (
            i, asset_of(r).split("/")[-1], r.get_editor_property("bDisabled"), str(r.cells[c_var]),
            b.group(1) if b else "?", sr.group(1) if sr else "?"))
if SAVE:
    log.append("saved chooser %s" % EAL.save_asset(CHT, False))
log.append("ESITO: %s generic rows enabled %d (expected %d)" % ("OK" if (n == 2 * len(srcs) or not SAVE) else "FAIL", n, 2 * len(srcs)))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:]))
