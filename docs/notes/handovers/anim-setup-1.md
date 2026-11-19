# Anim setup 1 (desktop _ue59, Opus, 2026-11-19 18:13-18:45 UTC) - audit + run stop hand-off PASS + skid turn side PASS
Branch claude/anim-setup-1 (from claude/editor-speed-up 57a33d20 = locomotion-finish-4 + speed-up). Build 1, ESITO OK (Saved/Logs/ci/build_setup1_1.log).
1. Audit + refactor plan: docs/notes/anim-setup-audit.md (designer-facing: GASP design, our additions, 9 findings rated G/C/O, 7-step plan).
2. Run stop, generic: IGSTN_PoseDriverSource (GSTNAnimationCore) -> any clip that owns the pose arms the blend-out foot lock like a
   montage; hand-off weight fades over RunStop.HandOffSeconds (0.75). Feet constant v255-299, right foot steps v300-308
   (Saved/AnimSetup1/cap1/step.txt from frames.jsonl). Fable PASS (docs/notes/reviews/anim-setup-1/review_fable.md).
3. Skid, generic: FGSTN_RunSkidTuning::TurnSide (Right = +yaw) + ReverseConeDeg 45, latched in Decide; cvar gstn.Skid.TurnSide.
   Both runs now turn +yaw (yaw column, Saved/AnimSetup1/cap1 vs Saved/Skid/cap6). Fable PASS; overshoot ~17 deg = follow-through, keep.
4. Speed-up side check: p1_after really was 1 player ('PIE worlds: 1'); 3 players one process = 125 ms clamp (<= 8 fps),
   Saved/SpeedUp/p3_one_after/result.txt. Captures now take PIE_CAPTURE_CLIENTS (0 = 1 player); used for this thread's capture.
Tests: 1839 Success, 1 Fail GSTN.Test.KitArchetypes.Content (KIT_Test_Light missing Stat.Acceleration/AirControl SetByCaller):
   kit content, not touched here, not in loco4's filtered run (Saved/Logs/ci/test_setup1_1.log.ue.log, count of "Result={Success}").
Open (Fable, gioco, old): skid right foot pop while planted at clip time 0.317->0.333 of SS_180_Turn_Root180 (hold f136-137 12.9 cm,
   release f137-138 14.8 cm): check the clip's foot track first (then dati montage) vs foot lock/contact_r; exit pops f145-149.
   Run stop minor: support foot slides 3.4 cm during the step. Capture note: run stop dust hides the feet; check feet without VFX.
Not started: run start stomp, air crouch landing (plan item 1: landing as a pose driver), plan items 2-7.
Applied choices: hand-off as an interface (not ABP edits), TurnSide default Right from the clip, 1-player captures.
Cost: ~35 min desktop, 1 build, 1 test run, 1 probe, 1 capture (3 seq), 1 Fable review.