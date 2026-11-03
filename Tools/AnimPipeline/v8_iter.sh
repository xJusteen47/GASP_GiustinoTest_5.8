#!/usr/bin/env bash
# One v8 iteration: targets -> Cascadeur (jab_v8.casc, body then fists) -> FBX -> Unreal AS_Pilot_JabV8 (not saved) -> clearance.
set -e
cd "$(dirname "$0")"
python v8_motion.py > sheets/v8_model_report.txt
python csc_run.py v6_apply.py TAB=jab_v8.casc SAVE_AS=jab_v8.casc TARGETS=v8_targets.json | tail -1 | cut -c1-260
python csc_run.py v8_fingers.py | tail -1 | cut -c1-60
python csc_run.py export_current.py TAB=jab_v8.casc | tail -1 | cut -c1-120
MSYS_NO_PATHCONV=1 python -I import_take.py jab_v8.fbx AS_Pilot_JabV8
MSYS_NO_PATHCONV=1 python clearance.py /Game/AnimPipeline/Pilot/AS_Pilot_JabV8 2 sheets/v8_clearance.json | tail -9
