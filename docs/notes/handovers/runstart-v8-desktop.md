# runstart-v8-desktop: run start v4r8 (Luigi rev-up) applied, captured; review pending (desktop, 2026-11-20/21)
Branch claude/runstart-v8-desktop (from claude/project-thread-64vizj 83aabc3a). Lock _ue59 taken 23:20Z, freed at the end; owner's
editor reopened. Re-check (rule 11): brief's predicted N_NEW 38 / EXIT_AT 0.367 = what the script returned (below).

## Done
- Tag approved/runstart-v4r7 (83e6f1f3) created and pushed; row added in docs/notes/anim-approvate.md.
- Cascadeur: RunStart_v4r8.casc = copy of v4r7, runstart_retime_casc_v4r8.py default (TARGET_S 0.23, CUT 6-16, decay) returned
  N_REV 7, N_NEW 38, EXIT_AT 0.3667, rate0 4.14, V_END 499.1 cm/s, rev_dist 67 cm, s_min 0.3. Keys 38-58 deleted, range 0-37,
  export_joints bake up Z -> Saved/AnimPipeline/runstart/RunStart_v4r8.fbx.
  Script fix (attempt 1 failed): box local_rotation is csc.math.Rotation, not a vector -> read as quaternion (w(),x(),y(),z() are
  methods), written back with Rotation.from_quaternion.
- Gate: runstart_arm_clearance.py CL_RANGE 0-37: min +4.9 cm (f7 fist_l-thigh_l), no negative frame. Rev-up sheet
  docs/notes/reviews/runstart-v4r8/casc_revup_f0-12.png: no visible knee pop (low res, the review judges).
- UE through the OPEN editor (MCP execute_python, owner not in PIE): runstart_v4r8_ue.py RSV8_EXIT_AT 0.3667 -> AS_GSTN_RunStart_v4r8
  (Saved/AnimPipeline/runstart/AS_GSTN_RunStart_v4r8_build.json, curves = v4r7 set), rows v4r7 off / v4r8 on (Saved/AnimPipeline/rows/
  runstart_v4r8_new.txt, ESITO OK). In-editor GSTN_SAVE is unset, so the chooser was saved by hand (save_asset CHT_GSTN_StockMover_PoseMatch).
- DA_Locomotion_Default: before = stage 1 ramp 30 f / 0.15 (owner's value), after 14 f / 0.15; stamp and stage 2 untouched
  (docs/notes/reviews/runstart-v4r8/da_ramp_before.txt, da_ramp_after.txt). Restore: stage 1 accel_ramp_frames 30.
- pie_capture_move.py: mv_run_start_v5 expect moved from stale v4r5 to v4r8 f97-115.
- Capture (owner closed the editor): pie_capture_timed.ps1 KIT_Test_Overlord, gstn.Loco.Base 1, mv_run_start_v5, -Assert
  Saved/RunStartV8/assert_v4r8.json -> ASSERT: OK v4r8 f97-f115 100 % min 1.00 (Saved/RunStartV8/v4r8/assert.txt).
- Evidence (runstart_v4_evidence.py EV_CAPS v4r7 vs v4r8, base capture Saved/RunStartV7/v4r7, frames v84-144 step 4): grids az90/az0/game,
  runstart_v4r8.mp4, metrics in docs/notes/reviews/runstart-v4r8/. Clip weight: v4r8 1.0 v100-115, 0 from v130 (v4r7 held to v170).

## Open
- Planted-feet travel f80-240 L/R 46.6/45.0 cm vs v4r7 36.7/33.3 (metrics_summary.json): possible skating in the rev-up; no foot
  contact detected inside the 7 rev-up frames (build json contact onsets L 7.5/25.5, R 17/35.5).
- NO review yet: cloud thread (rule 10) with docs/notes/reviews/runstart-v4r8/brief_review.md. Only PASS goes to the owner.
  If FAIL on feet: brief step 6 (CUT (0,0), TARGET_S 0.27) is attempt 2.
- The owner already plays v4r8 + 14 f ramp in his editor (data only, no build).
COSTO: ~8 $ | ~15 min desktop | review 0 token
