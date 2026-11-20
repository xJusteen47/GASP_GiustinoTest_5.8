"""loco-base-1: make the clean GASP Mover base under /Game/GSTN/Locomotion/StockBase.

Sources (both untouched GASP sample, see docs/notes/handovers/loco-base-1.md):
  ABP     = SandboxCharacter_Mover_ABP as first committed (f1a280d5, UE 5.8 sample, no GSTN nodes), extracted by
            extract_src.ps1 to /Game/GSTN/Locomotion/StockBase/_Src/SandboxCharacter_Mover_ABP
  choosers = the sample CHT_MoverCharacterAnimations_PoseMatch (internal PoseSearch databases) and
            CHT_PoseSearchDatabases_Relaxed (motion matching node), never edited in this project
Copies them with AssetTools.AdvancedCopyPackages (GSTN_ChooserTableToolset.CopyPackagesRemapped) (remaps references between the copied packages), then points the
chooser context class (root + every nested table) to the copy's generated class and compiles the ABP.
Dry run unless GSTN_SAVE=1 (Tools/CI/Save-Assets.ps1). Report: Saved/LocoBase/make_stock_base.txt
"""
import os
import unreal

OUT = unreal.Paths.project_saved_dir() + "LocoBase/make_stock_base.txt"
SAVE = os.environ.get("GSTN_SAVE") == "1"
DST = "/Game/GSTN/Locomotion/StockBase/"
SRC_ABP = DST + "_Src/SandboxCharacter_Mover_ABP"
SRC_CHT = "/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations_PoseMatch"
SRC_MM = "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Relaxed"
NEW_ABP = DST + "ABP_GSTN_StockMover"
NEW_CHT = DST + "CHT_GSTN_StockMover_PoseMatch"
NEW_MM = DST + "CHT_GSTN_StockMover_PSDRelaxed"
OLD_CLASS_NAMES = ("SandboxCharacter_Mover_ABP_C",)

log = []
def p(*a):
    log.append(" ".join(str(x) for x in a))

def flush():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(log) + "\n")

eal = unreal.EditorAssetLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/Game"], True)

def walk_tables(cht, seen=None):
    seen = seen if seen is not None else []
    if cht is None or cht in seen:
        return seen
    seen.append(cht)
    try:
        for n in cht.get_editor_property("nested_choosers"):
            walk_tables(n, seen)
    except Exception as e:
        p("  nested_choosers not readable on", cht.get_name(), repr(e))
    return seen

def retarget_context(root_path, gen_class):
    root = unreal.load_asset(root_path)
    tables = walk_tables(root)
    changed = 0
    for t in tables:
        cd = list(t.get_editor_property("context_data"))
        dirty = False
        for i, entry in enumerate(cd):
            try:
                v = entry.get_value()
                cls = v.get_editor_property("class")
            except Exception:
                continue
            if cls and cls.get_name() in OLD_CLASS_NAMES and cls != gen_class:
                v.set_editor_property("class", gen_class)
                entry.set_value(v)
                cd[i] = entry
                dirty = True
        if dirty:
            t.set_editor_property("context_data", cd)
            changed += 1
    # Report what every table ends up with
    bad = []
    for t in tables:
        for entry in t.get_editor_property("context_data"):
            try:
                cls = entry.get_value().get_editor_property("class")
            except Exception:
                continue
            if cls and cls != gen_class:
                bad.append("%s:%s" % (t.get_name(), cls.get_path_name()))
    p("context", root_path, "tables", len(tables), "retargeted", changed, "still other class", bad[:5])
    return root, not bad

def main():
    p("SAVE" if SAVE else "DRY RUN")
    for src in (SRC_ABP, SRC_CHT, SRC_MM):
        if not eal.does_asset_exist(src):
            p("MISSING source", src); return False
    if eal.does_asset_exist(NEW_ABP):
        p("already exists", NEW_ABP, "- nothing copied (delete the StockBase copies to redo)"); return False
    # AdvancedCopyPackages is not exposed to Python: GSTNAnimToolsets wraps it (CopyPackagesRemapped)
    mapping = {SRC_ABP: NEW_ABP, SRC_CHT: NEW_CHT, SRC_MM: NEW_MM}
    toolset = unreal.GSTN_ChooserTableToolset.get_default_object()
    ok = toolset.call_method("CopyPackagesRemapped", (mapping,))
    p("CopyPackagesRemapped", ok)
    reg.scan_paths_synchronous([DST[:-1]], True)
    abp = unreal.load_asset(NEW_ABP)
    if not abp:
        p("copy failed: no", NEW_ABP); return False
    gen = unreal.load_object(None, NEW_ABP + "." + NEW_ABP.rsplit("/", 1)[1] + "_C")
    p("generated class", gen.get_path_name() if gen else None)
    all_ok = bool(ok and gen)
    for path in (NEW_CHT, NEW_MM):
        _, good = retarget_context(path, gen)
        all_ok = all_ok and good
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    p("compiled", NEW_ABP, "status", abp.get_editor_property("status") if hasattr(abp, "get_editor_property") else "?")
    if SAVE:
        for path in (NEW_ABP, NEW_CHT, NEW_MM):
            p("saved", path, eal.save_asset(path, only_if_is_dirty=False))
    return all_ok

try:
    p("RESULT", "OK" if main() else "CHECK")
except Exception as e:
    p("EXCEPTION", repr(e))
flush()
