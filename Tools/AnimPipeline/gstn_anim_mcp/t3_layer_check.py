"""T3 (2026-11-24): first real UE run of anim_layer. src = w3 rebuilt with W3_ARM_R_OUT=0 (AS_GSTN_RunStart_Generic_w3_T3src),
stack = STEP_C1_02 of tests/test_anim_layer.py, dst = AS_GSTN_RunStart_Generic_w3_T3lay; then compare dst joints with w3.
Writes Saved/AnimPipeline/layers/t3_layer_check.json, prints <= 5 lines."""
import json
import math
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "tests"))
import anim_layer_ops as O  # noqa: E402
from test_anim_layer import STEP_C1_02  # noqa: E402

REPO = HERE.parents[2]
SRC, DST = "AS_GSTN_RunStart_Generic_w3_T3src", "AS_GSTN_RunStart_Generic_w3_T3lay"
out = {"steps": []}
r = O.anim_layer("dump", dst=DST, src=SRC, root=REPO, worktree_dir=str(REPO))
out["steps"].append({"dump": r})
if r.get("ok", True):
    for lay in STEP_C1_02:
        out["steps"].append({"add": O.anim_layer("add", dst=DST, src=SRC, layer=lay, root=REPO)})
    out["steps"].append({"build": O.anim_layer("build", dst=DST, src=SRC, root=REPO, worktree_dir=str(REPO))})
# compare joints: w3 (approved) vs dst
rs = REPO / "Saved" / "AnimPipeline" / "runstart"
w3p = REPO / "docs" / "notes" / "life-calib-run" / "dumps" / "joints_AS_GSTN_RunStart_Generic_w3_t3.json"
dp = rs / f"joints_{DST}.json"
cmp = {"w3": str(w3p), "dst": str(dp)}
if dp.exists():
    a, b = json.load(open(w3p))["joints"], json.load(open(dp))["joints"]
    per = {}
    for k in a:
        if k in b:
            n = min(len(a[k]), len(b[k]))
            per[k] = round(max(math.dist(a[k][i], b[k][i]) for i in range(n)), 3)
    cmp["max_cm_per_joint"] = per
    cmp["max_cm"] = max(per.values()) if per else None
out["compare"] = cmp
p = REPO / "Saved" / "AnimPipeline" / "layers"
p.mkdir(parents=True, exist_ok=True)
(p / "t3_layer_check.json").write_text(json.dumps(out, indent=1, default=str), encoding="utf-8")
print("LAYER dump ok=%s" % out["steps"][0]["dump"].get("ok"))
if len(out["steps"]) > 1:
    print("LAYER build: %s" % json.dumps(out["steps"][-1]["build"], default=str)[:300])
print("COMPARE max_cm=%s" % cmp.get("max_cm"))
