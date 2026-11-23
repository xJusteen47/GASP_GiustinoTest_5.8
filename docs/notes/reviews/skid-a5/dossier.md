# Skid attempt 5 dossier (2026-11-19, desktop _ue59, branch claude/locomotion-finish-3)

Previous: docs/notes/reviews/locomotion-finish-3/review_fable_skid_a4.md (FAIL: first plant slide fixed 111 -> 13 cm, but a stick
release during Skid Exit Run fell in the hand-back blend: 20 frozen frames, 84 cm slide; the loop start dragged 41 cm).
The skid clip is the owner's SS_180_Turn_Root180 (approved shape, not changed). Owner principle: the skid is one motion; the exit
goes into the run loop if the stick is held, into a stop facing the new way if released.

## Changes (attempt 5)
- New C++ getter UGSTN_RunPresentationComponent::GetAnimOwnerStickAlong (move input along the velocity, -1..1).
- ABP (Tools/GAS/skid_exit_run.py): Skid -> Skid Exit Run also needs StickAlong >= 0.3; Skid Exit Run -> Conduit leaves AT ONCE when
  StickAlong < 0.3 (was: only after 0.75 s or speed < 150). Run loop start 4 frames later than the matched phase (2.1833 s).
- Also new since a4: GASP curves + Foley notifies on the skid clip (contact_l/r, movedata_speed, phase), landing blend change
  (unrelated), run stop V5/V6 work (unrelated).

## Capture Saved/Skid/cap5 (KIT_Test_Playtest, gstn.RunStop.Mode 0; capture frame v = PRE 20 + sequence frame)
- mv_skid_release: run forward, stick reversed at v115, released at v145 (early, inside Skid Exit Run).
- mv_skid_hold: same reverse at v115, stick held to v205 (1.5 s), then released.
- Grids here: <seq>_az90.png (v110-204 every 2nd frame). Videos 1080p az90|game: <seq>_x1.mp4, _x4.mp4 (from v100).
- Frames: Saved/Skid/cap5/<seq>/{az90,game}/fNNN.png, rows <seq>/frames.jsonl (body: feet/pelvis/hands relative to the actor, actor
  world position). The ABP state is NOT logged (the logging call failed); infer it from the pose and the actor motion.
