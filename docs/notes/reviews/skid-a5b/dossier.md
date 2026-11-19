# Skid attempt 5b dossier (2026-11-19)
Same game change as attempt 5 (docs/notes/reviews/skid-a5/dossier.md, review there: FAIL). One change since: the curve movedata_speed is
REMOVED from the skid clip SS_180_Turn_Root180 (and the run stop clips). ABP Get_DynamicPlayRate plays any clip that has it at capsule
speed / curve speed, so the skid slowed and popped near speed 0 (a5 findings 1-2). contact_l/r, phase and Foley notifies stay.
Capture Saved/LocoR2/cap1 (v = PRE 20 + sequence frame): mv_skid_release (reverse v115, release v145), mv_skid_hold (reverse v115,
held to v265, then released). Grids: mv_skid_release_from110_az90.png, mv_skid_hold_from110_az90.png, mv_skid_hold_from206_az90.png.
Frames Saved/LocoR2/cap1/<seq>/{az90,az45,game}/fNNN.png, rows <seq>/frames.jsonl. ABP state still not logged.
