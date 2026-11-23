"""LANDING-SYNC (2026-11-19). Owner: the landing dust appeared while the body was still in the jump pose. The dust fires on the simulation's
touchdown frame (GSTN_PresentationLogic, Land event); the ABP enters its landing state on the same frame (JustLanded_*: MovementMode OnGround,
last frame InAir) but every landing row of CHT_GSTN_LocomotionAnimations (GASP's) blends its clip in over 0.2 s QuadraticInOut, so the pose is
about half way 6 frames later. Owner principle for landings: aerial -> landing is a near-hard cut (impact, weight).
This sets BlendTime (env LAND_BLEND, default 0.05 s = 3 frames) and optionally BlendCurve (env LAND_CURVE) on every row of the root chooser and
its nested choosers whose result is an animation with "_Land" in its name. Idempotent. Report: Saved/Routing/land_blend.txt.
Dry run: env LAND_DRY=1. Headless: copy to a path without spaces, -run=pythonscript."""
import os, re
import unreal

OUT = unreal.Paths.project_saved_dir() + "Routing/land_blend.txt"
ROOT = "/Game/GSTN/Animation/Locomotion/CHT_GSTN_LocomotionAnimations"
BLEND = float(os.environ.get("LAND_BLEND", "0.05"))
CURVE = os.environ.get("LAND_CURVE", "")
DRY = os.environ.get("LAND_DRY", "") == "1"
C = unreal.GSTN_ChooserTableToolset.get_default_object()
def c(fn, *a): return C.call_method(fn, tuple(a))
log = []
seen = set()
changed = 0

def walk(cht):
    global changed
    if not cht or cht.get_path_name() in seen:
        return
    seen.add(cht.get_path_name())
    info = c("GetChooserTable", cht)
    for ri, r in enumerate(info.rows):
        asset = str(r.asset or "") + str(r.result)
        if "_Land" not in asset:
            continue
        name = re.findall(r"\.([A-Za-z0-9_]+)'", asset)
        for ci, cell in enumerate(r.cells):
            cell = str(cell)
            if "BlendTime_" not in cell:
                continue
            new = re.sub(r"(BlendTime_[A-Za-z0-9_]+=)[0-9.]+", lambda m: m.group(1) + "%f" % BLEND, cell)
            if CURVE:
                new = re.sub(r"(BlendCurve_[A-Za-z0-9_]+=)[A-Za-z]+", lambda m: m.group(1) + CURVE, new)
            old = re.search(r"BlendTime_[A-Za-z0-9_]+=([0-9.]+)", cell).group(1)
            log.append("%s row %d %s blend %s -> %.3f%s" % (cht.get_name(), ri, name[-1] if name else "?", old, BLEND, "" if new != cell else " (same)"))
            if new != cell and not DRY:
                c("SetChooserCell", cht, ci, ri, new)
                changed += 1
    for n in info.nested_choosers:
        walk(n)

walk(unreal.load_asset(ROOT))
if changed and not DRY:
    log.append("saved %s" % unreal.EditorAssetLibrary.save_asset(ROOT, only_if_is_dirty=False))
log.append("changed %d" % changed)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
