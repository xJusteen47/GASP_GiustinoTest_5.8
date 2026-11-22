# Run stop (Ganondorf-style) - desktop, 2026-11-18/19 - wiring DONE, clip review FAIL (posa)
Committed on claude/desktop-jab-fk (the _ue59 checkout), owner 2026-11-19 00:07: "ok commit run stop su _ue59". Lock _ue59 taken and released. Brief: coordinator note + docs/notes/mosse/run-start-stop-skid-brief.md section 2.
Scope (owner 22:24Z): stop = stick let go with NO reverse input; reverse then release stays the skid clip.

## Done
- C++ (GSTNCombatNP): FGSTN_RunStopSettings in the locomotion profile (DA_AnimSet_Locomotion_Default.RunStop), FGSTN_RunStopTrack
  (clip root speed at 60 Hz) and FGSTN_RunStopState (start after ConfirmFrames with the stick < deadzone from >= MinEntrySpeed; clip time
  speed-matched to the sim brake, then SettleRate; ends on stick, air, crouch, skid, slide, MaxBrakeSeconds). Foot: the higher foot at
  release plants in front. Thread-safe statics IsAnimOwnerRunStopping/Blending, IsAnimOwnerRunStopLeftFoot, GetAnimOwnerRunStopClipTime.
  Cvars: gstn.RunStop.Mode (-1 profile, 0 off, 1 clip, 2 blend), gstn.RunStop.Log 1 (LogTemp line per frame). DEFAULT MODE = OFF.
- ABP_GSTN_Locomotion (script Tools/GAS/run_stop_states.py, headless, rebuilds its own nodes): alias "-> Run Stop" (Locomotion
  Loop/Transition, Idle Loop/Transition) -> "Run Stop" (0.06 s) and -> "Run Stop Blend" (0.3 s); both -> Conduit 0.2 s; pinned node
  comment on the alias (the toolset refuses comment boxes in the state machine graph); comment box inside Run Stop. Compile: up to date
  with the known thread-unsafe warnings (as Skid). Backup: Saved/Backups/RunStop/ABP_GSTN_Locomotion.before.uasset.
- Build: Saved/Logs/ci/runstop_build6.log (`grep -c "Result: Succeeded"` = 1). Tests GSTNCombat.NP.RunStop: runstop_test2.log.ue.log
  (`grep -c "Result={Success}"` = 2, Fail 0).
- Capture + review: docs/notes/reviews/run-stop-1/REVIEW.md (FAIL posa; timing OK). Capture sequence mv_run_stop_base added.

## Open / next
- Clip: GASP Neutral stop is upright (no lunge, torso up before the feet stop). Brief's fallback: a new pose-based stop clip
  (Cascadeur/agy) on the brief's frame table (plant + pelvis -6..8 cm + torso still leaning, rise frames 11-16 with 4 deg overshoot, arms
  open low). Opus/Fable thread; set DA RunStop.ClipLeftFoot/RightFoot to it, Mode = Clip after a PASS.
- Owner's Mixamo packs (Downloads/...UnigramPreview.../Sword and Shield Pack, Male Locomotion Pack): no stop clip, only turns; no root
  motion (owner) -> FGSTN_RunStopTrack would be invalid: a clip without root motion needs a time-based brake (not written).
- Owner: the knight clip fits the SPRINT end stop better, not the base stop (scope noted, rule 12). Walk turnaround not measured.
- To commit (branch claude/run-stop-1): the 5 C++ files, ABP_GSTN_Locomotion.uasset, Tools/GAS/run_stop_*.py, Tools/GASNPPTest/run_stop_*.py +
  pie_capture_move.py, docs/notes/reviews/run-stop-1/, this file. Checkout is on claude/desktop-jab-fk with other threads' changes.
