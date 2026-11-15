# Overlord DAir v9 code fix: review package (captured 2026-11-15 night)
Variant = branch `claude/project-thread-anim-r2-int` (anim-r2 + CP2 + montage fix n216qs 8ba977e1), built, captured with
`python Saved/AnimR2/run_capture.py KIT_Test_OverlordDAirV9 Saved/AnimR2/dair/variant dair_whiff`. Base = old code, `Saved/AnimR2/dair/base/` (round 1).
Both use the local, uncommitted `AM_OverlordDAirV9.uasset` of the desktop (left dirty on purpose since round 1).
Files: `dair_whiff_{base,variant}.mp4` (60 fps) and `_slow` (15 fps), `g1_dair_v36-80.jpg` (rows: variant game, variant az90, base game, base az90; v36-80 every 4).
Attack v37, montage v38-v70 in both (last frame 32.94 variant / 32.78 base); idle by v72-v76. Heights identical except v50 (225.9 vs 222.8) and v72 (226.2 vs 229.4).
At a glance (not the rule-10 review): variant and base read almost the same on the grid; the difference is in the exit frames v70-v76.
