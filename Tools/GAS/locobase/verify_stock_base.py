"""loco-base-1: read-only check of the clean GASP Mover base (headless or in the editor).

Checks that the copies only point at each other: ABP_GSTN_StockMover -> its two chooser copies, choosers -> the copy's class,
no reference to /Game/Blueprints/SandboxCharacter_Mover_ABP, to the _Src package or to the sample choosers; the ABP compiles;
every chooser table (root + nested) has the copy's class as context. Report: Saved/LocoBase/verify_stock_base.txt
"""
import os
import unreal

OUT = unreal.Paths.project_saved_dir() + "LocoBase/verify_stock_base.txt"
DST = "/Game/GSTN/Locomotion/StockBase/"
NEW = {"abp": DST + "ABP_GSTN_StockMover", "cht": DST + "CHT_GSTN_StockMover_PoseMatch", "mm": DST + "CHT_GSTN_StockMover_PSDRelaxed"}
FORBIDDEN = ("/Game/Blueprints/SandboxCharacter_Mover_ABP", DST + "_Src/",
             "/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations_PoseMatch",
             "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Relaxed",
             "/Game/GSTN/Animation/")

log, fails = [], []
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/Game"], True)
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
for key, path in NEW.items():
    deps = [str(d) for d in (reg.get_dependencies(path, opts) or [])]
    bad = [d for d in deps if any(d.startswith(f) for f in FORBIDDEN)]
    log.append("%s deps %d forbidden %s" % (path, len(deps), bad))
    if not deps:
        fails.append(key + ": not in registry")
    if bad:
        fails.append(key + ": forbidden deps")
abp_deps = [str(d) for d in (reg.get_dependencies(NEW["abp"], opts) or [])]
for k in ("cht", "mm"):
    if NEW[k] not in abp_deps:
        fails.append("abp does not use " + NEW[k])

abp = unreal.load_asset(NEW["abp"])
gen = unreal.load_object(None, NEW["abp"] + ".ABP_GSTN_StockMover_C")
if abp:
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    st = abp.get_editor_property("status")
    log.append("abp status %s parent %s" % (st, gen.get_super_class().get_name() if gen else None))
    if "ERROR" in str(st).upper() or "UP_TO_DATE" not in str(st).upper():
        fails.append("abp compile error")

def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for n in unreal.GSTN_ChooserTableToolset.get_default_object().call_method("GetChooserTable", (c,)).nested_choosers:
            tables(n, seen)
    return seen
for k in ("cht", "mm"):
    ts = tables(unreal.load_asset(NEW[k]), [])
    other = []
    for t in ts:
        for e in t.get_editor_property("context_data"):
            try:
                cls = e.get_value().get_editor_property("class")
            except Exception:
                continue
            if cls != gen:
                other.append(t.get_name())
    log.append("%s tables %d with other context class %s" % (NEW[k], len(ts), other[:5]))
    if other:
        fails.append(k + ": context class")
log.append("ESITO: " + ("OK" if not fails else "FAIL " + "; ".join(fails)))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
