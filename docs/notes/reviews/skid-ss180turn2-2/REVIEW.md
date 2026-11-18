# Skid SS_180_Turn2_Src (owner's original clip) - PASS on the two checks asked (facing, pelvis)
Capture 2026-11-18 08:20, KIT_Test_Overlord, mv_skid_base, PIE_CAPTURE_VIEWS=az90,game. Grid grid_rf_az90.png (frames 48-92), mp4 skid_rf.mp4 (0.25x, frames 20-110).
Layer data: setup_skid_anim_layer.py ESITO OK, skid_clip = SkidOwner/SS_180_Turn2_Src (Saved/Logs/ci/py_setup_skid_anim_layer2.log, Select-String 'skid_clip'); commit 6ad957a8.
1. Facing: no double turn. v48-56 runs right, v57-65 brakes and turns, v66-80 low push-off to the left with dust, v84-92 runs left facing left (dust behind). Looked at the grid once.
2. Pelvis: no sinking. Pelvis over capsule bottom 64-90 cm in frames 40-100, min of the whole capture 54.6 (f159, idle at the end); round 1 was -74..-109 (body_metrics.txt, from frames.jsonl: pelvis_z - cap_bottom_z).
Notes (not judged here, for a full Fable review if the owner wants it): largest pelvis step 11.3 cm at f56 (skid entry, faster than run steps); StopFraction 0.6 still the default.
