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
# motion-curves gate (motion_curves.py on the model's points, about 1 s): every limb's amplitude, the anticipation and the
# arms' direction against the move's reference (move sheet "reference"). It runs FIRST and stops the pass on a failure: a move
# whose motion does not match the reference fails whatever the other checks say (Ganondorf v2). CURVES_GATE=0 reports only.
# CURVES_PY: a Python with pybvh/dtaidistance (the animcurves venv); plain python works too (numpy fallbacks).
CURVES_PY=${CURVES_PY:-python}
STEP=curves t bash -o pipefail -c "$CURVES_PY motion_curves.py $TAKE --source=model --quiet > sheets/${TAKE}_curves_gate.txt || true"
tail -1 sheets/${TAKE}_curves_gate.txt
if [ "${CURVES_GATE:-1}" != 0 ] && ! python -c "import json,sys; sys.exit(1 if json.load(open('sheets/${TAKE}_model_curves.json')).get('reference',{}).get('fail') else 0)"; then
  sed -n '/^reference/,$p' sheets/${TAKE}_model_curves.txt; echo "motion curves failed against the reference: pass stopped (CURVES_GATE=0 to override)"; exit 1
fi
# pre-flight (preflight.py, < 0.1 s): the checks on the model's rig points before any Cascadeur/Unreal work.
# Report in sheets/${TAKE}_preflight.txt/.json; PREFLIGHT_GATE=1 stops the pass when it flags anything.
STEP=preflight t bash -o pipefail -c "python preflight.py ${TAKE}_targets_raw.json $TAKE > sheets/${TAKE}_preflight.txt || true"
head -1 sheets/${TAKE}_preflight.txt
if [ -n "$PREFLIGHT_GATE" ] && ! python -c "import json,sys; sys.exit(1 if json.load(open('sheets/${TAKE}_preflight.json'))['flags'] else 0)"; then
  cat sheets/${TAKE}_preflight.txt; echo "pre-flight flagged: pass stopped (PREFLIGHT_GATE=1)"; exit 1
fi
# bone lengths on the model's points (bone_lengths.py, < 0.1 s): rigid pairs keep the idle's distances. A model that moves
# trunk segments on their own stretches the chain (Ganondorf v3: head point 17 cm off the neck). BONELEN_GATE=0 reports only.
STEP=bone_lengths t bash -o pipefail -c "python bone_lengths.py ${TAKE}_targets_raw.json --ref=idle_points.json > sheets/${TAKE}_bonelen_model.txt || true"
head -1 sheets/${TAKE}_bonelen_model.txt
if [ "${BONELEN_GATE:-1}" != 0 ] && grep -q '^FAIL' sheets/${TAKE}_bonelen_model.txt; then
  cat sheets/${TAKE}_bonelen_model.txt; echo "bone lengths changed in the model: pass stopped (BONELEN_GATE=0 to override)"; exit 1
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
  # Cascadeur's own solve stretches clavicles/spine a little (v2: 0.4 cm): report only on its bones
  python bone_lengths.py sheets/raw/${TAKE}_legbones.json > sheets/${TAKE}_bonelen_$PASS.txt || true; head -1 sheets/${TAKE}_bonelen_$PASS.txt
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
