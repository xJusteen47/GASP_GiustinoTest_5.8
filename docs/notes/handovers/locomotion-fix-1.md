# Locomotion fix from the owner's playtest (desktop, 2026-11-18) - commit 9f65c7fb, branch claude/desktop-jab-fk
Owner: crouch + forward = upright run, slower; run start frantic/sped up; skid not his clip.

## Causes and fixes
- Crouch: run start scramble (UGSTN_RunPresentationComponent::TickScramble) ends at 0.9 x the STANDING run max; a crouch walk never gets
  there, so the standing M_Neutral_Run_Loop_F stayed overlaid. Fix: crouched (Mover_IsCrouching) counts as off the ground for the scramble.
- Run start: the scramble IS the "frantic" start (run loop + extra lean). DA_AnimSet_Locomotion_Default RunStart.bEnabled = false -> GASP's start.
- Skid: profile clip SS_180_Turn_Root180 was stretched in three sections (hold 24-92%, restart squeezed in the last 8%). Owner choice A:
  play the clip at its own speed from the first skid frame to its end (UGSTN_SkidAnimInstance, cvar gstn.Skid.PlayAtClipSpeed=1, 0 = before).
  Presentation only: the sim's skid distance/time does NOT yet follow the clip (the "movement follows the clip" half of A is open).
- C++ applied by Live Coding in the owner's editor (log: Saved/Logs/GASP_Giustino2.log, "Live coding succeeded" 19:39:07 UTC).
  The DLLs on disk are OLD: build before the next editor start, or the crouch/skid fix is gone.

## Not done / open
- No PIE capture or review (owner playtesting directly; rule 10 review still owed for the skid at clip speed).
- Run start proper fix (owner): a dedicated start clip. GASP already has M_Neutral_Run_Start_F_Lfoot/Rfoot (and FL/FR/BL...) in
  Content/Characters/UEFN_Mannequin/Animations/Run; per the GASP ANIM rule add it as a raw row in CHT_GSTN_LocomotionAnimations + a state
  machine transition, if GASP's MM start is not enough. The brief docs/notes/mosse/run-start-stop-skid-brief.md does not exist in _ue59.
- Sim follows clip (A part 2): make the RunSkid layered move use the clip's root speed curve (deterministic table baked from the clip).

## Skid turns with the movement (owner 19:42Z) - NOT fixed here, next code thread
- Sim (FGSTN_LayeredMove_RunSkid::GenerateMove): facing held while sliding (AngularVelocity 0), then in Exit the ACTOR turns to the new
  direction over ExitFrames. The clip SS_180_Turn_Root180 also turns its root 180 and the layer does not use root motion: the actor turn and
  the clip turn add up (double turn), and with clip speed (A) the two are no longer in step. GASP's Offset Root Bone / orientation warping
  after the override slot are the other suspects (skid-main-graph-1 list).
- Proposed fix (A part 2, Opus high): during the skid the sim takes facing AND speed from the clip's root track (baked table, deterministic),
  the anim side strips the root yaw (or the actor does not turn), orientation warping/offset root bone off while the Skid slot is active.
- Tried: reading the GASP ABP graph through GSTN_AnimBlueprintToolset from Python failed once (list_anim_graphs, line 10); not retried.
