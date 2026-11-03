#!/usr/bin/env bash
# v10: model -> Cascadeur raw -> UE (bones) -> wrist fix on those bones -> Cascadeur final -> UE -> clearance + fresh bones.
set -e
cd "$(dirname "$0")"
TAKE=${TAKE:-v10}; CASC=jab_${TAKE}.casc; ASSET=AS_Pilot_Jab${TAKE^^}
python ${TAKE}_motion.py > sheets/${TAKE}_model_report.txt
for PASS in raw final; do
  TG=${TAKE}_targets_raw.json; [ $PASS = final ] && TG=${TAKE}_targets.json
  python csc_run.py v6_apply.py TAB=$CASC SAVE_AS=$CASC TARGETS=$TG | tail -1 | cut -c1-120
  python csc_run.py fingers_fist.py TAB=$CASC | tail -1 | cut -c1-30
  python csc_run.py export_current.py TAB=$CASC | tail -1 | cut -c1-80
  MSYS_NO_PATHCONV=1 python -I import_take.py jab_${TAKE}.fbx $ASSET
  rm -f sheets/raw/${TAKE}_legbones.json
  MSYS_NO_PATHCONV=1 python -c "import legibility as L; L.sample('/Game/AnimPipeline/Pilot/$ASSET', '$TAKE')"
  [ $PASS = raw ] && python wrist_fix.py $TAKE ${TAKE}_targets_raw.json ${TAKE}_targets.json 10 all --smooth=${SMOOTH:-6-36}
done
MSYS_NO_PATHCONV=1 python clearance.py /Game/AnimPipeline/Pilot/$ASSET 2 sheets/${TAKE}_clearance.json | tail -3
