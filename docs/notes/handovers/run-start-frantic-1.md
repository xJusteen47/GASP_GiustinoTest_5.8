# Handover: frantic run start 1 (cloud + desktop no-editor, 2026-11-19) - branch claude/project-thread-96rsau
Brief: docs/notes/reviews/run-start-frantic/brief.md (owner 23:41-00:09Z: frantic start, Toad timing, Ganon weight, choice B stamp
in place, L3 = Smash dash skips it, dash ledge rule). Copy: project files handoffs/run-start-frantic-brief.md.

## Done
- Mixamo survey (survey/, 2eea2197): 61 clips, none fits -> hand-made.
- Sim code, NOT compiled: 69feafe1 + 024f787f. FGSTN_RunStagesTuning::StartStamp (stage 1 accel x AccelFraction for Frames from
  still, stick >= StickMin) and ::BoostDash (boost works during the whole stamp, ends it, accel x AccelMultiplier for Frames).
  Counters StampFrames / DashFrames in FGSTN_RunStageState (==, compact serialize, ShouldReconcile, ToString). Off by default;
  Tools/GAS/setup_run_start_stamp.py turns them on in DA_Locomotion_Default (21 f x0.05 stick 0.5; dash 6 f x8).
  Presentation getters (BlueprintThreadSafe, cached on game thread): GetAnimOwnerRunStartTime, GetAnimOwnerBoostDashTime (-1 = none).
  Tests: GSTNCombat.Core.RunStages.StartStamp, .BoostDash (new, not run).
- Clip: r1, r2 Fable FAIL (r*/REVIEW-fable.md); r3 593e2aa1 passes every numeric gate (r3/README.md table). 27 f @30 fps, root in
  place, UEFN skeleton from Saved/AnimPipeline/refs/idle FBX. Authored by script (agy timed out), not by agy.

## Next (desktop, _ue59 lock; was held by "crouch veloce 2" at 01:10 local, which ALSO edits ABP_GSTN_Locomotion: start after it)
1. Merge this branch, Run-Timed build + tests GSTNCombat.Core.RunStages + GSTNCombat.NP.
2. setup_run_start_stamp.py; import r3 FBX as AS_GSTN_RunStart_Frantic (check scale/root with bone_lengths.py).
3. ABP_GSTN_Locomotion: RunStart state via alias from Idle (GetAnimOwnerRunStartTime >= 0), Sequence Evaluator explicit time, exit at
   clip end or < 0; warping ON. Re-key/trim f27 to M_Neutral_Run_Loop_F's phase (unverified).
4. Capture base (stamp off) vs variant + one L3-during-stamp take; Fable review in-game (rule 10); only PASS to the owner.
## Open
- Dash clip + ledge rule = new move thread (brief section "Update 2026-11-19"); dash window length open.
- Fists' curl axis unchecked on the real mesh; f27 hand-over unverified.
Cost so far: estimate ~12 $ (cloud Opus + one Fable review agent ~160k tokens + desktop RC authoring).
