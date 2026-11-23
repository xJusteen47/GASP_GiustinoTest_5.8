#!/usr/bin/env bash
# One v7 iteration: targets -> Cascadeur (jab_v7.casc) -> FBX -> Unreal AS_Pilot_JabV7 (not saved) -> clearance + metrics.
set -e
cd "$(dirname "$0")"
python v7_motion.py > sheets/v7_model_report.txt
python csc_run.py v6_apply.py TAB=jab_v7.casc SAVE_AS=jab_v7.casc TARGETS=v7_targets.json | tail -1 | cut -c1-300
python csc_run.py export_current.py TAB=jab_v7.casc | tail -1 | cut -c1-160
python -I import_take.py jab_v7.fbx AS_Pilot_JabV7
MSYS_NO_PATHCONV=1 python clearance.py /Game/AnimPipeline/Pilot/AS_Pilot_JabV7 2 sheets/v7_clearance.json | tail -12
python csc_run.py metrics.py > v7_metrics_csc.txt 2>&1
python -c "
import json
t=open('v7_metrics_csc.txt').read(); j=json.loads(t[t.index('{'):t.rindex('}')+1])
for k in ('1-3','3-8','8-17','17-37'): print('metrics', k, j[k]['top_ang_speed_deg_per_frame'][:3], 'feet', [j[k][x] for x in ('foot_l_drift_cm','foot_r_drift_cm','ball_l_drift_cm','ball_r_drift_cm')])"
