# Skid in ABP_GSTN_Locomotion (desktop, 2026-11-18) - wiring DONE, review Fable FAIL (gioco)
Branch claude/project-thread-98dml5. Owner 1A: Claude wires the skid into the ABP copy with GSTNAnimToolsets (MCP). Lock _ue59 taken and released.

## Done (commit 5a644a8e)
- State Controller: the owner's copied Slide group became ONE state "Skid" (removed Skid Transition, "-> Skid" alias, "Re-Enter _1";
  cleared the copied OnStateEntry_SlideLoop binding). Sequence Evaluator on SkidOwner/SS_180_Turn_Root180, explicit time =
  GetAnimOwnerSkidClipTime (three sections). In: Locomotion Loop/Transition, Idle Loop/Transition/Idle Break -> Skid, rule
  IsAnimOwnerSkidding(), crossfade 0.08, priority 0. Out: Skid -> Conduit, !IsAnimOwnerSkidding(), crossfade 0.12.
- C++: static BlueprintPure IsAnimOwnerSkidding / GetAnimOwnerSkidClipTime (DefaultToSelf AnimInstance) on UGSTN_RunPresentationComponent.
  Build: Saved/Logs/ci/skid_wire_build1.log (`grep -c "Result: Succeeded"` = 1). They still raise the "potentially thread-unsafe" compile
  warning (7 lines) despite the BlueprintThreadSafe meta: harmless, to look at.
- Choosers (step 3): EvaluateChooser in SetBlendStackAnimFromChooser -> CHT_GSTN_LocomotionAnimations; Update_MotionMatching ->
  NEW copy CHT_GSTN_PoseSearchDatabases (of GASP's CHT_PoseSearchDatabases_Relaxed, original untouched). Both copies' context 0 is now
  ABP_GSTN_Locomotion_C: before that the copy ABP got "ContextData entry 0 expects SandboxCharacter_Mover_ABP_C" (LogChooser errors) =
  chooser picked nothing. Capture log: `grep -c "LogChooser: Error" docs/notes/reviews/skid-main-graph-1/cap/run.log.ue.log` = 0.
- Capture (MainAnimClass = ABP_GSTN_Locomotion, bSkidInMainGraph on): pie_capture_timed.ps1 -Kit KIT_Test_Overlord -Sequences mv_skid_base,
  VIEWS az90,game -> cap/ (ESITO OK, not committed, 66 MB). Grids + mp4 in docs/notes/reviews/skid-main-graph-1/, base = skid-root-1/cap2.

## Review Fable: FAIL (REVIEW-fable.md). Fixed: entry pop of base v58-59. FAIL, all "gioco":
- brake pose narrow/upright (no lunge), starts ~5 frames late; turn stops at ~90 deg in the state; exit v79->80 step; v80-94 sideways
  straddle gliding (restart slide kept, worse). Suspects: GASP post-state-machine nodes (Offset Root Bone / orientation warping / foot
  placement) eat the clip root yaw and widen/narrow the feet; actor yaw vs mesh yaw at exit; what the chooser/MM picks at v80-94.

## State left
- DA_AnimSet_Locomotion_Default REVERTED to the previous commit (MainAnimClass empty, skid in the layer): _ue59 plays as before.
  For the next capture set MainAnimClass = /Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion.ABP_GSTN_Locomotion_C and
  bSkidInMainGraph = true (Tools: prof.py logic, 3 lines of editor Python).
- Next thread (code, Opus high): debug the 4 suspects above with numbers from frames.jsonl, then recapture + Fable. Not done here: 1 attempt only.
- Decision applied (rule 6/12): single Skid state with the three-section clip (owner 2A, code 0127e6f4), NOT the demo's Loop/Exit two-clip
  machine in SkidLayer (SS_180_Turn2 is one motion, owner). SkidLayer in the ABP stays empty (the skid layer class handles the layer path).
- Not run: GSTN test suite (C++ change is two thin wrappers).
