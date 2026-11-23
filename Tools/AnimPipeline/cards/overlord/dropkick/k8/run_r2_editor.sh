#!/usr/bin/env bash
# K8 round 2, the editor part (run in Git Bash from Tools/AnimPipeline once editor_session.py is ready). Prints a few lines per step.
# Then: echo quit > ../../Saved/AnimPipeSession/quit.txt and capture_k8_r2.ps1.
set -u
C=cards/overlord/dropkick/overlord_dropkick_startup_k8.json
S="$(pwd)/cards/overlord/dropkick/k8/ue_step.py"
CALL="python $TEMP/k8_call.py"      # writes the bridge call (see dropkick-k8-1.md); recreated below if missing
[ -f "$TEMP/k8_call.py" ] || cat > "$TEMP/k8_call.py" <<'PY'
import sys, os, json
sys.path.insert(0, os.getcwd()); import ue
f, entry = sys.argv[1], sys.argv[2]
kw = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
r = ue.call("run_python_file", {"filePath": f.replace("\\", "/"), "entryPoint": entry, "kwargs": kw}, timeout=900)
print(str(r.get("result"))[:400]); print("errors:", r.get("logErrorCount"))
PY
FOLD_STARTS=3 python cards/overlord/dropkick/k8/make_k8_cards.py | tail -2
for v in author build check; do timeout 900 python -m animpipe $v $C 2>&1 | head -1; done
$CALL "$S" delete '{"path":"/Game/GASNPPTest/Combat/Overlord/AS_OverlordDropkick_StartupK8"}'
$CALL "$S" run '{"script":"Tools/GAS/setup_dropkick_kick_variant.py","env":{"DROPKICK_KICK_ROUND":"K8","DROPKICK_KICK_SETTLE":"K6","DROPKICK_KICK_LOOP_ROUND":"K6"}}'
tail -2 ../../Saved/DropkickK5/setup_dropkick_kick_variant.txt
$CALL "$S" run '{"script":"Tools/GAS/setup_dropkick_hitboxes.py","env":{"DROPKICK_TL":"TL_Overlord_DropkickTBK8","DROPKICK_HITBOX_DRY":"0","SETUP_FORCE":"TL_Overlord_DropkickTBK8"}}'
tail -1 ../../Saved/AnimR2/setup_dropkick_hitboxes.txt
$CALL "$S" run '{"script":"Tools/AnimPipeline/gstn_anim_mcp/clip_joints_dump_ue.py","env":{"CJD_ASSETS":"/Game/GASNPPTest/Combat/Overlord/AS_OverlordDropkick_StartupK8","CJD_FACING":"y","CJD_RANGE":""}}'
