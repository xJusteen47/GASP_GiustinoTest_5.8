"""Run start variants switchable at runtime (runstart-v8c-desktop, 2026-11-21). Stock pattern (Unreal Assistant,
Saved/UE59/assistant_runstart_v8c.json): a chooser filter column bound to an ABP variable that C++ fills every frame from a cvar.
  ABP_GSTN_StockMover.GSTN_RunStartVariant (float)  <- UGSTN_LocoBaseSubsystem <- cvar gstn.RunStart.Variant (0 = v4r7, default)
In leaf table "Run F Loco Transitions" of CHT_GSTN_StockMover_PoseMatch this script keeps:
  column 0 = FloatRangeColumn on GSTN_RunStartVariant; every row passes (no min/no max) except the run start rows:
  v4r7 rows [-0.5, 0.5]; for every ENABLED v4r7 row one sibling per variant (cells copied, asset swapped) at [k-0.5, k+0.5].
Idempotent (column found by binding, siblings found by asset). v4r7 rows are never disabled or removed (approved, locked).
Env: RSV_VARIANTS "1=/Game/.../AS_GSTN_RunStart_v4r8c;2=/Game/.../AS_GSTN_RunStart_v4r8a"; GSTN_SAVE=1 saves the chooser.
Report Saved/AnimPipeline/rows/runstart_variants.txt; prints max 3 lines."""
import os
import unreal

CHT = "/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch"
LEAF = "Run F Loco Transitions"
VAR = "GSTN_RunStartVariant"
V7 = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r7"
VARIANTS = {}
for part in os.environ.get("RSV_VARIANTS", "").split(";"):
    if "=" in part:
        k, p = part.split("=", 1)
        VARIANTS[p.strip()] = int(k)
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_variants.txt")
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


ANY = "(Min=0.000000,Max=0.000000,bNoMin=True,bNoMax=True)"
leaf = [t for t in tables(unreal.load_asset(CHT), []) if t.get_name() == LEAF][0]
info = call("GetChooserTable", leaf)
col = next((i for i, c in enumerate(info.columns) if VAR in [str(x) for x in c.property_chain]), None)
if col is None:
    col = call("AddChooserColumn", leaf, "/Script/Chooser.FloatRangeColumn")
    call("SetChooserColumnInput", leaf, col, "/Script/Chooser.FloatContextProperty", 0, [VAR])
    col = call("MoveChooserColumn", leaf, col, 0)
    log.append("column added at %s" % col)
    for r in range(len(info.rows)):
        call("SetChooserCell", leaf, col, r, ANY)

rows = list(call("GetChooserTable", leaf).rows)
src_rows = [i for i, r in enumerate(rows) if asset_of(r) == V7]
for i in src_rows:
    call("SetChooserCell", leaf, col, i, rng(0))
for i in reversed([i for i in src_rows if not rows[i].get_editor_property("bDisabled")]):
    rows = list(call("GetChooserTable", leaf).rows)
    have = set()
    j = i + 1
    while j < len(rows) and asset_of(rows[j]) in VARIANTS:
        have.add(asset_of(rows[j])); j += 1
    for dst, k in sorted(VARIANTS.items(), key=lambda x: -x[1]):
        if dst in have:
            continue
        n = call("AddChooserRow", leaf, rows[i].result_type, i + 1, unreal.load_asset(dst))
        for c, cell in enumerate(rows[i].cells):
            val = rng(k) if c == col else str(cell).replace(V7 + "." + V7.split("/")[-1], dst + "." + dst.split("/")[-1])
            try:
                call("SetChooserCell", leaf, c, n, val)
            except Exception as e:
                log.append("  cell %d not copied: %s" % (c, str(e)[:120]))
        call("SetChooserRowDisabled", leaf, n, False)
        log.append("row %d: added %s at %d" % (i, dst.split("/")[-1], k))

rows = list(call("GetChooserTable", leaf).rows)
for i, r in enumerate(rows):
    a = asset_of(r)
    if a == V7 or a in VARIANTS:
        log.append("check row %d %s disabled=%s range=%s" % (i, a.split("/")[-1], r.get_editor_property("bDisabled"), str(r.cells[col])))
if os.environ.get("GSTN_SAVE") == "1":
    log.append("saved %s" % unreal.EditorAssetLibrary.save_asset(CHT, False))
log.append("ESITO: OK column %d variants %d" % (col, len(VARIANTS)))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print("\n".join(log[-3:]))
