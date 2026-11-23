#!/usr/bin/env bash
# One v9 iteration: v9_targets.json -> Cascadeur (jab_v9.casc, body+hands then fists) -> FBX -> Unreal AS_Pilot_JabV9 -> clearance,
# fresh bone samples, wrist check.
set -e
cd "$(dirname "$0")"
python csc_run.py v6_apply.py TAB=jab_v9.casc SAVE_AS=jab_v9.casc TARGETS=v9_targets.json | tail -1 | cut -c1-200
python csc_run.py fingers_fist.py TAB=jab_v9.casc | tail -1 | cut -c1-40
python csc_run.py export_current.py TAB=jab_v9.casc | tail -1 | cut -c1-100
MSYS_NO_PATHCONV=1 python -I import_take.py jab_v9.fbx AS_Pilot_JabV9
MSYS_NO_PATHCONV=1 python clearance.py /Game/AnimPipeline/Pilot/AS_Pilot_JabV9 2 sheets/v9_clearance.json | tail -4
rm -f sheets/raw/v9_legbones.json
MSYS_NO_PATHCONV=1 python -c "import legibility as L; L.sample('/Game/AnimPipeline/Pilot/AS_Pilot_JabV9', 'v9')"
python wrist_check.py v9
