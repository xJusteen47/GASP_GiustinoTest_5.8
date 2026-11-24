# Crouch veloce, checkpoint 1 (cloud, Opus, 2026-11-18)
Branch `claude/project-thread-ymsnu8` from `claude/desktop-jab-fk` da26dfb3. NOT compiled (cloud).
Brief (identity traits, curve sheet, integration, desktop steps): `docs/notes/crouch-veloce-1-brief.md` = project files `handoffs/archivio/2026-11/crouch-veloce-1.md`.
Reference: project files `handoffs/archivio/2026-11/crouch-ref/` (owner's Ganondorf photo + 3 videos + grids; `grid_9542_in_out_30fps_t1.75.jpg` = timing:
down in 2 phone frames (~4 game frames), body+head overshoot then settle by 4 phone frames; up in 2-3, arms trailing, settled by 4).

## Code (this commit)
- `FGSTN_ReactionAnimRule::BlendOutProfile` (soft UBlendProfile): per-bone timing of the blend back to the character's own pose
  (follow-through: hips first, arms/head trail). Used only when returning to own animation (rule ends, or EndBehaviour BlendOut);
  a rule taking over still cross-fades evenly. Loaded with the set (`LoadedBlendOutProfiles`, by rule index).
  `StopActive` uses `FAnimMontageInstance::Stop(FMontageBlendSettings)` with the profile. Empty = unchanged behaviour.
- Purpose: air crouch exit (LT released, or landing) with follow-through without an exit clip, and no airborne tag needed.
  The same blend profile asset goes on the ground crouch transitions of ABP_GSTN_Locomotion (desktop).

## Desktop next (one thread, _ue59 lock, after the run-stop thread)
1. Build (Run-Timed Build). Tests: usual `GSTNCombat` set (no new test: montage blending needs PIE; check in capture).
2. Cheap BASE capture (rule 10): ground crouch in/out, air crouch in/out, landing with LT held.
3. Blend profile `BLP_GSTN_FollowThrough` on SK_UEFN_Mannequin's skeleton, Time Factor: pelvis/thighs/calves 0.5, spine 0.8, clavicles/
   upper arms 1.2, lower arms/hands/head 1.5 (starting values, Fable tunes).
4. Clips from the curve sheet (FK, local slerp; feet pinned): AS_GSTN_Crouch_{Enter,Idle,Exit}, AS_AirCrouch_Enter, new content in
   AS_AirCrouch_Loop_PLACEHOLDER (same path). Ground: chooser raw rows + state machine transitions with the blend profile, bFastCrouch setting.
5. DA_ReactionAnims (AirCrouch rule: Enter + BlendOutProfile): owner 23:48 says he edits nothing by hand, so free to edit; git status first.
6. Capture variant vs base (same frames), Fable review identity first. Max 3 pose rounds.

## Open
- Crouch walk keeps the GASP clips (bCrouchWalk ON); with the new low pose it may jar: review labels it "gioco" if so.
- `FMontageBlendSettings(float)` ctor + `Stop(FMontageBlendSettings)` assumed from UE 5.x AnimMontage.h: if the build disagrees, use
  `FMontageBlendSettings Settings; Settings.Blend.BlendTime = BlendSeconds;`.
Cost: ~110k context (Opus), no subagents.
