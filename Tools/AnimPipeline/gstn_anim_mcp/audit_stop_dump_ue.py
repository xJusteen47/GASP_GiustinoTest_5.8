"""Run in UE (headless, saves nothing), audit stop-and-go 2026-11-24: resolve clip NAMES or name patterns to AnimSequence
paths through the asset registry, then run clip_joints_dump_ue.py on them (same JSON, Saved/AnimPipeline/runstart/joints_<name>.json).
Why names: the content reorg (claude/content-reorg-apply) moved folders, so the audit must not hard-code /Game paths.
  & .\\Tools\\CI\\Save-Assets.ps1 -Script Tools\\AnimPipeline\\gstn_anim_mcp\\audit_stop_dump_ue.py -Tag audit_stop
Env AUDIT_CLIPS = comma-separated names or fnmatch patterns (default: the audit list below).
Writes Saved/AnimPipeline/audit_stop/resolved.json {pattern: [paths]}; prints <= 4 lines."""
import fnmatch
import json
import os

import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "audit_stop")
os.makedirs(OUT, exist_ok=True)
DEFAULT = [
    # run start (approved / in game)
    "AS_GSTN_RunStart_v4r5", "AS_GSTN_RunStart_v4r7", "AS_GSTN_RunStart_Generic_w1", "AS_GSTN_RunStart_Generic_w2",
    "AS_GSTN_RunStart_Generic_w3",
    # turnarounds: v27 (Lp), v28 style (Ls) and the other directions (s), with their bases (f) for the inherited parts
    "AS_GSTN_RunStart_Generic_w2t180Lp", "AS_GSTN_RunStart_Generic_w2t*s", "AS_GSTN_RunStart_Generic_w2t*f",
    # combat (card builds and pilots: the audit dumps both, the table says which one the kit plays)
    "AS_GanonFTiltV31_Build", "AS_Pilot_GanonFTilt_V3", "AS_Pilot_OverlordDAir_V4", "AS_OverlordDAirV4_Build",
    "AS_Pilot_OverlordDAirLand*", "AS_Pilot_JabV12", "AS_OverlordJab1V3_Build", "AS_OverlordJab2V3_Build",
    "AS_Pilot_OverlordJab*_V3", "AS_GSTN_Dash_Ganon*", "AS_Pilot_OverlordDropkick_StartupK7",
    "AS_Pilot_OverlordDropkick_SettleK6", "AS_Pilot_OverlordDropkick_LoopK6", "AS_*Dropkick_Land*", "AS_*Dropkick*TechBounce*",
    # reactions, getups and generic hits (every React clip: the r3 getups and the hit audit clips by pattern)
    "AS_React_*", "AS_Pilot_React_*", "AS_*GetUp*", "AS_*Getup*",
]
pats = [p.strip() for p in os.environ.get("AUDIT_CLIPS", "").split(",") if p.strip()] or DEFAULT
reg = unreal.AssetRegistryHelpers.get_asset_registry()
flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence")], package_paths=["/Game"],
                      recursive_paths=True)
allp = {str(a.asset_name): str(a.package_name) for a in reg.get_assets(flt)}
res, paths = {}, []
for p in pats:
    hit = sorted(v for k, v in allp.items() if fnmatch.fnmatchcase(k, p))
    res[p] = hit
    paths += [h for h in hit if h not in paths]
json.dump(res, open(os.path.join(OUT, "resolved.json"), "w"), indent=1)
miss = [p for p, h in res.items() if not h]
print("AUDIT_STOP resolved %d clips from %d patterns; missing: %s" % (len(paths), len(pats), ", ".join(miss)[:300] or "none"))
os.environ["CJD_ASSETS"] = ",".join(paths)
DUMP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "clip_joints_dump_ue.py")
exec(compile(open(DUMP).read(), DUMP, "exec"), {"__name__": "__main__", "__file__": DUMP})
