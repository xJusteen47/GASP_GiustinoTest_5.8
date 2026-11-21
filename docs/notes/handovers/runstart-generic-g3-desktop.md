# runstart-generic g3 + sprinter s1 (desktop, 2026-11-21): both built, captured ASSERT OK, Fable PASS (narrow) -> owner
Branch claude/runstart-generic-desktop (continues c6bb1668). Lock _ue59 taken 02:57Z, freed at the end; owner editor reopened.
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): handover "g2 = Variant 6/16, ASSERT OK" confirmed
(Saved/RunStartGeneric/g2_h1/assert.txt).
## Done
- Composer runstart_generic_compose.py new env knobs (old defaults unchanged): HOLD/RATE_SAG (push plateau), ENTRY_LEAD (pelvis/legs
  lead the entry), KNEE_DEG/KNEE_A/KNEE_B (swing-leg hip flexion, shin folds back), PRE/RATE_PRE (load), LEAN_OUT, DROP_CM/DROP_IN/
  DROP_HOLD/DROP_OUT (pelvis drop, IK bends knees), ARM_BACK (hands/elbows back).
- g3 = Variant 7/17: TAG=g3 R0=2.6 N_REV=21 HOLD=14 RATE_SAG=0.3 ENTRY=5 ENTRY_LEAD=1.5 BOUNCE=0.3 LEAN_DEG=22 LEAN_IN=4.5 ARM=1.7 KNEE_DEG=25.
- s1 (sprinter, Antigravity approach, stock root) = Variant 8/18: TAG=s1 R0=2.3 PRE=3 RATE_PRE=0.8 N_REV=15 HOLD=10 RATE_SAG=0.1
  LEAN_OUT=18 ENTRY=4 ENTRY_LEAD=1 BOUNCE=0.3 LEAN_DEG=28 LEAN_IN=4 ARM=1.6 ARM_BACK=10 DROP_CM=14 DROP_IN=4 DROP_HOLD=10 DROP_OUT=17
  (torso 40-46 deg f4-12, pelvis 89.5 -> ~80 cm; python one-off on generic_s1_pose.npz).
- Casc scenes Saved/AnimPipeline/runstart/RunStart_Generic_{g3,s1}.casc (copies of g2; s1 keys > 59 deleted), FBX up Z baked.
  Cascadeur write max_err 68-78 cm is the IK re-solving calf/LimbDir points (knees, elbows), MainPoints of pelvis/feet/head < 3 cm.
- Real build (Run-Timed Build OK 22 s, cvar help lists 5-8). Clips + rows via Save-Assets (rows "enabled 4 (expected 4)").
- Captures: ASSERT OK both, min 1.00 f97-140 (Saved/RunStartGeneric/{g3,s1}_h1/assert.txt). Travel f0-200 base/g3/s1 1017.0/1017.0/
  1007.4 cm (g3 capture spawned at x -48.8). Per-contact L foot travel max: base 27.1, g3 33.3, s1 29.1 cm (metrics_*.json, foot z<13).
- Divergence row 21b (g2/g3/s1). Evidence + review_fable.md in docs/notes/reviews/runstart-{g3,s1}_h1 (mp4 frame n = v(n+60)).
## Reviews (Fable, life first) + thread look at the in-game start frames
- g3 PASS: rev holds to clip f14, releases v111-117, all 3 traits. Minor posa: v100-112 slightly crouched glide (option lean 18, bounce 0.5).
- s1 PASS narrow: low drive and ta-ta-TUM read but fast (~0.1 s per "ta"); elbows read forward-down. Fix if owner wants more:
  R0 2.0 held to f12, DROP_HOLD 12 / DROP_OUT 19, LEAN_OUT 20, ARM_BACK 14. Reviewer: g3 louder/more cartoony, s1 cleaner on ground.
- gioco (code queue, unchanged): entry blend 0.1 s + lean layer half-hide the first step (weight 0.29/0.5/0.81 at v85/90/95).
## Next: owner picks (g3, s1, or s1 with the longer-drive fix); posa rounds for g-line are now 3 of 3 (rule 10: owner decides).
COSTO: ~11 $ | ~8 min desktop | ~192k token review
