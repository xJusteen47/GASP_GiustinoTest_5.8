#!/usr/bin/env bash
# Ganondorf ftilt: model -> Cascadeur (keys) raw -> UE bones -> wrist fix -> Cascadeur final -> UE -> clearance.
# Per-step wall-clock seconds go to sheets/${TAKE}_timings.tsv (owner, 31/10: find where the time goes).
set -e
cd "$(dirname "$0")"
TAKE=${TAKE:-ganon_v1}; CASC=${CASC:-ganon_ftilt_v1.casc}; ASSET=${ASSET:-AS_Pilot_GanonFTilt_V1}
TIM=sheets/${TAKE}_timings.tsv; RUN=$(date +%H:%M:%S)
# A failing step stops the pass: steps run under bash -o pipefail, so an error is not hidden by "| tail -1" (a silent Cascadeur
# failure once left the old keys in the scene and every check ran on a stale clip).
t() { local s=$(date +%s%N) rc=0; "$@" || rc=$?; local e=$(date +%s%N); printf "%s	%s	%s
" "$RUN" "$STEP" "$(( (e - s) / 100000000 ))e-1" >> $TIM; [ $rc = 0 ] || { echo "step $STEP failed (exit $rc)"; exit $rc; }; }
STEP=model t bash -o pipefail -c "python ${MODEL:-ganon_ftilt_motion.py} > sheets/${TAKE}_model_report.txt"
# pre-flight (preflight.py, < 0.1 s): the checks on the model's rig points before any Cascadeur/Unreal work.
# Report in sheets/${TAKE}_preflight.txt/.json; PREFLIGHT_GATE=1 stops the pass when it flags anything.
STEP=preflight t bash -o pipefail -c "python preflight.py ${TAKE}_targets_raw.json $TAKE > sheets/${TAKE}_preflight.txt || true"
head -1 sheets/${TAKE}_preflight.txt
if [ -n "$PREFLIGHT_GATE" ] && ! python -c "import json,sys; sys.exit(1 if json.load(open('sheets/${TAKE}_preflight.json'))['flags'] else 0)"; then
  cat sheets/${TAKE}_preflight.txt; echo "pre-flight flagged: pass stopped (PREFLIGHT_GATE=1)"; exit 1
fi
KEYS=$(python -c "import json; print(','.join(str(k) for k in json.load(open('${TAKE}_targets_raw.json'))['keyplan']['keys']))")
for PASS in raw final; do
  TG=${TAKE}_targets_raw.json; [ $PASS = final ] && TG=${TAKE}_targets.json
  STEP=cascadeur_keys_$PASS t bash -o pipefail -c "python csc_run.py sparse_apply.py TAB=$CASC SAVE_AS=$CASC TARGETS=$TG | tail -1 | cut -c1-200"
  STEP=cascadeur_fists_$PASS t bash -o pipefail -c "python csc_run.py fingers_fist.py TAB=$CASC FRAMES=$KEYS | tail -1 | cut -c1-30"
  STEP=export_fbx_$PASS t bash -o pipefail -c "python csc_run.py export_current.py TAB=$CASC | tail -1 | cut -c1-120"
  STEP=ue_import_$PASS t bash -o pipefail -c "MSYS_NO_PATHCONV=1 python -I import_take.py ${CASC%.casc}.fbx $ASSET"
  rm -f sheets/raw/${TAKE}_legbones.json
  STEP=ue_sample_bones_$PASS t bash -o pipefail -c "MSYS_NO_PATHCONV=1 python -c \"import legibility as L; L.sample('/Game/AnimPipeline/Pilot/$ASSET', '$TAKE')\""
  if [ $PASS = raw ]; then
    STEP=wrist_fix t bash -o pipefail -c "python wrist_fix.py $TAKE ${TAKE}_targets_raw.json ${TAKE}_targets.json 10 all --smooth=${SMOOTH:-6-36} > /dev/null"
  fi
done
STEP=clearance t bash -o pipefail -c "MSYS_NO_PATHCONV=1 python clearance.py /Game/AnimPipeline/Pilot/$ASSET 2 sheets/${TAKE}_clearance.json | tail -2"
if [ -n "$CHECKS" ]; then
  STEP=motion_check t bash -o pipefail -c "python motion_check.py $TAKE > sheets/${TAKE}_motion.txt 2>&1 || true"
  STEP=tangency t bash -o pipefail -c "python tangency.py $TAKE > sheets/${TAKE}_tangency.txt 2>&1 || true"
  STEP=legibility t bash -o pipefail -c "MSYS_NO_PATHCONV=1 python legibility.py measure /Game/AnimPipeline/Pilot/$ASSET $TAKE 9-11 37 > /dev/null 2>&1 || true"
fi
