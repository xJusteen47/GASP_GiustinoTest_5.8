# runstart-v4r4-elbows: Overlord run start, extra pose round 4 (desktop, 2026-11-20)
Branch claude/runstart-v4r4-elbows (from claude/runstart-v3-pose-2 e8cc43aa). _ue59 lock taken and freed. Re-check (rule 11): v4r3
assert_hires.txt read, ASSERT: OK f100-180 min 0.94 as stated.

## Owner feedback v4r3 -> result
Elbows too high ("zombie": upper arm stuck, only forearms move); back to v4r2 height, hands and everything else unchanged.
- runstart_elbow_targets_v4r4.py (offline, rig-point dumps Saved/AnimPipeline/runstart/v4r2_pts.json, v4r3_pts.json): target = v4r2
  elbow height (along torso up) at the same hand phase; rotation of the elbow about the shoulder->hand axis, only downward, smoothed,
  zero f0-11 and f>=59. Output v4r4_dpsi.json, max 40 deg (l) / 42 (r).
- runstart_elbows_casc_v4r4.py (Cascadeur, RunStart_v4r4.casc = copy of v4r3): forearm + hand points rotate rigidly about that axis.
  Result: hand drift 0.001 cm, shoulder 0.004 cm; elbow height above v4r2 f12-58 (python cmp, v4r4_pts.json): v4r3 mean +2.7 / +2.6
  cm (peak +9.1 / +10.5), v4r4 -0.1 / -0.1 (peak +1.9 / +1.8). Wrist (hand vs forearm) range identical to v4r3 (59-86 / 61-95 deg);
  peak per-frame hand rotation at 30 fps 66 / 60 deg vs 64 / 55 in v4r3 (run_python jump check in this thread).
- runstart_arm_clearance.py CL_RANGE (12,58): min +1.8 cm fist_l-chest f46 (= v4r3), 0 frames < 0; upper arm vs belly/thighs min +8.1 cm.
- Root travel 998.7 cm, curves contact_l/contact_r/phase, Block Transition: identical to v4r3 (AS_GSTN_RunStart_v4r4_build.json).

## Unreal
AS_GSTN_RunStart_v4r4 (Save-Assets runstart_v4_ue.py, RSV4_NAME/RSV4_FBX), rows: v4r3 off, v4r4 on (rows/runstart_v4r3off.txt,
runstart_v4r4_new.txt). Capture Saved/RunStartV4R4/hires (EDITOR_WIN=full, gstn.Loco.Base 1): ASSERT: OK f100-180 min 0.94.
Evidence docs/notes/reviews/runstart-v4r4/ (grids v4r2/v4r3/v4r4 per view). Review Fable r5: PASS with reservations
(review_fable_r5.md): wrist roll not verifiable from grids (covered by the numbers above); az0 v116/v136 elbows slightly open sideways
(low, optional). Owner videos owner/runstart_v4r4.mp4, _025x.mp4.

## Open
1. gioco: frozen frames f109/f112 on base 1 (unchanged, code queue).
2. posa (optional, Fable): elbows a bit open sideways at az0 v116/v136.
3. Next pose change needs the owner (extra round used).

COSTO: ~6 $ | ~5 min desktop | review ~0.12M token
